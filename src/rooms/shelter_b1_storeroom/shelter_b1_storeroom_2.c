#include "rooms/shelter_b1_storeroom.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
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
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/glow_draw.h"

#define D_shelter_b1_storeroom_80184A18 (D_shelter_b1_storeroom_80184998 + 16)
#define D_shelter_b1_storeroom_80184A38 (D_shelter_b1_storeroom_80184998 + 20)
#define D_shelter_b1_storeroom_80184A98 (D_shelter_b1_storeroom_80184998 + 32)
#define D_shelter_b1_storeroom_80184AB8 (D_shelter_b1_storeroom_80184998 + 36)

#include "../../shared/room_visual_effects.h"

static RoomFxShade _gRoomEffectHaloShades[3];

// Indexed views below share one contiguous table.
extern WorldCollisionGrid         D_shelter_b1_storeroom_801850D8[1];
extern WorldCollisionOccluder     D_shelter_b1_storeroom_80186D94[1];
extern WorldCollisionTrigger      D_shelter_b1_storeroom_801862E0[12];
extern WorldCollisionTrigger      D_shelter_b1_storeroom_80186670[15];
extern WorldCoordRoomAmbientEntry D_shelter_b1_storeroom_80186D4C[9];
extern WorldCoordRoomLights       D_shelter_b1_storeroom_801862C8[1];

SVECTOR D_shelter_b1_storeroom_80184998[49] = {
    { -1130, -2290, 1920, 0 },
    { -400, -2290, 1920, 0 },
    { 370, -2290, 1920, 0 },
    { 1110, -2290, 1920, 0 },
    { 1880, -2290, 1920, 0 },
    { 2620, -2290, 1920, 0 },
    { 3350, -2290, 1920, 0 },
    { 4090, -2290, 1920, 0 },
    { -1130, -2290, 1300, 0 },
    { -400, -2290, 1300, 0 },
    { 370, -2290, 1300, 0 },
    { 1110, -2290, 1300, 0 },
    { 1880, -2290, 1300, 0 },
    { 2620, -2290, 1300, 0 },
    { 3350, -2290, 1300, 0 },
    { 4090, -2290, 1300, 0 },
    { -1130, -2290, -1310, 0 },
    { -400, -2290, -1310, 0 },
    { 370, -2290, -1310, 0 },
    { 1110, -2290, -1310, 0 },
    { 1880, -2290, -1310, 0 },
    { 2620, -2290, -1310, 0 },
    { 3350, -2290, -1310, 0 },
    { 4090, -2290, -1310, 0 },
    { -1130, -2290, -1930, 0 },
    { -400, -2290, -1930, 0 },
    { 370, -2290, -1930, 0 },
    { 1110, -2290, -1930, 0 },
    { 1880, -2290, -1930, 0 },
    { 2620, -2290, -1930, 0 },
    { 3350, -2290, -1930, 0 },
    { 4090, -2290, -1930, 0 },
    { -3130, -2290, 390, 0 },
    { -3130, -2290, -340, 0 },
    { -2520, -2290, 390, 0 },
    { -2520, -2290, -340, 0 },
    { 5390, -2290, 390, 0 },
    { 5390, -2290, -340, 0 },
    { 6010, -2290, 390, 0 },
    { 6010, -2290, -340, 0 },
    { 5490, -2160, 2600, 0 },
    { 5770, -2160, 2600, 0 },
    { 6060, -2160, 2600, 0 },
    { -3580, -2160, -2570, 0 },
    { -3300, -2160, -2570, 0 },
    { -3010, -2160, -2570, 0 },
    { 5490, -2160, -2570, 0 },
    { 5770, -2160, -2570, 0 },
    { 6060, -2160, -2570, 0 },
};

#define ROOM_FX_HALO_STORAGE_INITIALIZER { \
    { 0, 1, 2 },                           \
    { 2, 1, 0 },                           \
    { 0, 2, 1 },                           \
}
#define ROOM_FX_HALO_STORAGE_TYPE  RoomFxShade
#define ROOM_FX_HALO_STORAGE_BOUND [3]
#include "../../shared/room_visual_effects_halo_data.inc.c"
#include "../../shared/room_visual_effects_trail_data.inc.c"
#include "../../shared/room_visual_effects_disc_data.inc.c"

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomEffectHaloShades;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

