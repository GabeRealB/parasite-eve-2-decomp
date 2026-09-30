#include "rooms/shelter_b4_upper_sewer.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

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
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
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

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "rooms/rooms_shared_8017dcb8.h"

#define D_shelter_b4_upper_sewer_80186520 (D_shelter_b4_upper_sewer_801864F0 + 6)

/// One water surface: a rectangle at (`x`, `z`) spanning `width` along X and
/// `depth` along Z, cut into `count` flat quads. The quads are laid along X
/// when `alongZ` is zero and along Z otherwise. A list of them ends at an entry
/// whose `count` is -1.
typedef struct ShelterB4UpperSewerSurface {
    s16 x;
    s16 z;
    s16 width;
    s16 depth;
    s16 count;
    s16 alongZ;
} ShelterB4UpperSewerSurface;

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_shelter_b4_upper_sewer_80188D2C[4];

extern GpEvsCmd   D_shelter_b4_upper_sewer_80186318[];
extern TaskDesc   D_shelter_b4_upper_sewer_80186300[];
extern GpMsgEntry D_shelter_b4_upper_sewer_801862D0[];

extern TaskDesc D_shelter_b4_upper_sewer_8018643C[];
/// Save location filled from the outgoing location just before a table task is
/// spawned: `field_2` / `field_4` / `field_1` take its `field_0` / `field_2` /
/// `field_3`.
extern RoomEventMsg D_shelter_b4_upper_sewer_80188D24;
/// Spawn argument for the task `func_shelter_b4_upper_sewer_8017D80C` starts
/// with `Task_Spawn(1, 0x31, ...)`.
extern RoomFadeStorage D_shelter_b4_upper_sewer_80188D1C;

extern ShelterB4UpperSewerSurface D_shelter_b4_upper_sewer_80186448[];
extern ShelterB4UpperSewerSurface D_shelter_b4_upper_sewer_80186454[];
extern u8*                        D_shelter_b4_upper_sewer_80188D30;

extern SVECTOR D_shelter_b4_upper_sewer_80186490[];
extern SVECTOR D_shelter_b4_upper_sewer_801864B0[];
extern SVECTOR D_shelter_b4_upper_sewer_801864D0[];

/// Colour shifts per spawn variant: each channel is the fade level shifted
/// right by the entry's value.
static RoomHaloShade RoomFx_HaloShades[];

/// The two points the trail is emitted from, relative to the effect's parent:
/// the first positions the effect's own coordinate, the second is the other
/// end of the trail.
/// The second of those points, which the per-frame state reaches through its
/// own label rather than by indexing the pair.

/// Per-variant right shifts applied to the red, green and blue channels of a
/// disc's brightness.
static RoomHaloShade RoomFx_DiscShades[];

