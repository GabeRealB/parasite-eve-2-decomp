#include "rooms/dryfield_saloon_g_r.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room_events.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_variants.h"

extern RoomEventActiveBytes gRoomEventActive;

/// The event message and request the gate latched for the event task, and the
/// flag saying one was latched this call.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;

/// Descriptor of the event task `roomEventTask`.
extern TaskDesc gRoomEventTaskDesc;

/// The room's message table, installed on the room task by its entry state.
extern TaskMessageEntry D_dryfield_saloon_g_r_8017ECBC[];

/// One view bitmask per effect, tested against `1 << view`: entries 0-10 gate
/// the sprites, 11 the beam and 12 the light shafts.
extern s16 D_dryfield_saloon_g_r_8017ED84[];

static void func_dryfield_saloon_g_r_8017D9CC(Task* task);
static void _dryfieldSaloonGRIdleTask(Task* task);

enum {
    DRYFIELD_SALOON_G_R_MESSAGE_USE_KEY_ITEM = 0x13F1,
};

static s32 _dryfieldSaloonGRRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
s32        func_dryfield_saloon_g_r_8017D99C(Task*, s32, s32, s32);
static s32 _dryfieldSaloonGRIgnoreRoomActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);

extern WorldCollisionGrid     D_dryfield_saloon_g_r_8017F780[1];
extern WorldCollisionOccluder D_dryfield_saloon_g_r_801817B0[2];
extern WorldCollisionTrigger  D_dryfield_saloon_g_r_80180EC8[16];
extern WorldCollisionTrigger  D_dryfield_saloon_g_r_80181388[14];
extern WorldCoordRoomLights   D_dryfield_saloon_g_r_80181AC8[1];

extern SpriteBatch  D_dryfield_saloon_g_r_8017F978[2];
extern SpriteBatch  D_dryfield_saloon_g_r_8017FADC[3];
extern SpriteBatch  D_dryfield_saloon_g_r_8017FC0C[4];
extern SpriteSource D_dryfield_saloon_g_r_8017F988[17];
extern SpriteSource D_dryfield_saloon_g_r_8017FAF4[14];

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskMessageEntry D_dryfield_saloon_g_r_8017ECBC[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, roomVariantSaloonMsg },
    { DRYFIELD_SALOON_G_R_MESSAGE_USE_KEY_ITEM, _dryfieldSaloonGRRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldSaloonGRIgnoreRoomActionMessage },
    { ROOM_MESSAGE_COMMAND, func_dryfield_saloon_g_r_8017D99C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// The room's effect positions in the model's local space. The frame hook
/// draws a sprite at each of 0-10; 12 and 13 are the two ends of the beam,
/// and 14-19 the roots and tip directions of the two light shafts.
SVECTOR gSaloonLightPoints[20] = {
    { 4915, -1870, -727, 0 },
    { 4915, -1870, -2339, 0 },
    { 4915, -1870, -4188, 0 },
    { -3905, -1870, -4460, 0 },
    { -100, -1870, 4785, 0 },
    { 1100, -1870, 4785, 0 },
    { 3500, -2097, 3000, 0 },
    { 3500, -2097, 2000, 0 },
    { 3500, -2097, 1000, 0 },
    { 3500, -2097, 0, 0 },
    { 4200, -2097, -500, 0 },
    { 4983, -2211, 4240, 0 },
    { 4983, -2211, 4467, 0 },
    { 4983, -2211, 4013, 0 },
    { 700, -1640, 1680, 0 },
    { 700, -1520, 1520, 0 },
    { 700, -1520, 1840, 0 },
    { -750, -1640, 1680, 0 },
    { -750, -1520, 1520, 0 },
    { -750, -1520, 1840, 0 },
};

s16 D_dryfield_saloon_g_r_8017ED84[14] = {
    128,
    12,
    4,
    8,
    48,
    48,
    144,
    144,
    128,
    128,
    128,
    144,
    48,
    1,
};

WorldCollisionRoomResources D_dryfield_saloon_g_r_8017EDA0[2] = {
    { D_dryfield_saloon_g_r_8017F780, D_dryfield_saloon_g_r_80180EC8, D_dryfield_saloon_g_r_80181388, D_dryfield_saloon_g_r_801817B0 },
    { D_dryfield_saloon_g_r_8017F780, D_dryfield_saloon_g_r_80180EC8, D_dryfield_saloon_g_r_80181388, D_dryfield_saloon_g_r_801817B0 },
};

u8 D_dryfield_saloon_g_r_8017EDC0[16] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    9,
    9,
    9,
    13,
    0,
    0,
    0,
};

