#include "rooms/shelter_b2_pod_access_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "shelter_b2_pod_access_tunnel_private.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
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
#include "main/mem.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_events.h"
// Exported instance: another image refers to this package's copy by name.
#define effectSpriteDriftTask shelterB2PodAccessTunnelEffectSpriteDriftTask
#include "../../shared/effect_sprite.h"

static void _effectSpriteDrawBanked(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);
static void _effectSpriteDrawRotated(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);

/// The beam placements the view-dependent beam task draws for camera views 2,
/// 3/6 and 4/7: pairs of end points, of which each view draws a subset.
extern SVECTOR D_shelter_b2_pod_access_tunnel_80183C08[];
extern SVECTOR D_shelter_b2_pod_access_tunnel_80183C48[];
extern SVECTOR D_shelter_b2_pod_access_tunnel_80183CC8[];

/// Tint rows for the expanding halo, indexed by the palette selector in its
/// spawn argument.

/// The two offsets the trail task places its coordinates at: `[0]` for the
/// object's own coordinate and `[1]` for the second trail. The per-tick path
/// reaches `[1]` under its own name,
/// `RoomFx_TrailOffsets[1]`.

extern WorldCollisionGrid     D_shelter_b2_pod_access_tunnel_801841B4[1];
extern WorldCollisionOccluder D_shelter_b2_pod_access_tunnel_80185664[1];
extern WorldCollisionTrigger  D_shelter_b2_pod_access_tunnel_80184FD8[4];
extern WorldCollisionTrigger  D_shelter_b2_pod_access_tunnel_80185108[3];
extern WorldCollisionTrigger  D_shelter_b2_pod_access_tunnel_801851EC[3];
extern WorldCoordRoomLights   D_shelter_b2_pod_access_tunnel_80184FC0[1];