static void func_shelter_b4_upper_sewer_8017DBA8(Task* task);
static void func_shelter_b4_upper_sewer_8017DC28(Task* task);
static void func_shelter_b4_upper_sewer_8017DD98(Task* task, ShelterB4UpperSewerSurface* e, s16 y, u8 c);
static void func_shelter_b4_upper_sewer_8017E55C(Task* arg0);
static void func_shelter_b4_upper_sewer_8017E59C(s32 arg0);
static void func_shelter_b4_upper_sewer_8017EA0C(GfxCoord* arg0, s32 arg1, s32 arg2);
static void func_shelter_b4_upper_sewer_8017F1FC(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_shelter_b4_upper_sewer_8017F5E8(GfxCoord* arg0, s16 arg1, s16 arg2);
static void func_shelter_b4_upper_sewer_8017F8CC(SVECTOR* arg0, s32 arg1, s32 arg2);
#include "../../shared/room_visual_effects.h"

/// State handlers of the task `func_shelter_b4_upper_sewer_8017DC30` runs,
/// which copies the table to the stack and calls the entry for the task's
/// state: the room's setup (message table, pointer slot, water level), an idle
/// state, and `taskKill`.
static const TaskFuncTable3 D_shelter_b4_upper_sewer_8017D5C4 = {
    { func_shelter_b4_upper_sewer_8017DBA8, func_shelter_b4_upper_sewer_8017DC28, taskKill }
};

void func_shelter_b4_upper_sewer_8017E4F4(Task*);

s32  func_shelter_b4_upper_sewer_8017D9BC(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_shelter_b4_upper_sewer_8017D9C4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b4_upper_sewer_8017DAB0(Task*, s32, s32, TaskMessageArg);
s32  func_shelter_b4_upper_sewer_8017DB50(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_shelter_b4_upper_sewer_8017DB58(Task*, s32, s32, TaskMessageArg);
void func_shelter_b4_upper_sewer_8017D660(Task*);
void func_shelter_b4_upper_sewer_8017D80C(Task*);
void func_shelter_b4_upper_sewer_8017DB94(void);

extern TaskDesc D_80147E48;

GpMsgEntry D_shelter_b4_upper_sewer_801862D0[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b4_upper_sewer_8017D9C4 },
    { 5105, func_shelter_b4_upper_sewer_8017D9BC },
    { 5103, func_shelter_b4_upper_sewer_8017DB50 },
    { 5104, func_shelter_b4_upper_sewer_8017DAB0 },
    { 5106, func_shelter_b4_upper_sewer_8017DB58 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_shelter_b4_upper_sewer_80186300[2] = {
    { 0, 32, func_shelter_b4_upper_sewer_8017D660, { .model = NULL } },
    { 0, 32, func_shelter_b4_upper_sewer_8017D80C, { .model = NULL } },
};

GpEvsCmd D_shelter_b4_upper_sewer_80186318[12] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x542C0005 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b4_upper_sewer_8017DB94 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 10 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

s16 D_shelter_b4_upper_sewer_80186438 = -1600;

TaskDesc D_shelter_b4_upper_sewer_8018643C[1] = {
    { 0, 96, func_shelter_b4_upper_sewer_8017E4F4, { .model = NULL } },
};

ShelterB4UpperSewerSurface D_shelter_b4_upper_sewer_80186448[1] = {
    { -9700, 5600, 1720, 1800, 16, 0 },
};

ShelterB4UpperSewerSurface D_shelter_b4_upper_sewer_80186454[5] = {
    { -8000, 5000, 7120, 2900, 32, 0 },
    { -900, 3100, 2800, 4800, 32, 1 },
    { -900, -5900, 1800, 9000, 32, 1 },
    { 900, -5900, 3000, 3800, 32, 1 },
    { 0, 0, 0, 0, -1, 0 },
};

SVECTOR D_shelter_b4_upper_sewer_80186490[4] = {
    { -7580, -5500, 9840, 0 },
    { -6420, -5500, 9840, 0 },
    { -3580, -5500, 9840, 0 },
    { -2420, -5500, 9840, 0 },
};

SVECTOR D_shelter_b4_upper_sewer_801864B0[4] = {
    { 420, -5500, 9840, 0 },
    { 1580, -5500, 9840, 0 },
    { 3600, -5500, 7080, 0 },
    { 3600, -5500, 5920, 0 },
};

SVECTOR D_shelter_b4_upper_sewer_801864D0[4] = {
    { 3600, -5500, 3080, 0 },
    { 3600, -5500, 1920, 0 },
    { 3600, -5500, -920, 0 },
    { 3600, -5500, -2080, 0 },
};

// Indexed views below share one contiguous table.
SVECTOR D_shelter_b4_upper_sewer_801864F0[14] = {
    { 2080, -3480, -5900, 0 },
    { 910, -3480, -5900, 0 },
    { -600, -3480, -80, 0 },
    { -600, -3480, 1080, 0 },
    { -2420, -3480, 5400, 0 },
    { -3580, -3480, 5400, 0 },
    { -6420, -3480, 5400, 0 },
    { -7580, -3480, 5400, 0 },
    { -8730, -4410, 5060, 0 },
    { -9280, -4410, 5060, 0 },
    { 3930, -2500, 8490, 0 },
    { 3930, -2500, 7790, 0 },
    { 3950, -3700, -210, 0 },
    { 3950, -3700, -790, 0 },
};

#define ROOM_FX_HALO_STORAGE_INITIALIZER { \
    { 0, 1, 2 },                           \
    { 2, 1, 0 },                           \
    { 0, 2, 1 },                           \
}
#define ROOM_FX_HALO_STORAGE_TYPE  RoomHaloShade
#define ROOM_FX_HALO_STORAGE_BOUND [3]
#include "../../shared/room_visual_effects_data.inc.c"

static inline RoomHaloShade* RoomFx_GetHaloShades(void)
{
    return RoomFx_HaloShades;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

u8* D_shelter_b4_upper_sewer_80186590[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_b4_upper_sewer_80186594[1] = {
    { { .bytes = { 14, 0 } } },
};

GpWarpRec D_shelter_b4_upper_sewer_80186598[4] = {
    { { .words = { 1024, -9200, 0, 6080 } }, { 0, 0, 0, 0 }, { .words = { 1024, -9200, 0, 6850 } }, { 0, 0, 0, 0 }, 0x542C0002, 0x542C0001, 0, 7, 0, 0 },
    { { .words = { 3072, 3344, 0, -3054 } }, { 0, 0, 0, 0 }, { .words = { 3840, 3350, 0, -3930 } }, { 0, 0, 0, 0 }, 0, 0x542C0003, 0, 2, 2, 446 },
    { { .words = { 3072, 3360, -2000, 9000 } }, { 0, 0, 0, 0 }, { .words = { 2816, 3360, -2000, 8300 } }, { 0, 0, 0, 0 }, 0, 0x542C0003, 0, 10, 0, 0 },
    { { .words = { 0, -8960, -2000, 5600 } }, { 0, 0, 0, 0 }, { .words = { 512, -9600, -2000, 6200 } }, { 0, 0, 0, 0 }, 0x542C0007, 0x542C0006, 0, 12, 0, 0 },
};

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_shelter_b4_upper_sewer_80186678[8] = {
    { 200, 1000, -3000, 0 },
    { 200, 1000, 1000, 0 },
    { 200, 1000, 5000, 0 },
    { -1700, 1000, 6800, 0 },
    { -3800, 1000, 6800, 0 },
    { 200, 1000, 3900, 0 },
    { 0, 1000, -800, 0 },
    { 400, 1000, -3200, 0 },
};

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_shelter_b4_upper_sewer_801866B8[8] = {
    { 160, 500, 6300, 0 },
    { -3680, 500, 6300, 0 },
    { -6500, 500, 6300, 0 },
    { 0, 500, 6300, 0 },
    { 0, 500, 2500, 0 },
    { 0, 500, -2800, 0 },
    { 0, 500, -800, 0 },
    { 0, 500, 1200, 0 },
};

// Retained data: Four retained references to the preceding eight-point coordinate pools; no runtime owner found.
SVECTOR* D_shelter_b4_upper_sewer_801866F8[4] = {
    D_shelter_b4_upper_sewer_80186678,
    D_shelter_b4_upper_sewer_801866B8,
    D_shelter_b4_upper_sewer_80186678,
    D_shelter_b4_upper_sewer_80186678,
};

// Retained data: Adjacent retained point data, ending with fourth halfword -1; no runtime owner found.
SVECTOR D_shelter_b4_upper_sewer_80186708[9] = {
    { 0, 0, 0, 0 },
    { 0, 1000, -2300, 0 },
    { 0, 1000, 700, 0 },
    { 200, 1000, 4000, 0 },
    { 500, 1000, 6600, 0 },
    { -2000, 1000, 6600, 0 },
    { -4500, 1000, 6600, 0 },
    { -6600, 1000, 6600, 0 },
    { 0, 0, 0, -1 },
};

SVECTOR D_shelter_b4_upper_sewer_80186750[7] = {
#include "assets/shelter_b4_upper_sewer_collision_09938_normals.inc"
};

SVECTOR D_shelter_b4_upper_sewer_80186788[87] = {
#include "assets/shelter_b4_upper_sewer_collision_09938_verts.inc"
};

GpGridFace D_shelter_b4_upper_sewer_80186A40[42] = {
#include "assets/shelter_b4_upper_sewer_collision_09938_faces.inc"
};

s16 D_shelter_b4_upper_sewer_80186C38[292] = {
#include "assets/shelter_b4_upper_sewer_collision_09938_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b4_upper_sewer_80186C38[i])
s16* D_shelter_b4_upper_sewer_80186E80[30] = {
#include "assets/shelter_b4_upper_sewer_collision_09938_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b4_upper_sewer_80186EF8 = { NULL, D_shelter_b4_upper_sewer_80186750, D_shelter_b4_upper_sewer_80186788, D_shelter_b4_upper_sewer_80186A40, D_shelter_b4_upper_sewer_80186E80, 0x271A, 6000, 6, 5, 4000, 42 };

GpViewRec D_shelter_b4_upper_sewer_80186F1C[14] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x7530, -1010 } }, 230 },
    { { { { -4063, 0, -514 }, { -449, 1990, 3551 }, { 249, 3580, -1974 } }, { -1380, 5260, 1530 } }, 257 },
    { { { { -4012, 0, -820 }, { -133, 4041, 653 }, { 809, 666, -3959 } }, { 130, 1900, -3960 } }, 257 },
    { { { { -3991, 0, -921 }, { -208, 3989, 902 }, { 897, 925, -3887 } }, { 360, 2120, -7690 } }, 257 },
    { { { { -247, 0, 4088 }, { 3783, 1552, 228 }, { -1550, 3790, -93 } }, { -2160, 5120, -6800 } }, 257 },
    { { { { -655, 0, 4043 }, { 1258, 3892, 203 }, { -3842, 1274, -622 } }, { -1210, 1950, -7340 } }, 257 },
    { { { { -509, 0, 4064 }, { 1212, 3909, 151 }, { -3879, 1221, -486 } }, { 5360, 1950, -6825 } }, 257 },
    { { { { -3832, 0, -1445 }, { -47, 4093, 126 }, { 1445, 135, -3830 } }, { -180, 3740, -6530 } }, 257 },
    { { { { -3906, 0, -1230 }, { -185, 4049, 589 }, { 1216, 617, -3862 } }, { -900, 3500, -9640 } }, 257 },
    { { { { 751, 0, -4026 }, { -274, 4086, -51 }, { 4017, 278, 749 } }, { 6060, 3500, -7800 } }, 257 },
    { { { { 683, 0, 4038 }, { 309, 4083, -52 }, { -4026, 313, 681 } }, { 1210, 3500, -7740 } }, 257 },
    { { { { -3964, 0, 1030 }, { 165, 4042, 636 }, { -1016, 657, -3912 } }, { 8020, 3500, -9160 } }, 257 },
    { { { { -1086, 0, -3949 }, { -519, 4060, 142 }, { 3914, 538, -1077 } }, { -3130, 3460, 250 } }, 380 },
    { { { { -1049, 0, -3959 }, { 841, 4002, -223 }, { 3868, -870, -1025 } }, { -0x2E41, 3890, 333 } }, 230 },
};

SpriteBatch D_shelter_b4_upper_sewer_80187114[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_upper_sewer_80187124[4] = {
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, 72, 750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 40 } }, -136, 80, 750, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -72, 88, 750, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, -16, 96, 750, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_upper_sewer_80187174[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_upper_sewer_8018718C[5] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, -48, 1250, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -24, -56, 1250, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 56, 1094, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 128 } }, -104, -56, 1250, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 136 } }, -160, -64, 1250, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_upper_sewer_801871F0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_upper_sewer_80187208[6] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, -56, 2200, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -56, 2125, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -8, 2125, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -16, -56, 2125, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 88 } }, -56, -64, 2125, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 80 } }, -160, -56, 2125, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_upper_sewer_80187280[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_upper_sewer_80187298[11] = {
    { 143, 0x3FC0, { .fields = { 64, 40 } }, -160, 80, 726, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, -96, 80, 726, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -40, 88, 722, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 8, 88, 722, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 56, 88, 722, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, 96, 96, 718, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 32 } }, 96, 64, 746, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 48 } }, 88, 16, 791, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 48 } }, 88, -32, 852, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 48 } }, 80, -80, 925, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 40 } }, 80, -120, 1000, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_upper_sewer_80187374[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_upper_sewer_8018738C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_upper_sewer_8018739C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_upper_sewer_801873AC[32] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, 32, 2555, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 40, 2250, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 48, 2150, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 56, 2000, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -40, 40, 2555, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 8, 40, 2450, { .fields = { 72, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -56, 48, 2128, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 0, 48, 2175, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -80, 56, 1900, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -8, 56, 2044, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -96, 64, 1825, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -48, 64, 1825, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 8, 64, 1825, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -112, 72, 1750, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -56, 72, 1750, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 8, 72, 1750, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -136, 80, 1700, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -96, 80, 1700, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -48, 80, 1700, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 8, 80, 1700, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -152, 88, 1625, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -96, 88, 1625, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -40, 88, 1625, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 16, 88, 1391, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 8, 96, 1275, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 8, 104, 1225, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -16, 112, 1200, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -56, 104, 1225, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -56, 96, 1275, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -80, 112, 892, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 104, 1225, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 96, 1275, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_upper_sewer_8018762C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 32, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_upper_sewer_80187644[8] = {
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -8, -8, 2450, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, -8, 2450, { .fields = { 80, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -24, 0, 2375, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 24, 0, 2375, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -40, 8, 2250, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 16, 8, 2250, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 16, 16, 2000, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 16, 24, 1692, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_upper_sewer_801876E4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_upper_sewer_801876FC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_upper_sewer_8018770C[3] = {
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -136, 24, 2000, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, -144, 32, 1750, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, -144, 40, 1625, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_upper_sewer_80187748[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_upper_sewer_80187760[26] = {
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -32, 48, 1175, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 16, 48, 1175, { .fields = { 72, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -72, 56, 1125, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -16, 56, 1125, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 56, 1125, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -72, 64, 1075, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -16, 64, 1075, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 40, 64, 1075, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -72, 72, 1025, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -16, 72, 1025, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 48, 72, 1025, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -72, 80, 975, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 80, 975, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 56, 80, 975, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -72, 88, 925, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 88, 925, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, 56, 88, 925, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -72, 96, 875, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 0, 96, 875, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, 64, 96, 875, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -64, 104, 825, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 24, 104, 825, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 88, 104, 825, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -64, 112, 775, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, 16, 112, 775, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 96, 112, 775, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_upper_sewer_80187968[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_upper_sewer_80187980[1] = {
    { 143, 0x3FC0, { .fields = { 64, 112 } }, 0, -56, 180, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_upper_sewer_80187994[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_upper_sewer_801879AC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b4_upper_sewer_801879BC[14] = {
    { { .empty = D_shelter_b4_upper_sewer_80187114 }, D_shelter_b4_upper_sewer_80187114, NULL },
    { { .elements = D_shelter_b4_upper_sewer_80187124 }, D_shelter_b4_upper_sewer_80187174, NULL },
    { { .elements = D_shelter_b4_upper_sewer_8018718C }, D_shelter_b4_upper_sewer_801871F0, NULL },
    { { .elements = D_shelter_b4_upper_sewer_80187208 }, D_shelter_b4_upper_sewer_80187280, NULL },
    { { .elements = D_shelter_b4_upper_sewer_80187298 }, D_shelter_b4_upper_sewer_80187374, NULL },
    { { .empty = D_shelter_b4_upper_sewer_8018738C }, D_shelter_b4_upper_sewer_8018738C, NULL },
    { { .empty = D_shelter_b4_upper_sewer_8018739C }, D_shelter_b4_upper_sewer_8018739C, NULL },
    { { .elements = D_shelter_b4_upper_sewer_801873AC }, D_shelter_b4_upper_sewer_8018762C, NULL },
    { { .elements = D_shelter_b4_upper_sewer_80187644 }, D_shelter_b4_upper_sewer_801876E4, NULL },
    { { .empty = D_shelter_b4_upper_sewer_801876FC }, D_shelter_b4_upper_sewer_801876FC, NULL },
    { { .elements = D_shelter_b4_upper_sewer_8018770C }, D_shelter_b4_upper_sewer_80187748, NULL },
    { { .elements = D_shelter_b4_upper_sewer_80187760 }, D_shelter_b4_upper_sewer_80187968, NULL },
    { { .elements = D_shelter_b4_upper_sewer_80187980 }, D_shelter_b4_upper_sewer_80187994, NULL },
    { { .empty = D_shelter_b4_upper_sewer_801879AC }, D_shelter_b4_upper_sewer_801879AC, NULL },
};

GpPointLight D_shelter_b4_upper_sewer_80187A64[19] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -3000, 500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3161, 4401 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -3000, 3995 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3241, 5061 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -7000, 0x2C24 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 5000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2710, -7000, 0x2C24 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 5000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -7000, 0x4650 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 5000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2710, -7000, 0x4650 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 5000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3500, -4000, 0x2C24 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1228, 1228, 1228 }, { 0, 0 } }, 6000, 7000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1500, -3000, -5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3621, 6283 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7000, -3000, 6000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3825, -3703, -500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 4096, 4096 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3805, -2489, 8145 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3276 }, { 0, 0 } }, 500, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9000, -4570, 5430 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 4096, 3276 }, { 0, 0 } }, 1500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3000, -3000, 6000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3364, 5642 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3000, -5500, 9000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1000, -5500, 9000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -5500, 6505 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -5500, 2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3000, -5500, -1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3000, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7000, -5500, 9000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 3000, 4000 },
};

GpRoomCoordSet D_shelter_b4_upper_sewer_80188184 = { 0, NULL, 19, D_shelter_b4_upper_sewer_80187A64, 0, NULL };

GpObj4C D_shelter_b4_upper_sewer_8018819C[18] = {
    { NULL, NULL, NULL, { -1938, -1921, -3234, 0 }, { { -3109, -4304, -1110, 0 }, { 2989, -4304, 1073, 0 }, { -3109, 4304, -1110, 0 }, { 2989, 4304, 1073, 0 } }, { 1382, 0, -3863, 0 }, { 0, 0, 4096, 0 }, 5369, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -1698, -1920, -3234, 0 }, { { 2834, -4304, 1035, 0 }, { -2923, -4304, -1085, 0 }, { 2834, 4304, 1035, 0 }, { -2923, 4304, -1085, 0 } }, { -1418, 0, 3848, 0 }, { 0, 0, 4096, 0 }, 5271, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -129, -2017, 4319, 0 }, { { 2168, -4304, 303, 0 }, { -2173, -4304, -308, 0 }, { 2168, 4304, 303, 0 }, { -2173, 4304, -308, 0 } }, { -575, 0, 4078, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 159, -2016, 4639, 0 }, { { -1890, -4304, -267, 0 }, { 1883, -4304, 260, 0 }, { -1890, 4304, -267, 0 }, { 1883, 4304, 260, 0 } }, { 566, 0, -4060, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -1196, -2400, 6326, 0 }, { { -23, -4304, -1879, 0 }, { 7, -4304, 1860, 0 }, { -23, 4304, -1879, 0 }, { 7, 4304, 1860, 0 } }, { 4099, 0, -34, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { -1156, -2401, 6284, 0 }, { { 6, -4304, 1882, 0 }, { -40, -4304, -1915, 0 }, { 6, 4304, 1882, 0 }, { -40, 4304, -1915, 0 } }, { -4103, 0, 49, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { -7553, -2369, 6143, 0 }, { { 169, -4304, 1881, 0 }, { -168, -4304, -1880, 0 }, { 169, 4304, 1881, 0 }, { -168, 4304, -1880, 0 } }, { -4082, 0, 365, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 7, 6, 1, 0 },
    { NULL, NULL, NULL, { -7841, -2368, 6175, 0 }, { { -199, -4304, -1861, 0 }, { 199, -4304, 1861, 0 }, { -199, 4304, -1861, 0 }, { 199, 4304, 1861, 0 } }, { 4073, 0, -436, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 6, 7, 1, 0 },
    { NULL, NULL, NULL, { 4160, -2177, 3264, 0 }, { { 2163, -4304, 302, 0 }, { -2177, -4304, -310, 0 }, { 2163, 4304, 302, 0 }, { -2177, 4304, -310, 0 } }, { -576, 0, 4077, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 9, 8, 1, 0 },
    { NULL, NULL, NULL, { 4175, -2209, 3376, 0 }, { { -2206, -4304, -326, 0 }, { 2200, -4304, 319, 0 }, { -2206, 4304, -326, 0 }, { 2200, 4304, 319, 0 } }, { 594, 0, -4065, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 8, 9, 1, 0 },
    { NULL, NULL, NULL, { 3983, -2368, 6863, 0 }, { { 2098, -4304, 7, 0 }, { -2100, -4304, -8, 0 }, { 2098, 4304, 7, 0 }, { -2100, 4304, -8, 0 } }, { -15, 0, 4101, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 10, 9, 1, 0 },
    { NULL, NULL, NULL, { 3776, -2369, 6912, 0 }, { { -1876, -4304, 12, 0 }, { 1868, -4304, -20, 0 }, { -1876, 4304, 12, 0 }, { 1868, 4304, -20, 0 } }, { -36, 0, -4098, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 9, 10, 1, 0 },
    { NULL, NULL, NULL, { -3968, -2369, 9760, 0 }, { { 74, -4304, -1872, 0 }, { -79, -4304, 1868, 0 }, { 74, 4304, -1872, 0 }, { -79, 4304, 1868, 0 } }, { 4092, 0, 167, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 10, 11, 1, 0 },
    { NULL, NULL, NULL, { -3777, -2336, 9791, 0 }, { { -19, -4304, 1886, 0 }, { 13, -4304, -1891, 0 }, { -19, 4304, 1886, 0 }, { 13, 4304, -1891, 0 } }, { -4099, 0, -36, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 11, 10, 1, 0 },
    { NULL, NULL, NULL, { -1095, -64, 543, 0 }, { { 2166, -4304, 328, 0 }, { -2168, -4304, -330, 0 }, { 2166, 4304, 328, 0 }, { -2168, 4304, -330, 0 } }, { -619, 0, 4072, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -1077, 0, 734, 0 }, { { -2173, -4304, -334, 0 }, { 2163, -4304, 324, 0 }, { -2173, 4304, -334, 0 }, { 2163, 4304, 324, 0 } }, { 614, 0, -4050, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -9793, -6208, 6912, 0 }, { { -1855, -4304, 352, 0 }, { 1855, -4304, -352, 0 }, { -1855, 4304, 352, 0 }, { 1855, 4304, -352, 0 } }, { -765, 0, -4027, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 12, 11, 1, 0 },
    { NULL, NULL, NULL, { -9825, -6240, 6816, 0 }, { { 1855, -4304, -352, 0 }, { -1855, -4304, 352, 0 }, { 1855, 4304, -352, 0 }, { -1855, 4304, 352, 0 } }, { 763, 0, 4025, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 11, 12, 129, 0 },
};

GpObj4C D_shelter_b4_upper_sewer_801886F4[8] = {
    { NULL, NULL, NULL, { 3744, -2096, 9024, 0 }, { { -448, 0, -736, 0 }, { 448, 0, -736, 0 }, { -448, 0, 736, 0 }, { 448, 0, 736, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 861, 0, 46, 49, 2, 0 },
    { NULL, NULL, NULL, { 3712, -48, -3008, 0 }, { { -448, 0, -464, 0 }, { 448, 0, -464, 0 }, { -448, 0, 464, 0 }, { 448, 0, 464, 0 } }, { 0, 4107, 0, 0 }, { -4096, 0, 0, 0 }, 643, 0, 45, 34, 2, 0 },
    { NULL, NULL, NULL, { -9408, -48, 6496, 0 }, { { -448, 0, -1008, 0 }, { 448, 0, -1008, 0 }, { -448, 0, 1008, 0 }, { 448, 0, 1008, 0 } }, { 0, 4105, 0, 0 }, { 4096, 0, 0, 0 }, 1101, 0, 43, 18, 2, 0 },
    { NULL, NULL, NULL, { -9024, -2080, 5632, 0 }, { { 1008, 0, -448, 0 }, { 1008, 0, 448, 0 }, { -1008, 0, -448, 0 }, { -1008, 0, 448, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 1101, 0, 43, 68, 2, 0 },
    { NULL, NULL, NULL, { 3344, -2080, -384, 0 }, { { -528, 0, -1008, 0 }, { 528, 0, -1008, 0 }, { -528, 0, 1008, 0 }, { 528, 0, 1008, 0 } }, { 0, 4119, 0, 0 }, { -4096, 0, 0, 0 }, 1137, 0x4002, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { 208, -64, -5663, 0 }, { { -944, 0, -1008, 0 }, { 944, 0, -1008, 0 }, { -944, 0, 1008, 0 }, { 944, 0, 1008, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 1378, 2, 3, 0, 2, 0 },
    { NULL, NULL, NULL, { -9568, -2080, 6400, 0 }, { { -448, 0, -656, 0 }, { 448, 0, -656, 0 }, { -448, 0, 656, 0 }, { 448, 0, 656, 0 } }, { 0, 4106, 0, 0 }, { 4096, 0, 0, 0 }, 794, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { 1568, -64, 3696, 0 }, { { -448, 0, -864, 0 }, { 448, 0, -864, 0 }, { -448, 0, 864, 0 }, { 448, 0, 864, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 972, 2, 4, 0, 130, 0 },
};

GpAreaTmdRec D_shelter_b4_upper_sewer_80188954[3] = {
    { 70, 70, 0, 0, { 0, 0 }, D_8013F5F0 },
    { 71, 71, 0, 0, { 0, 0 }, D_80139E60 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_upper_sewer_80188978[2] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_upper_sewer_80188990[3] = {
    { 24, 24, 0, 0, { 0, 0 }, D_8013647C },
    { 4, 4, 1, 0, { 0, 0 }, D_8015FE48 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_upper_sewer_801889B4[3] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 49, 49, 1, 0, { 0, 0 }, D_8015F400 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_upper_sewer_801889D8[2] = {
    { 23, 23, 0, 0, { 0, 0 }, D_80147AB8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_upper_sewer_801889F0[2] = {
    { 23, 23, 0, 0, { 0, 0 }, D_80147AB8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_upper_sewer_80188A08[3] = {
    { 3, 3, 0, 0, { 0, 0 }, D_80148110 },
    { 4, 4, 1, 0, { 0, 0 }, D_8015FE48 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b4_upper_sewer_80188A2C[5] = {
    { 70, 0, 0, -1600, 0, 6900, 3200, 0, 0, 2, 0 },
    { 70, 0, 0, 300, 0, 500, 4000, 0, 0, 2, 0 },
    { 71, 0, 1, 1350, 0, 4000, 3300, 0, 0, 2, 0 },
    { 71, 0, 1, 1350, 0, 7300, 2550, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_upper_sewer_80188A7C[2] = {
    { 4, 0, 1, 200, 1000, -3400, 0, 0, 2, 4, 4 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_upper_sewer_80188A9C[5] = {
    { 24, 0, 1, -4350, -2000, 9300, 2650, 0, 0, 2, 0 },
    { 24, 0, 1, 600, -2000, 8350, 3400, 0, 0, 2, 0 },
    { 4, 0, 1, 200, 1000, -3400, 0, 0, 2, 4, 2 },
    { 4, 0, 17, 160, 1000, 6300, 3072, 0, 2, 4, 2 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_upper_sewer_80188AEC[4] = {
    { 49, 1, 0, -3400, -2000, 9200, 2250, 0, 0, 2, 0 },
    { 4, 0, 1, 200, 1000, -3400, 0, 0, 2, 4, 2 },
    { 4, 0, 17, 160, 1000, 6300, 3072, 0, 2, 4, 2 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_upper_sewer_80188B2C[2] = {
    { 23, 3, 1, -3000, 0, 6700, 1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_upper_sewer_80188B4C[2] = {
    { 23, 5, 1, -3000, -2000, 9000, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_upper_sewer_80188B6C[3] = {
    { 3, 0, 0, 1000, -2000, 9000, 3072, 0, 0, 2, 0 },
    { 4, 0, 1, 200, 1000, -3400, 0, 0, 2, 4, 6 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_shelter_b4_upper_sewer_80188B9C[12] = {
    { NULL, NULL },
    { D_shelter_b4_upper_sewer_80188A2C, D_shelter_b4_upper_sewer_80188954 },
    { D_shelter_b4_upper_sewer_80188A7C, D_shelter_b4_upper_sewer_80188978 },
    { D_shelter_b4_upper_sewer_80188A9C, D_shelter_b4_upper_sewer_80188990 },
    { D_shelter_b4_upper_sewer_80188AEC, D_shelter_b4_upper_sewer_801889B4 },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b4_upper_sewer_80188B2C, D_shelter_b4_upper_sewer_801889D8 },
    { D_shelter_b4_upper_sewer_80188B4C, D_shelter_b4_upper_sewer_801889F0 },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b4_upper_sewer_80188B6C, D_shelter_b4_upper_sewer_80188A08 },
};

GpObj3A D_shelter_b4_upper_sewer_80188BFC[3] = {
    { NULL, NULL, { -4928, -3088, 208, 0 }, { { -3680, 4656, -4624, 0 }, { 3680, 4656, 4624, 0 }, { -3680, -4656, -4624, 0 }, { 3680, -4656, 4624, 0 } }, { -3206, 0, 2551, 0 }, { 83, 29 }, 1, 0 },
    { NULL, NULL, { 4464, -176, 2784, 0 }, { { -3216, 1920, -4624, 0 }, { 3216, 1920, 4624, 0 }, { -3216, -1920, -4624, 0 }, { 3216, -1920, 4624, 0 } }, { -3368, 0, 2342, 0 }, { 60, 23 }, 1, 0 },
    { NULL, NULL, { -9041, -1824, 6399, 0 }, { { -1056, 8, -992, 0 }, { -1056, -7, 993, 0 }, { 1056, 8, -992, 0 }, { 1056, -7, 993, 0 } }, { 0, -4096, -32, 0 }, { -88, 5 }, 129, 0 },
};

s32 D_shelter_b4_upper_sewer_80188CB0[3] = {
    0x10000041,
    0x10000043,
    0x10000041,
};

s32 D_shelter_b4_upper_sewer_80188CBC[3] = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

s32 D_shelter_b4_upper_sewer_80188CC8[3] = {
    0x10000015,
    0x10000017,
    0x10000015,
};

GpRoomParamRec D_shelter_b4_upper_sewer_80188CD4[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b4_upper_sewer_80188CDC[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec D_shelter_b4_upper_sewer_80188CE4[1] = {
    { 0, 0, 1, 0, D_shelter_b4_upper_sewer_80188CBC },
};

GpRoomParamRec D_shelter_b4_upper_sewer_80188CEC[1] = {
    { 0, 0, 1, 0, D_shelter_b4_upper_sewer_80188CC8 },
};

GpRoomParamRec D_shelter_b4_upper_sewer_80188CF4[1] = {
    { 0, 0, 1, 0, D_shelter_b4_upper_sewer_80188CB0 },
};

GpRoomParamRec* D_shelter_b4_upper_sewer_80188CFC[8] = {
    D_shelter_b4_upper_sewer_80188CD4,
    D_shelter_b4_upper_sewer_80188CDC,
    D_shelter_b4_upper_sewer_80188CE4,
    D_shelter_b4_upper_sewer_80188CEC,
    D_shelter_b4_upper_sewer_80188CD4,
    D_shelter_b4_upper_sewer_80188CF4,
    D_shelter_b4_upper_sewer_80188CD4,
    D_shelter_b4_upper_sewer_80188CD4,
};

RoomFadeStorage D_shelter_b4_upper_sewer_80188D1C = { 0 };

RoomEventMsg D_shelter_b4_upper_sewer_80188D24 = { 0 };

u8 D_shelter_b4_upper_sewer_80188D2C[4] = {
    0,
    162,
    51,
    44,
};

u8* D_shelter_b4_upper_sewer_80188D30;

static void func_shelter_b4_upper_sewer_8017DC88(Task* task);

void func_shelter_b4_upper_sewer_8017D660(Task* task)
{
    switch (task->state) {
        case 0:
            func_shelter_b4_upper_sewer_8017E59C(0);
            gGameSession->eventState = 1;
            task->state++;
            break;
        case 1:
            gGameSession->hideHud = 1;
            Gp_RunCapCmd(1, 0);
            D_80115690 = 1;
            D_80115680 = 5;
            task->state++;
            break;
        case 2:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 3:
            if (Gp_GetCapEventKey() == 0xC) {
                taskKill(task);
                Mc_SaveData[0].state.at4.loc.view = D_shelter_b4_upper_sewer_80188D2C[0];
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
                Gp_MsgAllyWeapon(1);
                Gp_MsgAlly3F3(1);
                gGameSession->eventState = 0;
                gGameSession->hideHud    = 0;
                Gp_StateF0.field_4       = 0;
                D_80114D08               = 0xA;
                break;
            }
            func_800E8614(D_shelter_b4_upper_sewer_80186318, 0);
            GameFlag_SetNibble(0xB8, 1);
            GameFlag_SetNibble(0x1BD, 0);
            task->state++;
            break;
        case 4:
            if (gGameSession->eventState == 0) {
                D_80114D08         = 0xA;
                Gp_StateF0.field_4 = 0;
                taskKill(task);
            }
            break;
    }
}

void func_shelter_b4_upper_sewer_8017D80C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
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
                Gp_StateF0.field_4 = 0;
                D_80114D08         = 0xA;
                break;
            }
            Gp_StateF0.field_4 = 1;
            Gp_TriggerPeIfArmed();
            D_shelter_b4_upper_sewer_80188D1C.fade.field_0 = 0;
            D_shelter_b4_upper_sewer_80188D1C.fade.field_1 = 0;
            D_shelter_b4_upper_sewer_80188D1C.fade.field_2 = 0x1E;
            Task_Spawn(1, 0x31, 0, &D_shelter_b4_upper_sewer_80188D1C.fade);
            arg0->killCountdown = 0x1E;
            arg0->state++;
            break;
        case 3:
            if (--arg0->killCountdown == 0) {
                SndEvt_EnqueueType6(0x542C0003, 0, 0);
                arg0->state++;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(0x542C0003) == 0) {
                arg0->state++;
            }
            break;
        case 5:
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_shelter_b4_upper_sewer_80188D24.warp;
            Mc_SaveData[0].state.at4.loc.warp = D_shelter_b4_upper_sewer_80188D24.field_4;
            Mc_SaveData[0].state.at4.loc.room = ((u8*)&D_shelter_b4_upper_sewer_80188D24.areaId)[1];
            Task_Spawn(0, 0x11, 0x10, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b4_upper_sewer_8017D9BC(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b4_upper_sewer_8017D9C4(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    func_map_shelter_80179A04(src, dst);
    if (src->areaId == 0x2D) {
        if (src->queryOnly == ROOM_EVENT_EXECUTE) {
            D_shelter_b4_upper_sewer_80188D24.warp              = (u8)dst->areaId;
            D_shelter_b4_upper_sewer_80188D24.field_4           = dst->warp;
            ((u8*)&D_shelter_b4_upper_sewer_80188D24.areaId)[1] = dst->room;
            Task_SpawnFromTable(D_shelter_b4_upper_sewer_80186300, 1, 7, 0);
        }
        return 0;
    }
    if (src->areaId == 0x2E) {
        if (src->queryOnly == ROOM_EVENT_EXECUTE) {
            D_shelter_b4_upper_sewer_80188D24.warp              = (u8)dst->areaId;
            D_shelter_b4_upper_sewer_80188D24.field_4           = dst->warp;
            ((u8*)&D_shelter_b4_upper_sewer_80188D24.areaId)[1] = dst->room;
            Task_SpawnFromTable(D_shelter_b4_upper_sewer_80186300, 1, 8, 0);
        }
        return 0;
    }
    return 1;
}

s32 func_shelter_b4_upper_sewer_8017DAB0(Task* task, s32 msgId, s32 arg2, TaskMessageArg arg3)
{
    u8 temp_a1;

    if (arg2 == 1) {
        if (GameFlag_GetNibble(0xB8) == 0) {
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
            Gp_MsgPlayerWeapon(0);
            Gp_MsgAllyWeapon(0);
            Gp_StateF0.field_4                   = 2;
            temp_a1                              = Mc_SaveData[0].state.at4.loc.view;
            Mc_SaveData[0].state.at4.loc.view    = 0xD;
            D_shelter_b4_upper_sewer_80188D2C[0] = temp_a1;
            Task_SpawnFromTable(D_shelter_b4_upper_sewer_80186300, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(6);
        }
    }
    return 0;
}

s32 func_shelter_b4_upper_sewer_8017DB50(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b4_upper_sewer_8017DB58(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    if (arg2 == 4) {
        func_shelter_b4_upper_sewer_8017E59C(1);
        SndEvt_EnqueueType6(0x542C0004, 0, 0);
    }
    return 0;
}

void func_shelter_b4_upper_sewer_8017DB94(void)
{
    Mc_SaveData[0].state.at4.loc.view = D_shelter_b4_upper_sewer_80188D2C[0];
}

static void func_shelter_b4_upper_sewer_8017DBA8(Task* task)
{
    task->msgTable = D_shelter_b4_upper_sewer_801862D0;
    Game_SetPtrSlot(task, 7);
    if (GameFlag_GetNibble(0xB7) != 0) {
        D_shelter_b4_upper_sewer_80186438 = -0x708;
        Task_SpawnFromTable(D_shelter_b4_upper_sewer_8018643C, 0, 0, 0);
    } else {
        D_shelter_b4_upper_sewer_80186438 = 0;
    }
    task->state = (s32)(task->state + 1);
}

static void func_shelter_b4_upper_sewer_8017DC28(Task* task)
{
}

void func_shelter_b4_upper_sewer_8017DC30(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b4_upper_sewer_8017D5C4;
    sp.funcs[task->state](task);
}

/// Water state of the room's water task: clamps the water level
/// `D_shelter_b4_upper_sewer_80186438` to -0x640..0, publishes it as the
/// session's water height, and draws the surfaces of the current camera view at
/// that height, in a blue that brightens as the level moves away from 0.
static void func_shelter_b4_upper_sewer_8017DC88(Task* task)
{
    s16 w;
    u8  c;
    s32 h;

    if (Mc_SaveData[0].state.companionType == 0) {
        D_shelter_b4_upper_sewer_80188D30 = (u8*)Fs_ActorLoadBase2 + gDisplayState.otBuffer * 0xC000;
    } else {
        D_shelter_b4_upper_sewer_80188D30 = (u8*)Fs_ActorLoadBase1 + gDisplayState.otBuffer * 0xC000;
    }
    if (D_shelter_b4_upper_sewer_80186438 < -0x640) {
        D_shelter_b4_upper_sewer_80186438 = -0x640;
    } else if (D_shelter_b4_upper_sewer_80186438 > 0) {
        D_shelter_b4_upper_sewer_80186438 = 0;
    }
    w                    = D_shelter_b4_upper_sewer_80186438;
    gGameSession->waterY = w;
    h                    = w;
    c                    = (-h * 16) / 225;
    if (gGameSession->location.loc.view != 0xC) {
        func_shelter_b4_upper_sewer_8017DD98(task, D_shelter_b4_upper_sewer_80186448, h, c);
    } else {
        func_shelter_b4_upper_sewer_8017DD98(task, D_shelter_b4_upper_sewer_80186454, h, c);
    }
}

/// Draws each surface in `e` at height `y` as a strip of `count` flat
/// semi-transparent quads coloured (0, `c` / 4, `c`), projected through the
/// view matrix and each followed by a draw-mode packet selecting blend mode 2.
/// Quads are linked 0x60 deeper in the ordering table than their projected
/// depth, and those the projection flags as invalid are skipped. The
/// per-surface values live in a work block pushed on the scratchpad stack for
/// the duration of the call. `task` is unused.
static void func_shelter_b4_upper_sewer_8017DD98(Task* task, ShelterB4UpperSewerSurface* e, s16 y, u8 c)
{
    SVECTOR           v0, v1, v2, v3;
    long              sxy0, sxy1, sxy2, sxy3;
    long              p, flag;
    RoomWaterScratch* w;
    u8*               head;
    POLY_F4*          poly;
    DR_MODE*          dr;
    s32               otz;
    s32               i;

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    head                       = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8)           = head - 0xC;
    w                          = (RoomWaterScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    w->y = y;
    for (; e->count != -1; e++) {
        if (e->alongZ == 0) {
            w->dx = e->width / e->count;
            w->dz = e->depth;
            w->x  = e->x;
            w->z  = e->z;
            for (i = 0; i < e->count; i++) {
                v0.vx   = w->x + w->dx * i;
                v0.vy   = w->y;
                v0.vz   = w->z;
                v1.vx   = w->x + w->dx * (i + 1);
                v1.vy   = w->y;
                v1.vz   = w->z;
                w->wave = 0;
                v2.vx   = w->x + w->dx * i;
                v2.vy   = w->y + w->wave;
                v2.vz   = w->z + w->dz;
                w->wave = 0;
                v3.vx   = w->x + w->dx * (i + 1);
                v3.vy   = w->y + w->wave;
                v3.vz   = w->z + w->dz;
                otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
                if (flag >= 0) {
                    poly                              = (POLY_F4*)D_shelter_b4_upper_sewer_80188D30;
                    D_shelter_b4_upper_sewer_80188D30 = (u8*)(poly + 1);
                    setlen(poly, 5);
                    setcode(poly, 0x2A);
                    PRIM_XY_WORD(poly, 0) = sxy0;
                    PRIM_XY_WORD(poly, 1) = sxy1;
                    PRIM_XY_WORD(poly, 2) = sxy2;
                    PRIM_XY_WORD(poly, 3) = sxy3;
                    poly->r0              = 0;
                    poly->g0              = c >> 2;
                    poly->b0              = c;
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(otz + 0x60) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            poly);
                    dr                                = (DR_MODE*)D_shelter_b4_upper_sewer_80188D30;
                    D_shelter_b4_upper_sewer_80188D30 = (u8*)(dr + 1);
                    setlen(dr, 1);
                    dr->code[0] = 0xE100004A;
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(otz + 0x60) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            dr);
                }
            }
        } else {
            w->dx = e->width;
            w->dz = e->depth / e->count;
            w->x  = e->x;
            w->z  = e->z;
            for (i = 0; i < e->count; i++) {
                v0.vx   = w->x;
                v0.vy   = w->y;
                v0.vz   = w->z + w->dz * i;
                v1.vx   = w->x;
                v1.vy   = w->y;
                v1.vz   = w->z + w->dz * (i + 1);
                w->wave = 0;
                v2.vx   = w->x + w->dx;
                v2.vy   = w->y + w->wave;
                v2.vz   = w->z + w->dz * i;
                w->wave = 0;
                v3.vx   = w->x + w->dx;
                v3.vy   = w->y + w->wave;
                v3.vz   = w->z + w->dz * (i + 1);
                otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
                if (flag >= 0) {
                    poly                              = (POLY_F4*)D_shelter_b4_upper_sewer_80188D30;
                    D_shelter_b4_upper_sewer_80188D30 = (u8*)(poly + 1);
                    setlen(poly, 5);
                    setcode(poly, 0x2A);
                    PRIM_XY_WORD(poly, 0) = sxy0;
                    PRIM_XY_WORD(poly, 1) = sxy1;
                    PRIM_XY_WORD(poly, 2) = sxy2;
                    PRIM_XY_WORD(poly, 3) = sxy3;
                    poly->r0              = 0;
                    poly->g0              = c >> 2;
                    poly->b0              = c;
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(otz + 0x60) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            poly);
                    dr                                = (DR_MODE*)D_shelter_b4_upper_sewer_80188D30;
                    D_shelter_b4_upper_sewer_80188D30 = (u8*)(dr + 1);
                    setlen(dr, 1);
                    dr->code[0] = 0xE100004A;
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(otz + 0x60) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            dr);
                }
            }
        }
    }
    SCRATCH_POP_BYTES(0xC);
}

/// The room's water task: runs its state (`func_shelter_b4_upper_sewer_8017E55C`
/// once, then `func_shelter_b4_upper_sewer_8017DC88` every frame) and publishes
/// the water level as the session's water height.
void func_shelter_b4_upper_sewer_8017E4F4(Task* task)
{
    TaskFunc states[2] = { func_shelter_b4_upper_sewer_8017E55C, func_shelter_b4_upper_sewer_8017DC88 };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b4_upper_sewer_80186438;
}

static void func_shelter_b4_upper_sewer_8017E55C(Task* arg0)
{
    if (Mc_SaveData[0].state.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Sets or clears `field_4` of the second sprite command in view 13 of the
/// current room's sprite table, from the low byte of `arg0` (zero clears it,
/// anything else sets it to 1).
static void func_shelter_b4_upper_sewer_8017E59C(s32 arg0)
{
    GameLocationKey* sess = &gGameSession->location.loc;
    SpriteBatch*     batches;

    batches = Gp_SprtTables[sess->stage - 1][0].field_0[sess->area - 1][12].field_4;
    if ((arg0 & 0xFF) == 0) {
        batches[1].hidden = 0;
    } else {
        batches[1].hidden = 1;
    }
}

/// Publishes the sewer's effect ids on the task's first tick - two extra ids
/// only while `GameFlag_GetNibble(0xB7)` is 1 - then draws the
/// `func_shelter_b4_upper_sewer_8017F8CC` capsules the current camera view
/// shows. Views 4 and 13 both end on
/// `D_shelter_b4_upper_sewer_801864F0[12]`, and writing that address off the
/// array (rather than through its own symbol) is what keeps view 4's array
/// base live across the first call while the shared tail is cross-jumped.
void func_shelter_b4_upper_sewer_8017E5F8(Task* arg0)
{
    if (arg0->state == 0) {
        if (GameFlag_GetNibble(0xB7) == 1) {
            D_8011574C = 0x60170;
            D_80115738 = 0x60171;
        }
        D_80115728  = 0x6024E;
        D_80115744  = 0x6025A;
        D_8011573C  = 0x60265;
        D_80115720  = 0x60271;
        D_80115758  = 0x600F0;
        D_8011572C  = 0x600F1;
        D_80115750  = 0x600F2;
        D_80115734  = 0x60224;
        D_80115730  = 0x6022F;
        D_80115754  = 0x6023A;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 3:
            func_shelter_b4_upper_sewer_8017F8CC(D_shelter_b4_upper_sewer_801864F0, 0x200, 0x444);
            break;
        case 4:
            func_shelter_b4_upper_sewer_8017F8CC(&D_shelter_b4_upper_sewer_801864F0[0], 0x200, 0x222);
            func_shelter_b4_upper_sewer_8017F8CC(&D_shelter_b4_upper_sewer_801864F0[12], 0x200, 0x124);
            break;
        case 8: {
            SVECTOR* p = D_shelter_b4_upper_sewer_801864D0;
            func_shelter_b4_upper_sewer_8017F8CC(&p[0], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[2], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[4], 0x200, 0x222);
            func_shelter_b4_upper_sewer_8017F8CC(&p[6], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[16], 0x200, 0x124);
            break;
        }
        case 9: {
            SVECTOR* p = D_shelter_b4_upper_sewer_801864D0;
            func_shelter_b4_upper_sewer_8017F8CC(&p[0], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[2], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[4], 0x200, 0x222);
            func_shelter_b4_upper_sewer_8017F8CC(&p[6], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[16], 0x200, 0x124);
            break;
        }
        case 10: {
            SVECTOR* p = D_shelter_b4_upper_sewer_801864B0;
            func_shelter_b4_upper_sewer_8017F8CC(&p[0], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[2], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[18], 0x200, 0x343);
            break;
        }
        case 11: {
            SVECTOR* p = D_shelter_b4_upper_sewer_80186490;
            func_shelter_b4_upper_sewer_8017F8CC(&p[0], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[6], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[18], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[20], 0x200, 0x343);
            break;
        }
        case 12:
            func_shelter_b4_upper_sewer_8017F8CC(D_shelter_b4_upper_sewer_80186520, 0x200, 0x444);
            break;
        case 13:
            func_shelter_b4_upper_sewer_8017F8CC(&D_shelter_b4_upper_sewer_801864F0[12], 0x200, 0x124);
            break;
    }
}

/// Effect task drawing a flat quad through `func_shelter_b4_upper_sewer_8017EA0C`
/// that grows and fades out. On its first frame it starts the size (`angle`)
/// at the low 12 bits of the spawn argument and the brightness (`scale`) at
/// 0x40, and turns its coordinate to a random angle about Y. Each frame it then
/// grows the size by 0x20 and dims the brightness by 2, releasing the effect
/// once the brightness falls under 2. While `Gp_State1C->effectControl` is
/// non-zero it only redraws at the current values, and releases from state 4.
void func_shelter_b4_upper_sewer_8017E8B8(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        func_shelter_b4_upper_sewer_8017EA0C(coord, work->angle, work->scale);
        if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
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
        func_shelter_b4_upper_sewer_8017EA0C(coord, work->angle, work->scale);
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
static void func_shelter_b4_upper_sewer_8017EA0C(GfxCoord* arg0, s32 arg1, s32 arg2)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;

    scratch  = SCRATCH_STACK_CURSOR_SLOT;
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
        prim           = gGpuPrimCursor;
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}

/// Particle task drawn as the spinning sprite of
/// `func_shelter_b4_upper_sewer_8017F1FC` (state 1) or the standing sprite of
/// `func_shelter_b4_upper_sewer_8017F5E8` (state 2), which the top nibble of the
/// spawn argument selects. The first frame takes the size from the argument's
/// low 12 bits, a random angle, and the ticks per animation frame from bits
/// 12-15. Unless the work already carries a velocity it picks one by the kind
/// in bits 24-27, scaled to the speed in bits 16-23 (0x40 when zero). Each later tick draws, moves the coordinate by the velocity with
/// gravity pulling it down, and releases the task after animation frame 7.
/// While an event runs it only draws, and is released once the event state
/// reaches 4.
void func_shelter_b4_upper_sewer_8017ED40(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    SVECTOR*   vec;
    s32        kind;
    s32        step;
    s32        state;
    s32        level;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (Gp_State1C->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (task->state < 2) {
                func_shelter_b4_upper_sewer_8017F1FC(coord, work->index, work->scale, work->angle);
            } else {
                func_shelter_b4_upper_sewer_8017F5E8(coord, work->index, work->scale);
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
            func_shelter_b4_upper_sewer_8017F1FC(coord, work->index, work->scale, work->angle);
            break;
        case 2:
            func_shelter_b4_upper_sewer_8017F5E8(coord, work->index, work->scale);
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

/// Draws a spinning sprite at the coordinate's world position: one
/// semi-transparent textured quad (tpage 0x2B, clut 0x43D3) centred on the
/// projected point, unless the projection flags an error. `arg1` picks the
/// 32-texel-wide frame at U `arg1 * 32` in the strip at V 0xE0..0xFF, `arg2` is
/// the size (a screen half-extent of `arg2 * 31 / otz`) and `arg3` the angle
/// the corners are turned by.
static void func_shelter_b4_upper_sewer_8017F1FC(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
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

    scratch                                   = SCRATCH_STACK_CURSOR_SLOT;
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
        prim           = gGpuPrimCursor;
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x1C);
}

/// Draws a sprite at the coordinate's world position: one semi-transparent
/// textured quad (tpage 0x2B, clut 0x43D2) around the projected point, unless
/// the projection flags an error. `arg1` picks one of eight 56-texel frames,
/// four across and two down from V 0x70. `arg2` is the size, a screen
/// half-extent of `arg2 * 55 / otz`; the quad is that wide on each side and
/// stands on the point, reaching one and a half extents above it and half an
/// extent below.
static void func_shelter_b4_upper_sewer_8017F5E8(GfxCoord* arg0, s16 arg1, s16 arg2)
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
        prim           = gGpuPrimCursor;
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpRingScratch);
}

/// Draws a glowing capsule between the points `arg0[0]` and `arg0[1]`,
/// projected through `gGfxViewCoord.workm`; nothing is drawn unless both project.
/// Each end is a half-disc of screen radius `arg1 * 64 / otz` and the two are
/// joined by a band, built from gouraud quads bright at the centre line and
/// black at the rim, in two 0x400 steps around the angle between the projected
/// points. `arg2` is the colour as three 4-bit channels (0xRGB), brightened
/// slightly on odd frames.
static void func_shelter_b4_upper_sewer_8017F8CC(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                   scratch;
    u8*                      head;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    SVECTOR*                 p1;
    s32                      ang;
    s32                      t;
    s32                      t3;
    s32                      t2;
    s32                      limit;
    s32                      angStart;
    s32                      packed;
    s32                      blend;
    s32                      tr;
    s32                      tg;
    s32                      scaled;
    s32                      conn;
    u8                       r;
    u8                       g;
    u8                       b;

    p1       = arg0 + 1;
    scratch  = SCRATCH_STACK_CURSOR_SLOT;
    head     = *scratch;
    *scratch = head - 0x1C;
    block    = (OverlayPointPairScratch*)(head - 0x1C);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx1);
        gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((OverlayPointPairScratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((OverlayPointPairScratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            ang       = (s16)ang;
            blend     = ((u8)ds->animFrame & 1) * 8;
            packed    = arg2 << 16;
            tr        = (packed >> 20) & 0xF0;
            tg        = (packed >> 16) & 0xF0;
            r         = blend | tr;
            g         = blend | tg;
            b         = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    conn           = angStart + ((ang - angStart) * 2);
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(conn)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(conn)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(conn)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(conn)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    t3             = ang + 0x800;
                    prim           = gGpuPrimCursor;
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

#include "../../shared/room_visual_effects.inc.c"

void func_shelter_b4_upper_sewer_80180110(Task* task)
{
    RoomFx_MoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void func_shelter_b4_upper_sewer_80180E58(Task* arg0)
{
    RoomFx_HaloTask(arg0);
}

void func_shelter_b4_upper_sewer_801811F0(Task* arg0)
{
    RoomFx_OrangeBurstTask(arg0);
}

#include "../../shared/room_visual_effects_flash.inc.c"

void func_shelter_b4_upper_sewer_80182600(Task* arg0)
{
    RoomFx_SparkEmitterTask(arg0);
}

void func_shelter_b4_upper_sewer_80182734(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_shelter_b4_upper_sewer_80183198(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b4_upper_sewer_80183A80(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"

void func_shelter_b4_upper_sewer_801846C8(Task* arg0)
{
    RoomFx_GlowDiscTask(arg0);
}

void func_shelter_b4_upper_sewer_80184C20(Task* task)
{
    RoomFx_FlyingSparkTask(task);
}

#include "../../shared/room_visual_effects_burst.inc.c"

void func_shelter_b4_upper_sewer_80185880(Task* arg0)
{
    RoomFx_OrangeBurst2Task(arg0);
}

#include "../../shared/room_visual_effects_burst_draw.inc.c"