u8* D_dryfield_saloon_g_r_8017EDD0[2] = {
    D_dryfield_saloon_g_r_8017EDC0,
    D_dryfield_saloon_g_r_8017EDC0,
};

ViewCount D_dryfield_saloon_g_r_8017EDD8[2] = { 13, 13 };

WorldCoordRoomLighting D_dryfield_saloon_g_r_8017EDDC[2] = {
    { D_dryfield_saloon_g_r_80181AC8, NULL },
    { D_dryfield_saloon_g_r_80181AC8, NULL },
};

DirectionWarpEntry D_dryfield_saloon_g_r_8017EDEC[2] = {
    { { { .word = 0 }, 2386, 0, -4661 }, { 0, 0, 0, 0 }, { { .word = 0 }, 2386, 0, -4661 }, { 0, 0, 0, 0 }, 0x52120002, 0x52120001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 479 },
    { { { .word = 3072 }, 4665, 0, 4567 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 4665, 0, 4567 }, { 0, 0, 0, 0 }, 0x52120004, 0x52120003, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, 478 },
};

static SVECTOR _gDryfieldSaloonGRCollision021C0Normals[9] = {
#include "assets/dryfield_saloon_g_r_collision_021C0_normals.inc"
};

static SVECTOR _gDryfieldSaloonGRCollision021C0Verts[116] = {
#include "assets/dryfield_saloon_g_r_collision_021C0_verts.inc"
};

static WorldCollisionGridFace _gDryfieldSaloonGRCollision021C0Faces[61] = {
#include "assets/dryfield_saloon_g_r_collision_021C0_faces.inc"
};

static s16 _gDryfieldSaloonGRCollision021C0Cells[280] = {
#include "assets/dryfield_saloon_g_r_collision_021C0_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldSaloonGRCollision021C0Cells[i])
static s16* _gDryfieldSaloonGRCollision021C0Table[12] = {
#include "assets/dryfield_saloon_g_r_collision_021C0_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_saloon_g_r_8017F780[1] = {
    { NULL, _gDryfieldSaloonGRCollision021C0Normals, _gDryfieldSaloonGRCollision021C0Verts, _gDryfieldSaloonGRCollision021C0Faces, _gDryfieldSaloonGRCollision021C0Table, 4500, 5400, 3, 4, 4000, 61 },
};

ViewCamera D_dryfield_saloon_g_r_8017F7A4[13] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x7530, -680 } }, 519 },
    { { { { -3183, 0, -2576 }, { -1148, 3666, 1418 }, { 2306, 1824, -2850 } }, { 315, 2637, 16 } }, 257 },
    { { { { -4045, 0, -638 }, { -348, 3430, 2210 }, { 534, 2237, -3388 } }, { 724, 3602, -2788 } }, 230 },
    { { { { 4063, 0, -511 }, { -261, 3522, -2074 }, { 439, 2090, 3494 } }, { 628, 3274, 3211 } }, 230 },
    { { { { 3790, 0, -1551 }, { -715, 3635, -1746 }, { 1377, 1887, 3364 } }, { 1033, 2561, -668 } }, 230 },
    { { { { -171, 0, 4092 }, { 3010, 2774, 126 }, { -2771, 3013, -116 } }, { -1881, 2483, -5950 } }, 188 },
    { { { { 3128, 0, -2643 }, { -887, 3858, -1050 }, { 2490, 1375, 2946 } }, { -1421, 2204, 1801 } }, 230 },
    { { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -620, 1520, -5560 } }, 230 },
    { { { { 702, 0, 4035 }, { 1935, 3594, -337 }, { -3540, 1964, 616 } }, { 2150, 1500, -3200 } }, 289 },
    { { { { 2919, 0, 2873 }, { 1386, 3587, -1408 }, { -2516, 1976, 2557 } }, { 1395, 2355, -2735 } }, 257 },
    { { { { 2919, 0, 2873 }, { 1386, 3587, -1408 }, { -2516, 1976, 2557 } }, { 1395, 2355, -2735 } }, 257 },
    { { { { 2919, 0, 2873 }, { 1386, 3587, -1408 }, { -2516, 1976, 2557 } }, { 1395, 2355, -2735 } }, 257 },
    { { { { 2919, 0, 2873 }, { 1429, 3553, -1451 }, { -2492, 2037, 2532 } }, { 1395, 2500, -2735 } }, 257 },
};

