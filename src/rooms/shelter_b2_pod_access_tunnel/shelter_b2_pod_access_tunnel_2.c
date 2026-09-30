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
#include "gameplay/direction_input.h"
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
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "rooms/rooms_shared_8017dcb8.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"

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

static void func_shelter_b2_pod_access_tunnel_8017ED5C(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3);
static void func_shelter_b2_pod_access_tunnel_8017F1BC(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3);

extern GpGridParams   D_shelter_b2_pod_access_tunnel_801841B4[1];
extern GpObj3A        D_shelter_b2_pod_access_tunnel_80185664[1];
extern GpObj4C        D_shelter_b2_pod_access_tunnel_80184FD8[4];
extern GpObj4C        D_shelter_b2_pod_access_tunnel_80185108[3];
extern GpObj4C        D_shelter_b2_pod_access_tunnel_801851EC[3];
extern GpRoomCoordSet D_shelter_b2_pod_access_tunnel_80184FC0[1];

TaskDesc D_shelter_b2_pod_access_tunnel_80183BC0 = { 0, 32, func_shelter_b2_pod_access_tunnel_8017D62C, { .model = NULL } };

GpMsgEntry D_shelter_b2_pod_access_tunnel_80183BCC[6] = {
    { 5102, func_shelter_b2_pod_access_tunnel_8017D7C4 },
    { 5105, func_shelter_b2_pod_access_tunnel_8017DB28 },
    { 5103, func_shelter_b2_pod_access_tunnel_8017DB70 },
    { 5104, func_shelter_b2_pod_access_tunnel_8017DB30 },
    { 5106, func_shelter_b2_pod_access_tunnel_8017DB78 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_shelter_b2_pod_access_tunnel_80183BFC = { 0, 32, func_shelter_b2_pod_access_tunnel_8017D9A8, { .model = NULL } };

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

static inline RoomHaloShade* RoomFx_GetHaloShades(void)
{
    return _gRoomEffectHaloShades.entries;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

#include "../../shared/room_visual_effects_trail_data.inc.c"

GpRoomObjRec D_shelter_b2_pod_access_tunnel_80183DEC[2] = {
    { D_shelter_b2_pod_access_tunnel_801841B4, D_shelter_b2_pod_access_tunnel_80184FD8, D_shelter_b2_pod_access_tunnel_80185108, D_shelter_b2_pod_access_tunnel_80185664 },
    { D_shelter_b2_pod_access_tunnel_801841B4, D_shelter_b2_pod_access_tunnel_80184FD8, D_shelter_b2_pod_access_tunnel_801851EC, D_shelter_b2_pod_access_tunnel_80185664 },
};

GpRoomCoordRec D_shelter_b2_pod_access_tunnel_80183E0C[2] = {
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
    D_8010CAF8,
    D_shelter_b2_pod_access_tunnel_80183E1C,
};

GpViewCountRec D_shelter_b2_pod_access_tunnel_80183E2C[2] = {
    { { .bytes = { 7, 0 } } },
    { { .bytes = { 7, 0 } } },
};

GpWarpRec D_shelter_b2_pod_access_tunnel_80183E30[3] = {
    { { .words = { 0, 1700, 0, -0x28CD } }, { 0, 0, 0, 0 }, { .words = { 256, 1050, 0, -0x2710 } }, { 0, 0, 0, 0 }, 0x54230002, 0x54230001, 0, 2, 0, 0 },
    { { .words = { 3072, 5500, 0, -1700 } }, { 0, 0, 0, 0 }, { .words = { 3072, 5500, 0, -1700 } }, { 0, 0, 0, 0 }, 0, 0x54230006, 0x54230003, 4, 0, 449 },
    { { .words = { 0, 4800, 0, -2400 } }, { 0, 0, 0, 0 }, { .words = { 2560, 4500, 0, -1250 } }, { 0, 0, 0, 0 }, 0x54230005, 0, 0, 4, 0, 438 },
};

SVECTOR D_shelter_b2_pod_access_tunnel_80183ED8[5] = {
#include "assets/shelter_b2_pod_access_tunnel_collision_06BF4_normals.inc"
};

SVECTOR D_shelter_b2_pod_access_tunnel_80183F00[40] = {
#include "assets/shelter_b2_pod_access_tunnel_collision_06BF4_verts.inc"
};

GpGridFace D_shelter_b2_pod_access_tunnel_80184040[17] = {
#include "assets/shelter_b2_pod_access_tunnel_collision_06BF4_faces.inc"
};

s16 D_shelter_b2_pod_access_tunnel_8018410C[72] = {
#include "assets/shelter_b2_pod_access_tunnel_collision_06BF4_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b2_pod_access_tunnel_8018410C[i])
s16* D_shelter_b2_pod_access_tunnel_8018419C[6] = {
#include "assets/shelter_b2_pod_access_tunnel_collision_06BF4_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b2_pod_access_tunnel_801841B4[1] = {
    { NULL, D_shelter_b2_pod_access_tunnel_80183ED8, D_shelter_b2_pod_access_tunnel_80183F00, D_shelter_b2_pod_access_tunnel_80184040, D_shelter_b2_pod_access_tunnel_8018419C, -500, 0x2B8E, 2, 3, 4000, 17 },
};

GpViewRec D_shelter_b2_pod_access_tunnel_801841D8[7] = {
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

GpSprtRec D_shelter_b2_pod_access_tunnel_80184C6C[7] = {
    { { .empty = D_shelter_b2_pod_access_tunnel_801842D4 }, D_shelter_b2_pod_access_tunnel_801842D4, NULL },
    { { .empty = D_shelter_b2_pod_access_tunnel_801842E4 }, D_shelter_b2_pod_access_tunnel_801842E4, NULL },
    { { .elements = D_shelter_b2_pod_access_tunnel_801842F4 }, D_shelter_b2_pod_access_tunnel_801847A4, NULL },
    { { .empty = D_shelter_b2_pod_access_tunnel_801847D4 }, D_shelter_b2_pod_access_tunnel_801847D4, NULL },
    { { .empty = D_shelter_b2_pod_access_tunnel_801847E4 }, D_shelter_b2_pod_access_tunnel_801847E4, NULL },
    { { .elements = D_shelter_b2_pod_access_tunnel_801847F4 }, D_shelter_b2_pod_access_tunnel_80184C2C, NULL },
    { { .empty = D_shelter_b2_pod_access_tunnel_80184C5C }, D_shelter_b2_pod_access_tunnel_80184C5C, NULL },
};

GpPointLight D_shelter_b2_pod_access_tunnel_80184CC0[8] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -3949 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -5618 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -6721 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -8147 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -9711 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4126, -1155, -2029 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3378, 3157, 3496 }, { 0, 0 } }, 1200, 2620 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3177, 0, -1757 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -2395 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 1200, 2500 },
};

GpRoomCoordSet D_shelter_b2_pod_access_tunnel_80184FC0[1] = {
    { 0, NULL, 8, D_shelter_b2_pod_access_tunnel_80184CC0, 0, NULL },
};

GpObj4C D_shelter_b2_pod_access_tunnel_80184FD8[4] = {
    { NULL, NULL, NULL, { 1760, -1648, -4513, 0 }, { { -1824, -2224, 64, 0 }, { 1824, -2224, -64, 0 }, { -1824, 2224, 64, 0 }, { 1824, 2224, -64, 0 } }, { -144, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 2873, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 1744, -1632, -4625, 0 }, { { 1808, -2224, -80, 0 }, { -1808, -2224, 80, 0 }, { 1808, 2224, -80, 0 }, { -1808, 2224, 80, 0 } }, { 180, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 2862, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 3520, -1632, -1360, 0 }, { { -285, -2224, -1838, 0 }, { 254, -2224, 1809, 0 }, { -285, 2224, -1838, 0 }, { 254, 2224, 1809, 0 } }, { 4054, 0, -601, 0 }, { 0, 0, 4096, 0 }, 2884, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 3606, -1664, -1488, 0 }, { { 250, -2224, 1567, 0 }, { -271, -2224, -1585, 0 }, { 250, 2224, 1567, 0 }, { -271, 2224, -1585, 0 } }, { -4049, 0, 668, 0 }, { 0, 0, 4096, 0 }, 2733, 0, 3, 4, 129, 0 },
};

