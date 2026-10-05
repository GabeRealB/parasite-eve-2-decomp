#include "rooms/mine_secret_passage.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "mine_secret_passage_private.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag_ids.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"

/// The passage's per-view emitter placements, one `SVECTOR` per position, 8
/// bytes apart. All four names address the same 24-entry run: `ED8` is `EC8[2]`,
/// `EE8` is `EC8[4]` and `F08` is `EC8[8]`, so view 2's last two positions are
/// `F08[8]` / `F08[9]` and view 4's last is `F08[15]`.
extern SVECTOR D_mine_secret_passage_80180EC8[];
extern SVECTOR D_mine_secret_passage_80180ED8[];
extern SVECTOR D_mine_secret_passage_80180EE8[];
extern SVECTOR D_mine_secret_passage_80180F08[];

/// Shift per colour channel for each of the halo's tints, indexed by the tint
/// selector the spawn argument carries.

extern WorldCollisionGrid         D_mine_secret_passage_801815E0[1];
extern WorldCollisionOccluder     D_mine_secret_passage_801831A8[2];
extern WorldCollisionTrigger      D_mine_secret_passage_80182DCC[10];
extern WorldCollisionTrigger      D_mine_secret_passage_801830C4[3];
extern WorldCoordRoomAmbientEntry D_mine_secret_passage_801833A0[9];
extern WorldCoordRoomLights       D_mine_secret_passage_80182DB4[1];