SpriteBatch D_dryfield_saloon_g_r_8017F978[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_saloon_g_r_8017F988[17] = {
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, 48, 875, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -136, 40, 875, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, -8, 850, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 40, 850, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -24, 48, 550, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 0, 88, 550, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 16, 96, 500, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, 16, 64, 525, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 72, 80, 500, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 72, 104, 500, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 120, 104, 500, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, 8, 1075, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 64, -16, 1075, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, -8, 1025, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, -24, 1075, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 0, 1050, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -16, 1050, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_saloon_g_r_8017FADC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_saloon_g_r_8017FAF4[14] = {
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, -16, 1200, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -80, 8, 1050, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -56, 16, 975, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -32, -16, 1250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, -32, 16, 975, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, 48, 112, 719, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 16 } }, 16, 104, 730, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -8, 104, 516, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -40, 96, 522, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -64, 96, 521, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -96, 88, 529, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -128, 88, 529, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -136, 96, 532, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -144, 112, 527, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_saloon_g_r_8017FC0C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_saloon_g_r_8017FC2C[56] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -32, -112, 1250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 16, -112, 1250, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 16 } }, -40, -56, 1250, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, -32, 1325, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 32, -32, 1325, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -32, -32, 1375, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, -16, 1325, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 40, 0, 1375, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -64, -16, 1325, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 0, 1325, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -48, 0, 1375, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 16 } }, -40, -16, 1375, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -48, 16, 1375, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -136, -32, 1350, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, -16, 1325, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, 0, 1325, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, -16, 1350, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -160, 0, 1325, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -160, 16, 1250, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -152, 24, 1250, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 112, 40 } }, -8, 80, 705, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 32 } }, -8, 48, 798, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 56, 8, 925, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 32 } }, -24, 16, 1000, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -24, 48, 925, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -48, 64, 925, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -56, 32, 925, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 88, -32, 1833, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 88, -48, 1615, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 88, -64, 1862, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 80, -64, 2213, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 80, -40, 1611, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 96, -64, 1775, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 96, -40, 1588, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 96, -24, 1724, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 104, -16, 1623, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 104, -40, 1414, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 104, -64, 1727, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 112, -56, 1608, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 112, -32, 1377, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 112, -8, 1540, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 120, 0, 1455, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 120, -24, 1509, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 120, -48, 1480, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 128, -48, 1424, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 128, -16, 1229, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 128, 8, 1378, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 136, 8, 1327, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 136, -16, 1213, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 136, -40, 1371, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 144, -40, 1323, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 144, -8, 1292, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 144, 16, 1059, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 152, 24, 997, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 152, 0, 1065, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 152, -32, 1251, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_saloon_g_r_8018008C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 2, 0 } },
    { 20, 7, 0, 0, { 1, 0 } },
    { 27, 29, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_saloon_g_r_801800B4[52] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -120, 250, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -72, 250, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 16, -24, 250, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -88, 250, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -32, 40, 250, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 8, 32, 250, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 48, 24, 250, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, 16, 250, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 8, 250, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 128, -48, 250, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, -88, 1125, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -56, -48, 1125, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -48, -32, 1200, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -48, -8, 1250, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -88, -88, 1125, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -88, -48, 1125, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -88, -32, 1250, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -88, -8, 1250, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 112, -40, 1150, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 32 } }, 88, -8, 1175, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -88, 1250, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -72, 1250, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -56, 1250, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -40, 1325, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -24, 1375, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -8, 1375, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 8, -88, 1250, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 8, -72, 1250, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 8, -56, 1312, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 8, -40, 1325, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 8, -8, 1375, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 8, 0, 1500, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 32, -88, 1250, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 32, -72, 1312, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, -56, 1325, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 32, -40, 1375, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, -8, 1500, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 8, -24, 1337, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 32, -24, 1500, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 8, -16, 1375, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 32, -16, 1500, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -96, 88, 500, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -48, 80, 500, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -24, 72, 500, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 0, 64, 500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 24, 56, 500, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 56, 48, 500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 80, 40, 500, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 104, 32, 500, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 80, 88, 625, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, 40, 500, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 128, 88, 500, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_saloon_g_r_801804C4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 1, 0 } },
    { 10, 31, 0, 0, { 2, 0 } },
    { 41, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_saloon_g_r_801804EC[91] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 96, 375, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 80, 375, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, 72, 375, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, 72, 375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, 72, 375, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, 72, 375, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, 72, 375, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -32, 80, 375, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -16, 80, 375, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 0, 80, 250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, 72, 250, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, 72, 250, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, 80, 250, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, 80, 250, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, 80, 250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, 72, 250, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, 80, 250, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 88, 250, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 88, 156, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, -56, 742, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -40, 750, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, -8, 689, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -80, -24, 843, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, -64, 547, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -64, 541, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -64, -64, 589, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -56, -56, 598, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -40, 645, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, -40, 723, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, -40, 686, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, -40, 751, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -72, -40, 576, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 56, 549, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, 24, 460, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 16, 384, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, 0, 386, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, -8, 354, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, -24, 301, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, -40, 295, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, -48, 293, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, -64, 276, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, -80, 261, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, -104, 247, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -136, -120, 299, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -160, -120, 258, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -152, -104, 267, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -120, -120, 325, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, -120, 562, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, -24, 641, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, -24, 748, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, -32, 698, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -64, -48, 649, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -64, 866, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -40, -88, 919, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -40, -120, 827, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -72, -120, 492, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -72, -88, 564, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -96, 625, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -120, 423, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -120, 453, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, -120, 490, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, -56, 246, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, -48, 259, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -32, 271, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, -16, 272, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, -8, 307, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 8, 328, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, -16, 239, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 8, 252, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, 32, 324, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 32, 260, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, 64, 262, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -48, 625, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -72, -8, 625, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -72, -24, 625, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, -32, 625, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -48, 625, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, -56, 625, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, -80, 500, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -96, -96, 625, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, 0, 731, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, -64, 625, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -80, 625, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, -72, 625, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -112, 24, 358, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -104, 32, 386, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 48, 420, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 56, 448, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, 40, 492, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -64, 64, 613, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 88, 274, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_saloon_g_r_80180C08[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 1, 0 } },
    { 19, 13, 0, 0, { 2, 0 } },
    { 32, 59, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_saloon_g_r_80180C30[19] = {
    { 143, 0x3FC0, { .fields = { 56, 80 } }, 104, -32, 550, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, 104, 48, 550, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 64, -8, 525, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 64, 64, 525, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 56 } }, 0, -16, 500, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 48 } }, 0, 40, 500, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 32 } }, 0, 88, 500, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, -32, -32, 500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -32, 48, 500, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -56, -40, 550, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -56, 40, 550, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -88, -40, 625, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -88, 32, 625, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -128, 8, 750, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -112, -32, 750, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -56, -80, 1550, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -56, -32, 1550, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -24, -32, 1550, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -24, -80, 1550, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_saloon_g_r_80180DAC[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 1, 0 } },
    { 15, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_saloon_g_r_80180DCC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_saloon_g_r_80180DDC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_saloon_g_r_80180DEC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_saloon_g_r_80180DFC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_saloon_g_r_80180E0C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_saloon_g_r_80180E1C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_saloon_g_r_80180E2C[13] = {
    { { .empty = D_dryfield_saloon_g_r_8017F978 }, D_dryfield_saloon_g_r_8017F978, NULL },
    { { .elements = D_dryfield_saloon_g_r_8017F988 }, D_dryfield_saloon_g_r_8017FADC, NULL },
    { { .elements = D_dryfield_saloon_g_r_8017FAF4 }, D_dryfield_saloon_g_r_8017FC0C, NULL },
    { { .elements = D_dryfield_saloon_g_r_8017FC2C }, D_dryfield_saloon_g_r_8018008C, NULL },
    { { .elements = D_dryfield_saloon_g_r_801800B4 }, D_dryfield_saloon_g_r_801804C4, NULL },
    { { .elements = D_dryfield_saloon_g_r_801804EC }, D_dryfield_saloon_g_r_80180C08, NULL },
    { { .elements = D_dryfield_saloon_g_r_80180C30 }, D_dryfield_saloon_g_r_80180DAC, NULL },
    { { .empty = D_dryfield_saloon_g_r_80180DCC }, D_dryfield_saloon_g_r_80180DCC, NULL },
    { { .empty = D_dryfield_saloon_g_r_80180DDC }, D_dryfield_saloon_g_r_80180DDC, NULL },
    { { .empty = D_dryfield_saloon_g_r_80180DEC }, D_dryfield_saloon_g_r_80180DEC, NULL },
    { { .empty = D_dryfield_saloon_g_r_80180DFC }, D_dryfield_saloon_g_r_80180DFC, NULL },
    { { .empty = D_dryfield_saloon_g_r_80180E0C }, D_dryfield_saloon_g_r_80180E0C, NULL },
    { { .empty = D_dryfield_saloon_g_r_80180E1C }, D_dryfield_saloon_g_r_80180E1C, NULL },
};

WorldCollisionTrigger D_dryfield_saloon_g_r_80180EC8[16] = {
    { NULL, NULL, NULL, { 2633, -1376, -1224, 0 }, { { -2531, -2400, -372, 0 }, { 2524, -2400, 366, 0 }, { -2531, 2400, -372, 0 }, { 2524, 2400, 366, 0 } }, { 592, 0, -4063, 0 }, { 0, 0, 4096, 0 }, 3500, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2782, -1248, -1346, 0 }, { { 2519, -2272, 364, 0 }, { -2534, -2272, -375, 0 }, { 2519, 2272, 364, 0 }, { -2534, 2272, -375, 0 } }, { -594, 0, 4058, 0 }, { 0, 0, 4096, 0 }, 3415, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2005, -1152, 53, 0 }, { { 1454, -2176, 297, 0 }, { -1453, -2176, -297, 0 }, { 1454, 2176, 297, 0 }, { -1453, 2176, -297, 0 } }, { -823, 0, 4019, 0 }, { 0, 0, 4096, 0 }, 2623, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1995, -1280, 172, 0 }, { { -1438, -2304, -264, 0 }, { 1439, -2304, 264, 0 }, { -1438, 2304, -264, 0 }, { 1439, 2304, 264, 0 } }, { 739, 0, -4031, 0 }, { 0, 0, 4096, 0 }, 2721, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1801, -1520, 3012, 0 }, { { -878, -2544, -837, 0 }, { 878, -2544, 837, 0 }, { -878, 2544, -837, 0 }, { 878, 2544, 837, 0 } }, { 2831, 0, -2972, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1185, -1632, 3294, 0 }, { { -299, -2656, 1167, 0 }, { 295, -2656, -1172, 0 }, { -299, 2656, 1167, 0 }, { 295, 2656, -1172, 0 } }, { -3977, 0, -1011, 0 }, { 0, 0, 4096, 0 }, 2907, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1913, -1456, 2983, 0 }, { { 801, -2480, 790, 0 }, { -800, -2480, -790, 0 }, { 801, 2480, 790, 0 }, { -800, 2480, -790, 0 } }, { -2881, 0, 2916, 0 }, { 0, 0, 4096, 0 }, 2721, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1377, -1344, 3391, 0 }, { { 300, -2368, -1106, 0 }, { -309, -2368, 1096, 0 }, { 300, 2368, -1106, 0 }, { -309, 2368, 1096, 0 } }, { 3955, 0, 1093, 0 }, { 0, 0, 4096, 0 }, 2623, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 431, -1552, 5183, 0 }, { { -1068, -2576, -9, 0 }, { 1069, -2576, 10, 0 }, { -1068, 2576, -9, 0 }, { 1069, 2576, 10, 0 } }, { 35, 0, -4101, 0 }, { 0, 0, 4096, 0 }, 2780, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 383, -1376, 5055, 0 }, { { 1069, -2400, 0, 0 }, { -1069, -2400, 0, 0 }, { 1069, 2400, 0, 0 }, { -1069, 2400, 0, 0 } }, { 0, 0, 4110, 0 }, { 0, 0, 4096, 0 }, 2623, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2906, -1200, 4312, 0 }, { { 1, -2224, -1068, 0 }, { -1, -2224, 1068, 0 }, { 1, 2224, -1068, 0 }, { -1, 2224, 1068, 0 } }, { 4096, 0, 3, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 7, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3066, -1216, 4376, 0 }, { { -1, -2240, 1068, 0 }, { 1, -2240, -1068, 0 }, { -1, 2240, 1068, 0 }, { 1, 2240, -1068, 0 } }, { -4104, 0, -6, 0 }, { 0, 0, 4096, 0 }, 2468, 0, 5, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 158, -1216, -3715, 0 }, { { 4, -2272, -2047, 0 }, { -13, -2272, 2033, 0 }, { 4, 2272, -2047, 0 }, { -13, 2272, 2033, 0 } }, { 4099, 0, 16, 0 }, { 0, 0, 4096, 0 }, 3050, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 303, -1216, -3729, 0 }, { { 11, -2272, 1935, 0 }, { -21, -2272, -1950, 0 }, { 11, 2272, 1935, 0 }, { -21, 2272, -1950, 0 } }, { -4121, 0, 33, 0 }, { 0, 0, 4096, 0 }, 2985, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2096, -1248, -193, 0 }, { { -1157, -2304, -193, 0 }, { 1157, -2304, 194, 0 }, { -1157, 2304, -193, 0 }, { 1157, 2304, 194, 0 } }, { 675, 0, -4046, 0 }, { 0, 0, 4096, 0 }, 2585, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2016, -1280, -289, 0 }, { { 1157, -2304, 194, 0 }, { -1157, -2304, -193, 0 }, { 1157, 2304, 194, 0 }, { -1157, 2304, -193, 0 } }, { -678, 0, 4044, 0 }, { 0, 0, 4096, 0 }, 2585, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_saloon_g_r_80181388[14] = {
    { NULL, NULL, NULL, { 2464, -48, -4720, 0 }, { { 624, 0, -272, 0 }, { 624, 0, 272, 0 }, { -624, 0, -272, 0 }, { -624, 0, 272, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 680, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 20, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4480, -48, 4368, 0 }, { { -448, 0, -624, 0 }, { 448, 0, -624, 0 }, { -448, 0, 624, 0 }, { 448, 0, 624, 0 } }, { 0, 4117, 0, 0 }, { -4096, 0, 0, 0 }, 768, WORLD_COLLISION_TRIGGER_ACTION_WARP, 19, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3232, -64, 3392, 0 }, { { 624, 0, -880, 0 }, { 624, 0, 880, 0 }, { -624, 0, -880, 0 }, { -624, 0, 880, 0 } }, { 0, 4099, 0, 0 }, { 4076, 0, 401, 0 }, 1078, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -32, -64, 1728, 0 }, { { 2032, 0, -1392, 0 }, { 2032, 0, 1392, 0 }, { -2032, 0, -1392, 0 }, { -2032, 0, 1392, 0 } }, { 0, 4102, 0, 0 }, { 4076, 0, 401, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4048, -64, -4672, 0 }, { { 896, 0, -848, 0 }, { 896, 0, 848, 0 }, { -896, 0, -848, 0 }, { -896, 0, 848, 0 } }, { 0, 4101, 0, 0 }, { -1380, 0, 3856, 0 }, 1227, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 544, -64, 6432, 0 }, { { 464, 0, -272, 0 }, { 464, 0, 272, 0 }, { -464, 0, -272, 0 }, { -464, 0, 272, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 535, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4416, -64, -1840, 0 }, { { 768, 0, -640, 0 }, { 768, 0, 640, 0 }, { -768, 0, -640, 0 }, { -768, 0, 640, 0 } }, { 0, 4095, 0, 0 }, { -4076, 0, -402, 0 }, 999, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2432, -64, -1120, 0 }, { { 768, 0, -1312, 0 }, { 768, 0, 1312, 0 }, { -768, 0, -1312, 0 }, { -768, 0, 1312, 0 } }, { 0, 4099, 0, 0 }, { 4095, 0, 0, 0 }, 1519, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4448, -64, 672, 0 }, { { 768, 0, -1088, 0 }, { 768, 0, 1088, 0 }, { -768, 0, -1088, 0 }, { -768, 0, 1088, 0 } }, { 0, 4102, 0, 0 }, { -4091, 0, 200, 0 }, 1330, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3680, -64, 1616, 0 }, { { 480, 0, -2016, 0 }, { 480, 0, 2016, 0 }, { -480, 0, -2016, 0 }, { -480, 0, 2016, 0 } }, { 0, 4095, 0, 0 }, { 4095, 0, 0, 0 }, 2063, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2544, -64, 4928, 0 }, { { 1296, 0, -560, 0 }, { 1296, 0, 560, 0 }, { -1296, 0, -560, 0 }, { -1296, 0, 560, 0 } }, { 0, 4101, 0, 0 }, { -201, 0, -4091, 0 }, 1408, WORLD_COLLISION_TRIGGER_ACTION_CAP, 16, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1184, -64, 4320, 0 }, { { 752, 0, -560, 0 }, { 752, 0, 560, 0 }, { -752, 0, -560, 0 }, { -752, 0, 560, 0 } }, { 0, 4106, 0, 0 }, { -201, 0, -4091, 0 }, 936, WORLD_COLLISION_TRIGGER_ACTION_CAP, 17, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2784, -64, 3680, 0 }, { { 976, 0, -560, 0 }, { 976, 0, 560, 0 }, { -976, 0, -560, 0 }, { -976, 0, 560, 0 } }, { 0, 4105, 0, 0 }, { -201, 0, -4091, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4416, -64, 2624, 0 }, { { 768, 0, -1088, 0 }, { 768, 0, 1088, 0 }, { -768, 0, -1088, 0 }, { -768, 0, 1088, 0 } }, { 0, 4102, 0, 0 }, { -4091, 0, 200, 0 }, 1330, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_dryfield_saloon_g_r_801817B0[2] = {
    { NULL, NULL, { -1056, -1392, 5216, 0 }, { { -1024, -2224, 0, 0 }, { 1024, -2224, 0, 0 }, { -1024, 2224, 0, 0 }, { 1024, 2224, 0, 0 } }, { 0, 0, -4109, 0 }, 2442, 1, 0 },
    { NULL, NULL, { 2048, -1376, 5215, 0 }, { { -1024, -2224, 0, 0 }, { 1024, -2224, 0, 0 }, { -1024, 2224, 0, 0 }, { 1024, 2224, 0, 0 } }, { 0, 0, -4109, 0 }, 2442, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCoordPointLight D_dryfield_saloon_g_r_80181828[7] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 0x186A0, 0x186A0 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 570, -1500, 5040 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 2867, 2048 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1210, -1500, -2790 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2560, -1500, -3820 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3890, -1600, 1450 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3031, 2457 }, { 0, 0 } }, 2000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4750, -1400, 4320 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3686, 3686 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2830, -1000, 3150 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3276, 2048 }, { 0, 0 } }, 500, 2000 },
};

