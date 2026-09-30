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

extern SVECTOR D_shelter_b2_pod_access_tunnel_80183DDC[2];

// Preserve the nonzero halfword after the three effect records.
// Its role is unresolved; it may be retained exporter padding.
typedef struct {
    RoomHaloShade entries[3];
    u16           retained;
} ShelterB2PodAccessTunnelHaloStorage;
STATIC_ASSERT_SIZEOF(ShelterB2PodAccessTunnelHaloStorage, 20);
extern ShelterB2PodAccessTunnelHaloStorage D_shelter_b2_pod_access_tunnel_80183DC8;

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
/// `D_shelter_b2_pod_access_tunnel_80183DDC[1]`.

static void func_shelter_b2_pod_access_tunnel_8017DF64(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b2_pod_access_tunnel_8017ED5C(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3);
static void func_shelter_b2_pod_access_tunnel_8017F1BC(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3);
static void func_shelter_b2_pod_access_tunnel_8017F8D4(GfxCoord* arg0, u16 arg1, u16 arg2, u16 arg3);
static void func_shelter_b2_pod_access_tunnel_80180894(GfxCoord* coord, s16 size);
static void func_shelter_b2_pod_access_tunnel_80180DC0(GfxCoord* arg0, s32 arg1);
static void func_shelter_b2_pod_access_tunnel_80181138(GfxCoord* arg0, s16 arg1, u8* arg2);
static void func_shelter_b2_pod_access_tunnel_80181ED0(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_shelter_b2_pod_access_tunnel_801822FC(GfxCoord* arg0, s16 arg1, u8* rgb);
static void func_shelter_b2_pod_access_tunnel_80182B80(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3);
static void func_shelter_b2_pod_access_tunnel_80183200(GfxCoord* arg0, s16 arg1, u8* arg2);

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

ShelterB2PodAccessTunnelHaloStorage D_shelter_b2_pod_access_tunnel_80183DC8 = { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 0xA041 };

// The following record is dereferenced through an indexed view of this base; keep the complete bounded pool.
SVECTOR D_shelter_b2_pod_access_tunnel_80183DDC[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

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

GpSprtElem D_shelter_b2_pod_access_tunnel_801842F4[60] = {
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

GpSprtElem D_shelter_b2_pod_access_tunnel_801847F4[54] = {
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
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -3949 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 1200, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -5618 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 1200, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -6721 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 1200, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -8147 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 1200, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -9711 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 1200, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4126, -1155, -2029 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3378, 3157, 3496, { 0, 0 } }, 1200, 2620 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3177, 0, -1757 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 1200, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1793, 0, -2395 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 1200, 2500 },
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

static void func_shelter_b2_pod_access_tunnel_8017FB98(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_shelter_b2_pod_access_tunnel_8017FFBC(GfxCoord* arg0, s16 arg1, u8* rgb);

/// On the task's first tick stores seven room-specific values into resident
/// gameplay globals, then draws the beams
/// (`func_shelter_b2_pod_access_tunnel_8017DF64`) the current camera view
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
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[0], 0x200, 0x400);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[2], 0x200, 0x400);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[4], 0x200, 0x400);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[6], 0x200, 0x400);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[12], 0x200, 0);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[14], 0x200, 0);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[16], 0x200, 0);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[18], 0x200, 0);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[28], 0x200, 0x400);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[30], 0x200, 0x400);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[32], 0x200, 0x400);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[34], 0x200, 0x400);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[40], 0x200, 0);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[42], 0x200, 0);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[44], 0x200, 0);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[46], 0x200, 0);
            break;
        }
        case 3:
        case 6: {
            SVECTOR* p;
            p = D_shelter_b2_pod_access_tunnel_80183C48;
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[0], 0x200, -0x400);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[2], 0x200, -0x400);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[10], 0x200, 0);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[12], 0x200, 0);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[14], 0x200, 0x800);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[16], 0x200, 0x800);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[28], 0x200, -0x400);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[30], 0x200, -0x400);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[38], 0x200, 0);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[40], 0x200, 0);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[42], 0x200, 0x800);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[44], 0x200, 0x800);
            break;
        }
        case 4:
        case 7: {
            SVECTOR* p;
            p = D_shelter_b2_pod_access_tunnel_80183CC8;
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[0], 0x200, -0x400);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[2], 0x200, -0x400);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[28], 0x200, -0x400);
            func_shelter_b2_pod_access_tunnel_8017DF64(&p[30], 0x200, -0x400);
            break;
        }
    }
}