WorldCollisionRoomResources D_shelter_b1_storeroom_80184B50[1] = {
    { D_shelter_b1_storeroom_801850D8, D_shelter_b1_storeroom_801862E0, D_shelter_b1_storeroom_80186670, D_shelter_b1_storeroom_80186D94 },
};

WorldCoordRoomLighting D_shelter_b1_storeroom_80184B60[1] = {
    { D_shelter_b1_storeroom_801862C8, D_shelter_b1_storeroom_80186D4C },
};

u8* D_shelter_b1_storeroom_80184B68[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b1_storeroom_80184B6C[1] = { 8 };

DirectionWarpEntry D_shelter_b1_storeroom_80184B70[3] = {
    { { { .word = 0 }, 5750, 0, -2200 }, { 0, 0, 0, 0 }, { { .word = 0 }, 5750, 0, -2200 }, { 0, 0, 0, 0 }, 0x540B0002, 0x540B0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SHELTER_1AE },
    { { { .word = 0 }, -3250, 0, -2200 }, { 0, 0, 0, 0 }, { { .word = 0 }, -3250, 0, -2200 }, { 0, 0, 0, 0 }, 0x540B0006, 0x540B0005, 0x540B0007, 4, DIRECTION_WARP_FLAG_NONE, 460 },
    { { { .word = 2048 }, 5750, 0, 2200 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 5750, 0, 2200 }, { 0, 0, 0, 0 }, 0x540B0004, 0x540B0003, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SHELTER_1B0 },
};

static SVECTOR _gShelterB1StoreroomCollision07B18Normals[18] = {
#include "assets/shelter_b1_storeroom_collision_07B18_normals.inc"
};

static SVECTOR _gShelterB1StoreroomCollision07B18Verts[64] = {
#include "assets/shelter_b1_storeroom_collision_07B18_verts.inc"
};

static WorldCollisionGridFace _gShelterB1StoreroomCollision07B18Faces[26] = {
#include "assets/shelter_b1_storeroom_collision_07B18_faces.inc"
};

static s16 _gShelterB1StoreroomCollision07B18Cells[108] = {
#include "assets/shelter_b1_storeroom_collision_07B18_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB1StoreroomCollision07B18Cells[i])
static s16* _gShelterB1StoreroomCollision07B18Table[8] = {
#include "assets/shelter_b1_storeroom_collision_07B18_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b1_storeroom_801850D8[1] = {
    { NULL, _gShelterB1StoreroomCollision07B18Normals, _gShelterB1StoreroomCollision07B18Verts, _gShelterB1StoreroomCollision07B18Faces, _gShelterB1StoreroomCollision07B18Table, 5082, 3225, 4, 2, 4000, 26 },
};

ViewCamera D_shelter_b1_storeroom_801850FC[8] = {
    { { { { 4096, 0, 0 }, { 0, 79, -4095 }, { 0, 4095, 79 } }, { -1330, 0x4498, 600 } }, 380 },
    { { { { -4076, 0, 397 }, { 181, 3645, 1858 }, { -353, 1867, -3628 } }, { -5703, 2542, -2371 } }, 230 },
    { { { { 467, 0, -4069 }, { -480, 4067, -55 }, { 4040, 483, 464 } }, { 3954, 1236, 1851 } }, 230 },
    { { { { -3766, 0, -1609 }, { 160, 4075, -376 }, { 1601, -409, -3747 } }, { 3763, 694, -2329 } }, 230 },
    { { { { -1050, 0, 3959 }, { 235, 4088, 62 }, { -3952, 244, -1048 } }, { -5790, 1021, -2119 } }, 230 },
    { { { { -1137, 0, -3934 }, { -600, 4047, 173 }, { 3888, 625, -1124 } }, { -1651, 1315, -2026 } }, 230 },
    { { { { -1050, 0, 3959 }, { 235, 4088, 62 }, { -3952, 244, -1048 } }, { -1645, 1021, -2119 } }, 230 },
    { { { { 4094, 0, 116 }, { 51, 3680, -1796 }, { -104, 1797, 3679 } }, { 594, 1676, -1050 } }, 303 },
};

SpriteBatch D_shelter_b1_storeroom_8018521C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_storeroom_8018522C[52] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -120, 1110, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -120, 433, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, -120, 410, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, -120, 437, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -48, -120, 428, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -48, -96, 441, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, -96, 454, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -112, -96, 428, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -80, -120, 429, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 0, -120, 443, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, -120, 665, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, -120, 606, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, -120, 535, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, -120, 479, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -120, 422, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 72, -120, 394, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, -120, 381, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, -120, 377, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 120, -120, 373, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 136, -120, 373, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -120, 373, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -80, 760, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -72, 757, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -8, 697, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, 56, 825, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 56, 809, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -64, 704, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -40, 710, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -24, 710, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 0, 688, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -16, 685, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 0, 706, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 16, 794, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 32, 813, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 48, 812, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 16, 796, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 32, 801, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -64, 710, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -48, 702, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -32, 709, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 40, 32, 909, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, 112, 32, 825, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, 96, 875, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, 64, 32, 850, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -16, 750, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, 72, -80, 650, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, 112, -80, 591, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 56, -72, 807, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -16, 750, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -16, 797, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 0, 917, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 16, 921, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_storeroom_8018563C[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 3, 0 } },
    { 10, 11, 0, 0, { 0, 0 } },
    { 21, 19, 0, 0, { 2, 0 } },
    { 40, 12, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_storeroom_8018566C[37] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, -24, 2098, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 32, -32, 1723, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 32, -8, 1722, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 40, 732, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 72, 40, 722, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -8, 1384, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 24, 0, 1562, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, -16, 1852, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, -16, 1381, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 40, 16, 1029, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, 16, 1020, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 40, -8, 948, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, -8, 948, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 48, -24, 919, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 48, -40, 912, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 72, -48, 982, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 40, 818, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 32, 825, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 48, 818, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 32, 823, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 8, 833, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -24, 848, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, -48, 876, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 56, -48, 858, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 56, -32, 837, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 72, 8, 822, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, 8, 840, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, -8, 850, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 72, -16, 835, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 144 } }, -72, -88, 1112, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -48, -72, 1450, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -32, -72, 1875, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -16, -56, 2192, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 96 } }, -160, -112, 752, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -88, -96, 850, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 96 } }, -160, -16, 750, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -88, -16, 850, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_storeroom_80185950[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 3, 0 } },
    { 3, 2, 0, 0, { 0, 0 } },
    { 5, 4, 0, 0, { 5, 0 } },
    { 9, 7, 0, 0, { 1, 0 } },
    { 16, 13, 0, 0, { 4, 0 } },
    { 29, 8, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_storeroom_80185990[10] = {
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -88, -80, 882, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -88, 0, 873, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -72, -72, 877, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -72, 0, 876, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -128, -8, 832, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -128, -96, 811, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -112, -8, 854, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -112, -96, 847, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -160, -104, 819, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, -160, -8, 814, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_storeroom_80185A58[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_storeroom_80185A70[21] = {
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 56, -8, 1467, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, -24, 1581, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 64, -32, 1524, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 72, 16, 901, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, 8, 1022, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 64, 8, 985, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, 40, 797, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 88, 40, 832, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 16, 877, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -8, 828, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, -24, 789, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 8, -48, 1950, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, -160, -120, 625, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 120 } }, -160, -16, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, -16, -64, 1825, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 96 } }, -128, -16, 750, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -88, -16, 950, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 88 } }, -128, -104, 750, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 88 } }, -88, -104, 950, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -48, -80, 1300, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 72 } }, -48, -16, 1300, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_storeroom_80185C14[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 3, 0 } },
    { 3, 3, 0, 0, { 0, 0 } },
    { 6, 5, 0, 0, { 2, 0 } },
    { 11, 10, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_storeroom_80185C44[19] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 48, 804, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 64, 732, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -120, -8, 778, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -160, -120, 629, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -144, -56, 658, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -144, 16, 688, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 48, 860, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, -96, 776, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 40, -48, 802, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 40, 0, 831, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 120, 40, 537, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 120, -40, 537, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 120, -120, 537, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 88, -112, 650, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 88, 24, 650, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 88, -40, 650, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 64 } }, 56, -112, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 56, -48, 750, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 56, 16, 750, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_storeroom_80185DC0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 1, 0 } },
    { 6, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_storeroom_80185DE0[32] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -24, 430, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -8, 550, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -40, 533, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 72, -48, 533, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 64, -40, 504, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 88, -40, 524, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 104, -40, 526, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 104, -16, 432, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 64, 8, 421, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 96, 8, 396, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 128, 8, 391, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, 24, 438, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, 72, 442, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 120, 24, 405, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 120, 72, 411, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 48 } }, 64, 24, 455, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, 64, 72, 459, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, 112, 452, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 88, 0, 453, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 128, 0, 411, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 40, 0, 427, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, 0, 477, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -56, -88, 830, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -56, -8, 846, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -48, -56, 850, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -48, -8, 865, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -88, -120, 812, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 88 } }, -88, -24, 812, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 104 } }, -160, -8, 625, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 112 } }, -160, -120, 625, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -120, -16, 725, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, -120, -120, 725, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_storeroom_80186060[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 1, 0 } },
    { 22, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_storeroom_80186080[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b1_storeroom_80186090[8] = {
    { { .empty = D_shelter_b1_storeroom_8018521C }, D_shelter_b1_storeroom_8018521C, NULL },
    { { .elements = D_shelter_b1_storeroom_8018522C }, D_shelter_b1_storeroom_8018563C, NULL },
    { { .elements = D_shelter_b1_storeroom_8018566C }, D_shelter_b1_storeroom_80185950, NULL },
    { { .elements = D_shelter_b1_storeroom_80185990 }, D_shelter_b1_storeroom_80185A58, NULL },
    { { .elements = D_shelter_b1_storeroom_80185A70 }, D_shelter_b1_storeroom_80185C14, NULL },
    { { .elements = D_shelter_b1_storeroom_80185C44 }, D_shelter_b1_storeroom_80185DC0, NULL },
    { { .elements = D_shelter_b1_storeroom_80185DE0 }, D_shelter_b1_storeroom_80186060, NULL },
    { { .empty = D_shelter_b1_storeroom_80186080 }, D_shelter_b1_storeroom_80186080, NULL },
};

WorldCoordLight D_shelter_b1_storeroom_801860F0[1] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 10, -4227, -10 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } },
};