WorldCoordRoomLights D_dryfield_saloon_g_r_80181AC8[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_saloon_g_r_80181828), D_dryfield_saloon_g_r_80181828, 0, NULL },
};

AreaResource D_dryfield_saloon_g_r_80181AE0[2] = {
    { 15, 15, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor01500_D0A008 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_saloon_g_r_80181AF8[3] = {
    { 15, 15, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor01500_D0A008 },
    { 12, 12, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_201200_80150E98 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_saloon_g_r_80181B1C[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017B394, D_dryfield_saloon_g_r_80181AE0 },
    { D_map_dryfield_8017B3F4, D_dryfield_saloon_g_r_80181AF8 },
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
};

WorldCollisionFootstepSounds D_dryfield_saloon_g_r_80181B84 = {
    0x10000035,
    0x10000037,
    0x10000035,
};

WorldCollisionFootstepSounds D_dryfield_saloon_g_r_80181B90 = {
    0x10000001,
    0x10000003,
    0x10000001,
};

WorldCollisionSurfaceProperties D_dryfield_saloon_g_r_80181B9C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_saloon_g_r_80181BA4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_saloon_g_r_80181B84 },
};

WorldCollisionSurfaceProperties D_dryfield_saloon_g_r_80181BAC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_saloon_g_r_80181B90 },
};