TaskDesc D_shelter_b2_pod_access_tunnel_80183BC0 = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskMessageEntry D_shelter_b2_pod_access_tunnel_80183BCC[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b2_pod_access_tunnel_8017D7C4 },
    { 5105, func_shelter_b2_pod_access_tunnel_8017DB28 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b2_pod_access_tunnel_8017DB70 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b2_pod_access_tunnel_8017DB30 },
    { ROOM_MESSAGE_SOUND, func_shelter_b2_pod_access_tunnel_8017DB78 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b2_pod_access_tunnel_80183BFC = { { { TASK_BODY_NONE, 32 } }, func_shelter_b2_pod_access_tunnel_8017D9A8, { .value = 0 } };

SVECTOR D_shelter_b2_pod_access_tunnel_80183C08[8] = {
    { 920, 200, -0x29D6, 0 },
    { 920, 200, -9710, 0 },
    { 920, 200, -9105, 0 },
    { 920, 200, -8105, 0 },
    { 920, 200, -7300, 0 },
    { 920, 200, -6300, 0 },
    { 920, 200, -5660, 0 },
    { 920, 200, -4660, 0 },
};

SVECTOR D_shelter_b2_pod_access_tunnel_80183C48[16] = {
    { 920, 200, -3960, 0 },
    { 920, 200, -2960, 0 },
    { 920, 200, -2300, 0 },
    { 920, 200, -1300, 0 },
    { 2623, 200, -0x29D6, 0 },
    { 2623, 200, -9710, 0 },
    { 2623, 200, -9105, 0 },
    { 2623, 200, -8105, 0 },
    { 2623, 200, -7300, 0 },
    { 2623, 200, -6300, 0 },
    { 2623, 200, -5660, 0 },
    { 2623, 200, -4660, 0 },
    { 2623, 200, -3960, 0 },
    { 2623, 200, -2960, 0 },
    { 1250, 200, -923, 0 },
    { 2250, 200, -923, 0 },
};

SVECTOR D_shelter_b2_pod_access_tunnel_80183CC8[32] = {
    { 3150, 200, -923, 0 },
    { 4150, 200, -923, 0 },
    { 3150, 200, -2620, 0 },
    { 4150, 200, -2620, 0 },
    { 1020, 410, -0x29D6, 0 },
    { 1020, 410, -9710, 0 },
    { 1020, 410, -9105, 0 },
    { 1020, 410, -8105, 0 },
    { 1020, 410, -7300, 0 },
    { 1020, 410, -6300, 0 },
    { 1020, 410, -5660, 0 },
    { 1020, 410, -4660, 0 },
    { 1020, 410, -3960, 0 },
    { 1020, 410, -2960, 0 },
    { 1020, 410, -2300, 0 },
    { 1020, 410, -1300, 0 },
    { 2523, 410, -0x29D6, 0 },
    { 2523, 410, -9710, 0 },
    { 2523, 410, -9100, 0 },
    { 2523, 410, -8100, 0 },
    { 2523, 410, -7300, 0 },
    { 2523, 410, -6300, 0 },
    { 2523, 410, -5660, 0 },
    { 2523, 410, -4660, 0 },
    { 2523, 410, -3960, 0 },
    { 2523, 410, -2960, 0 },
    { 1250, 410, -1023, 0 },
    { 2250, 410, -1023, 0 },
    { 3150, 410, -1023, 0 },
    { 4150, 410, -1023, 0 },
    { 3150, 410, -2520, 0 },
    { 4150, 410, -2520, 0 },
};

#define ROOM_FX_HALO_STORAGE_INITIALIZER { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 0xA041 }
#define ROOM_FX_HALO_STORAGE_TYPE        RoomFxHaloStorage
#define ROOM_FX_HALO_STORAGE_BOUND
#include "../../shared/room_visual_effects_halo_data.inc.c"

static inline RoomFxShade* RoomFx_GetHaloShades(void)
{
    return _gRoomEffectHaloShades.entries;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources D_shelter_b2_pod_access_tunnel_80183DEC[2] = {
    { D_shelter_b2_pod_access_tunnel_801841B4, D_shelter_b2_pod_access_tunnel_80184FD8, D_shelter_b2_pod_access_tunnel_80185108, D_shelter_b2_pod_access_tunnel_80185664 },
    { D_shelter_b2_pod_access_tunnel_801841B4, D_shelter_b2_pod_access_tunnel_80184FD8, D_shelter_b2_pod_access_tunnel_801851EC, D_shelter_b2_pod_access_tunnel_80185664 },
};

WorldCoordRoomLighting D_shelter_b2_pod_access_tunnel_80183E0C[2] = {
    { D_shelter_b2_pod_access_tunnel_80184FC0, NULL },
    { D_shelter_b2_pod_access_tunnel_80184FC0, NULL },
};

u8 D_shelter_b2_pod_access_tunnel_80183E1C[8] = {
    1,
    2,
    6,
    7,
    5,
    3,
    4,
    0,
};

u8* D_shelter_b2_pod_access_tunnel_80183E24[2] = {
    gViewIdentityMap,
    D_shelter_b2_pod_access_tunnel_80183E1C,
};

ViewCount D_shelter_b2_pod_access_tunnel_80183E2C[2] = { 7, 7 };

DirectionWarpEntry D_shelter_b2_pod_access_tunnel_80183E30[3] = {
    { { { .word = 0 }, 1700, 0, -0x28CD }, { 0, 0, 0, 0 }, { { .word = 256 }, 1050, 0, -0x2710 }, { 0, 0, 0, 0 }, 0x54230002, 0x54230001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 5500, 0, -1700 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 5500, 0, -1700 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, 0x54230006, 0x54230003, 4, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_POD_SERVICE_GANTRY },
    { { { .word = 0 }, 4800, 0, -2400 }, { 0, 0, 0, 0 }, { { .word = 2560 }, 4500, 0, -1250 }, { 0, 0, 0, 0 }, 0x54230005, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_POD },
};

static SVECTOR _gShelterB2PodAccessTunnelCollision06BF4Normals[5] = {
#include "assets/shelter_b2_pod_access_tunnel_collision_06BF4_normals.inc"
};

static SVECTOR _gShelterB2PodAccessTunnelCollision06BF4Verts[40] = {
#include "assets/shelter_b2_pod_access_tunnel_collision_06BF4_verts.inc"
};

static WorldCollisionGridFace _gShelterB2PodAccessTunnelCollision06BF4Faces[17] = {
#include "assets/shelter_b2_pod_access_tunnel_collision_06BF4_faces.inc"
};

static s16 _gShelterB2PodAccessTunnelCollision06BF4Cells[72] = {
#include "assets/shelter_b2_pod_access_tunnel_collision_06BF4_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB2PodAccessTunnelCollision06BF4Cells[i])
static s16* _gShelterB2PodAccessTunnelCollision06BF4Table[6] = {
#include "assets/shelter_b2_pod_access_tunnel_collision_06BF4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b2_pod_access_tunnel_801841B4[1] = {
    { NULL, _gShelterB2PodAccessTunnelCollision06BF4Normals, _gShelterB2PodAccessTunnelCollision06BF4Verts, _gShelterB2PodAccessTunnelCollision06BF4Faces, _gShelterB2PodAccessTunnelCollision06BF4Table, -500, 0x2B8E, 2, 3, 4000, 17 },
};

ViewCamera D_shelter_b2_pod_access_tunnel_801841D8[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -4000, 0x36B0, 5500 } }, 230 },
    { { { { -3971, 0, -1003 }, { -45, 4091, 179 }, { 1002, 185, -3967 } }, { -949, 1422, 2295 } }, 230 },
    { { { { 3920, 0, -1185 }, { -147, 4064, -488 }, { 1176, 510, 3890 } }, { -899, 1609, 7345 } }, 230 },
    { { { { -672, 0, -4040 }, { -824, 4009, 137 }, { 3955, 835, -658 } }, { -779, 1872, 1145 } }, 225 },
    { { { { -3803, 0, -1520 }, { -283, 4024, 709 }, { 1493, 764, -3736 } }, { -4209, 1711, 1892 } }, 246 },
    { { { { 3920, 0, -1185 }, { -147, 4064, -488 }, { 1176, 510, 3890 } }, { -899, 1609, 7345 } }, 230 },
    { { { { -672, 0, -4040 }, { -824, 4009, 137 }, { 3955, 835, -658 } }, { -779, 1872, 1145 } }, 225 },
};

