#include "rooms/shelter_b2_septic_tank.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
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

/// The room's message table, installed by its first task state.
extern GpMsgEntry D_shelter_b2_septic_tank_80182F4C[];
extern GpEvsCmd   D_shelter_b2_septic_tank_80183004[];
extern GpEvsCmd   D_shelter_b2_septic_tank_8018310C[];

/// Tasks the room's first task state spawns.
extern TaskDesc D_shelter_b2_septic_tank_801832C0[];

/// The room's water surfaces, as two lists drawn by separate functions.
extern RoomWaterSurface D_shelter_b2_septic_tank_801832CC[];
extern RoomWaterSurface D_shelter_b2_septic_tank_801832F0[];

extern SVECTOR D_shelter_b2_septic_tank_80183314[];
extern SVECTOR D_shelter_b2_septic_tank_80183344[];
extern SVECTOR D_shelter_b2_septic_tank_80183374[];
extern SVECTOR D_shelter_b2_septic_tank_80183514[];
extern SVECTOR D_shelter_b2_septic_tank_80183524[];
extern SVECTOR D_shelter_b2_septic_tank_80183534[];

/// The two points, relative to the effect's parent coordinate, that the trail
/// effect's two edges follow. The second is also read by its own name.

extern u8 D_shelter_b2_septic_tank_80187045;