WorldCollisionSurfaceProperties D_dryfield_saloon_g_r_80181BB4[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_saloon_g_r_80181B90 },
};

WorldCollisionSurfaceProperties* D_dryfield_saloon_g_r_80181BBC[8] = {
    D_dryfield_saloon_g_r_80181B9C,
    D_dryfield_saloon_g_r_80181BA4,
    D_dryfield_saloon_g_r_80181BAC,
    D_dryfield_saloon_g_r_80181BB4,
    D_dryfield_saloon_g_r_80181B9C,
    D_dryfield_saloon_g_r_80181B9C,
    D_dryfield_saloon_g_r_80181B9C,
    D_dryfield_saloon_g_r_80181B9C,
};

RoomEventMsg gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 32, 1, 0 } };

RoomEventReq gRoomEventReq;

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

/// The room task's three-state table, run from a stack copy by
/// `func_dryfield_saloon_g_r_8017DA18`: the entry tick
/// `func_dryfield_saloon_g_r_8017D9CC`, the idle state
/// `_dryfieldSaloonGRIdleTask`, then `taskKill`.
static const TaskFuncTable3 D_dryfield_saloon_g_r_8017D5DC = {
    { func_dryfield_saloon_g_r_8017D9CC, _dryfieldSaloonGRIdleTask, taskKill },
};