WorldCoordPointLight D_shelter_b1_storeroom_80186148[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 920, -1702, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3276 }, { 0, 0 } }, 20, 7321 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5582, -1606, 2511 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3522, 3276, 2293 }, { 0, 0 } }, 602, 1600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5780, -1749, -1968 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3522, 3276, 2293 }, { 0, 0 } }, 621, 1721 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3270, -1736, -2109 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3522, 3276, 2293 }, { 0, 0 } }, 661, 1921 },
};

WorldCoordRoomLights D_shelter_b1_storeroom_801862C8[1] = {
    { ARRAY_SIZE(D_shelter_b1_storeroom_801860F0), D_shelter_b1_storeroom_801860F0, ARRAY_SIZE(D_shelter_b1_storeroom_80186148), D_shelter_b1_storeroom_80186148, 0, NULL },
};

WorldCollisionTrigger D_shelter_b1_storeroom_801862E0[12] = {
    { NULL, NULL, NULL, { 3616, -1568, 1839, 0 }, { { -64, -1904, -1664, 0 }, { 64, -1904, 1664, 0 }, { -64, 1904, -1664, 0 }, { 64, 1904, 1664, 0 } }, { 4095, 0, -158, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3760, -1600, 1872, 0 }, { { 80, -1904, 1712, 0 }, { -80, -1904, -1712, 0 }, { 80, 1904, 1712, 0 }, { -80, 1904, -1712, 0 } }, { -4091, 0, 190, 0 }, { 0, 0, 4096, 0 }, 2560, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5696, -1568, 175, 0 }, { { 1936, -1904, 0, 0 }, { -1936, -1904, 0, 0 }, { 1936, 1904, 0, 0 }, { -1936, 1904, 0, 0 } }, { 0, 0, 4101, 0 }, { 0, 0, 4096, 0 }, 2709, 0, 6, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5568, -1536, 288, 0 }, { { -1584, -1904, 0, 0 }, { 1584, -1904, 0, 0 }, { -1584, 1904, 0, 0 }, { 1584, 1904, 0, 0 } }, { 0, 0, -4103, 0 }, { 0, 0, 4096, 0 }, 2468, 0, 2, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4348, -1569, -1829, 0 }, { { 99, -1904, -1663, 0 }, { -99, -1904, 1664, 0 }, { 99, 1904, -1663, 0 }, { -99, 1904, 1664, 0 } }, { 4094, 0, 243, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4478, -1568, -1828, 0 }, { { -88, -1904, 1711, 0 }, { 88, -1904, -1710, 0 }, { -88, 1904, 1711, 0 }, { 88, 1904, -1710, 0 } }, { -4101, 0, -212, 0 }, { 0, 0, 4096, 0 }, 2560, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1440, -1537, -1633, 0 }, { { -64, -1904, -1664, 0 }, { 64, -1904, 1664, 0 }, { -64, 1904, -1664, 0 }, { 64, 1904, 1664, 0 } }, { 4095, 0, -158, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1184, -1536, -1600, 0 }, { { 80, -1904, 1712, 0 }, { -80, -1904, -1712, 0 }, { 80, 1904, 1712, 0 }, { -80, 1904, -1712, 0 } }, { -4091, 0, 190, 0 }, { 0, 0, 4096, 0 }, 2560, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2912, -1600, 352, 0 }, { { -1904, -1904, 128, 0 }, { 1904, -1904, -128, 0 }, { -1904, 1904, 128, 0 }, { 1904, 1904, -128, 0 } }, { -275, 0, -4088, 0 }, { 0, 0, 4096, 0 }, 2684, 0, 4, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3072, -1600, 176, 0 }, { { 1904, -1904, -144, 0 }, { -1904, -1904, 144, 0 }, { 1904, 1904, -144, 0 }, { -1904, 1904, 144, 0 } }, { 308, 0, 4086, 0 }, { 0, 0, 4096, 0 }, 2684, 0, 7, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -320, -1473, 1888, 0 }, { { -88, -1904, 1711, 0 }, { 88, -1904, -1710, 0 }, { -88, 1904, 1711, 0 }, { 88, 1904, -1710, 0 } }, { -4101, 0, -212, 0 }, { 0, 0, 4096, 0 }, 2560, 0, 7, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -704, -1696, 1759, 0 }, { { 88, -1904, -1710, 0 }, { -88, -1904, 1711, 0 }, { 88, 1904, -1710, 0 }, { -88, 1904, 1711, 0 } }, { 4099, 0, 210, 0 }, { 0, 0, 4096, 0 }, 2560, 0, 5, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b1_storeroom_80186670[15] = {
    { NULL, NULL, NULL, { 5776, -48, -2256, 0 }, { { -688, 0, -432, 0 }, { 688, 0, -432, 0 }, { -688, 0, 432, 0 }, { 688, 0, 432, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 812, WORLD_COLLISION_TRIGGER_ACTION_WARP, 10, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3312, -48, -2176, 0 }, { { -688, 0, -528, 0 }, { 688, 0, -528, 0 }, { -688, 0, 528, 0 }, { 688, 0, 528, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 865, WORLD_COLLISION_TRIGGER_ACTION_WARP, 13, 34, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5712, -80, 2432, 0 }, { { -784, 0, -464, 0 }, { 784, 0, -464, 0 }, { -784, 0, 464, 0 }, { 784, 0, 464, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 909, WORLD_COLLISION_TRIGGER_ACTION_WARP, 12, 50, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6224, -64, -160, 0 }, { { -432, 0, -592, 0 }, { 432, 0, -592, 0 }, { -432, 0, 592, 0 }, { 432, 0, 592, 0 } }, { 0, 4096, 0, 0 }, { -4076, 0, 401, 0 }, 732, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6208, -64, 1472, 0 }, { { -432, 0, -864, 0 }, { 432, 0, -864, 0 }, { -432, 0, 864, 0 }, { 432, 0, 864, 0 } }, { 0, 4102, 0, 0 }, { -4076, 0, 401, 0 }, 964, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2992, -64, 784, 0 }, { { -1472, 0, -368, 0 }, { 1472, 0, -368, 0 }, { -1472, 0, 368, 0 }, { 1472, 0, 368, 0 } }, { 0, 4104, 0, 0 }, { -202, 0, 4091, 0 }, 1514, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, 736, 0 }, { { -1472, 0, -368, 0 }, { 1472, 0, -368, 0 }, { -1472, 0, 368, 0 }, { 1472, 0, 368, 0 } }, { 0, 4104, 0, 0 }, { -202, 0, 4091, 0 }, 1514, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -768, 0 }, { { -1472, 0, -368, 0 }, { 1472, 0, -368, 0 }, { -1472, 0, 368, 0 }, { 1472, 0, 368, 0 } }, { 0, 4104, 0, 0 }, { -200, 0, -4091, 0 }, 1514, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2976, -64, -832, 0 }, { { -1472, 0, -368, 0 }, { 1472, 0, -368, 0 }, { -1472, 0, 368, 0 }, { 1472, 0, 368, 0 } }, { 0, 4104, 0, 0 }, { 402, 0, -4077, 0 }, 1514, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3664, -64, -32, 0 }, { { -432, 0, -2384, 0 }, { 432, 0, -2384, 0 }, { -432, 0, 2384, 0 }, { 432, 0, 2384, 0 } }, { 0, 4100, 0, 0 }, { 4075, 0, 400, 0 }, 2415, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -352, -64, 2336, 0 }, { { -1024, 0, -912, 0 }, { 1024, 0, -912, 0 }, { -1024, 0, 208, 0 }, { 1024, 0, 208, 0 } }, { 0, 4110, 0, 0 }, { -201, 0, -4091, 0 }, 1366, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3520, -64, -1776, 0 }, { { -1024, 0, -352, 0 }, { 1024, 0, -352, 0 }, { -1024, 0, 352, 0 }, { 1024, 0, 352, 0 } }, { 0, 4094, 0, 0 }, { 0, 0, 4096, 0 }, 1078, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -480, -64, -2464, 0 }, { { -1568, 0, -224, 0 }, { 1504, 0, -224, 0 }, { -608, 0, 832, 0 }, { 1024, 0, 832, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 1583, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1888, -64, 2624, 0 }, { { -768, 0, -800, 0 }, { 1024, 0, -800, 0 }, { -1472, 0, 96, 0 }, { 1760, 0, 128, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1764, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6304, -64, -1584, 0 }, { { -432, 0, -736, 0 }, { 432, 0, -736, 0 }, { -432, 0, 736, 0 }, { 432, 0, 736, 0 } }, { 0, 4113, 0, 0 }, { -4076, 0, 401, 0 }, 851, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b1_storeroom_80186AE4[3] = {
    { 18, 18, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401800_80155AC4 },
    { 7, 7, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &D_actor_300700_801693AC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_storeroom_80186B08[2] = {
    { 24, 24, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102400_8013647C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_storeroom_80186B20[2] = {
    { 3, 3, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_100300_80148110 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_storeroom_80186B38[3] = {
    { 20, 20, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, Actor02000_D15FD0 },
    { 56, 56, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205600_801602C0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_storeroom_80186B5C[6] = {
    { 18, 16, 1, 3000, 0, 1350, 1024, 0, 0, 2, 0 },
    { 18, 16, 1, -500, 0, -1550, 3072, 0, 0, 2, 0 },
    { 7, 0, 0, 6200, 0, -600, 700, 0, 2, 4, 4 },
    { 7, 0, 0, 6200, 0, -100, 900, 0, 2, 4, 4 },
    { 7, 0, 0, 6250, 0, 0, 1700, 0, 2, 4, 4 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_storeroom_80186BBC[8] = {
    { 24, 0, 1, -3750, 0, 2050, 1900, 0, 0, 2, 0 },
    { 24, 0, 0, 6200, 0, -300, 1200, 0, 0, 2, 0 },
    { 24, 0, 0, 6100, 0, -200, 1024, 0, 0, 2, 0 },
    { 24, 0, 0, 6300, 0, -100, 1400, 0, 0, 2, 0 },
    { 24, 0, 0, 3300, 0, 1000, 900, 0, 0, 2, 0 },
    { 24, 0, 0, -1850, 0, 0, 1024, 0, 0, 2, 0 },
    { 24, 0, 0, -500, 0, -1550, 800, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_storeroom_80186C3C[3] = {
    { 3, 0, 0, 4000, 0, 1300, 3072, 0, 0, 2, 0 },
    { 3, 0, 1, -1000, 0, -1300, 1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_storeroom_80186C6C[3] = {
    { 20, 8, 1, -3000, 0, 1400, 1024, 0, 0, 2, 0 },
    { 56, 6, 1, 3000, 0, -1400, 3072, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b1_storeroom_80186C9C[22] = {
    { NULL, NULL },
    { D_shelter_b1_storeroom_80186B5C, D_shelter_b1_storeroom_80186AE4 },
    { D_shelter_b1_storeroom_80186BBC, D_shelter_b1_storeroom_80186B08 },
    { D_shelter_b1_storeroom_80186C3C, D_shelter_b1_storeroom_80186B20 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_storeroom_80186C6C, D_shelter_b1_storeroom_80186B38 },
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

WorldCoordRoomAmbientEntry D_shelter_b1_storeroom_80186D4C[9] = {
    { .viewCount = ARRAY_SIZE(D_shelter_b1_storeroom_80186D4C) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 470, 470, 470, 470 } },
    { .color = { 650, 650, 650, 650 } },
    { .color = { 470, 470, 470, 470 } },
    { .color = { 650, 650, 650, 650 } },
    { .color = { 350, 350, 350, 350 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionOccluder D_shelter_b1_storeroom_80186D94[1] = {
    { NULL, NULL, { 1568, -240, 0, 0 }, { { -2976, 1840, 0, 0 }, { 2976, 1840, 0, 0 }, { -2976, -1840, 0, 0 }, { 2976, -1840, 0, 0 } }, { 0, 0, 4096, 0 }, 3491, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionFootstepSounds D_shelter_b1_storeroom_80186DD0 = {
    0x10000041,
    0x10000043,
    0x10000041,
};

WorldCollisionSurfaceProperties D_shelter_b1_storeroom_80186DDC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b1_storeroom_80186DE4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b1_storeroom_80186DD0 },
};

WorldCollisionSurfaceProperties* D_shelter_b1_storeroom_80186DEC[8] = {
    D_shelter_b1_storeroom_80186DDC,
    D_shelter_b1_storeroom_80186DE4,
    D_shelter_b1_storeroom_80186DDC,
    D_shelter_b1_storeroom_80186DDC,
    D_shelter_b1_storeroom_80186DDC,
    D_shelter_b1_storeroom_80186DDC,
    D_shelter_b1_storeroom_80186DDC,
    D_shelter_b1_storeroom_80186DDC,
};

void func_shelter_b1_storeroom_8017D7EC(Task* arg0)
{
    if (arg0->state == 0) {
        gRoomEffectMoteId         = EFFECT_SHELTER_B1_STOREROOM_MOTE;
        gRoomEffectHaloId         = EFFECT_SHELTER_B1_STOREROOM_HALO;
        gRoomEffectOrangeBurstId  = EFFECT_SHELTER_B1_STOREROOM_ORANGE_BURST;
        gRoomEffectSparkEmitterId = EFFECT_SHELTER_B1_STOREROOM_SPARK_EMITTER;
        gRoomEffectFlashId        = EFFECT_SHELTER_B1_STOREROOM_FLASH;
        gRoomEffectTwinTrailId    = EFFECT_SHELTER_B1_STOREROOM_TWIN_TRAIL;
        gRoomEffectSparkBurstId   = EFFECT_SHELTER_B1_STOREROOM_SPARK_BURST;
        gRoomEffectGlowDiscId     = EFFECT_SHELTER_B1_STOREROOM_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B1_STOREROOM_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B1_STOREROOM_ORANGE_BURST_2;
        arg0->state               = 1;
    }

    switch (viewGetMappedIndex() & 0xFF) {
        case 2:
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A38[0], 0x100, 0x222);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A38[2], 0x100, 0x222);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A38[8], 0x100, 0x222);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A38[10], 0x100, 0x222);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A38[16], 0x100, 0x444);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A38[18], 0x100, 0x444);
            glowDrawDisc(&D_shelter_b1_storeroom_80184A38[26], 0x100, 0x222);
            glowDrawDisc(&D_shelter_b1_storeroom_80184A38[27], 0x100, 0x222);
            glowDrawDisc(&D_shelter_b1_storeroom_80184A38[28], 0x100, 0x222);
            break;
        case 3:
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A18[0], 0x100, 0x444);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A18[2], 0x100, 0x444);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A18[4], 0x100, 0x444);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A18[6], 0x100, 0x444);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A18[8], 0x100, 0x444);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A18[10], 0x100, 0x444);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A18[12], 0x100, 0x444);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A18[14], 0x100, 0x444);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A18[20], 0x100, 0x222);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A18[22], 0x100, 0x222);
            break;
        case 4:
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A18[0], 0x100, 0x444);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A18[8], 0x100, 0x444);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A18[16], 0x100, 0x333);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A18[18], 0x100, 0x333);
            glowDrawDisc(&D_shelter_b1_storeroom_80184A18[27], 0x100, 0x222);
            glowDrawDisc(&D_shelter_b1_storeroom_80184A18[28], 0x100, 0x222);
            glowDrawDisc(&D_shelter_b1_storeroom_80184A18[29], 0x100, 0x222);
            break;
        case 5:
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184998[0], 0x100, 0x444);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184998[2], 0x100, 0x444);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184998[4], 0x100, 0x444);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184998[6], 0x100, 0x444);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184998[8], 0x100, 0x444);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184998[10], 0x100, 0x444);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184998[12], 0x100, 0x444);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184998[32], 0x100, 0x222);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184998[34], 0x100, 0x222);
            break;
        case 6:
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184AB8[0], 0x100, 0x444);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184AB8[2], 0x100, 0x444);
            glowDrawDisc(&D_shelter_b1_storeroom_80184AB8[4], 0x100, 0x222);
            glowDrawDisc(&D_shelter_b1_storeroom_80184AB8[5], 0x100, 0x222);
            glowDrawDisc(&D_shelter_b1_storeroom_80184AB8[6], 0x100, 0x222);
            break;
        case 7:
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A98[0], 0x100, 0x333);
            _glowDrawCapsule(&D_shelter_b1_storeroom_80184A98[2], 0x100, 0x333);
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/glow_draw_disc.inc.c"