SpriteBatch D_shelter_b2_pod_access_tunnel_801842D4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_pod_access_tunnel_801842E4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_pod_access_tunnel_801842F4[60] = {
    { 142, 0x3FC0, { .fields = { 40, 64 } }, 48, -120, 918, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 64 } }, 88, -120, 1250, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 72 } }, 128, -120, 1202, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 48, -56, 943, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 72 } }, 88, -56, 1170, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 112, -56, 1170, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 128, -48, 1154, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 144, -48, 1170, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 72, 16, 900, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, 48, 16, 1098, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 48, 32, 998, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 72, 40, 954, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 88, 48, 838, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 112, 56, 784, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 128, 72, 722, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 144, 80, 676, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 144, 8, 1186, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 48, 570, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 8, 1186, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 40, 678, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 16, 1234, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 112, 32, 738, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 88, 16, 862, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, 88, 24, 809, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 32, -112, 1110, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 32, -56, 1139, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 32, -16, 1166, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 152 } }, -88, -80, 0, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 152 } }, -24, -80, 0, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 152 } }, 24, -80, 0, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 64 } }, 56, -80, 854, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, 56, -16, 882, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 56, 24, 879, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, -136, 48, 0, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -96, -80, 0, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -56, 721, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -96, -32, 712, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -8, 720, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -96, 16, 732, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, -120, 16, 729, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, -120, 0, 690, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, -120, -24, 698, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, -120, -48, 691, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, -120, -80, 684, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -136, -80, 653, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -136, -48, 682, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -136, -24, 683, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -136, 0, 673, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -136, 16, 714, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 24, 0, 1211, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 40, 0, 1070, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 56, 0, 909, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 16 } }, 72, 0, 0, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, 80, 16, 860, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 96, 24, 746, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 72, 16, 877, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, 24, 810, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, 32, 723, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 112, 32, 665, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 32, 613, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_pod_access_tunnel_801847A4[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 3, 0 } },
    { 24, 3, 0, 0, { 0, 0 } },
    { 27, 22, 0, 0, { 2, 0 } },
    { 49, 11, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_pod_access_tunnel_801847D4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_pod_access_tunnel_801847E4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_pod_access_tunnel_801847F4[54] = {
    { 143, 0x3FC0, { .fields = { 40, 64 } }, 48, -120, 918, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, 88, -120, 1250, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 128, -120, 1202, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 48, -56, 943, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 88, -56, 1170, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 112, -56, 1170, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 128, -48, 1154, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 144, -48, 1170, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, 16, 900, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 48, 16, 1098, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 48, 32, 998, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 72, 40, 954, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 88, 48, 838, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 112, 56, 784, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, 72, 722, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 144, 80, 676, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 144, 8, 1186, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 144, 48, 570, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 128, 8, 1186, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 128, 40, 678, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 16, 1234, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 112, 32, 738, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 88, 16, 862, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 88, 24, 809, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 32, -112, 1110, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 32, -56, 1139, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 32, -16, 1166, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, 56, -80, 854, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 56, -16, 882, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 56, 24, 879, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -96, -32, 712, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -8, 720, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 16, 732, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -120, 16, 729, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, 0, 690, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -120, -24, 698, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -120, -48, 691, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -120, -80, 684, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -136, -80, 653, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -136, -48, 682, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -136, -24, 683, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 0, 673, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -136, 16, 714, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, -48, 721, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 24, 0, 1211, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 40, 0, 1070, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 56, 0, 909, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 72, 16, 877, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, 24, 810, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 96, 32, 723, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 112, 32, 665, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 128, 32, 613, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 80, 16, 860, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 24, 758, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_pod_access_tunnel_80184C2C[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 3, 0 } },
    { 24, 3, 0, 0, { 0, 0 } },
    { 27, 17, 0, 0, { 2, 0 } },
    { 44, 10, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_pod_access_tunnel_80184C5C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b2_pod_access_tunnel_80184C6C[7] = {
    { { .empty = D_shelter_b2_pod_access_tunnel_801842D4 }, D_shelter_b2_pod_access_tunnel_801842D4, NULL },
    { { .empty = D_shelter_b2_pod_access_tunnel_801842E4 }, D_shelter_b2_pod_access_tunnel_801842E4, NULL },
    { { .elements = D_shelter_b2_pod_access_tunnel_801842F4 }, D_shelter_b2_pod_access_tunnel_801847A4, NULL },
    { { .empty = D_shelter_b2_pod_access_tunnel_801847D4 }, D_shelter_b2_pod_access_tunnel_801847D4, NULL },
    { { .empty = D_shelter_b2_pod_access_tunnel_801847E4 }, D_shelter_b2_pod_access_tunnel_801847E4, NULL },
    { { .elements = D_shelter_b2_pod_access_tunnel_801847F4 }, D_shelter_b2_pod_access_tunnel_80184C2C, NULL },
    { { .empty = D_shelter_b2_pod_access_tunnel_80184C5C }, D_shelter_b2_pod_access_tunnel_80184C5C, NULL },
};

WorldCoordPointLight D_shelter_b2_pod_access_tunnel_80184CC0[8] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -3949 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -5618 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -6721 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -8147 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -9711 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4126, -1155, -2029 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3378, 3157, 3496 }, { 0, 0 } }, 1200, 2620 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3177, 0, -1757 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -2395 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
};