#include "../../shared/room_variants_saloon.inc.c"

static void _glowDrawFlareLocal(const GfxCoord* coord, const SVECTOR* localPoint, s32 textureIndex, s32 radiusScale);

/// Refuses every key-item use request in the saloon.
///
/// Ignores the selected item ID and returns zero, so the item menu reports
/// that the item cannot be used here. Does not consume or retain any payload.
static s32 _dryfieldSaloonGRRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    enum {
        DRYFIELD_SALOON_G_R_KEY_ITEM_UNUSABLE = 0,
    };

    return DRYFIELD_SALOON_G_R_KEY_ITEM_UNUSABLE;
}

/// Handler for message 0x13F0 in the room's message table: on action 4 it
/// runs cap command 4. Always returns 0.
s32 func_dryfield_saloon_g_r_8017D99C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 4) {
        Gp_RunCapCmd1(4);
    }
    return 0;
}

/// Leaves trigger-driven room-action requests without an action in the saloon.
///
/// The synchronous sender supplies a borrowed request and a zero second
/// payload word. Neither payload is read or retained. Returns zero; the
/// direction dispatcher discards the result.
static s32 _dryfieldSaloonGRIgnoreRoomActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Entry state of the room task: installs the room's message table, registers
/// the task in pointer slot 7 and advances to the idle state.
static void func_dryfield_saloon_g_r_8017D9CC(Task* task)
{
    task->msgTable = D_dryfield_saloon_g_r_8017ECBC;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// Keeps the saloon room task idle while its message table remains installed.
///
/// This state performs no frame work and leaves the task's state unchanged.
static void _dryfieldSaloonGRIdleTask(Task* task)
{
}

/// The room task: runs the state the task is in from a stack copy of the
/// room's three-state table.
void func_dryfield_saloon_g_r_8017DA18(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_saloon_g_r_8017D5DC;
    sp.funcs[task->state](task);
}

void dryfieldSaloonGRDrawLightEffectsTask(Task* task)
{
    enum {
        DRYFIELD_SALOON_G_R_FIRST_FLARE_GROUP_END       = 6,
        DRYFIELD_SALOON_G_R_FLARE_COUNT                 = 11,
        DRYFIELD_SALOON_G_R_FIRST_FLARE_TEXTURE_COLUMN  = 0,
        DRYFIELD_SALOON_G_R_SECOND_FLARE_TEXTURE_COLUMN = 2,
        DRYFIELD_SALOON_G_R_FLARE_RADIUS_SCALE          = 512,
        DRYFIELD_SALOON_G_R_SHAFT_VIEW_MASK_INDEX       = 12,
        DRYFIELD_SALOON_G_R_BEAM_VIEW_MASK_INDEX        = 11,
        DRYFIELD_SALOON_G_R_BEAM_START_POINT            = 13,
        DRYFIELD_SALOON_G_R_BEAM_END_POINT              = 12,
        DRYFIELD_SALOON_G_R_BEAM_RADIUS_SCALE           = 256,
    };

    const GfxCoord* roomCoord;
    s32             viewMask;
    s32             flareIndex;

    /// Draws a half-open group of view-visible flares using one atlas column.
    ///
    /// Captures roomCoord, viewMask, flareIndex, the room's point/mask tables
    /// and the fixed flare scale. Writes flareIndex through the range's end.
    /// Arguments must be pure signed-integer values with 0 <= firstPoint <=
    /// pointLimit <= DRYFIELD_SALOON_G_R_FLARE_COUNT: firstPoint is evaluated
    /// once, pointLimit on every loop test and textureColumn for each visible
    /// point. Expands to one for statement; invoke inside a braced block.
#define DRYFIELD_SALOON_G_R_DRAW_FLARE_RANGE(firstPoint, pointLimit, textureColumn)       \
    for (flareIndex = (firstPoint); flareIndex < (pointLimit); flareIndex++) {            \
        if (viewMask & D_dryfield_saloon_g_r_8017ED84[flareIndex]) {                      \
            _glowDrawFlareLocal(roomCoord, &gSaloonLightPoints[flareIndex],               \
                                (textureColumn), DRYFIELD_SALOON_G_R_FLARE_RADIUS_SCALE); \
        }                                                                                 \
    }

    roomCoord = task->extra.coordBody->coord;
    viewMask  = 1 << gGameSession->location.loc.view;

    // The two flare groups use separate columns of the same texture atlas.
    DRYFIELD_SALOON_G_R_DRAW_FLARE_RANGE(0, DRYFIELD_SALOON_G_R_FIRST_FLARE_GROUP_END,
                                         DRYFIELD_SALOON_G_R_FIRST_FLARE_TEXTURE_COLUMN);
    DRYFIELD_SALOON_G_R_DRAW_FLARE_RANGE(DRYFIELD_SALOON_G_R_FIRST_FLARE_GROUP_END, DRYFIELD_SALOON_G_R_FLARE_COUNT,
                                         DRYFIELD_SALOON_G_R_SECOND_FLARE_TEXTURE_COLUMN);
#undef DRYFIELD_SALOON_G_R_DRAW_FLARE_RANGE

    // Shafts share a visibility mask; endpoint order fixes the beam's orientation.
    if (viewMask & D_dryfield_saloon_g_r_8017ED84[DRYFIELD_SALOON_G_R_SHAFT_VIEW_MASK_INDEX]) {
        _glowDrawTwinShafts(roomCoord);
    }
    if (viewMask & D_dryfield_saloon_g_r_8017ED84[DRYFIELD_SALOON_G_R_BEAM_VIEW_MASK_INDEX]) {
        _glowDrawTaperedBeam(roomCoord, &gSaloonLightPoints[DRYFIELD_SALOON_G_R_BEAM_START_POINT],
                             &gSaloonLightPoints[DRYFIELD_SALOON_G_R_BEAM_END_POINT], DRYFIELD_SALOON_G_R_BEAM_RADIUS_SCALE);
    }
}

#include "../../shared/glow_draw_flare_local.inc.c"

#include "../../shared/glow_draw_twin_shafts.inc.c"

#include "../../shared/glow_draw_tapered_beam.inc.c"