static void func_shelter_b2_septic_tank_8017DB68(Task* task);
static void func_shelter_b2_septic_tank_8017E2DC(Task* task);
static void func_shelter_b2_septic_tank_8017EAB8(Task* arg0);
static void func_shelter_b2_septic_tank_8017EAF8(Task* task);
static void func_shelter_b2_septic_tank_8017F194(GfxCoord* arg0, s32 arg1, s32 arg2);
static void func_shelter_b2_septic_tank_8017F984(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_shelter_b2_septic_tank_8017FD70(GfxCoord* arg0, s16 arg1, s16 arg2);
static void func_shelter_b2_septic_tank_80180054(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_shelter_b2_septic_tank_8018083C(SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);
static void func_shelter_b2_septic_tank_80180E84(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_shelter_b2_septic_tank_801812B0(GfxCoord* arg0, s16 arg1, u8* rgb);
static void func_shelter_b2_septic_tank_80181B34(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3);
static void func_shelter_b2_septic_tank_801821B4(GfxCoord* arg0, s16 arg1, u8* arg2);

extern TaskDesc         D_shelter_b2_septic_tank_80182F40;
extern RoomFadeStorage  D_shelter_b2_septic_tank_80187034;
extern RoomEventMsg     D_shelter_b2_septic_tank_8018703C;
extern u8               D_shelter_b2_septic_tank_80187044;
extern RoomLatchedEvent D_shelter_b2_septic_tank_80187048;

/// Cursor into the primitive area the water surface is written to.
extern u8* D_shelter_b2_septic_tank_80187054;

void func_shelter_b2_septic_tank_8017EA50(Task*);

extern GpAnimArg  D_shelter_b2_septic_tank_80182F80;
extern GpAnimArg  D_shelter_b2_septic_tank_80182FC0;
extern GpCmdArg   D_shelter_b2_septic_tank_80182FA0;
extern GpCmdArg   D_shelter_b2_septic_tank_80182FA4;
extern GpCmdArg   D_shelter_b2_septic_tank_80182FA8;
extern GpCopyArg  D_shelter_b2_septic_tank_80182F78;
extern GpXformArg D_shelter_b2_septic_tank_80182FD4;
extern GpXformArg D_shelter_b2_septic_tank_80182FEC;
void              func_shelter_b2_septic_tank_8017D97C(s32);
void              func_shelter_b2_septic_tank_8017D9A0(void);

extern TaskDesc D_80147E48;

s32  func_shelter_b2_septic_tank_8017D7AC(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b2_septic_tank_8017D7B4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b2_septic_tank_8017D904(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b2_septic_tank_8017D90C(Task*, s32, RoomEventMsg*, GpMessageArg);
void func_shelter_b2_septic_tank_8017D614(Task*);

AnimationPackedPose D_shelter_b2_septic_tank_80182B74[6] = {
#include "assets/shelter_b2_septic_tank_animation_05958_bank1.inc"
};

AnimationPackedRotation D_shelter_b2_septic_tank_80182BBC[64] = {
#include "assets/shelter_b2_septic_tank_animation_05958_bank4.inc"
};

AnimationRecord D_shelter_b2_septic_tank_80182CBC[141] = {
#include "assets/shelter_b2_septic_tank_animation_05958_records.inc"
};

u16 D_shelter_b2_septic_tank_80182EF0[20] = {
#include "assets/shelter_b2_septic_tank_animation_05958_indices.inc"
};

GpAnimSet D_shelter_b2_septic_tank_80182F18 = {
    D_shelter_b2_septic_tank_80182CBC,
    D_shelter_b2_septic_tank_80182EF0,
    { NULL, D_shelter_b2_septic_tank_80182B74, NULL, NULL, D_shelter_b2_septic_tank_80182BBC, NULL, NULL, NULL },
};

TaskDesc D_shelter_b2_septic_tank_80182F40 = { 0, 32, func_shelter_b2_septic_tank_8017D614, { .model = NULL } };

GpMsgEntry D_shelter_b2_septic_tank_80182F4C[5] = {
    { 5102, func_shelter_b2_septic_tank_8017D7B4 },
    { 5105, func_shelter_b2_septic_tank_8017D7AC },
    { 5103, func_shelter_b2_septic_tank_8017D90C },
    { 5104, func_shelter_b2_septic_tank_8017D904 },
    { 0x7FFFFFFF, NULL },
};

GpAnimSet* D_shelter_b2_septic_tank_80182F74[1] = {
    &D_shelter_b2_septic_tank_80182F18,
};

GpCopyArg D_shelter_b2_septic_tank_80182F78 = { { .sets = D_shelter_b2_septic_tank_80182F74 }, 1 };

GpAnimArg D_shelter_b2_septic_tank_80182F80 = { { .index = 1 }, 47, 1, 8, 1 };

GpCmdArg D_shelter_b2_septic_tank_80182F94[3] = {
    { { .loc = { 4, 34 } }, 0 },
    { { .loc = { 4, 34 } }, 1 },
    { { .loc = { 4, 34 } }, 2 },
};

GpCmdArg D_shelter_b2_septic_tank_80182FA0 = { { .loc = { 4, 34 } }, 3 };

GpCmdArg D_shelter_b2_septic_tank_80182FA4 = { { .loc = { 4, 34 } }, 4 };

GpCmdArg D_shelter_b2_septic_tank_80182FA8 = { { .loc = { 4, 34 } }, 5 };

GpAnimArg D_shelter_b2_septic_tank_80182FAC = { { .index = 1 }, 1, 0, 0, 1 };

GpAnimArg D_shelter_b2_septic_tank_80182FC0 = { { .index = 1 }, 1, 1, 8, 1 };

GpXformArg D_shelter_b2_septic_tank_80182FD4 = { { 256, 0, -6802, 0 }, { 0, -1536, 0, 0 } };

GpXformArg D_shelter_b2_septic_tank_80182FEC = { 0 };

GpEvsCmd D_shelter_b2_septic_tank_80183004[11] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_shelter_b2_septic_tank_80182F78 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b2_septic_tank_80182F80 }, { .value = 0 } },
    { 41, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = -1 }, { .value = 2010 }, { .storage = &D_shelter_b2_septic_tank_80182FA0 }, { .value = 2011 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 1 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b2_septic_tank_8017D97C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_shelter_b2_septic_tank_8018310C[18] = {
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b2_septic_tank_80182FC0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b2_septic_tank_8017D9A0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1006 }, { .storage = &D_shelter_b2_septic_tank_80182FEC }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 29, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = -1 }, { .value = 2010 }, { .storage = &D_shelter_b2_septic_tank_80182FA4 }, { .value = 2011 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = -1 }, { .value = 2010 }, { .storage = &D_shelter_b2_septic_tank_80182FA8 }, { .value = 2011 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_shelter_b2_septic_tank_80182FD4 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b2_septic_tank_8017D97C }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

s16 D_shelter_b2_septic_tank_801832BC = 150;

TaskDesc D_shelter_b2_septic_tank_801832C0[1] = {
    { 0, 96, func_shelter_b2_septic_tank_8017EA50, { .model = NULL } },
};

RoomWaterSurface D_shelter_b2_septic_tank_801832CC[3] = {
    { -3500, -0x34BC, 2500, 7000, 0 },
    { 1100, -0x34BC, 2400, 7000, 0 },
    { 0, 0, 0, 0, -1 },
};

RoomWaterSurface D_shelter_b2_septic_tank_801832F0[3] = {
    { -3500, -6500, 2500, 7000, 0 },
    { 1100, -6500, 2400, 7000, 0 },
    { 0, 0, 0, 0, -1 },
};

SVECTOR D_shelter_b2_septic_tank_80183314[6] = {
    { -813, -33, -0x31FC, 0 },
    { -813, -33, -0x3022, 0 },
    { -813, -33, -0x29AC, 0 },
    { -813, -33, -0x27DA, 0 },
    { -813, -33, -8572, 0 },
    { -813, -33, -8114, 0 },
};

SVECTOR D_shelter_b2_septic_tank_80183344[6] = {
    { -813, -33, -6503, 0 },
    { -813, -33, -6051, 0 },
    { -813, -33, -4335, 0 },
    { -813, -33, -3968, 0 },
    { -813, -33, -2372, 0 },
    { -813, -33, -1929, 0 },
};

SVECTOR D_shelter_b2_septic_tank_80183374[52] = {
    { -1941, -33, -404, 0 },
    { -1941, -33, 38, 0 },
    { 813, -33, -0x31FC, 0 },
    { 813, -33, -0x3022, 0 },
    { 813, -33, -0x29AC, 0 },
    { 813, -33, -0x27DA, 0 },
    { 813, -33, -8572, 0 },
    { 813, -33, -8114, 0 },
    { 813, -33, -6503, 0 },
    { 813, -33, -6051, 0 },
    { 813, -33, -4335, 0 },
    { 813, -33, -3968, 0 },
    { 813, -33, -2372, 0 },
    { 813, -33, -1929, 0 },
    { 1962, -33, -404, 0 },
    { 1962, -33, 38, 0 },
    { -3238, -3345, -0x32B1, 0 },
    { -3238, -3345, -0x2F8D, 0 },
    { -3238, -3345, -0x2CF9, 0 },
    { -3238, -3345, -0x29D5, 0 },
    { -3238, -3345, -9590, 0 },
    { -3238, -3345, -8678, 0 },
    { -3238, -3345, -7933, 0 },
    { -3238, -3345, -7018, 0 },
    { -3238, -3345, -5910, 0 },
    { -3238, -3345, -5104, 0 },
    { -3238, -3345, -4443, 0 },
    { -3238, -3345, -3639, 0 },
    { -3238, -3345, -2527, 0 },
    { -3238, -3345, -1613, 0 },
    { -3238, -3345, -868, 0 },
    { -3238, -3345, 42, 0 },
    { 3238, -3345, -0x32B1, 0 },
    { 3238, -3345, -0x2F8D, 0 },
    { 3238, -3345, -0x2CF9, 0 },
    { 3238, -3345, -0x29D5, 0 },
    { 3238, -3345, -9590, 0 },
    { 3238, -3345, -8678, 0 },
    { 3238, -3345, -7933, 0 },
    { 3238, -3345, -7018, 0 },
    { 3238, -3345, -5910, 0 },
    { 3238, -3345, -5104, 0 },
    { 3238, -3345, -4443, 0 },
    { 3238, -3345, -3639, 0 },
    { 3238, -3345, -2527, 0 },
    { 3238, -3345, -1613, 0 },
    { 3238, -3345, -868, 0 },
    { 3238, -3345, 42, 0 },
    { -2971, -2126, -0x352D, 0 },
    { -2223, -2126, -0x352D, 0 },
    { 2153, -2126, -0x352D, 0 },
    { 2891, -2126, -0x352D, 0 },
};

SVECTOR D_shelter_b2_septic_tank_80183514[2] = {
    { -2739, -2387, 577, 0 },
    { -1999, -2387, 577, 0 },
};

SVECTOR D_shelter_b2_septic_tank_80183524[2] = {
    { 1527, -2387, 577, 0 },
    { 2265, -2387, 577, 0 },
};

SVECTOR D_shelter_b2_septic_tank_80183534[5] = {
    { -368, -2420, -105, 0 },
    { 370, -2420, -105, 0 },
    { 29, -3157, -0x34E8, 0 },
    { 407, -4428, -0x34E8, 0 },
    { -2557, -2725, -0x34E8, 0 },
};

SVECTOR D_shelter_b2_septic_tank_8018355C[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

u8* D_shelter_b2_septic_tank_8018356C[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_b2_septic_tank_80183570[1] = {
    { { .bytes = { 6, 0 } } },
};

GpWarpRec D_shelter_b2_septic_tank_80183574[2] = {
    { { .words = { 0, -89, 0, -0x3395 } }, { 0, 0, 0, 0 }, { .words = { 3840, 400, 0, -0x3106 } }, { 0, 0, 0, 0 }, 0x54220002, 0x54220001, 0, 2, 0, 0 },
    { { .words = { 2048, -89, 0, -112 } }, { 0, 0, 0, 0 }, { .words = { 2560, 800, 0, -300 } }, { 0, 0, 0, 0 }, 0x54220004, 0x54220003, 0, 5, 0, 0 },
};

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_shelter_b2_septic_tank_801835E4[8] = {
    { -1700, 1500, -6500, 0 },
    { 1700, 1500, -9000, 0 },
    { -1700, 1500, -9000, 0 },
    { -1700, 1500, -3900, 0 },
    { 1700, 1500, -6500, 0 },
    { -1700, 1500, -9000, 0 },
    { 1700, 1500, -9000, 0 },
    { 1700, 1500, -3900, 0 },
};

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_shelter_b2_septic_tank_80183624[8] = {
    { 1700, 3000, -8000, 0 },
    { -1700, 3000, -5500, 0 },
    { -1700, 3000, -0x2AF8, 0 },
    { 1700, 3000, -5500, 0 },
    { -1700, 3000, -5500, 0 },
    { 1700, 3000, -0x2AF8, 0 },
    { -1700, 3000, -8000, 0 },
    { 1700, 3000, -5500, 0 },
};

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_shelter_b2_septic_tank_80183664[8] = {
    { -1700, 2200, -7200, 0 },
    { 1700, 2200, -7200, 0 },
    { 1700, 2200, -3000, 0 },
    { -1700, 2200, -3000, 0 },
    { -1700, 2200, -7200, 0 },
    { 1700, 2200, -7200, 0 },
    { 1700, 2200, -0x2710, 0 },
    { -1700, 2200, -0x2710, 0 },
};

// Retained data: Four retained references to the preceding eight-point coordinate pools; no runtime owner found.
SVECTOR* D_shelter_b2_septic_tank_801836A4[4] = {
    D_shelter_b2_septic_tank_801835E4,
    D_shelter_b2_septic_tank_80183624,
    D_shelter_b2_septic_tank_80183664,
    D_shelter_b2_septic_tank_80183624,
};

// Retained data: Adjacent retained point data, ending with fourth halfword -1; no runtime owner found.
SVECTOR D_shelter_b2_septic_tank_801836B4[12] = {
    { 0, 0, 0, 0 },
    { -1700, 0, -3900, 0 },
    { 1700, 0, -3900, 0 },
    { -1700, 0, -6000, 0 },
    { 1700, 0, -6000, 0 },
    { -1700, 0, -8000, 0 },
    { 1700, 0, -8000, 0 },
    { -1700, 0, -0x2904, 0 },
    { 1700, 0, -0x2904, 0 },
    { -1700, 0, -0x2FA8, 0 },
    { 1700, 0, -0x2FA8, 0 },
    { 0, 0, 0, -1 },
};

SVECTOR D_shelter_b2_septic_tank_80183714[18] = {
#include "assets/shelter_b2_septic_tank_collision_0684C_normals.inc"
};

SVECTOR D_shelter_b2_septic_tank_801837A4[82] = {
#include "assets/shelter_b2_septic_tank_collision_0684C_verts.inc"
};

GpGridFace D_shelter_b2_septic_tank_80183A34[31] = {
#include "assets/shelter_b2_septic_tank_collision_0684C_faces.inc"
};

s16 D_shelter_b2_septic_tank_80183BA8[270] = {
#include "assets/shelter_b2_septic_tank_collision_0684C_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b2_septic_tank_80183BA8[i])
s16* D_shelter_b2_septic_tank_80183DC4[18] = {
#include "assets/shelter_b2_septic_tank_collision_0684C_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b2_septic_tank_80183E0C = { NULL, D_shelter_b2_septic_tank_80183714, D_shelter_b2_septic_tank_801837A4, D_shelter_b2_septic_tank_80183A34, D_shelter_b2_septic_tank_80183DC4, 5374, 0x47A9, 3, 6, 4000, 31 };

GpViewRec D_shelter_b2_septic_tank_80183E30[6] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 34, 0x510F, 6334 } }, 230 },
    { { { { -4042, 0, -661 }, { -25, 4093, 153 }, { 661, 155, -4039 } }, { 821, 1121, 8316 } }, 275 },
    { { { { -4032, 0, -719 }, { 3, 4095, -19 }, { 719, -19, -4032 } }, { 792, 1135, 3708 } }, 269 },
    { { { { 4049, 0, -613 }, { -63, 4074, -418 }, { 609, 422, 4028 } }, { 750, 1507, 9189 } }, 269 },
    { { { { 3860, 0, -1369 }, { -607, 3671, -1712 }, { 1227, 1816, 3459 } }, { 1731, 2887, 3765 } }, 257 },
    { { { { -4024, 0, 763 }, { -152, 4013, -804 }, { -748, -819, -3942 } }, { 1196, 493, 5408 } }, 297 },
};

GpSprtCmd D_shelter_b2_septic_tank_80183F08[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b2_septic_tank_80183F18[83] = {
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 112, 525, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 104, 535, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 96, 534, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 88, 574, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 80, 794, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 72, 890, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 64, 937, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 56, 1000, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 48, 1048, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 40, 1092, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 112, 674, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 104, 662, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -144, 104, 667, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 96, 678, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -144, 96, 662, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 88, 666, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -152, 88, 670, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, 32, 1335, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 40, 1314, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 40, 1247, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 48, 1264, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 48, 1217, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 56, 1039, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 56, 1108, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 80, 734, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 80, 858, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -120, 72, 799, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 72, 947, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -112, 64, 879, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 64, 1036, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 72, 1000, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 64, 1025, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 56, 1037, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 48, 1176, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 40, 1460, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 64, 1408, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 56, 1050, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -96, 48, 1125, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -96, 40, 1370, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -112, 112, 895, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -64, 112, 895, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 112, 911, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -104, 104, 936, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -64, 104, 936, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 104, 936, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -96, 96, 983, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 96, 983, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, 96, 983, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -88, 88, 1036, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -48, 88, 1036, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -16, 88, 1036, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 80, 1099, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -48, 80, 1083, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 80, 1099, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 72, 1173, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 72, 1173, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 0, 72, 1157, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 64, 1247, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -16, 64, 1247, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -48, 56, 1340, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -8, 56, 1340, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -40, 48, 1428, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 48, 1444, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -40, 40, 1564, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 40, 1386, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 8, 112, 788, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 8, 104, 818, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 8, 96, 864, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 16, 88, 936, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 16, 80, 996, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 16, 72, 1057, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 16, 64, 1141, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 24, 56, 1242, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 24, 48, 1363, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -152, 112, 783, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -144, 104, 816, { .fields = { 0, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -136, 96, 869, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -120, 88, 931, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -104, 80, 992, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -96, 72, 1056, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -80, 64, 1130, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -72, 56, 1238, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, 48, 1235, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b2_septic_tank_80184594[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 30, 0, 0, { 3, 0 } },
    { 30, 9, 0, 0, { 0, 0 } },
    { 39, 26, 0, 0, { 2, 0 } },
    { 65, 18, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b2_septic_tank_801845C4[127] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 64, 1575, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 64, 1575, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 72, 1575, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 80, 1575, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 88, 1575, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 96, 1575, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 96, 1575, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 112, 666, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 104, 709, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 96, 760, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 88, 819, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 80, 890, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 72, 979, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 64, 1098, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 56, 1200, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 48, 1392, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 40, 1632, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 32, 1964, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 24, 2363, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 24, 2528, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 32, 2382, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 32, 1928, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -16, 40, 1953, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 40, 1632, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, 48, 1589, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, 48, 1367, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 56, 1351, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 56, 1216, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, 64, 1226, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 64, 1053, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 72, 1113, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 72, 951, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -88, 80, 956, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 80, 915, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 88, 895, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -120, 88, 844, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -112, 96, 800, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 96, 801, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -120, 104, 754, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -152, 104, 759, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -136, 112, 717, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 112, 700, { .fields = { 80, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 24, 2070, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 24, 2139, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 80, 1278, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 72, 1164, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 64, 1165, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 56, 1165, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 56, 0, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 40, 0, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 48, 1352, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 48, 1240, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 40, 1224, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 40, 1352, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 32, 0, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 24, 1988, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 16, 4569, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 32, 1605, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 40, 1604, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 48, 1604, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 56, 1604, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 32, 1988, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 40, 1988, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 48, 1987, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 56, 0, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -96, 112, 917, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 0, 24, 2631, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 0, 32, 2384, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -32, 112, 917, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -80, 104, 966, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -24, 104, 966, { .fields = { 32, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -72, 96, 1023, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -16, 96, 1039, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -64, 88, 1090, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -8, 88, 1090, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, 80, 1170, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -8, 80, 1170, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -48, 72, 1267, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 72, 1267, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -40, 64, 1386, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 64, 1386, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -24, 56, 1538, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 8, 56, 1538, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, 48, 1736, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 16, 48, 1736, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -8, 40, 1991, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -128, 112, 890, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -112, 104, 927, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -8, 24, 2636, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -8, 32, 2269, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 40, 1982, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 48, 1710, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -48, 56, 1505, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 64, 1352, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -72, 72, 1231, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -80, 80, 1138, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -96, 88, 1052, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -104, 96, 991, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 64, 2472, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 32, 2175, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 40, 1987, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 48, 1696, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 56, 1489, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 64, 1352, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 72, 1231, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 80, 1140, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 88, 1036, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 96, 991, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 16, 104, 933, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 16, 112, 896, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 72, 1446, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 128, 64, 1500, { .fields = { 8, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 128, 56, 1372, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 128, 48, 1372, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 128, 40, 1372, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 128, 32, 1372, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 128, 24, 1373, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 104, 24, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 40, 1750, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 48, 1750, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 32, 1750, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 24, 1766, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 16, 1776, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 8, 1767, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 0, 4276, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 64, 0, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 56, 1940, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b2_septic_tank_80184FB0[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 6, 0 } },
    { 7, 35, 0, 0, { 1, 0 } },
    { 42, 2, 0, 0, { 4, 0 } },
    { 44, 21, 0, 0, { 0, 0 } },
    { 65, 21, 0, 0, { 5, 0 } },
    { 86, 12, 0, 0, { 3, 0 } },
    { 98, 12, 0, 0, { 7, 0 } },
    { 110, 17, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b2_septic_tank_80185000[144] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 48, 3599, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -56, 64, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 88, 3202, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 48, 1612, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, 48, 1691, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 96, 1065, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 96, 1102, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 72, 1603, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 104, 1110, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, 112, 1111, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 80, 1640, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 72, 1650, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 64, 1613, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 112, 676, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 104, 714, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 96, 756, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 88, 804, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 80, 858, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 72, 922, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 64, 1000, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 56, 1242, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 48, 1233, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 40, 1328, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 32, 1325, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 24, 1687, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 16, 1923, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 16, 2027, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -88, 8, 2385, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 24, 2014, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 72, 1302, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 64, 1285, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 56, 1250, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 48, 1246, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 40, 1243, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 24, 3770, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 8, 3764, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, 0, 4069, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 16, 1574, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 64, 0, 1985, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 16, 1994, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 24, 2000, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 24, 1594, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 32, 1567, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 32, 2118, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 88, 40, 1683, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 48, 1710, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -80, 8, 2377, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -40, 16, 2086, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -40, 8, 2406, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -80, 16, 2008, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 0, 16, 2086, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 0, 8, 2481, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -24, 112, 784, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 32, 112, 784, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 24, 104, 826, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -24, 104, 826, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 24, 96, 872, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -24, 96, 856, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -24, 88, 908, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 24, 88, 908, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 16, 80, 968, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 80, 968, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 72, 1037, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 16, 72, 1037, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 8, 64, 1117, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -32, 64, 1117, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -32, 56, 1212, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 8, 56, 1212, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 48, 1325, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 48, 1325, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 40, 1464, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -32, 40, 1464, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 32, 1636, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -32, 32, 1636, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -32, 24, 1873, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 8, 8, 4590, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -32, 8, 4772, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -88, 8, 4772, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 72, 112, 807, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 64, 104, 832, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 56, 96, 884, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 48, 88, 944, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 48, 80, 993, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 40, 72, 1063, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 32, 64, 1149, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 24, 56, 1241, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 16, 48, 1367, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 8, 40, 1513, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 8, 32, 1666, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 0, 24, 1908, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 0, 16, 2069, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -152, 64, 1488, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -152, 56, 1354, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -152, 48, 1335, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, 40, 1881, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, 32, 1748, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, 24, 1743, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, 16, 1771, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, 8, 1782, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -152, 40, 1324, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -152, 32, 1368, { .fields = { 0, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -152, 24, 1364, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, 48, 1085, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, 8, 1157, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 16, 2022, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 24, 1874, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 32, 1658, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 40, 1479, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 48, 1337, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 56, 1222, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, 64, 1128, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -48, 72, 1058, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -48, 80, 976, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -56, 88, 937, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -56, 96, 877, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -56, 104, 817, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -56, 112, 787, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 96, 112, 811, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 96, 104, 833, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 128, 112, 724, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 120, 104, 762, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 88, 96, 865, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 112, 96, 804, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 96, 88, 852, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 80, 88, 937, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 72, 80, 1004, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 88, 80, 906, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 80, 72, 970, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 56, 72, 1087, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, 64, 1153, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 72, 64, 1043, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 56, 1130, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 56, 1267, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 48, 48, 1297, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 24, 48, 1448, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 24, 40, 1566, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 40, 40, 1360, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 32, 32, 1474, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 8, 32, 1786, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 8, 24, 1991, { .fields = { 112, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 24, 24, 1691, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 8, 16, 2024, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, 0, 2461, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 24, 16, 2172, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b2_septic_tank_80185B40[11] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { 13, 16, 0, 0, { 5, 0 } },
    { 29, 17, 0, 0, { 4, 0 } },
    { 46, 6, 0, 0, { 6, 0 } },
    { 52, 26, 0, 0, { 1, 0 } },
    { 78, 13, 0, 0, { 7, 0 } },
    { 91, 13, 0, 0, { 3, 0 } },
    { 104, 13, 0, 0, { 8, 0 } },
    { 117, 27, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b2_septic_tank_80185B98[143] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 112, 810, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 136, 112, 887, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 104, 885, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, 104, 915, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 96, 952, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 136, 96, 915, { .fields = { 112, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -8, 112, 842, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -16, 104, 869, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 96, 899, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -16, 88, 898, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 80, 916, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -32, 96, 939, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, 88, 946, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 80, 980, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 88, 985, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, 80, 1026, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 112, 903, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 112, 876, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 104, 837, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 104, 849, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, 104, 914, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, 96, 870, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 96, 851, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 96, 931, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 88, 1042, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 80, 1057, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 72, 1050, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 64, 1040, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 56, 1116, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 48, 1161, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 40, 1196, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 32, 1204, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 24, 1238, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 88, 883, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 88, 878, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 88, 0, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, 72, 1076, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 72, 1016, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 64, 1046, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 64, 1105, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 56, 1086, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, 48, 1117, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, 56, 1121, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 120, 56, 1162, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 48, 1213, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 48, 1129, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, 48, 1156, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 40, 1260, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 40, 1177, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, 32, 1308, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 120, 32, 1316, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 24, 1406, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, 16, 0, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 16, 1480, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 104, 16, 1316, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, 8, 1580, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 8, 1490, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, 0, 1553, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, -8, 1525, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 24, 1325, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 120, 112, 939, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 112, 104, 961, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 104, 96, 991, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 96, 88, 1486, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 88, 80, 1062, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 72, 1088, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, 64, 1140, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 112, 932, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -8, 104, 1561, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 96, 1006, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -8, 88, 1025, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 80, 1060, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 96, 999, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 88, 1020, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, 88, 986, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, 80, 1056, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -24, 80, 1021, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 112, 928, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 112, 928, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 72, 104, 957, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 32, 104, 957, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 96, 987, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 56, 96, 987, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 56, 88, 1020, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 16, 88, 1020, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 48, 80, 1056, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 16, 80, 1056, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 72, 1062, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -16, 72, 1062, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 72, 1062, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -16, 64, 1135, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 64, 1135, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -40, 56, 1180, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -96, 40, 1275, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, 40, 1228, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -8, 40, 1234, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 32, 1302, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -64, 32, 1326, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -16, 32, 1341, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 32, 1325, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 112, 32, 1293, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -72, 24, 1312, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -24, 24, 1397, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 24, 24, 1389, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 16, 16, 1469, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -32, 16, 1428, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 8, 8, 1479, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 112, 40, 1202, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, 40, 1234, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 24, 1373, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, 24, 1389, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, 16, 1413, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, 16, 1429, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 8, 1468, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 8, 1516, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 48, 1213, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 48, 1229, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 56, 1164, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 56, 1180, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, 40, 1282, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 40, 1282, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 32, 1325, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 32, 1341, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 56, 1180, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, 56, 1200, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 8, 56, 1180, { .fields = { 0, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 56, 1180, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 24, 48, 1229, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 0, 48, 1229, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 48, 1229, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, 48, 1245, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -24, 48, 1229, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 48, 1229, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, 64, 1135, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 64, 1135, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, 64, 1119, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 64, 1135, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, 72, 1078, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 72, 1094, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 80, 1056, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, 80, 1040, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 48, 1226, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 48, 1180, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b2_septic_tank_801866C4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 60, 0, 0, { 1, 0 } },
    { 60, 83, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b2_septic_tank_801866E4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b2_septic_tank_801866F4[6] = {
    { { .empty = D_shelter_b2_septic_tank_80183F08 }, D_shelter_b2_septic_tank_80183F08, NULL },
    { { .elements = D_shelter_b2_septic_tank_80183F18 }, D_shelter_b2_septic_tank_80184594, NULL },
    { { .elements = D_shelter_b2_septic_tank_801845C4 }, D_shelter_b2_septic_tank_80184FB0, NULL },
    { { .elements = D_shelter_b2_septic_tank_80185000 }, D_shelter_b2_septic_tank_80185B40, NULL },
    { { .elements = D_shelter_b2_septic_tank_80185B98 }, D_shelter_b2_septic_tank_801866C4, NULL },
    { { .empty = D_shelter_b2_septic_tank_801866E4 }, D_shelter_b2_septic_tank_801866E4, NULL },
};

GpPointLight D_shelter_b2_septic_tank_8018673C[8] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 20, -41, -4221 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2048, 2048, 2048, { 0, 0 } }, 941, 2662 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 44, -41, -6238 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2048, 2048, 2048, { 0, 0 } }, 961, 2502 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -21, -41, -8592 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2048, 2048, 2048, { 0, 0 } }, 1121, 2522 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 31, -41, -0x28AE } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2048, 2048, 2048, { 0, 0 } }, 921, 2502 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -18, -41, -0x3112 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2048, 2048, 2048, { 0, 0 } }, 1061, 2542 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -499, -7288 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2846, 2876, 2873, { 0, 0 } }, 1501, 0x359B },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1441, -388 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3038, 1650, 1695, { 0, 0 } }, 1961, 4841 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 78, -41, -2140 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2048, 2048, 2048, { 0, 0 } }, 861, 4123 },
};

GpRoomCoordSet D_shelter_b2_septic_tank_80186A3C = { 0, NULL, 8, D_shelter_b2_septic_tank_8018673C, 0, NULL };

GpObj4C D_shelter_b2_septic_tank_80186A54[6] = {
    { NULL, NULL, NULL, { -48, -1872, -1809, 0 }, { { 2576, -3904, -176, 0 }, { -2576, -3904, 176, 0 }, { 2576, 3904, -176, 0 }, { -2576, 3904, 176, 0 } }, { 279, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 80, -1985, -1712, 0 }, { { -2400, -3904, 192, 0 }, { 2400, -3904, -192, 0 }, { -2400, 3904, 192, 0 }, { 2400, 3904, -192, 0 } }, { -328, 0, -4092, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -32, -1889, -6671, 0 }, { { 2576, -3904, 0, 0 }, { -2576, -3904, 1, 0 }, { 2576, 3904, 0, 0 }, { -2576, 3904, 1, 0 } }, { 0, 0, 4118, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 0, -1953, -6463, 0 }, { { -2400, -3904, 0, 0 }, { 2400, -3904, 0, 0 }, { -2400, 3904, 0, 0 }, { 2400, 3904, 0, 0 } }, { 0, 0, -4118, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 0, -1920, -0x2BD1, 0 }, { { 2576, -3904, 336, 0 }, { -2576, -3904, -335, 0 }, { 2576, 3904, 336, 0 }, { -2576, 3904, -335, 0 } }, { -531, 0, 4072, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 0, -1952, -0x2B40, 0 }, { { -2400, -3904, -320, 0 }, { 2400, -3904, 320, 0 }, { -2400, 3904, -320, 0 }, { 2400, 3904, 320, 0 } }, { 542, 0, -4066, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 2, 3, 129, 0 },
};

GpObj4C D_shelter_b2_septic_tank_80186C1C[4] = {
    { NULL, NULL, NULL, { 0, -48, -0x3290, 0 }, { { -1024, 0, -624, 0 }, { 1024, 0, -624, 0 }, { -1024, 0, 624, 0 }, { 1024, 0, 624, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1193, 0, 33, 22, 2, 0 },
    { NULL, NULL, NULL, { 0, -48, 192, 0 }, { { -1024, 0, -624, 0 }, { 1024, 0, -624, 0 }, { -1024, 0, 624, 0 }, { 1024, 0, 624, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 1193, 0, 35, 33, 2, 0 },
    { NULL, NULL, NULL, { 0, -64, -1793, 0 }, { { -1024, 0, -624, 0 }, { 1024, 0, -624, 0 }, { -1024, 0, 624, 0 }, { 1024, 0, 624, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1193, 0x8005, 1, 0, 3, 0 },
    { NULL, NULL, NULL, { 0, -64, -6080, 0 }, { { -1024, 0, -624, 0 }, { 1024, 0, -624, 0 }, { -1024, 0, 624, 0 }, { 1024, 0, 624, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1193, 0x8005, 2, 0, 131, 0 },
};

GpAreaTmdRec D_shelter_b2_septic_tank_80186D4C[2] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b2_septic_tank_80186D64[2] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b2_septic_tank_80186D7C[3] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 49, 49, 1, 0, { 0, 0 }, D_8015F400 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b2_septic_tank_80186DA0[2] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b2_septic_tank_80186DB8[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 4, 4, 1, 0, { 0, 0 }, D_8015FE48 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b2_septic_tank_80186DDC[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 20, 20, 1, 0, { 0, 0 }, D_8015FDF0 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaPlace D_shelter_b2_septic_tank_80186E00[3] = {
    { 4, 0, 4, -1952, 3000, -7000, 0, 0, 0, 2, 0 },
    { 4, 0, 5, 1792, 3000, -0x2E60, 0, 0, 0, 2, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b2_septic_tank_80186E30[3] = {
    { 4, 0, 1, 1700, 1500, -3900, 2500, 0, 0, 2, 0 },
    { 4, 0, 17, -1700, 3000, -5500, 2048, 0, 0, 2, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b2_septic_tank_80186E60[3] = {
    { 4, 0, 33, -1700, 2200, -0x2EE0, 0, 0, 0, 2, 4 },
    { 49, 1, 0, 300, 0, -5500, 2200, 0, 2, 4, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b2_septic_tank_80186E90[2] = {
    { 4, 0, 33, -1700, 2200, -0x2EE0, 0, 0, 0, 2, 4 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b2_septic_tank_80186EB0[5] = {
    { 21, 4, 0, -700, -2400, 300, 2048, 0, 0, 2, 5 },
    { 21, 4, 0, 700, -2400, 300, 2048, 0, 0, 2, 5 },
    { 4, 0, 7, 1700, 0, -8300, 0, 0, 2, 4, 0 },
    { 4, 0, 7, -1800, 0, -8000, 1600, 0, 2, 4, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaPlace D_shelter_b2_septic_tank_80186F00[4] = {
    { 21, 4, 0, -700, -2400, -0x3200, 0, 0, 0, 2, 5 },
    { 21, 4, 0, 700, -2400, -0x3200, 0, 0, 0, 2, 5 },
    { 20, 9, 1, 0, 0, -0x2710, 0, 0, 2, 4, 0 },
    { 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_shelter_b2_septic_tank_80186F40[22] = {
    { NULL, NULL },
    { D_shelter_b2_septic_tank_80186E00, D_shelter_b2_septic_tank_80186D4C },
    { D_shelter_b2_septic_tank_80186E30, D_shelter_b2_septic_tank_80186D64 },
    { D_shelter_b2_septic_tank_80186E60, D_shelter_b2_septic_tank_80186D7C },
    { D_shelter_b2_septic_tank_80186E90, D_shelter_b2_septic_tank_80186DA0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_septic_tank_80186EB0, D_shelter_b2_septic_tank_80186DB8 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_septic_tank_80186F00, D_shelter_b2_septic_tank_80186DDC },
};

s32 D_shelter_b2_septic_tank_80186FF0[3] = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

GpRoomParamRec D_shelter_b2_septic_tank_80186FFC[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b2_septic_tank_80187004[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec D_shelter_b2_septic_tank_8018700C[1] = {
    { 0, 0, 1, 0, D_shelter_b2_septic_tank_80186FF0 },
};

GpRoomParamRec* D_shelter_b2_septic_tank_80187014[8] = {
    D_shelter_b2_septic_tank_80186FFC,
    D_shelter_b2_septic_tank_80187004,
    D_shelter_b2_septic_tank_8018700C,
    D_shelter_b2_septic_tank_80186FFC,
    D_shelter_b2_septic_tank_80186FFC,
    D_shelter_b2_septic_tank_80186FFC,
    D_shelter_b2_septic_tank_80186FFC,
    D_shelter_b2_septic_tank_80186FFC,
};

RoomFadeStorage D_shelter_b2_septic_tank_80187034 = { 0 };

RoomEventMsg D_shelter_b2_septic_tank_8018703C = { 0 };

u8 D_shelter_b2_septic_tank_80187044 = 0;

u8 D_shelter_b2_septic_tank_80187045 = 0;

u16 D_shelter_b2_septic_tank_80187046 = 0x5868;

RoomLatchedEvent D_shelter_b2_septic_tank_80187048;

u8* D_shelter_b2_septic_tank_80187054;

static __inline__ s32 _shelterB2SepticTankStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event);
static void           func_shelter_b2_septic_tank_8017DA18(Task* arg0);
static void           func_shelter_b2_septic_tank_8017DA74(Task* task);

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->field_5` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _shelterB2SepticTankStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_b2_septic_tank_80187044 = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b2_septic_tank_8018703C = *dst;
            D_shelter_b2_septic_tank_80187048 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b2_septic_tank_80182F40, 0, 0, 0);
            D_shelter_b2_septic_tank_80187044 = 1;
        }
        return 2;
    }
    return 1;
}

/// The room's event task, spawned by `_shelterB2SepticTankStartEvent` for the
/// event it latched in `D_shelter_b2_septic_tank_80187048`. State 0 runs the
/// event's CAP command; state 1 waits for it to finish and, when the event
/// asks for it, starts helper task 0x31; states 2 and 3 play the event's stage
/// sound, if any, and wait for it to end. State 4 copies the latched message's
/// destination into the save data and spawns the room-load task 0x11.
void func_shelter_b2_septic_tank_8017D614(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b2_septic_tank_80187048.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b2_septic_tank_80187048.fade != 0) {
                    D_shelter_b2_septic_tank_80187034.fade.field_0 = 0;
                    D_shelter_b2_septic_tank_80187034.fade.field_1 = 0;
                    D_shelter_b2_septic_tank_80187034.fade.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, &D_shelter_b2_septic_tank_80187034.fade);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b2_septic_tank_80187048.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b2_septic_tank_80187048.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b2_septic_tank_80187048.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_shelter_b2_septic_tank_8018703C.prefix.packed;
            Mc_SaveData[0].state.at4.loc.warp = D_shelter_b2_septic_tank_8018703C.field_2;
            Mc_SaveData[0].state.at4.loc.room = D_shelter_b2_septic_tank_8018703C.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b2_septic_tank_8017D7AC(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_map_shelter_80179A04`. Message 0x21 starts the room's event on flag 0x131; any
/// other message answers 1.
s32 func_shelter_b2_septic_tank_8017D7B4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent event;

    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->prefix.packed != 0x21) {
        return 1;
    }
    event.capCmd   = 3;
    event.stageSnd = 0x54220001;
    event.flagId   = 0x131;
    event.fade     = 0;
    return _shelterB2SepticTankStartEvent(out, &event);
}

s32 func_shelter_b2_septic_tank_8017D904(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b2_septic_tank_8017D90C(Task* arg0, s32 arg1, RoomEventMsg* arg2, GpMessageArg arg3)
{
    u8  kind;
    s32 flag;

    kind = arg2->field_2;
    if (kind == 2) {
        flag = GameFlag_GetNibble(0xEB);
        if (flag == 1 && D_shelter_b2_septic_tank_80187045 == flag) {
            func_800E8614(D_shelter_b2_septic_tank_8018310C, 0);
            D_shelter_b2_septic_tank_80187045 = kind;
        }
    }
    return 0;
}

void func_shelter_b2_septic_tank_8017D97C(s32 arg0)
{
    GameFlag_SetNibble(0xEB, arg0);
}

void func_shelter_b2_septic_tank_8017D9A0(void)
{
    Task*     target;
    GfxCoord* player;
    GfxCoord* coords;

    target = Gp_LookupSlot4(0);
    player = gameGetPtrSlot(3)->extra.tmd->coords;
    if (target != NULL) {
        coords = target->extra.tmd->coords;
        D_shelter_b2_septic_tank_80182FEC.rot.vy =
            (ratan2(coords->coord.t[0] - player->coord.t[0], coords->coord.t[2] - player->coord.t[2]) + 0x1000) & 0xFFF;
    }
}

static void func_shelter_b2_septic_tank_8017DA18(Task* arg0)
{
    arg0->msgTable = D_shelter_b2_septic_tank_80182F4C;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_shelter_b2_septic_tank_801832C0, 0, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

static void func_shelter_b2_septic_tank_8017DA74(Task* task)
{
    s32 place;

    if (gGameSession->at4.loc.view == 4) {
        place = gGameSession->at4.loc.variant;
        if (place == 1 && Gp_StateC08.field_A != place && gDisplayState.pendingMode == DISPLAY_MODE_NONE && D_shelter_b2_septic_tank_80187045 == 0) {
            if (GameFlag_GetNibble(0xEB) == 0) {
                func_800E8614(D_shelter_b2_septic_tank_80183004, 0);
            }
            D_shelter_b2_septic_tank_80187045 = place;
        }
    }
}

/// The room task's state table, dispatched by
/// `func_shelter_b2_septic_tank_8017DB10` from a stack copy.
static const TaskFuncTable3 D_shelter_b2_septic_tank_8017D5D8 = {
    {
        func_shelter_b2_septic_tank_8017DA18,
        func_shelter_b2_septic_tank_8017DA74,
        taskKill,
    },
};

/// The room task: copies the three-state table `D_shelter_b2_septic_tank_8017D5D8`
/// onto the stack and runs the entry for the task's current state.
void func_shelter_b2_septic_tank_8017DB10(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_septic_tank_8017D5D8;
    sp.funcs[task->state](task);
}

/// Draws each surface in `D_shelter_b2_septic_tank_801832CC` exactly as
/// `func_shelter_b2_septic_tank_8017E2DC` draws its own list: two strips of 16
/// wave-lifted semi-transparent Gouraud quads per surface at height
/// `D_shelter_b2_septic_tank_801832BC`.
static void func_shelter_b2_septic_tank_8017DB68(Task* task)
{
    SVECTOR           v0, v1, v2, v3;
    long              sxy0, sxy1, sxy2, sxy3;
    long              p, flag;
    s32               phase;
    RoomWaterSurface* e;
    RoomWaterScratch* w;
    u8*               head;
    POLY_G4*          poly;
    DR_MODE*          dr;
    s32               otz;
    s32               i;

    e                          = D_shelter_b2_septic_tank_801832CC;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    head                       = SCRATCH_HEAD(u8);
    phase                      = -(gDisplayState.animFrame * 16);
    SCRATCH_HEAD(u8)           = head - 0xC;
    w                          = (RoomWaterScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    w->y = D_shelter_b2_septic_tank_801832BC;
    for (; e->count != -1; e++) {
        w->dx = e->width / 2;
        w->dz = e->depth / 16;
        w->x  = e->x;
        w->z  = e->z;
        for (i = 0; i < 16; i++) {
            v0.vx   = w->x;
            v0.vy   = w->y;
            v0.vz   = w->z + w->dz * i;
            v1.vx   = w->x;
            v1.vy   = w->y;
            v1.vz   = w->z + w->dz * (i + 1);
            w->wave = (u32)rsin(phase + (i << 9)) >> 5;
            v2.vx   = w->x + w->dx;
            v2.vy   = w->y + w->wave;
            v2.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 5;
            v3.vx   = w->x + w->dx;
            v3.vy   = w->y + w->wave;
            v3.vz   = w->z + w->dz * (i + 1);
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                              = (POLY_G4*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                PRIM_XY_WORD(poly, 0) = sxy0;
                PRIM_XY_WORD(poly, 1) = sxy1;
                PRIM_XY_WORD(poly, 2) = sxy2;
                PRIM_XY_WORD(poly, 3) = sxy3;
                setRGB0(poly, 0, 0x40, 0x80);
                setRGB1(poly, 0, 0x40, 0x80);
                setRGB2(poly, 0, 0x10, 0x20);
                setRGB3(poly, 0, 0x10, 0x20);
                addPrim(Gpu_OtEntryAtByteOffset(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                        poly);
                dr                                = (DR_MODE*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(Gpu_OtEntryAtByteOffset(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                        dr);
            }
        }
        for (i = 0; i < 16; i++) {
            w->wave = (u32)rsin(phase + (i << 9)) >> 5;
            v0.vx   = w->x + w->dx;
            v0.vy   = w->y + w->wave;
            v0.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 5;
            v1.vx   = w->x + w->dx;
            v1.vy   = w->y + w->wave;
            v1.vz   = w->z + w->dz * (i + 1);
            v2.vx   = w->x + w->dx * 2;
            v2.vy   = w->y;
            v2.vz   = w->z + w->dz * i;
            v3.vx   = w->x + w->dx * 2;
            v3.vy   = w->y;
            v3.vz   = w->z + w->dz * (i + 1);
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                              = (POLY_G4*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                PRIM_XY_WORD(poly, 0) = sxy0;
                PRIM_XY_WORD(poly, 1) = sxy1;
                PRIM_XY_WORD(poly, 2) = sxy2;
                PRIM_XY_WORD(poly, 3) = sxy3;
                setRGB0(poly, 0, 0x40, 0x80);
                setRGB1(poly, 0, 0x40, 0x80);
                setRGB2(poly, 0, 0x10, 0x20);
                setRGB3(poly, 0, 0x10, 0x20);
                addPrim(Gpu_OtEntryAtByteOffset(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                        poly);
                dr                                = (DR_MODE*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(Gpu_OtEntryAtByteOffset(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                        dr);
            }
        }
    }
    SCRATCH_POP_BYTES(0xC);
}

/// Draws each surface in `D_shelter_b2_septic_tank_801832F0` at height
/// `D_shelter_b2_septic_tank_801832BC` as two strips of 16 semi-transparent
/// Gouraud quads laid side by side along X, each strip running along Z and
/// projected through the view matrix. The seam between the strips is lifted by
/// a sine wave that runs along Z and scrolls with the display frame counter.
/// In both strips the quad's lower-X edge is coloured (0, 0x40, 0x80) and its
/// upper-X edge (0, 0x10, 0x20); each quad is followed by a draw-mode packet
/// selecting blend mode 2. Quads the projection flags as invalid are skipped.
/// The per-surface values live in a work block pushed on the scratchpad stack
/// for the duration of the call.
static void func_shelter_b2_septic_tank_8017E2DC(Task* task)
{
    SVECTOR           v0, v1, v2, v3;
    long              sxy0, sxy1, sxy2, sxy3;
    long              p, flag;
    s32               phase;
    RoomWaterSurface* e;
    RoomWaterScratch* w;
    u8*               head;
    POLY_G4*          poly;
    DR_MODE*          dr;
    s32               otz;
    s32               i;

    e                          = D_shelter_b2_septic_tank_801832F0;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    head                       = SCRATCH_HEAD(u8);
    phase                      = -(gDisplayState.animFrame * 16);
    SCRATCH_HEAD(u8)           = head - 0xC;
    w                          = (RoomWaterScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    w->y = D_shelter_b2_septic_tank_801832BC;
    for (; e->count != -1; e++) {
        w->dx = e->width / 2;
        w->dz = e->depth / 16;
        w->x  = e->x;
        w->z  = e->z;
        for (i = 0; i < 16; i++) {
            v0.vx   = w->x;
            v0.vy   = w->y;
            v0.vz   = w->z + w->dz * i;
            v1.vx   = w->x;
            v1.vy   = w->y;
            v1.vz   = w->z + w->dz * (i + 1);
            w->wave = (u32)rsin(phase + (i << 9)) >> 5;
            v2.vx   = w->x + w->dx;
            v2.vy   = w->y + w->wave;
            v2.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 5;
            v3.vx   = w->x + w->dx;
            v3.vy   = w->y + w->wave;
            v3.vz   = w->z + w->dz * (i + 1);
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                              = (POLY_G4*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                PRIM_XY_WORD(poly, 0) = sxy0;
                PRIM_XY_WORD(poly, 1) = sxy1;
                PRIM_XY_WORD(poly, 2) = sxy2;
                PRIM_XY_WORD(poly, 3) = sxy3;
                setRGB0(poly, 0, 0x40, 0x80);
                setRGB1(poly, 0, 0x40, 0x80);
                setRGB2(poly, 0, 0x10, 0x20);
                setRGB3(poly, 0, 0x10, 0x20);
                addPrim(Gpu_OtEntryAtByteOffset(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                        poly);
                dr                                = (DR_MODE*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(Gpu_OtEntryAtByteOffset(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                        dr);
            }
        }
        for (i = 0; i < 16; i++) {
            w->wave = (u32)rsin(phase + (i << 9)) >> 5;
            v0.vx   = w->x + w->dx;
            v0.vy   = w->y + w->wave;
            v0.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 5;
            v1.vx   = w->x + w->dx;
            v1.vy   = w->y + w->wave;
            v1.vz   = w->z + w->dz * (i + 1);
            v2.vx   = w->x + w->dx * 2;
            v2.vy   = w->y;
            v2.vz   = w->z + w->dz * i;
            v3.vx   = w->x + w->dx * 2;
            v3.vy   = w->y;
            v3.vz   = w->z + w->dz * (i + 1);
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                              = (POLY_G4*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                PRIM_XY_WORD(poly, 0) = sxy0;
                PRIM_XY_WORD(poly, 1) = sxy1;
                PRIM_XY_WORD(poly, 2) = sxy2;
                PRIM_XY_WORD(poly, 3) = sxy3;
                setRGB0(poly, 0, 0x40, 0x80);
                setRGB1(poly, 0, 0x40, 0x80);
                setRGB2(poly, 0, 0x10, 0x20);
                setRGB3(poly, 0, 0x10, 0x20);
                addPrim(Gpu_OtEntryAtByteOffset(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                        poly);
                dr                                = (DR_MODE*)D_shelter_b2_septic_tank_80187054;
                D_shelter_b2_septic_tank_80187054 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(Gpu_OtEntryAtByteOffset(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                        dr);
            }
        }
    }
    SCRATCH_POP_BYTES(0xC);
}

/// The water task: runs its current state - `func_shelter_b2_septic_tank_8017EAB8`
/// once, then `func_shelter_b2_septic_tank_8017EAF8`, which draws the surfaces -
/// and each tick publishes the room's water height to the session.
void func_shelter_b2_septic_tank_8017EA50(Task* task)
{
    TaskFunc states[2] = { func_shelter_b2_septic_tank_8017EAB8, func_shelter_b2_septic_tank_8017EAF8 };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b2_septic_tank_801832BC;
}

/// Clears the session's `field_80` or `field_7E`, chosen by `Mc_SaveData[0].state.companionType`, and
/// advances the task to its next state.
static void func_shelter_b2_septic_tank_8017EAB8(Task* arg0)
{
    if (Mc_SaveData[0].state.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// The water task's drawing state: points the primitive cursor
/// `D_shelter_b2_septic_tank_80187054` at the current buffer's 0xC000-byte
/// slice of one of two primitive areas, chosen by `Mc_SaveData[0].state.companionType`, then draws both
/// lists of water surfaces.
static void func_shelter_b2_septic_tank_8017EAF8(Task* task)
{
    if (Mc_SaveData[0].state.companionType == 0) {
        D_shelter_b2_septic_tank_80187054 = (u8*)Fs_ActorLoadBase2 + gDisplayState.otBuffer * 0xC000;
    } else {
        D_shelter_b2_septic_tank_80187054 = (u8*)Fs_ActorLoadBase1 + gDisplayState.otBuffer * 0xC000;
    }
    func_shelter_b2_septic_tank_8017DB68(task);
    func_shelter_b2_septic_tank_8017E2DC(task);
}

void func_shelter_b2_septic_tank_8017EB7C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_80115758  = 0x601D4;
            D_8011572C  = 0x601F0;
            D_80115750  = 0x6020C;
            D_8011574C  = 0x6016C;
            D_80115738  = 0x6016D;
            arg0->state = 1;
        case 1:
            switch (Gp_GetViewIndex() & 0xFF) {
                case 2: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183314;
                    func_shelter_b2_septic_tank_80180054(&p[0], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[2], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[14], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[16], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[60], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[62], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_8018083C(&p[70], 0x300, 0x10);
                    func_shelter_b2_septic_tank_8018083C(&p[72], 0x300, 0x100);
                    break;
                }
                case 3: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183314;
                    func_shelter_b2_septic_tank_80180054(&p[0], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[2], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[4], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[6], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[14], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[16], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[18], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[20], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[28], 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[30], 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[44], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[46], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[48], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[60], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[62], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_8018083C(&p[70], 0x300, 0x10);
                    func_shelter_b2_septic_tank_8018083C(&p[71], 0x300, 0x100);
                    func_shelter_b2_septic_tank_8018083C(&p[72], 0x300, 0x100);
                    break;
                }
                case 4: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183344;
                    func_shelter_b2_septic_tank_80180054(&p[0], 0x200, -0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[2], 0x200, -0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[4], 0x200, -0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[6], 0x200, -0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[14], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[16], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[18], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[20], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[34], 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[36], 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[48], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[50], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[52], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(D_shelter_b2_septic_tank_80183514, 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(D_shelter_b2_septic_tank_80183524, 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(D_shelter_b2_septic_tank_80183534, 0x200, 0x800, 0x100);
                    break;
                }
                case 5: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183374;
                    func_shelter_b2_septic_tank_80180054(&p[0], 0x200, -0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[14], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(D_shelter_b2_septic_tank_80183514, 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(D_shelter_b2_septic_tank_80183524, 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(D_shelter_b2_septic_tank_80183534, 0x200, 0x800, 0x100);
                    break;
                }
                case 6: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183314;
                    func_shelter_b2_septic_tank_80180054(&p[0], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[2], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[4], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[14], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[28], 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[30], 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[60], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[62], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_8018083C(&p[70], 0x300, 0x10);
                    func_shelter_b2_septic_tank_8018083C(&p[71], 0x300, 0x100);
                    func_shelter_b2_septic_tank_8018083C(&p[72], 0x300, 0x100);
                    break;
                }
            }
            break;
    }
}

/// Per-frame driver of an expanding, fading flash. While the room's event
/// state is 0 it updates the task's coordinate, ticks the age counter and
/// draws the flash through `func_shelter_b2_septic_tank_8017F194` at size
/// `angle`, seeded from the spawn argument and grown by 0x20 a frame, and
/// brightness `scale`, which starts at 0x40 and drops by 2 a frame; the
/// first frame also turns the coordinate about Y by a random angle. The work
/// block is released once the brightness falls under 2. Once the event state
/// is non-zero it only draws, releasing the block from event state 4 on.
void func_shelter_b2_septic_tank_8017F040(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_b2_septic_tank_8017F194(coord, work->angle, work->scale);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        if (task->state == 0) {
            work->scale = 0x40;
            work->angle = task->spawnArg1.halves.low & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 1);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state         = 1;
        }
        work->angle += 0x20;
        func_shelter_b2_septic_tank_8017F194(coord, work->angle, work->scale);
        work->scale -= 2;
        if (work->scale < 2) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a flat textured quad at `arg0`: the four corners of the unit quad
/// `D_80111E38`, scaled by `arg1`, are rotated by the coordinate's world
/// matrix and offset by its translation, then projected through `GsWSMATRIX`.
/// If the projection is valid, one semi-transparent `POLY_FT4` (tpage 0x2B,
/// clut 0x43D1, UV 0,0x38 to 0x37,0x6F) is queued with all three colour
/// channels set to `arg2`. The work block lives on the scratchpad stack.
static void func_shelter_b2_septic_tank_8017F194(GfxCoord* arg0, s32 arg1, s32 arg2)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 4; i++) {
        tbl   = &D_80111E38[i];
        v     = &block->vec[i];
        prod  = tbl->x * arg1;
        v->vy = 0;
        v->vx = prod;
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&arg0->workm);
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
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D1;
        prim->v0    = 0x38;
        prim->v1    = 0x38;
        setRGB0(prim, arg2, arg2, arg2);
        prim->u0 = 0;
        prim->u1 = 0x37;
        prim->u2 = 0;
        prim->v2 = 0x6F;
        prim->u3 = 0x37;
        prim->v3 = 0x6F;
        setSemiTrans(prim, 1);
        prim->x0 = block->sxy0.vx;
        prim->y0 = block->sxy0.vy;
        prim->x1 = block->sxy1.vx;
        prim->y1 = block->sxy1.vy;
        prim->x2 = block->sxy2.vx;
        prim->y2 = block->sxy2.vy;
        prim->x3 = block->sxy3.vx;
        prim->y3 = block->sxy3.vy;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}

/// Per-frame driver of a particle, drawn as the spinning sprite of
/// `func_shelter_b2_septic_tank_8017F984` or, when the spawn argument's top
/// nibble is set, the upright sprite of `func_shelter_b2_septic_tank_8017FD70`.
/// The first frame takes the size from the argument's low 12 bits, a random
/// angle, and the ticks per animation frame from bits 12-15. Unless the work
/// block already carries a velocity it picks one by the kind in bits 24-27 -
/// none, a random upward burst, a random spray, a narrow upward jet, or the
/// work block's stored direction - scaled to the speed in bits 16-23 (0x40
/// when zero). Every later tick draws, moves the coordinate by the velocity
/// with gravity pulling it down, and releases the block after animation frame
/// 7. While the room's event state is non-zero it only draws, releasing the
/// block from event state 4 on.
void func_shelter_b2_septic_tank_8017F4C8(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    SVECTOR*   vec;
    s32        kind;
    s32        step;
    s32        state;
    s32        level;

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            if (task->state < 2) {
                func_shelter_b2_septic_tank_8017F984(coord, work->index, work->scale, work->angle);
            } else {
                func_shelter_b2_septic_tank_8017FD70(coord, work->index, work->scale);
            }
            return;
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    Gp_UpdateCoord(coord);
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = task->spawnArg1.halves.low & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1.value & 0xF000) {
                step = (task->spawnArg1.value >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            state        = 1;
            if (task->spawnArg1.value & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1.value & 0xFF0000) {
                    level = (task->spawnArg1.value >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                kind       = task->spawnArg1.signedBytes[3];
                switch (kind & 0xF) {
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
            return;
        case 1:
            func_shelter_b2_septic_tank_8017F984(coord, work->index, work->scale, work->angle);
            break;
        case 2:
            func_shelter_b2_septic_tank_8017FD70(coord, work->index, work->scale);
            break;
        default:
            return;
    }
    if (work->step != 0) {
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->move.vy      += 6;
    }
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a spinning sprite at the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, one
/// semi-transparent, unshaded `POLY_FT4` (tpage 0x2B, clut 0x43D3) is queued
/// as a square rotated by angle `arg3` about the projected point, with
/// on-screen half-diagonal `(s16)arg2 * 31 / otz`. `arg1` picks the 32-texel
/// frame at u = `arg1 * 32`, v 0xE0 to 0xFF.
static void func_shelter_b2_septic_tank_8017F984(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              ang;
    s32              ang2;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)arg0->workm.t[1];
    vz                                        = (u16)arg0->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ang            = (s16)arg3;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D3;
        u0          = (s16)arg1 << 5;
        setUV4(prim, u0, 0xE0, u0 + 0x1F, 0xE0, u0, 0xFF, u0 + 0x1F, 0xFF);
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = ang + 0x400;
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x1C);
}

/// Draws an upright sprite at the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, one
/// semi-transparent, unshaded `POLY_FT4` (tpage 0x2B, clut 0x43D2) is queued
/// as an axis-aligned square of half-side `r = arg2 * 55 / otz`, raised so the
/// projected point sits three quarters of the way down it. `arg1` picks one of
/// eight 56-texel frames in a grid four wide, starting at v 0x70.
static void func_shelter_b2_septic_tank_8017FD70(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    GpRingScratch* block;
    POLY_FT4*      prim;

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
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D2;
        setUVWH(prim, (arg1 % 4) * 0x38, (arg1 % 8) / 4 * 0x38 + 0x70, 0x37, 0x37);
        block->step = (arg2 * 0x37) / block->otz;
        prim->x0 = prim->x2 = block->sx - block->step;
        prim->x1 = prim->x3 = block->sx + block->step;
        prim->y0 = prim->y1 = block->sy - block->step - (block->step >> 1);
        prim->y2 = prim->y3 = block->sy + (block->step >> 1);
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP(GpRingScratch);
}

/// Draws a flickering light beam from `arg0[0]` to `arg0[1]`. Both points are
/// projected through the view matrix; unless the far end is nearer than OTZ
/// 0x11, gouraud `POLY_G4` wedges around each end (radius `(s16)arg1 * 64 /
/// otz` at that end) are joined by quads between the two, each fading from the
/// beam colour on the axis to black at the rim. `arg2` turns the wedges about
/// the axis. `arg3` packs the colour: the red factor in bits 8-15 and the
/// green and blue factors in bits 4 and 0, each multiplying an intensity that
/// alternates between 0x20 and 0x28 with the display frame counter.
static void func_shelter_b2_septic_tank_80180054(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3)
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
    s32                packed;
    s32                extent;
    s32                r0;
    s32                r1;
    s32                base;
    u8                 blend;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch  = (void**)G_SCRATCH_HEAD;
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
        packed    = arg3 << 16;
        blend     = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        r         = blend * (packed >> 24);
        g         = blend * ((packed >> 20) & 1);
        base      = (s16)arg2;
        b         = blend * (arg3 & 1);
        ang       = 0;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            p        = prim;
            p->r2    = r;
            p->g2    = g;
            prim->b2 = b;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, r, g, b);
            prim->x0 = block->sx0 + ((block->r0 * rsin(base + (ang * 2))) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(base + (ang * 2))) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(base + (ang * 2))) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base + (ang * 2))) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            t3             = ang - 0x1000;
            prim           = (POLY_G4*)gGpuPrimCursor;
            t              = ang - 0x1000;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Draws a flickering glow at the world-space point `worldPoint`. The point is
/// projected through the view matrix; unless it is nearer than OTZ 0x11, four
/// gouraud `POLY_G4` wedges of on-screen radius `(s16)radiusScale * 64 / otz` are
/// queued around it, coloured at the centre and black at the rim. `packedColor` packs
/// the colour: the red factor in bits 8-15 and the green and blue factors in
/// bits 4 and 0, each multiplying an intensity that alternates between 0x20
/// and 0x28 with the display frame counter.
///
/// `worldPoint` uses world coordinates; `radiusScale` is narrowed to signed 16 bits
/// before division by camera depth/4. Angles use 4096 units per turn and the
/// trigonometric coordinates use a 12-bit fractional scale. Colour bytes wrap.
static void func_shelter_b2_septic_tank_8018083C(SVECTOR* worldPoint, s32 radiusScale, s32 packedColor)
{
    enum {
        ROOM_VISUAL_EFFECTS_GLOW_MIN_DEPTH       = 17,
        ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_BASE = 0x20,
        ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_STEP = 8,
        ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT      = 12,
        ROOM_VISUAL_EFFECTS_GLOW_FULL_TURN       = 0x1000,
    };

    u8*                head;
    RoomDraw25Scratch* block;
    POLY_G4*           prim;
    DisplayState*      displayBase;
    DisplayState*      ds;
    s32                radius;
    s32                angle;
    s32                halfStepAngle;
    s32                nextAngle;
    s32                shiftedColor;
    u8                 brightness;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - sizeof(*block));
        block   = (RoomDraw25Scratch*)tmp;
    }

    // Project the world point before allocating its glow packets.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&((RoomDraw25Scratch*)(head - sizeof(*block)))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw25Scratch*)(head - sizeof(*block)))->otz >= ROOM_VISUAL_EFFECTS_GLOW_MIN_DEPTH) {
        radius        = ((s16)radiusScale * 64) / ((RoomDraw25Scratch*)(head - sizeof(*block)))->otz;
        displayBase   = &gDisplayState;
        shiftedColor  = packedColor << 16;
        brightness    = (((u8)displayBase->animFrame & 1) * ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_STEP) | ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_BASE;
        r             = brightness * (shiftedColor >> 24);
        g             = brightness * ((shiftedColor >> 20) & 1);
        b             = brightness * (packedColor & 1);
        angle         = 0;
        ds            = displayBase;
        block->radius = radius;
        // Build four glow wedges and quantize their shared camera depth.
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0      = block->sx + ((block->radius * rsin(angle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            halfStepAngle = angle + 0x200;
            prim->y0      = block->sy + ((block->radius * rcos(angle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            prim->x1      = block->sx + ((block->radius * rsin(halfStepAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            prim->y1      = block->sy + ((block->radius * rcos(halfStepAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            nextAngle     = angle + 0x400;
            prim->x2      = block->sx;
            prim->y2      = block->sy;
            prim->x3      = block->sx + ((block->radius * rsin(nextAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            prim->y3      = block->sy + ((block->radius * rcos(nextAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            angle         = nextAngle;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (angle < ROOM_VISUAL_EFFECTS_GLOW_FULL_TURN);
    }
    SCRATCH_POP_BYTES(sizeof(*block));
}

/// Per-frame driver of a burst of light in red, half blue and quarter green.
/// Over the number of frames given by the spawn argument the burst's
/// brightness `scale` and size `angle` grow together, drawn as a glow at that
/// size, a dimmer glow at twice it and a ring closing in around them. At the
/// peak the screen is flashed in the burst's colour, and the burst then fades
/// out through a larger billboard glow, shrinking by 8 and dimming by 0x10 a
/// frame until its brightness drops to 0x10. The work block is then released,
/// as it is once the room's event state reaches 4; from event state 1 on the
/// burst is no longer advanced or drawn.
void func_shelter_b2_septic_tank_80180BE0(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    u8         rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1.value;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1.value--;
                rgb[0] = work->scale;
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale >> 1;
                func_shelter_b2_septic_tank_801812B0(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_shelter_b2_septic_tank_801812B0(coord, (s16)((u16)work->angle * 2), rgb);
                func_shelter_b2_septic_tank_80180E84(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = work->scale >> 2;
                    rgb[2]      = work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale >> 1;
                    func_shelter_b2_septic_tank_801821B4(coord, (s16)(work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}

/// Draws a ring around the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, sixteen gouraud
/// `POLY_G4` segments are queued between on-screen radii `(s16)arg1 * 64 /
/// (otz + 1)` and `(s16)(arg1 + arg2) * 64 / (otz + 1)`, black at the first
/// and coloured `rgb` at the second.
static void func_shelter_b2_septic_tank_80180E84(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
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
            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomDraw02Scratch);
}

/// Draws a round glow at the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, eight gouraud
/// `POLY_G4` wedges of on-screen radius `arg1 * 64 / (otz + 1)` are
/// queued around the projected point, coloured `rgb` at the centre and black
/// at the rim.
static void func_shelter_b2_septic_tank_801812B0(GfxCoord* arg0, s16 arg1, u8* rgb)
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
            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Per-frame driver of a ribbon trail swept by two points of a moving parent.
/// The first frame allocates two eight-slot histories of coordinates, places
/// the task's own coordinate at the first point under the parent and fills
/// every slot with the two points' current positions. Each later frame records
/// the two positions in the slot the age counter selects and draws the ribbon
/// through `func_shelter_b2_septic_tank_80181B34`. The work block is
/// released once the age reaches the spawn argument; from the room's event
/// state 2 on the effect is frozen.
void func_shelter_b2_septic_tank_80181644(Task* task)
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
    objCoord = task->extra.tmd->coords;

    if (Gp_State1C->eventState < 2) {
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
                objCoord->coord.t[0]   = D_shelter_b2_septic_tank_8018355C[0].vx;
                objCoord->coord.t[1]   = D_shelter_b2_septic_tank_8018355C[0].vy;
                objCoord->coord.t[2]   = D_shelter_b2_septic_tank_8018355C[0].vz;
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                task->state        = 1;
                coord.parent       = work->parent;
                vec                = &D_shelter_b2_septic_tank_8018355C[1];
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
                    SVECTOR* edge    = &D_shelter_b2_septic_tank_8018355C[1];
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
                func_shelter_b2_septic_tank_80181B34(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the ribbon between two eight-slot coordinate histories `arg0` and
/// `arg1` as seven gouraud `POLY_G4` quads, walking back from the newest slot
/// `arg2`; each quad joins the positions of two consecutive slots in both
/// histories and is skipped when its projection is invalid. The ribbon fades
/// from intensity 0x40 at the newest slot by 9 per slot. `arg3` packs the
/// colour: the red factor in bits 8 up and the green and blue factors in bits
/// 4-5 and 0-1, each multiplying that intensity.
static void func_shelter_b2_septic_tank_80181B34(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3)
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
        blk->v[0].vx = (u16)a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = (u16)a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = (u16)a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = (u16)b->workm.t[0];
        blk->v[1].vy = (u16)b->workm.t[1];
        blk->v[1].vz = (u16)b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = (u16)a->workm.t[0];
        blk->v[2].vy = (u16)a->workm.t[1];
        blk->v[2].vz = (u16)a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = (u16)b->workm.t[0];
        blk->v[3].vy = (u16)b->workm.t[1];
        blk->v[3].vz = (u16)b->workm.t[2];
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
            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_POP(RoomDraw03Scratch);
}

/// Per-frame driver of an explosion at the task's coordinate. The first frame
/// spawns effect 0x60076 and then either, with a non-zero spawn argument,
/// effect 0x60070 and a spray of further 0x60070 sparks thrown at random
/// velocities over the next frames, or two 0x6007C effects and two expanding
/// rings drawn through `func_shelter_b2_septic_tank_80180E84` in a fading
/// orange. The work block is released once the age reaches 7, or when the
/// room's event state reaches 4; from event state 1 on nothing is advanced or
/// drawn.
void func_shelter_b2_septic_tank_80181F2C(Task* task)
{
    GfxCoord*  objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.tmd->coords;
    work     = (GpEffWork*)task->spawnArg2.pointer;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
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
            func_shelter_b2_septic_tank_80180E84(objCoord, 0x100, 0x100, rgb);
            func_shelter_b2_septic_tank_80180E84(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Draws a star-shaped flare at the coordinate's world position. The position
/// is projected through `GsWSMATRIX`; if the projection is valid, gouraud
/// `POLY_G4` wedges are queued around the projected point: a glow of on-screen
/// radius `r = arg1 * 64 / (otz + 1)` at half the colour `arg2`, a glow of
/// radius `r / 2` at the full colour, and four spikes at half the colour,
/// reaching alternately to `r` and `2 * r`. Every wedge fades from its colour
/// at the centre to black.
static void func_shelter_b2_septic_tank_801821B4(GfxCoord* arg0, s16 arg1, u8* arg2)
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
            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomBillboardScratch);
}