WorldCoordRoomLights D_shelter_b2_pod_access_tunnel_80184FC0[1] = {
    { 0, NULL, ARRAY_SIZE(D_shelter_b2_pod_access_tunnel_80184CC0), D_shelter_b2_pod_access_tunnel_80184CC0, 0, NULL },
};

WorldCollisionTrigger D_shelter_b2_pod_access_tunnel_80184FD8[4] = {
    { NULL, NULL, NULL, { 1760, -1648, -4513, 0 }, { { -1824, -2224, 64, 0 }, { 1824, -2224, -64, 0 }, { -1824, 2224, 64, 0 }, { 1824, 2224, -64, 0 } }, { -144, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 2873, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1744, -1632, -4625, 0 }, { { 1808, -2224, -80, 0 }, { -1808, -2224, 80, 0 }, { 1808, 2224, -80, 0 }, { -1808, 2224, 80, 0 } }, { 180, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 2862, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3520, -1632, -1360, 0 }, { { -285, -2224, -1838, 0 }, { 254, -2224, 1809, 0 }, { -285, 2224, -1838, 0 }, { 254, 2224, 1809, 0 } }, { 4054, 0, -601, 0 }, { 0, 0, 4096, 0 }, 2884, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3606, -1664, -1488, 0 }, { { 250, -2224, 1567, 0 }, { -271, -2224, -1585, 0 }, { 250, 2224, 1567, 0 }, { -271, 2224, -1585, 0 } }, { -4049, 0, 668, 0 }, { 0, 0, 4096, 0 }, 2733, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b2_pod_access_tunnel_80185108[3] = {
    { NULL, NULL, NULL, { 1728, -48, -0x28CF, 0 }, { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_WARP, 34, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5856, -48, -1728, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, 48, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4592, -64, -2784, 0 }, { { -944, 0, -464, 0 }, { 944, 0, -464, 0 }, { -944, 0, 464, 0 }, { 944, 0, 464, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b2_pod_access_tunnel_801851EC[3] = {
    { NULL, NULL, NULL, { 1728, -48, -0x28CF, 0 }, { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_WARP, 34, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5856, -48, -1728, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, WORLD_COLLISION_TRIGGER_ACTION_WARP, 48, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4592, -64, -2784, 0 }, { { -944, 0, -464, 0 }, { 944, 0, -464, 0 }, { -944, 0, 464, 0 }, { 944, 0, 464, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b2_pod_access_tunnel_801852D0[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_80135C30 },
    { 11, 11, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_8015F400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_pod_access_tunnel_801852F4[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_80135C30 },
    { 11, 11, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_8015F400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_pod_access_tunnel_80185318[3] = {
    { 26, 26, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gMaggotCaterpillarBodyTask },
    { 49, 49, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_8015F400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_pod_access_tunnel_8018533C[2] = {
    { 3, 3, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_80148110 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_pod_access_tunnel_80185354[2] = {
    { 23, 23, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_80147AB8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_pod_access_tunnel_8018536C[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_80135C30 },
    { 23, 23, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_8015FAB8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_pod_access_tunnel_80185390[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_80135C30 },
    { 56, 56, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_801602C0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b2_pod_access_tunnel_801853B4[2] = {
    { 39, 39, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, D_actor_403900_801540E0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b2_pod_access_tunnel_801853CC[5] = {
    { 21, 4, 0, 1200, -2200, -800, 2048, 0, 0, 2, 0 },
    { 21, 4, 0, 1950, -2200, -800, 2048, 0, 0, 2, 0 },
    { 21, 4, 0, 2700, -2200, -800, 2048, 0, 0, 2, 0 },
    { 11, 0, 0, 1600, 0, -5200, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_pod_access_tunnel_8018541C[5] = {
    { 21, 34, 2048, 1000, -1900, -800, 2048, 0, 0, 2, 1 },
    { 21, 34, 2048, 1000, -1400, -800, 2048, 0, 0, 2, 1 },
    { 11, 2, 0, 3200, 0, -1300, 4000, 0, 2, 4, 0 },
    { 11, 2, 0, 1950, 0, -4700, 3900, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_pod_access_tunnel_8018546C[5] = {
    { 26, 0, 0, 1200, 0, -2800, 1700, 0, 0, 2, 0 },
    { 26, 0, 0, 2500, 0, -1000, 1300, 0, 0, 2, 0 },
    { 26, 0, 0, 1100, 0, -1100, 1900, 0, 0, 2, 0 },
    { 49, 1, 0, 1600, 0, -1950, 2048, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_pod_access_tunnel_801854BC[2] = {
    { 3, 0, 0, 1700, 0, -5000, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_pod_access_tunnel_801854DC[2] = {
    { 23, 7, 1, 1700, 0, -8600, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_pod_access_tunnel_801854FC[4] = {
    { 21, 34, 2048, 1000, -1900, -600, 2048, 0, 0, 2, 3 },
    { 21, 34, 2048, 1000, -1400, -600, 2048, 0, 0, 2, 3 },
    { 23, 0, 0, 5200, 0, -1750, 3072, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_pod_access_tunnel_8018553C[5] = {
    { 21, 1, 0, 1200, -2200, -800, 2048, 0, 0, 2, 1 },
    { 21, 1, 0, 1950, -2200, -800, 2048, 0, 0, 2, 1 },
    { 21, 1, 0, 2700, -2200, -800, 2048, 0, 0, 2, 1 },
    { 56, 0, 0, 1700, 0, -8000, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_pod_access_tunnel_8018558C[2] = {
    { 39, 0, 0, 7000, 0, 1500, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b2_pod_access_tunnel_801855AC[23] = {
    { NULL, NULL },
    { D_shelter_b2_pod_access_tunnel_801853CC, D_shelter_b2_pod_access_tunnel_801852D0 },
    { D_shelter_b2_pod_access_tunnel_8018541C, D_shelter_b2_pod_access_tunnel_801852F4 },
    { D_shelter_b2_pod_access_tunnel_8018546C, D_shelter_b2_pod_access_tunnel_80185318 },
    { D_shelter_b2_pod_access_tunnel_801854BC, D_shelter_b2_pod_access_tunnel_8018533C },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_pod_access_tunnel_801854DC, D_shelter_b2_pod_access_tunnel_80185354 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_pod_access_tunnel_801854FC, D_shelter_b2_pod_access_tunnel_8018536C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_pod_access_tunnel_8018553C, D_shelter_b2_pod_access_tunnel_80185390 },
    { D_shelter_b2_pod_access_tunnel_8018558C, D_shelter_b2_pod_access_tunnel_801853B4 },
};

WorldCollisionOccluder D_shelter_b2_pod_access_tunnel_80185664[1] = {
    { NULL, NULL, { 4815, -1856, -6112, 0 }, { { -1689, 2880, 2964, 0 }, { 1690, 2880, -2964, 0 }, { -1689, -2880, 2964, 0 }, { 1690, -2880, -2964, 0 } }, { 3574, 0, 2037, 0 }, 4463, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// On the task's first tick stores seven room-specific values into resident
/// gameplay globals, then draws the beams
/// (`glowDrawCone`) the current camera view
/// shows.
void func_shelter_b2_pod_access_tunnel_8017DC6C(Task* arg0)
{
    if (arg0->state == 0) {
        gRoomEffectMoteId         = EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_MOTE;
        gRoomEffectHaloId         = EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_HALO;
        gRoomEffectOrangeBurstId  = EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_ORANGE_BURST;
        gRoomEffectSparkEmitterId = EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_SPARK_EMITTER;
        gRoomEffectFlashId        = EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_FLASH;
        gRoomEffectTwinTrailId    = EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_TWIN_TRAIL;
        gRoomEffectSparkBurstId   = EFFECT_SHELTER_B2_POD_ACCESS_TUNNEL_SPARK_BURST;
        arg0->state               = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2: {
            SVECTOR* p;
            p = D_shelter_b2_pod_access_tunnel_80183C08;
            glowDrawCone(&p[0], 0x200, 0x400);
            glowDrawCone(&p[2], 0x200, 0x400);
            glowDrawCone(&p[4], 0x200, 0x400);
            glowDrawCone(&p[6], 0x200, 0x400);
            glowDrawCone(&p[12], 0x200, 0);
            glowDrawCone(&p[14], 0x200, 0);
            glowDrawCone(&p[16], 0x200, 0);
            glowDrawCone(&p[18], 0x200, 0);
            glowDrawCone(&p[28], 0x200, 0x400);
            glowDrawCone(&p[30], 0x200, 0x400);
            glowDrawCone(&p[32], 0x200, 0x400);
            glowDrawCone(&p[34], 0x200, 0x400);
            glowDrawCone(&p[40], 0x200, 0);
            glowDrawCone(&p[42], 0x200, 0);
            glowDrawCone(&p[44], 0x200, 0);
            glowDrawCone(&p[46], 0x200, 0);
            break;
        }
        case 3:
        case 6: {
            SVECTOR* p;
            p = D_shelter_b2_pod_access_tunnel_80183C48;
            glowDrawCone(&p[0], 0x200, -0x400);
            glowDrawCone(&p[2], 0x200, -0x400);
            glowDrawCone(&p[10], 0x200, 0);
            glowDrawCone(&p[12], 0x200, 0);
            glowDrawCone(&p[14], 0x200, 0x800);
            glowDrawCone(&p[16], 0x200, 0x800);
            glowDrawCone(&p[28], 0x200, -0x400);
            glowDrawCone(&p[30], 0x200, -0x400);
            glowDrawCone(&p[38], 0x200, 0);
            glowDrawCone(&p[40], 0x200, 0);
            glowDrawCone(&p[42], 0x200, 0x800);
            glowDrawCone(&p[44], 0x200, 0x800);
            break;
        }
        case 4:
        case 7: {
            SVECTOR* p;
            p = D_shelter_b2_pod_access_tunnel_80183CC8;
            glowDrawCone(&p[0], 0x200, -0x400);
            glowDrawCone(&p[2], 0x200, -0x400);
            glowDrawCone(&p[28], 0x200, -0x400);
            glowDrawCone(&p[30], 0x200, -0x400);
            break;
        }
    }
}

#include "../../shared/glow_draw_cone.inc.c"

#include "../../shared/effect_sprite_drift.inc.c"

#include "../../shared/effect_sprite_draw_banked.inc.c"

#include "../../shared/effect_sprite_draw_rotated.inc.c"

#include "../../shared/room_visual_effects.inc.c"

void func_shelter_b2_pod_access_tunnel_8017F608(Task* task)
{
    RoomFx_MoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void func_shelter_b2_pod_access_tunnel_80180350(Task* arg0)
{
    RoomFx_HaloTask(arg0);
}

void func_shelter_b2_pod_access_tunnel_801806E8(Task* arg0)
{
    RoomFx_OrangeBurstTask(arg0);
}

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void func_shelter_b2_pod_access_tunnel_80181AF8(Task* arg0)
{
    RoomFx_SparkEmitterTask(arg0);
}

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_shelter_b2_pod_access_tunnel_80181C2C(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_shelter_b2_pod_access_tunnel_80182690(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b2_pod_access_tunnel_80182F78(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