TaskMessageEntry D_mine_secret_passage_80180E8C[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_mine_secret_passage_8017D7CC },
    { 5105, func_mine_secret_passage_8017D7C4 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_mine_secret_passage_8017D890 },
    { ROOM_MESSAGE_COMMAND, func_mine_secret_passage_8017D888 },
    { ROOM_MESSAGE_SOUND, func_mine_secret_passage_8017D898 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_mine_secret_passage_80180EBC = { { { TASK_BODY_NONE, 32 } }, func_mine_secret_passage_8017D60C, { .value = 0 } };

SVECTOR D_mine_secret_passage_80180EC8[2] = {
    { 80, -3140, 0x3692, 0 },
    { 80, -3140, 0x33FE, 0 },
};

SVECTOR D_mine_secret_passage_80180ED8[2] = {
    { 670, -1780, 1870, 0 },
    { 1010, -1780, 1870, 0 },
};

SVECTOR D_mine_secret_passage_80180EE8[4] = {
    { 0x4916, -2390, 4270, 0 },
    { 0x4AE2, -2390, 4270, 0 },
    { 0x4E5C, -2390, 4270, 0 },
    { 0x501E, -2390, 4270, 0 },
};

SVECTOR D_mine_secret_passage_80180F08[16] = {
    { 0x4F10, -990, 4690, 0 },
    { 0x5000, -990, 4690, 0 },
    { 0x3A48, -2570, 4090, 0 },
    { 0x411E, -2570, 4090, 0 },
    { 0x3A48, -2570, 2340, 0 },
    { 0x411E, -2570, 2340, 0 },
    { 0x4876, 310, 1950, 0 },
    { 0x50B4, 310, 1950, 0 },
    { -60, -2130, 8770, 0 },
    { 3080, -2130, 8770, 0 },
    { -60, -2130, 5300, 0 },
    { 3080, -2130, 5300, 0 },
    { 3520, -2130, 4880, 0 },
    { 7740, -2130, 4880, 0 },
    { 3520, -2130, 2010, 0 },
    { 7740, -2130, 2010, 0 },
};

#define ROOM_FX_HALO_STORAGE_INITIALIZER { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 0x3D9B }
#define ROOM_FX_HALO_STORAGE_TYPE        RoomFxHaloStorage
#define ROOM_FX_HALO_STORAGE_BOUND
#include "../../shared/room_visual_effects_halo_data.inc.c"

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomEffectHaloShades.entries;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

WorldCoordRoomLighting D_mine_secret_passage_80180F9C[1] = {
    { D_mine_secret_passage_80182DB4, D_mine_secret_passage_801833A0 },
};

WorldCollisionRoomResources D_mine_secret_passage_80180FA4[1] = {
    { D_mine_secret_passage_801815E0, D_mine_secret_passage_80182DCC, D_mine_secret_passage_801830C4, D_mine_secret_passage_801831A8 },
};

u8* D_mine_secret_passage_80180FB4[1] = {
    gViewIdentityMap,
};

ViewCount D_mine_secret_passage_80180FB8[1] = { 8 };

DirectionWarpEntry D_mine_secret_passage_80180FBC[2] = {
    { { { .word = 2048 }, 1400, 0, 0x36B0 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 1400, 0, 0x36B0 }, { 0, 0, 0, 0 }, 0x54080002, 0x54080001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_MINE_CAVERN },
    { { { .word = 3072 }, 0x5014, 0, 3900 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x5014, 0, 3900 }, { 0, 0, 0, 0 }, 0x54080004, 0x54080003, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_FADE_DEPARTURE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gMineSecretPassageCollision04020Normals[21] = {
#include "assets/mine_secret_passage_collision_04020_normals.inc"
};

static SVECTOR _gMineSecretPassageCollision04020Verts[66] = {
#include "assets/mine_secret_passage_collision_04020_verts.inc"
};

static WorldCollisionGridFace _gMineSecretPassageCollision04020Faces[27] = {
#include "assets/mine_secret_passage_collision_04020_faces.inc"
};

static s16 _gMineSecretPassageCollision04020Cells[172] = {
#include "assets/mine_secret_passage_collision_04020_cells.inc"
};

#define GRID_CELL(i) (&_gMineSecretPassageCollision04020Cells[i])
static s16* _gMineSecretPassageCollision04020Table[24] = {
#include "assets/mine_secret_passage_collision_04020_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_mine_secret_passage_801815E0[1] = {
    { NULL, _gMineSecretPassageCollision04020Normals, _gMineSecretPassageCollision04020Verts, _gMineSecretPassageCollision04020Faces, _gMineSecretPassageCollision04020Table, 1210, -30, 6, 4, 4000, 27 },
};

ViewCamera D_mine_secret_passage_80181604[9] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x2710, 0x5910, -7500 } }, 269 },
    { { { { 4008, 0, -844 }, { -11, 4095, -55 }, { 844, 57, 4007 } }, { -599, 1432, -5060 } }, 282 },
    { { { { -3964, 0, -1029 }, { -49, 4091, 188 }, { 1028, 195, -3960 } }, { -800, 1568, -0x31FF } }, 282 },
    { { { { 858, 0, 4005 }, { 148, 4093, -31 }, { -4002, 152, 857 } }, { -0x3C28, 1299, -2749 } }, 282 },
    { { { { 3579, 0, 1991 }, { -105, 4090, 189 }, { -1988, -216, 3574 } }, { -0x493C, 972, 3153 } }, 282 },
    { { { { 3616, 0, 1923 }, { 1402, 2803, -2636 }, { -1316, 2986, 2474 } }, { -0x5022, 5312, 870 } }, 282 },
    { { { { 744, 0, 4027 }, { 477, 4067, -88 }, { -3999, 485, 738 } }, { -8848, 1939, -2749 } }, 282 },
    { { { { 4049, 0, -616 }, { -402, 3099, -2647 }, { 466, 2678, 3063 } }, { -0x4F3F, 2207, -2780 } }, 505 },
    { { { { 4008, 0, -844 }, { -11, 4095, -55 }, { 844, 57, 4007 } }, { -599, 1432, -5060 } }, 282 },
};