GpObj4C D_shelter_b2_pod_access_tunnel_80185108[3] = {
    { NULL, NULL, NULL, { 1728, -48, -0x28CF, 0 }, { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1123, 0, 34, 18, 2, 0 },
    { NULL, NULL, NULL, { 5856, -48, -1728, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, 0, 48, 33, 2, 0 },
    { NULL, NULL, NULL, { 4592, -64, -2784, 0 }, { { -944, 0, -464, 0 }, { 944, 0, -464, 0 }, { -944, 0, 464, 0 }, { 944, 0, 464, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1047, 2, 1, 255, 130, 0 },
};

GpObj4C D_shelter_b2_pod_access_tunnel_801851EC[3] = {
    { NULL, NULL, NULL, { 1728, -48, -0x28CF, 0 }, { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1123, 0, 34, 18, 2, 0 },
    { NULL, NULL, NULL, { 5856, -48, -1728, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, 0, 48, 33, 2, 0 },
    { NULL, NULL, NULL, { 4592, -64, -2784, 0 }, { { -944, 0, -464, 0 }, { 944, 0, -464, 0 }, { -944, 0, 464, 0 }, { 944, 0, 464, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1047, 2, 1, 255, 130, 0 },
};

GpAreaTmdRec D_shelter_b2_pod_access_tunnel_801852D0[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 11, 11, 1, 0, { 0, 0 }, D_8015F400 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b2_pod_access_tunnel_801852F4[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 11, 11, 1, 0, { 0, 0 }, D_8015F400 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b2_pod_access_tunnel_80185318[3] = {
    { 26, 26, 0, 0, { 0, 0 }, D_8013A8D4 },
    { 49, 49, 1, 0, { 0, 0 }, D_8015F400 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b2_pod_access_tunnel_8018533C[2] = {
    { 3, 3, 0, 0, { 0, 0 }, D_80148110 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b2_pod_access_tunnel_80185354[2] = {
    { 23, 23, 0, 0, { 0, 0 }, D_80147AB8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b2_pod_access_tunnel_8018536C[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 23, 23, 1, 0, { 0, 0 }, D_8015FAB8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b2_pod_access_tunnel_80185390[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 56, 56, 1, 0, { 0, 0 }, D_801602C0 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b2_pod_access_tunnel_801853B4[2] = {
    { 39, 39, 3, 0, { 0, 0 }, D_801540E0 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
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

GpAreaVariant D_shelter_b2_pod_access_tunnel_801855AC[23] = {
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

GpObj3A D_shelter_b2_pod_access_tunnel_80185664[1] = {
    { NULL, NULL, { 4815, -1856, -6112, 0 }, { { -1689, 2880, 2964, 0 }, { 1690, 2880, -2964, 0 }, { -1689, -2880, 2964, 0 }, { 1690, -2880, -2964, 0 } }, { 3574, 0, 2037, 0 }, { 111, 17 }, 129, 0 },
};

/// On the task's first tick stores seven room-specific values into resident
/// gameplay globals, then draws the beams
/// (`glowDrawCone`) the current camera view
/// shows.
void func_shelter_b2_pod_access_tunnel_8017DC6C(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115728  = 0x6024C;
        D_80115744  = 0x60258;
        D_8011573C  = 0x60263;
        D_80115720  = 0x6026F;
        D_80115758  = 0x601D5;
        D_8011572C  = 0x601F1;
        D_80115750  = 0x6020D;
        arg0->state = 1;
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

/// An animated sprite effect, drawn through
/// `func_shelter_b2_pod_access_tunnel_8017ED5C` (state 1) or
/// `func_shelter_b2_pod_access_tunnel_8017F1BC` (state 2, when the spawn
/// argument is negative). The first tick unpacks the spawn argument: the low
/// 12 bits are the sprite size, bits 12..14 the ticks per animation cell (1
/// when zero) and bits 28..30 the drawer's palette bank. When the work block
/// arrives without a velocity, bits 24..27 choose how one is rolled from
/// `Gp_LcgState` (0 leaves the sprite still) and it is scaled to the speed in
/// bits 16..23 (0x40 when zero). Each later tick draws the current cell, moves
/// the coordinate and bends the vertical velocity, and releases the work block
/// after the drawer's last cell (12 or 10). While the room is in an event it
/// only draws, and releases once the event state reaches 4.
void func_shelter_b2_pod_access_tunnel_8017E6E0(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    SVECTOR*   vec;
    s32        step;
    s32        level;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (task->state < 2) {
            func_shelter_b2_pod_access_tunnel_8017ED5C(coord, work->index | work->pos.vx, work->scale, work->angle);
        } else {
            func_shelter_b2_pod_access_tunnel_8017F1BC(coord, work->index | work->pos.vx, work->scale, work->angle);
        }
        if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = task->spawnArg1.value & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1.value & 0xF000) {
                step = (task->spawnArg1.value >> 12) & 7;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            task->state  = 1;
            task->state  = task->spawnArg1.value < 0 ? 2 : 1;
            work->pos.vx = (task->spawnArg1.value >> 16) & 0x7000;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1.value & 0xFF0000) {
                    level = (task->spawnArg1.value >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                switch ((task->spawnArg1.value >> 24) & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                    case 6:
                        work->move.vy = 0;
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            break;
        case 1:
            func_shelter_b2_pod_access_tunnel_8017ED5C(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                if (((task->spawnArg1.value >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 2;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 12) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
        case 2:
            func_shelter_b2_pod_access_tunnel_8017F1BC(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                if (((task->spawnArg1.value >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 1;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 10) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
    }
}

/// Draws one cell of a 5-column, 48-texel sprite sheet (tpage 0x2B) as a
/// semi-transparent `POLY_FT4` centred on the coordinate's projected position.
/// `arg1`'s low 12 bits are the cell index and its top nibble the palette
/// bank, `arg2` the half-extent (scaled by 47 over depth) and `arg3` the
/// quad's rotation. Nothing is drawn when the projection fails.
static void func_shelter_b2_pod_access_tunnel_8017ED5C(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    u16              col;
    u16              row;
    s32              u0;
    s32              v0;
    s32              ang;
    s32              ang2;
    u16              bank;
    u32              idx;

    head                                      = SCRATCH_STACK_CURSOR(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    SCRATCH_STACK_CURSOR(void)                = head - 0x1C;
    block                                     = SCRATCH_STACK_CURSOR(GpFxQuadScratch);
    block->vec.vy                             = arg0->workm.t[1];
    block->vec.vz                             = arg0->workm.t[2];
    idx                                       = arg1;
    idx                                      &= 0xFFF;
    bank                                      = arg1 >> 12;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        if (bank >= 2) {
            prim->clut = 0x428F;
        } else {
            prim->clut = ((bank + 0x10E) << 6) | (idx & 0x3F);
        }
        col = (u16)idx % 5;
        row = (u16)idx / 5;
        ang = arg3;
        u0  = col * 0x30;
        v0  = row * 0x30;
        setUV4(prim, u0, v0 + 0x68, u0 + 0x2F, v0 + 0x68, u0, v0 - 0x69, u0 + 0x2F, v0 - 0x69);
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = ang + 0x400;
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}

/// The same sprite drawer as `func_shelter_b2_pod_access_tunnel_8017ED5C` for
/// the sheet on tpage 0x2C, with one of two fixed palettes chosen by the top
/// nibble of `arg1`.
static void func_shelter_b2_pod_access_tunnel_8017F1BC(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    u16              col;
    u16              row;
    s32              u0;
    s32              v0;
    s32              ang;
    s32              ang2;
    u16              bank;
    u16              vz;

    bank                                      = arg1 >> 12;
    arg1                                     &= 0xFFF;
    head                                      = SCRATCH_STACK_CURSOR(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    SCRATCH_STACK_CURSOR(void)                = head - 0x1C;
    block                                     = SCRATCH_STACK_CURSOR(GpFxQuadScratch);
    block->vec.vy                             = arg0->workm.t[1];
    block->vec.vz                             = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2C;
        prim->clut  = bank ? 0x428F : 0x43D0;
        col         = arg1 % 5;
        row         = arg1 / 5;
        ang         = arg3;
        u0          = col * 0x30;
        v0          = row * 0x30;
        setUV4(prim, u0, v0 - 0x80, u0 + 0x2F, v0 - 0x80, u0, v0 - 0x51, u0 + 0x2F, v0 - 0x51);
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = ang + 0x400;
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}

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