#include "../../shared/room_visual_effects.inc.c"

void func_shelter_b1_storeroom_8017E7A8(Task* task)
{
    RoomFx_MoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void func_shelter_b1_storeroom_8017F4F0(Task* arg0)
{
    _roomVisualEffectsHaloTask(arg0);
}

void func_shelter_b1_storeroom_8017F888(Task* arg0)
{
    _roomVisualEffectsHaloOrangeBurstTask(arg0);
}

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void func_shelter_b1_storeroom_80180C98(Task* arg0)
{
    RoomFx_SparkEmitterTask(arg0);
}

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_shelter_b1_storeroom_80180DCC(Task* arg0)
{
    _roomVisualEffectsFlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_shelter_b1_storeroom_80181830(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b1_storeroom_80182118(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
#include "../../shared/room_visual_effects_flying_tasks.inc.c"

void func_shelter_b1_storeroom_80182D60(Task* arg0)
{
    RoomFx_GlowDiscTask(arg0);
}

void func_shelter_b1_storeroom_801832B8(Task* task)
{
    _roomVisualEffectsFlyingSparkTask(task);
}

#include "../../shared/room_visual_effects_burst.inc.c"

void func_shelter_b1_storeroom_80183F18(Task* arg0)
{
    _roomVisualEffectsFlyingOrangeBurstTask(arg0);
}

#include "../../shared/room_visual_effects_burst_draw.inc.c"