SpriteBatch D_mine_secret_passage_80181748[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mine_secret_passage_80181758[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_secret_passage_80181768[48] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, -40, 1940, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, -32, 1983, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, 16, 2017, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -160, -120, 889, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -160, -64, 867, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -160, -8, 866, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -160, 56, 841, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -120, 48, 952, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -120, -16, 1024, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -120, -72, 1020, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, -56, 1127, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -96, -16, 1132, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, -96, 1122, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, -120, 1118, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -120, -104, 1052, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -120, -120, 1050, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, -120, 1407, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -72, -80, 1385, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -72, -16, 1327, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, 40, 1109, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, 72, 1043, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, 40, 1170, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 40, 1344, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 40, 1362, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, 64, 1151, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -80, 56, 1177, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, 64, 1078, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, -72, 1629, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, -16, 1425, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 48, 1484, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 40, 1530, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, 40, 1744, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, 24, 1849, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, 24, 1941, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -16, 0, 1941, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, -80, 1674, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, -80, 1898, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, -80, 1969, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, -64, 1908, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -32, -16, 1907, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, -56, 2896, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, -24, 1988, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -56, -120, 1497, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, -96, 1637, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, -120, 1649, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -96, 1849, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, -120, 1937, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, -96, 1937, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_secret_passage_80181B28[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 48, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_secret_passage_80181B40[50] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, -48, 3070, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, -8, 3071, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, -80, 3070, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 72 } }, -16, -96, 3071, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, 0, 2765, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -8, -104, 2765, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 88 } }, 0, -120, 2765, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 88 } }, 40, -120, 2685, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 80 } }, 0, -32, 2765, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 104 } }, 40, -32, 2685, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 88, 730, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 88, 755, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 88, -120, 968, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 88, -16, 973, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 96, -120, 926, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 96, -16, 936, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 104, -120, 900, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, 104, -16, 914, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 72, -120, 1073, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 80, -120, 1021, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 72, -16, 1087, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 80, -16, 1027, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 120, -80, 889, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 56 } }, 128, -80, 884, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -120, 916, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 120, -24, 921, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 120, 24, 926, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, 40, 909, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 144, 40, 949, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 128, 24, 866, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 144, 24, 796, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -104, -88, 920, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -104, 8, 926, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -112, -120, 846, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -112, -16, 830, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -120, -120, 778, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -120, -8, 784, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -128, -120, 754, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -128, -8, 764, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 56 } }, -160, -120, 741, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -144, -64, 749, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -160, -64, 729, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 56 } }, -160, -40, 748, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -144, 16, 756, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -160, 16, 736, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -152, 32, 691, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, 32, 624, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -160, 56, 740, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -160, 96, 733, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, -144, 40, 760, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_secret_passage_80181F28[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 1, 0 } },
    { 10, 40, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_secret_passage_80181F48[55] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -8, 16, 1431, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, 16, 1409, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -128, 16, 1943, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -120, 16, 1830, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -112, 16, 1790, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -104, 16, 1725, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -96, 16, 1700, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -88, 16, 1674, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -80, 16, 1641, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, 16, 1575, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -64, 16, 1555, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -48, 16, 1542, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -40, 16, 1525, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -32, 16, 1516, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -32, 40, 1479, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -24, 16, 1454, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -16, 16, 1450, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 0, 32, 1386, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 8, 16, 1368, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, 16, 1368, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 112, 8, 1311, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 120, 8, 1188, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 128, 8, 1153, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 144, 8, 1123, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -24, 24, 1012, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -16, 24, 972, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 0, 24, 950, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 8, 24, 912, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 24, 24, 877, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 40, 24, 875, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 48, 24, 881, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 56, 24, 911, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 64, 24, 950, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 72, 16, 1058, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 80, 16, 1156, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 88, 16, 1178, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 48, 1165, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 56, 1019, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -16, 24, 1064, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -8, 24, 1108, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 0, 16, 1163, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 8, 16, 1245, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 16, 16, 1302, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 24, 16, 1389, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 88 } }, -160, -120, 1538, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -112, -64, 1720, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -104, -64, 1736, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -96, -64, 1771, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, -40, 1822, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 80 } }, -160, -32, 1525, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 72 } }, -160, 48, 1515, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 88 } }, -120, -40, 3125, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 88 } }, -80, -40, 3125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, 144, 24, 1138, { .fields = { 88, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 48 } }, 120, 24, 1174, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_mine_secret_passage_80182394[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 3, 0 } },
    { 20, 4, 0, 0, { 4, 0 } },
    { 24, 13, 0, 0, { 1, 0 } },
    { 37, 7, 0, 0, { 5, 0 } },
    { 44, 7, 0, 0, { 0, 0 } },
    { 51, 2, 0, 0, { 6, 0 } },
    { 53, 2, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_secret_passage_801823DC[55] = {
    { 143, 0x3FC0, { .fields = { 40, 72 } }, -88, -120, 1000, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, -48, -120, 1000, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 88 } }, -160, -120, 1000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 48, 1372, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, 48, 1350, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, 56, 1334, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, 56, 1322, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, 64, 1301, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -120, 64, 1298, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -112, 56, 1311, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -104, 48, 1331, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -96, 40, 1362, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -88, 32, 1386, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -80, 24, 1430, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -72, 16, 1462, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -64, 8, 1495, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, 8, 1502, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, 16, 1488, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 40, 1416, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -152, 32, 1450, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, 24, 1484, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -136, 16, 1511, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -128, 8, 1537, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, 0, 1568, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -112, -8, 1610, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, -8, 1647, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 8, 1684, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 8, 1596, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, -32, 1725, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -152, -32, 1705, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, -24, 1689, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, -24, 1671, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, -16, 1650, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, -16, 1629, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, -16, 1611, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -40, -8, 1584, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, 16, 1432, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -24, 16, 1405, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -8, 16, 1379, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 8, 24, 1352, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 24, 32, 1331, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, 40, 1307, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 56, 48, 1280, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, 56, 1251, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, 56, 1244, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 56, 1380, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, 48, 1423, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -32, 16, 1493, { .fields = { 8, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 32 } }, -16, 24, 1448, { .fields = { 0, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 32 } }, 8, 40, 1415, { .fields = { 24, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 32 } }, 24, 40, 1386, { .fields = { 24, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 32 } }, 40, 48, 1367, { .fields = { 16, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 32 } }, 56, 56, 1340, { .fields = { 24, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 32 } }, 72, 64, 1323, { .fields = { 8, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 24 } }, 88, 72, 1296, { .fields = { 56, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_mine_secret_passage_80182828[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 3, 0 } },
    { 3, 15, 0, 0, { 0, 0 } },
    { 18, 9, 0, 0, { 5, 0 } },
    { 27, 8, 0, 0, { 1, 0 } },
    { 35, 12, 0, 0, { 4, 0 } },
    { 47, 8, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_secret_passage_80182868[13] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -48, 1397, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 8, 1397, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 72, -120, 1398, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 72, 32, 1398, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 72, -40, 1398, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 80 } }, 112, -120, 1398, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, 112, -40, 1398, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 88 } }, 112, 32, 1398, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 56, -120, 1397, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -40, 1397, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 48, 8, 1397, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 40, 1397, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, -80, 1432, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_secret_passage_8018296C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mine_secret_passage_80182984[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_mine_secret_passage_80182994[8] = {
    { { .empty = D_mine_secret_passage_80181748 }, D_mine_secret_passage_80181748, NULL },
    { { .empty = D_mine_secret_passage_80181758 }, D_mine_secret_passage_80181758, NULL },
    { { .elements = D_mine_secret_passage_80181768 }, D_mine_secret_passage_80181B28, NULL },
    { { .elements = D_mine_secret_passage_80181B40 }, D_mine_secret_passage_80181F28, NULL },
    { { .elements = D_mine_secret_passage_80181F48 }, D_mine_secret_passage_80182394, NULL },
    { { .elements = D_mine_secret_passage_801823DC }, D_mine_secret_passage_80182828, NULL },
    { { .elements = D_mine_secret_passage_80182868 }, D_mine_secret_passage_8018296C, NULL },
    { { .empty = D_mine_secret_passage_80182984 }, D_mine_secret_passage_80182984, NULL },
};

WorldCoordPointLight D_mine_secret_passage_801829F4[10] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 527, -2289, 0x3098 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1301, 3600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -246, -2470, 4900 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4045, 2789, 1583 }, { 0, 0 } }, 0, 3600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2879, -2470, 8762 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4045, 2789, 1583 }, { 0, 0 } }, 0, 3600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3063, -2690, 4838 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4045, 2789, 1583 }, { 0, 0 } }, 0, 4700 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8663, -2470, 4778 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4045, 2789, 1583 }, { 0, 0 } }, 0, 4800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1728, -1859, 3259 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1261, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3F20, -2470, 2820 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 0, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4D52, -2250, 2756 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 0, 3600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -246, -2470, 8762 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4045, 2789, 1583 }, { 0, 0 } }, 0, 3600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7287, -2470, 2020 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4045, 2789, 1583 }, { 0, 0 } }, 0, 4800 },
};