/// Projects the two ends of the segment `arg0[0]`..`arg0[1]` through
/// `gGfxViewCoord.workm` and, when the second end lies beyond depth 0x10, queues a
/// glowing beam of gouraud `POLY_G4` quads: a half-disc around the first end,
/// the band joining the two ends and a half-disc around the second. `arg1` is
/// the world half-width, scaled by 64 over each end's depth, and `arg2` the
/// angle the discs start from. The centre colour flickers between two greys
/// with the frame counter; every rim fades to black.
static void func_shelter_b2_pod_access_tunnel_8017DF64(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw11Scratch* block;
    POLY_G4*           prim;
    POLY_G4*           p;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t2;
    s32                t3;
    s32                rgb;
    s32                extent;
    s32                r0;
    s32                r1;
    s32                base;

    {
        void** scratch;
        u8*    tmp;

        scratch  = SCRATCH_STACK_CURSOR_SLOT;
        head     = *scratch;
        tmp      = head - 0x18;
        *scratch = tmp;
        p1       = arg0 + 1;
        block    = (RoomDraw11Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(p1);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx1);
    gte_stszotz(&((RoomDraw11Scratch*)(head - 0x18))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw11Scratch*)(head - 0x18))->otz0 < 0x10) {
            ((RoomDraw11Scratch*)(head - 0x18))->otz0 = 0x10;
        }
        extent    = (s16)arg1 * 64;
        r0        = extent / ((RoomDraw11Scratch*)(head - 0x18))->otz0;
        r1        = extent / block->otz1;
        ang       = 0;
        base      = (s16)arg2;
        rgb       = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            p        = prim;
            p->r2    = rgb;
            p->g2    = rgb;
            prim->b2 = rgb;
            p->r3    = 0;
            p->g3    = 0;
            p->b3    = 0;
            p->x0    = block->sx0 + ((block->r0 * rsin(base + ang)) >> 12);
            p->y0    = block->sy0 + ((block->r0 * rcos(base + ang)) >> 12);
            t        = ang + 0x200;
            prim->x1 = block->sx0 + ((block->r0 * rsin(base + t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(base + t)) >> 12);
            t2       = ang + 0x400;
            p->x2    = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(base + t2)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(base + t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, rgb, rgb, rgb);
            prim->x0 = block->sx0 + ((block->r0 * rsin(base + (ang * 2))) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(base + (ang * 2))) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(base + (ang * 2))) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base + (ang * 2))) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            t3             = ang - 0x1000;
            prim           = gGpuPrimCursor;
            t              = ang - 0x1000;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(base - t3)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xE00;
            prim->x1 = block->sx1 + ((block->r1 * rsin(base - t)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xC00;
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            t        = base - t;
            prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_POP_BYTES(0x18);
}

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

    head                                      = SCRATCH_HEAD(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    SCRATCH_HEAD(void)                        = head - 0x1C;
    block                                     = SCRATCH_HEAD(GpFxQuadScratch);
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
    SCRATCH_POP_BYTES(0x1C);
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
    head                                      = SCRATCH_HEAD(u8);
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    SCRATCH_HEAD(void)                        = head - 0x1C;
    block                                     = SCRATCH_HEAD(GpFxQuadScratch);
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
    SCRATCH_POP_BYTES(0x1C);
}

/// A drifting mote. The first tick unpacks the spawn argument: with either of
/// its low two bits set the mote starts at full brightness 0x80 and moves at
/// the packed speed (upwards when bit 1 is set); otherwise it starts dim at
/// 0x20 and rises at the packed speed plus a random 0..0x3F. Each later tick
/// moves the task's coordinate by that speed and, every other tick, draws it
/// through `func_shelter_b2_pod_access_tunnel_8017F8D4`. The dim kind brightens
/// towards 0x80 until eight ticks before its lifetime runs out, and both fade
/// by 0x10 a tick from then on. The work block is released once the mote is
/// dark, or when the room's event state reaches 4.
void func_shelter_b2_pod_access_tunnel_8017F608(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    s32        lifetime;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->age++;
        switch (task->state) {
            case 0:
                if (task->spawnArg1.value & 3) {
                    work->scale   = 0x80;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1.value)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = ((RoomMoteArg*)&task->spawnArg1.value)->speed;
                    work->move.vz = 0;
                    if (task->spawnArg1.value & 2) {
                        work->move.vy = -work->move.vy;
                    }
                    task->state = 2;
                } else {
                    work->scale   = 0x20;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1.value)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = -((RoomMoteArg*)&task->spawnArg1.value)->speed - (((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x3F);
                    work->move.vz = 0;
                    task->state   = (task->spawnArg1.value & 1) + 1;
                }
                break;
            case 1:
                coord->coord.t[1]  += work->move.vy;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_shelter_b2_pod_access_tunnel_8017F8D4(coord, work->index, work->angle | 0x1000, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    } else if (work->scale < 0x80) {
                        work->scale += 0x20;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
            case 2:
                coord->coord.t[1]  += work->move.vy;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_shelter_b2_pod_access_tunnel_8017F8D4(coord, work->index, work->angle, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws one mote: projects the coordinate's world position through
/// `GsWSMATRIX` and, unless the GTE flags the projection, queues one
/// semi-transparent textured square centred on it. `arg1`'s low two bits and
/// `arg2`'s top nibble pick the 24-texel texture cell, `arg2`'s low twelve
/// bits are the half-extent (scaled by 23 / (otz + 1)), `arg3`'s low byte is
/// the grey level and its top nibble picks the palette.
static void func_shelter_b2_pod_access_tunnel_8017F8D4(GfxCoord* arg0, u16 arg1, u16 arg2, u16 arg3)
{
    GpRingScratch* block;
    POLY_FT4*      prim;
    DisplayState*  ds;
    u16            row;
    u16            pal;
    s32            u0;
    s32            u1;
    s16            xy;

    row           = arg2 >> 12;
    arg2         &= 0xFFF;
    pal           = arg3 >> 12;
    arg3         &= 0xFF;
    block         = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        setRGB0(prim, arg3, arg3, arg3);
        if (pal != 0) {
            prim->clut = getClut(pal * 16 + 0xF0, 0x10B);
        } else {
            prim->clut = getClut(0xB0, 0x10B);
        }
        u0 = row * 0x60 + (arg1 & 3) * 24;
        u1 = u0 + 0x17;
        setUV4(prim, u0, 0, u1, 0, u0, 0x17, u1, 0x17);
        block->step = arg2 * 23 / block->otz;
        xy          = block->sx - block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = block->sx + block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = block->sy - block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = block->sy + block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpRingScratch);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when it
/// projects, queues a ring of sixteen gouraud `POLY_G4` wedges between the
/// radii `arg1` and `arg1 + arg2`, each scaled by 64 over `otz + 1`. The edge
/// at `arg1` is black and the edge at `arg1 + arg2` takes the colour `rgb`.
static void func_shelter_b2_pod_access_tunnel_8017FB98(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   next;
    s32                   inner;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    inner         = arg1 + arg2;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = ((s16)arg1 * 64) / block->otz;
        block->rInner = ((s16)inner * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(next)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(next)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(next)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(next)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomBillboardScratch);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when it
/// projects, queues eight gouraud `POLY_G4` wedges filling a disc around the
/// point, `rgb` at the centre and black at the rim. `arg1` is the radius,
/// scaled by 64 over `otz + 1`.
static void func_shelter_b2_pod_access_tunnel_8017FFBC(GfxCoord* arg0, s16 arg1, u8* rgb)
{
    RoomFanScratch* block;
    POLY_G4*        prim;
    s32             ang;
    s32             otz;

    block         = SCRATCH_PUSH(RoomFanScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        otz           = block->otz + 1;
        block->otz    = otz;
        block->radius = (arg1 * 64) / otz;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomFanScratch);
}

/// An expanding halo. The first tick hangs the effect's coordinate off its
/// parent at the spawn position, takes the tint row and the tick count from
/// the spawn argument and works out the per-tick step. While the count runs
/// down the level and angle grow by that step and the halo is drawn through
/// `func_shelter_b2_pod_access_tunnel_8017FFBC`, with a half-bright copy 0x100
/// further round on every odd tick, and a ring through
/// `func_shelter_b2_pod_access_tunnel_8017FB98` whose inner radius shrinks as
/// the angle grows. The level then jumps to 0xFF and fades by 0x10 a tick
/// through `func_shelter_b2_pod_access_tunnel_80181138` before the work block
/// is released, as it is when the room's event state reaches 4. Each channel
/// is the level shifted right by the tint row's entry.
void func_shelter_b2_pod_access_tunnel_80180350(Task* arg0)
{
    u8          rgb[3];
    GpEffWork*  mem;
    GfxCoord*   coord;
    GpMtxWords* rot;
    s16         flag;
    s32         shift;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        switch (arg0->state) {
            case 0:
                rot                 = (GpMtxWords*)&coord->coord;
                coord->parent       = mem->parent;
                rot->m00_m01        = 0x1000;
                rot->m02_m10        = 0;
                rot->m11_m12        = 0x1000;
                rot->m20_m21        = 0;
                rot->m22            = 0x1000;
                coord->coord.t[0]   = mem->pos.vx;
                coord->coord.t[1]   = mem->pos.vy;
                coord->coord.t[2]   = mem->pos.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                shift                 = arg0->spawnArg1.halves.high;
                mem->index            = shift;
                arg0->spawnArg1.value = arg0->spawnArg1.halves.low;
                arg0->state           = 1;
                mem->step             = 0x100 / arg0->spawnArg1.value;
                return;
            case 1:
                Gp_UpdateCoord(coord);
                mem->scale            += mem->step;
                mem->angle            += mem->step;
                arg0->spawnArg1.value -= 1;
                rgb[0]                 = mem->scale >> D_shelter_b2_pod_access_tunnel_80183DC8.entries[mem->index].r;
                rgb[1]                 = mem->scale >> D_shelter_b2_pod_access_tunnel_80183DC8.entries[mem->index].g;
                rgb[2]                 = mem->scale >> D_shelter_b2_pod_access_tunnel_80183DC8.entries[mem->index].b;
                func_shelter_b2_pod_access_tunnel_8017FFBC(coord, mem->angle, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                if (mem->age & 1) {
                    func_shelter_b2_pod_access_tunnel_8017FFBC(coord, (s16)(mem->angle + 0x100), rgb);
                }
                func_shelter_b2_pod_access_tunnel_8017FB98(coord, (s16)(0x300 - (u16)mem->angle * 2), 0x80, rgb);
                if (arg0->spawnArg1.value == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    return;
                }
                return;
            case 2:
                Gp_UpdateCoord(coord);
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale >> D_shelter_b2_pod_access_tunnel_80183DC8.entries[mem->index].r;
                    rgb[1] = mem->scale >> D_shelter_b2_pod_access_tunnel_80183DC8.entries[mem->index].g;
                    rgb[2] = mem->scale >> D_shelter_b2_pod_access_tunnel_80183DC8.entries[mem->index].b;
                    func_shelter_b2_pod_access_tunnel_80181138(coord, (u16)mem->angle * 4, rgb);
                    mem->scale -= 0x10;
                    mem->angle += 8;
                    return;
                }
                /* fallthrough */
            case 3:
                goto kill;
            default:
                return;
        }
    }
kill:
    Gp_ReleaseState1CMem(mem, arg0);
}

/// A twin-ring burst. The first tick seeds a bright level and a dimmer echo
/// level. Each tick then draws the halo through
/// `func_shelter_b2_pod_access_tunnel_8017FFBC` and a ring through
/// `func_shelter_b2_pod_access_tunnel_80180894` at a growing angle, in an
/// orange tint (full red, half green, quarter blue), while the echo is drawn
/// as a widening ring through `func_shelter_b2_pod_access_tunnel_8017FB98`
/// until it has faded. The work block is released once the main level falls
/// below 0x18, or when the room's event state reaches 4.
void func_shelter_b2_pod_access_tunnel_801806E8(Task* arg0)
{
    u8         rgb[3];
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        flag;
    s16        step;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        if (arg0->state == 0) {
            mem->age    = 1;
            mem->scale  = 0xE0;
            mem->angle  = 0x80;
            mem->period = 0xE0;
            mem->step   = 0x80;
            arg0->state = 1;
        }
        Gp_UpdateCoord(coord);
        rgb[0]     = mem->scale;
        rgb[1]     = mem->scale >> 1;
        rgb[2]     = mem->scale >> 2;
        step       = mem->angle + 0x10;
        mem->angle = step;
        func_shelter_b2_pod_access_tunnel_8017FFBC(coord, (s16)(step * 2), rgb);
        func_shelter_b2_pod_access_tunnel_80180894(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            func_shelter_b2_pod_access_tunnel_8017FB98(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
            mem->period -= 0x18;
            mem->step   += 0x30;
            return;
        }
        mem->scale -= 0x18;
        if (mem->scale < 0x18) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

/// Draws a glow at the coordinate: two camera-facing textured squares, an
/// inner one of half-extent `size` and an outer one of `size * 3 / 2`
/// (each scaled by 0x37 / otz), plus a flat quad on the ground beneath it.
/// It also points the `Gp_RoomCoords[2]` light at the
/// coordinate with a randomly flickering intensity. Nothing is drawn when the
/// GTE flags the projection.
static void func_shelter_b2_pod_access_tunnel_80180894(GfxCoord* coord, s16 size)
{
    GfxCoord       ground;
    POLY_FT4*      prim;
    s16            outerLeft;
    s16            outerRight;
    s16            outerTop;
    s16            outerBottom;
    s16            intensity;
    s16            left;
    s16            right;
    s16            top;
    s16            bottom;
    s32            outerSize;
    s32            shifted;
    u32            random;
    GpCoord64*     slot;
    GpPointLight*  light;
    GpRingScratch* block;

    slot                                  = &Gp_RoomCoords[2];
    slot->framesLeft                      = 2;
    light                                 = &slot->light;
    light->inner                          = 0x300;
    light->outer                          = 0x3000;
    random                                = (Gp_LcgState * 5) + 0x71357911;
    intensity                             = ((random >> 0x10) & 0x700) + 0x800;
    light->head.r                         = intensity;
    shifted                               = intensity << 0x10;
    light->head.g                         = shifted >> 0x11;
    light->head.b                         = shifted >> 0x12;
    light->head.u.at.local.t[0]           = coord->coord.t[0];
    light->head.u.at.local.t[1]           = coord->coord.t[1];
    light->head.u.at.local.t[2]           = coord->coord.t[2];
    slot->light.head.u.coord.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_LcgState                           = random;
    block                                 = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx                         = coord->workm.t[0];
    block->vec.vy                         = coord->workm.t[1];
    block->vec.vz                         = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2EU;
        prim->tpage = 0x29;
        if (gDisplayState.animFrame & 1) {
            prim->r0   = 0xA0;
            prim->g0   = 0x80;
            prim->b0   = 0x60;
            prim->clut = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            prim->clut = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            prim->code |= 1;
        }
        block->step = size * 0x37 / block->otz;
        left        = block->sx - block->step;
        prim->x2    = left;
        prim->x0    = left;
        right       = block->sx + block->step;
        prim->x3    = right;
        prim->x1    = right;
        top         = block->sy - block->step;
        prim->y1    = top;
        prim->y0    = top;
        bottom      = block->sy + block->step;
        prim->y3    = bottom;
        prim->y2    = bottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x29;
        prim->clut =
            (((gDisplayState.animFrame & 1) * 0x10 + 0x120) >> 4) | 0x4300;
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)(size * 3 / 2);
        block->step = outerSize * 0x37 / block->otz;
        outerLeft   = block->sx - block->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = block->sx + block->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = block->sy - block->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = block->sy + block->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
        if (Gp_TraceGroundCoord(coord, &ground) == 1) {
            func_shelter_b2_pod_access_tunnel_80180DC0(&ground, outerSize);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpRingScratch);
}

/// Queues one semi-transparent textured quad lying flat at the coordinate's
/// world position: the unit quad `D_80111E38` is scaled by `arg1`, turned by
/// the view matrix and projected through `GsWSMATRIX`. Unless the GTE flags
/// the projection, the quad is coloured (0x30, 0x20, 0x20) and its texture
/// alternates between two 32-pixel columns on successive frames.
static void func_shelter_b2_pod_access_tunnel_80180DC0(GfxCoord* arg0, s32 arg1)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;
    s32            u;

    scratch  = SCRATCH_STACK_CURSOR_SLOT;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 4; i++) {
        v     = &block->vec[i];
        tbl   = &D_80111E38[i];
        prod  = tbl->x * arg1;
        v->vy = 0;
        v->vx = prod;
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        v->vx += arg0->workm.t[0];
        v->vy += arg0->workm.t[1];
        v->vz += arg0->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x30, 0x20, 0x20);
        prim->tpage = 0x28;
        prim->clut  = 0x428C;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = block->sxy0.vx;
        prim->y0    = block->sxy0.vy;
        prim->x1    = block->sxy1.vx;
        prim->y1    = block->sxy1.vy;
        prim->x2    = block->sxy2.vx;
        prim->y2    = block->sxy2.vy;
        prim->x3    = block->sxy3.vx;
        prim->y3    = block->sxy3.vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when it
/// projects, queues a star-shaped glow of gouraud `POLY_G4` wedges: a full
/// disc at half brightness, a half-radius disc at full brightness, then four
/// spikes a quarter-turn apart, alternately reaching the full radius and twice
/// it. `arg1` sizes it, scaled by 64 over depth; every wedge is black at its
/// rim.
static void func_shelter_b2_pod_access_tunnel_80181138(GfxCoord* arg0, s16 arg1, u8* arg2)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   t;
    s32                   t2;
    s32                   u;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomBillboardScratch);
}

/// A rising spark trail. Each tick turns the effect's angle on by a random
/// 0x200..0x3FF, sets its velocity to 3/16 of that direction in X and Z with
/// an upward Y that grows with age, and spawns the effect `D_80115728` at the
/// task's coordinate with it. The work block is released after 0x15 ticks, or
/// when the room's event state reaches 4.
void func_shelter_b2_pod_access_tunnel_80181AF8(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        flag;
    s16        ang;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto kill;
    } else {
        Gp_UpdateCoord(coord);
        mem->age++;
        if (mem->age >= 0x15) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
        }
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        ang          = mem->scale + ((((u32)Gp_LcgState >> 16) & 0x1FF) + 0x200);
        mem->scale   = ang;
        mem->move.vx = (u32)(rcos(ang) * 3) >> 4;
        mem->move.vy = -mem->age * 128;
        mem->move.vz = (u32)(rsin(mem->scale) * 3) >> 4;
        Gp_SpawnEff(D_80115728, coord, 0x30080201, &mem->move);
    }
}

/// A flash effect task. State 1 ramps its level up over `spawnArg1` ticks,
/// drawing two fans and an inward-shrinking ring in a colour derived from the
/// level, and queues a fade quad in that colour when it peaks; state 2 fades
/// out through the star draw before the work block is released.
void func_shelter_b2_pod_access_tunnel_80181C2C(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    u8         rgb[3];

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
    } else {
        Gp_UpdateCoord(coord);
        mem->age++;
        switch (arg0->state) {
            case 0:
                mem->scale  = 0;
                mem->angle  = 0x80;
                mem->step   = 0x100 / arg0->spawnArg1.value;
                arg0->state = 1;
                break;
            case 1:
                mem->scale += mem->step;
                mem->angle += mem->step;
                arg0->spawnArg1.value--;
                rgb[0] = mem->scale;
                rgb[1] = mem->scale >> 2;
                rgb[2] = mem->scale >> 1;
                func_shelter_b2_pod_access_tunnel_801822FC(coord, mem->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_shelter_b2_pod_access_tunnel_801822FC(coord, (s16)((u16)mem->angle * 2), rgb);
                func_shelter_b2_pod_access_tunnel_80181ED0(coord, (s16)(0x300 - (u16)mem->angle * 2), 0x80, rgb);
                if (arg0->spawnArg1.value == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    rgb[0]      = mem->scale;
                    rgb[1]      = mem->scale >> 2;
                    rgb[2]      = mem->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale;
                    rgb[1] = mem->scale >> 2;
                    rgb[2] = mem->scale >> 1;
                    func_shelter_b2_pod_access_tunnel_80183200(coord, mem->angle * 3, rgb);
                    mem->scale -= 0x10;
                    mem->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(mem, arg0);
                break;
        }
    }
}

/// The same ring as `func_shelter_b2_pod_access_tunnel_8017FB98`, built in a
/// scratch block with its fields in a different order: sixteen gouraud
/// `POLY_G4` wedges between the radii `arg1` and `arg1 + arg2`, each scaled by
/// 64 over `otz + 1`, black at `arg1` and coloured `rgb` at `arg1 + arg2`.
static void func_shelter_b2_pod_access_tunnel_80181ED0(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s16                blackRadius = arg1;
    s16                tintRadius  = arg1 + arg2;

    block         = SCRATCH_PUSH(RoomDraw02Scratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = (blackRadius * 64) / block->otz;
        block->rInner = (tintRadius * 64) / block->otz;

        ang = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomDraw02Scratch);
}

/// The same disc as `func_shelter_b2_pod_access_tunnel_8017FFBC`.
static void func_shelter_b2_pod_access_tunnel_801822FC(GfxCoord* arg0, s16 arg1, u8* rgb)
{
    RoomFanScratch* block;
    POLY_G4*        prim;
    s32             ang;
    s32             otz;

    block         = SCRATCH_PUSH(RoomFanScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        otz           = block->otz + 1;
        block->otz    = otz;
        block->radius = (arg1 * 64) / otz;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

/// A twin light trail. The first tick allocates sixteen `GfxCoord`s,
/// eight per trail, places the object's coordinate and a second one at the
/// two offsets in `D_shelter_b2_pod_access_tunnel_80183DDC`, and seeds every
/// trail slot from them in view space. Each later tick overwrites the oldest
/// slot of each trail with the current positions, refreshes all sixteen and
/// draws the pair through `func_shelter_b2_pod_access_tunnel_80182B80`. The
/// work block is released when the tick count reaches the spawn argument; the
/// task idles while the room's event state is 2 or more.
void func_shelter_b2_pod_access_tunnel_80182690(Task* task)
{
    GfxCoord   coord;
    GfxCoord*  coords;
    GfxCoord*  objCoord;
    GfxCoord*  dst;
    GpEffWork* work;
    SVECTOR*   vec;
    s32        i;

    coords   = task->work;
    work     = (GpEffWork*)task->spawnArg2.pointer;
    objCoord = task->extra.coordBody->coord;

    if (Gp_State1C->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = memCalloc(sizeof(GfxCoord[16]), 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work             = coords;
                objCoord->parent       = work->parent;
                objCoord->coord.t[0]   = D_shelter_b2_pod_access_tunnel_80183DDC[0].vx;
                objCoord->coord.t[1]   = D_shelter_b2_pod_access_tunnel_80183DDC[0].vy;
                objCoord->coord.t[2]   = D_shelter_b2_pod_access_tunnel_80183DDC[0].vz;
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                task->state        = 1;
                coord.parent       = work->parent;
                vec                = &D_shelter_b2_pod_access_tunnel_80183DDC[1];
                coord.coord.t[0]   = vec->vx;
                coord.coord.t[1]   = vec->vy;
                coord.coord.t[2]   = vec->vz;
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst         = &coords[i];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst         = &coords[i + 8];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                coord.parent = work->parent;
                {
                    SVECTOR* edge    = &D_shelter_b2_pod_access_tunnel_80183DDC[1];
                    coord.coord.t[0] = edge->vx;
                    coord.coord.t[1] = edge->vy;
                    coord.coord.t[2] = edge->vz;
                }
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                dst         = &coords[work->age & 7];
                dst->parent = &gGfxViewCoord;
                dst->workm  = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst         = &coords[(work->age & 7) + 8];
                dst->parent = &gGfxViewCoord;
                dst->workm  = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst               = &coords[i];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                    dst               = &coords[i + 8];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                }
                func_shelter_b2_pod_access_tunnel_80182B80(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the two eight-slot trails `arg0` and `arg1` as seven gouraud
/// `POLY_G4` quads, walking back from slot `arg2`; each quad joins two adjacent
/// slots of both trails. `arg3` packs three 2-bit colour multipliers at bits 8,
/// 4 and 0, applied to a brightness that falls by 9 per quad from 0x40, so the
/// trail fades towards its tail. Quads that fail to project are dropped.
static void func_shelter_b2_pod_access_tunnel_80182B80(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GfxCoord*          a;
    GfxCoord*          b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    blk = SCRATCH_PUSH(RoomDraw03Scratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = b->workm.t[0];
        blk->v[1].vy = b->workm.t[1];
        blk->v[1].vz = b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = a->workm.t[0];
        blk->v[2].vy = a->workm.t[1];
        blk->v[2].vz = a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = b->workm.t[0];
        blk->v[3].vy = b->workm.t[1];
        blk->v[3].vz = b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = gGpuPrimCursor;
            blk->otz       = blk->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_STACK_RELEASE_BLOCK(RoomDraw03Scratch);
}

/// A spark burst. The first tick spawns effect 0x60076 at the task's
/// coordinate, then either starts a stream (non-zero spawn argument: effect
/// 0x60070, then one jittered spark a tick) or a pair of expanding rings (two
/// 0x6007C effects, then two rings through
/// `func_shelter_b2_pod_access_tunnel_80181ED0` whose radius grows by 0x30 and
/// whose brightness falls by 0x20 a tick). Either way the work block is
/// released after seven ticks, or when the room's event state reaches 4.
void func_shelter_b2_pod_access_tunnel_80182F78(Task* task)
{
    GfxCoord*  objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.coordBody->coord;
    work     = (GpEffWork*)task->spawnArg2.pointer;

    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1.value != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = work->angle >> 1;
            rgb[2]       = work->angle >> 2;
            func_shelter_b2_pod_access_tunnel_80181ED0(objCoord, 0x100, 0x100, rgb);
            func_shelter_b2_pod_access_tunnel_80181ED0(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// The same star-shaped glow as `func_shelter_b2_pod_access_tunnel_80181138`.
static void func_shelter_b2_pod_access_tunnel_80183200(GfxCoord* arg0, s16 arg1, u8* arg2)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   t;
    s32                   t2;
    s32                   u;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomBillboardScratch);
}