WorldCoordRoomLights D_mine_secret_passage_80182DB4[1] = {
    { 0, NULL, ARRAY_SIZE(D_mine_secret_passage_801829F4), D_mine_secret_passage_801829F4, 0, NULL },
};

WorldCollisionTrigger D_mine_secret_passage_80182DCC[10] = {
    { NULL, NULL, NULL, { 1488, -1504, 9344, 0 }, { { 2032, -1904, 0, 0 }, { -2032, -1904, 0, 0 }, { 2032, 1904, 0, 0 }, { -2032, 1904, 0, 0 } }, { 0, 0, 4102, 0 }, { 0, 0, 4096, 0 }, 2780, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1520, -1505, 9552, 0 }, { { -2000, -1904, 16, 0 }, { 2000, -1904, -16, 0 }, { -2000, 1904, 16, 0 }, { 2000, 1904, -16, 0 } }, { -34, 0, -4095, 0 }, { 0, 0, 4096, 0 }, 2757, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5101, -1568, 3435, 0 }, { { -467, -1904, -2668, 0 }, { 464, -1904, 2665, 0 }, { -467, 1904, -2668, 0 }, { 464, 1904, 2665, 0 } }, { 4047, 0, -708, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 4, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5352, -1600, 3467, 0 }, { { 435, -1904, 2702, 0 }, { -442, -1904, -2708, 0 }, { 435, 1904, 2702, 0 }, { -442, 1904, -2708, 0 } }, { -4044, 0, 655, 0 }, { 0, 0, 4096, 0 }, 3328, 0, 7, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2E60, -1441, 3328, 0 }, { { 0, -1904, 2144, 0 }, { 0, -1904, -2144, 0 }, { 0, 1904, 2144, 0 }, { 0, 1904, -2144, 0 } }, { -4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2862, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2D80, -1601, 3296, 0 }, { { 0, -1904, -2000, 0 }, { 0, -1904, 2000, 0 }, { 0, 1904, -2000, 0 }, { 0, 1904, 2000, 0 } }, { 4104, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2757, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x4371, -1440, 4000, 0 }, { { 624, -1904, -2000, 0 }, { -624, -1904, 2000, 0 }, { 624, 1904, -2000, 0 }, { -624, 1904, 2000, 0 } }, { 3912, 0, 1220, 0 }, { 0, 0, 4096, 0 }, 2827, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x4411, -1504, 4016, 0 }, { { -624, -1904, 2000, 0 }, { 624, -1904, -2000, 0 }, { -624, 1904, 2000, 0 }, { 624, 1904, -2000, 0 } }, { -3914, 0, -1222, 0 }, { 0, 0, 4096, 0 }, 2827, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1312, -1536, 4847, 0 }, { { -2721, -1904, -431, 0 }, { 2719, -1904, 430, 0 }, { -2721, 1904, -431, 0 }, { 2719, 1904, 430, 0 } }, { 639, 0, -4046, 0 }, { 0, 0, 4096, 0 }, 3347, 0, 7, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1312, -1440, 4671, 0 }, { { 2719, -1904, 430, 0 }, { -2721, -1904, -431, 0 }, { 2719, 1904, 430, 0 }, { -2721, 1904, -431, 0 } }, { -641, 0, 4044, 0 }, { 0, 0, 4096, 0 }, 3347, 0, 3, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_mine_secret_passage_801830C4[3] = {
    { NULL, NULL, NULL, { 1440, -48, 0x33F0, 0 }, { { -1472, 0, -496, 0 }, { 1472, 0, -496, 0 }, { -1472, 0, 496, 0 }, { 1472, 0, 496, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 1551, WORLD_COLLISION_TRIGGER_ACTION_WARP, 2, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x50F0, -48, 4272, 0 }, { { -720, 0, -544, 0 }, { 720, 0, -544, 0 }, { -720, 0, 544, 0 }, { 720, 0, 544, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -4096, 0 }, 900, WORLD_COLLISION_TRIGGER_ACTION_WARP, 9, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x42A0, -64, 800, 0 }, { { -672, 0, -560, 0 }, { 672, 0, -560, 0 }, { -672, 0, 560, 0 }, { 672, 0, 560, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_mine_secret_passage_801831A8[2] = {
    { NULL, NULL, { 0x2990, -1584, 8864, 0 }, { { -7408, -2608, -3648, 0 }, { 7408, -2608, 3648, 0 }, { -7408, 2608, -3648, 0 }, { 7408, 2608, 3648, 0 } }, { 1812, 0, -3682, 0 }, 8628, 1, 0 },
    { NULL, NULL, { 0x398E, -1520, 623, 0 }, { { -1546, -2544, -1372, 0 }, { 1547, -2544, 1373, 0 }, { -1546, 2544, -1372, 0 }, { 1547, 2544, 1373, 0 } }, { 2726, 0, -3074, 0 }, 3278, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

AreaResource D_mine_secret_passage_80183220[2] = {
    { 58, 58, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_405800_801514CC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_secret_passage_80183238[2] = {
    { 6, 6, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_400600_80151B10 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_secret_passage_80183250[2] = {
    { 3, 3, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_100300_80148110 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_secret_passage_80183268[2] = {
    { 6, 6, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_400600_80151B10 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_mine_secret_passage_80183280[2] = {
    { 58, 0, 0, 960, 0, 4500, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_secret_passage_801832A0[3] = {
    { 6, 0, 0, 960, 0, 7600, 0, 0, 0, 2, 0 },
    { 6, 0, 0, 2304, 0, 9400, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_secret_passage_801832D0[3] = {
    { 3, 0, 0, 1500, 0, 0x2710, 2048, 0, 0, 2, 0 },
    { 3, 0, 1, 0x2904, 0, 3500, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_secret_passage_80183300[4] = {
    { 6, 0, 0, 0x4074, 0, 4700, 1500, 0, 0, 2, 0 },
    { 6, 0, 0, 0x41A0, 0, 2200, 800, 0, 0, 2, 0 },
    { 6, 0, 0, 1500, 0, 0x2904, 1900, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_mine_secret_passage_80183340[12] = {
    { NULL, NULL },
    { D_mine_secret_passage_80183280, D_mine_secret_passage_80183220 },
    { D_mine_secret_passage_801832A0, D_mine_secret_passage_80183238 },
    { D_mine_secret_passage_801832D0, D_mine_secret_passage_80183250 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_mine_secret_passage_80183300, D_mine_secret_passage_80183268 },
};

WorldCoordRoomAmbientEntry D_mine_secret_passage_801833A0[9] = {
    { .viewCount = ARRAY_SIZE(D_mine_secret_passage_801833A0) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 680, 750, 500, 692 } },
    { .color = { 800, 820, 650, 791 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_mine_secret_passage_801833E8 = {
    0x10000015,
    0x10000017,
    0x10000015,
};

WorldCollisionFootstepSounds D_mine_secret_passage_801833F4 = {
    0x10000041,
    0x10000043,
    0x10000041,
};

WorldCollisionSurfaceProperties D_mine_secret_passage_80183400[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_mine_secret_passage_80183408[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_mine_secret_passage_80183410[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_mine_secret_passage_801833E8 },
};

WorldCollisionSurfaceProperties D_mine_secret_passage_80183418[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_mine_secret_passage_801833F4 },
};

WorldCollisionSurfaceProperties* D_mine_secret_passage_80183420[8] = {
    D_mine_secret_passage_80183400,
    D_mine_secret_passage_80183408,
    D_mine_secret_passage_80183410,
    D_mine_secret_passage_80183418,
    D_mine_secret_passage_80183400,
    D_mine_secret_passage_80183400,
    D_mine_secret_passage_80183400,
    D_mine_secret_passage_80183400,
};

RoomFadeStorage D_mine_secret_passage_80183440;

/// Publishes this passage's four emitter ids on the task's first tick - the
/// `gRoomEffectOrangeBurstId` / `gRoomEffectHaloId` / `gRoomEffectMoteId` / `gRoomEffectSparkEmitterId` slots take
/// 0x60240-0x60243 in that order, the same slot order every other room uses -
/// then draws the emitters the current camera view shows: a run of placements
/// out of one of the passage's arrays, each drawn as a
/// `_glowDrawCapsule` glow (half-extent 0x200-0x280, colour
/// 0x444 except view 6's 0x44) or a `glowDrawDisc` disc
/// (half-extent 0x200 or 0x400, colour 0x421 or 0x444).
void func_mine_secret_passage_8017D9D4(Task* arg0)
{
    if (arg0->state == 0) {
        gRoomEffectMoteId         = EFFECT_MINE_SECRET_PASSAGE_MOTE;
        gRoomEffectHaloId         = EFFECT_MINE_SECRET_PASSAGE_HALO;
        gRoomEffectOrangeBurstId  = EFFECT_MINE_SECRET_PASSAGE_ORANGE_BURST;
        gRoomEffectSparkEmitterId = EFFECT_MINE_SECRET_PASSAGE_SPARK_EMITTER;
        arg0->state               = 1;
    }

    switch (viewGetMappedIndex() & 0xFF) {
        case 2: {
            SVECTOR* p = D_mine_secret_passage_80180EC8;
            _glowDrawCapsule(&p[0], 0x200, 0x444);
            glowDrawDisc(&p[16], 0x200, 0x421);
            glowDrawDisc(&p[17], 0x200, 0x421);
            break;
        }
        case 3: {
            SVECTOR* p = D_mine_secret_passage_80180ED8;
            _glowDrawCapsule(&p[0], 0x200, 0x444);
            glowDrawDisc(&p[14], 0x200, 0x421);
            glowDrawDisc(&p[15], 0x200, 0x421);
            glowDrawDisc(&p[16], 0x200, 0x421);
            glowDrawDisc(&p[17], 0x200, 0x421);
            glowDrawDisc(&p[20], 0x200, 0x421);
            break;
        }
        case 4: {
            SVECTOR* p = D_mine_secret_passage_80180ED8;
            _glowDrawCapsule(&p[0], 0x200, 0x444);
            glowDrawDisc(&p[16], 0x200, 0x421);
            glowDrawDisc(&p[18], 0x200, 0x421);
            glowDrawDisc(&p[19], 0x200, 0x421);
            glowDrawDisc(&p[20], 0x200, 0x421);
            glowDrawDisc(&p[21], 0x200, 0x421);
            break;
        }
        case 5: {
            SVECTOR* p = D_mine_secret_passage_80180EE8;
            _glowDrawCapsule(&p[0], 0x200, 0x444);
            glowDrawDisc(&p[6], 0x400, 0x444);
            glowDrawDisc(&p[7], 0x400, 0x444);
            glowDrawDisc(&p[8], 0x400, 0x444);
            glowDrawDisc(&p[9], 0x400, 0x444);
            glowDrawDisc(&p[10], 0x200, 0x421);
            break;
        }
        case 6: {
            SVECTOR* p = D_mine_secret_passage_80180EE8;
            _glowDrawCapsule(&p[0], 0x200, 0x444);
            _glowDrawCapsule(&p[2], 0x200, 0x444);
            _glowDrawCapsule(&p[4], 0x280, 0x44);
            glowDrawDisc(&p[9], 0x400, 0x444);
            glowDrawDisc(&p[10], 0x200, 0x421);
            glowDrawDisc(&p[11], 0x200, 0x421);
            break;
        }
        case 7: {
            SVECTOR* p = D_mine_secret_passage_80180ED8;
            _glowDrawCapsule(&p[0], 0x200, 0x444);
            glowDrawDisc(&p[16], 0x200, 0x421);
            glowDrawDisc(&p[18], 0x200, 0x421);
            glowDrawDisc(&p[20], 0x200, 0x421);
            break;
        }
        case 8: {
            SVECTOR* p = D_mine_secret_passage_80180F08;
            _glowDrawCapsule(&p[0], 0x280, 0x44);
            break;
        }
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_disc.inc.c"

#include "../../shared/room_visual_effects.inc.c"

void func_mine_secret_passage_8017E868(Task* task)
{
    RoomFx_MoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void func_mine_secret_passage_8017F5B0(Task* arg0)
{
    RoomFx_HaloTask(arg0);
}

void func_mine_secret_passage_8017F948(Task* arg0)
{
    _roomVisualEffectsHaloOrangeBurstTask(arg0);
}

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void func_mine_secret_passage_80180D58(Task* arg0)
{
    RoomFx_SparkEmitterTask(arg0);
}
