#include "rooms/shelter_b4_upper_sewer.h"

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
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

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

#include "../../shared/glow_draw.h"
#include "../../shared/water_effects.h"

#define D_shelter_b4_upper_sewer_80186520 (D_shelter_b4_upper_sewer_801864F0 + 6)

/// A rectangular water patch in world coordinates whose entry chooses its own strip axis.
///
/// The same rectangle as `RoomWaterSurface`, with height supplied by its
/// drawer, but with a 16-bit segment count followed by a flag selecting which
/// extent that count divides, using integer division; the other extent spans
/// the strip. A list ends at an entry with
/// `segmentCount == WATER_SURFACE_LIST_END`, whose other fields are not read.
/// Every entry before it needs a nonzero `segmentCount`, and a positive one to
/// draw anything.
typedef struct {
    s16 x;              // Starting X in world units
    s16 z;              // Starting Z in world units
    s16 width;          // Extent along +X in world units
    s16 depth;          // Extent along +Z in world units
    s16 segmentCount;   // Quads in the strip (>0 drawable entry, -1 list end)
    s16 segmentsAlongZ; // Divided extent (0 width, quads side by side along X; nonzero depth, along Z)
} _ShelterB4UpperSewerWaterSurface;
STATIC_ASSERT_SIZEOF(_ShelterB4UpperSewerWaterSurface, 0xC);

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_shelter_b4_upper_sewer_80188D2C[4];

extern EvsCommand       D_shelter_b4_upper_sewer_80186318[];
extern TaskDesc         D_shelter_b4_upper_sewer_80186300[];
extern TaskMessageEntry D_shelter_b4_upper_sewer_801862D0[];

extern TaskDesc D_shelter_b4_upper_sewer_8018643C[];
/// Save location filled from the outgoing location just before a table task is
/// spawned: `field_2` / `field_4` / `field_1` take its `field_0` / `field_2` /
/// `field_3`.
extern RoomEventMsg D_shelter_b4_upper_sewer_80188D24;
/// Spawn argument for the task `func_shelter_b4_upper_sewer_8017D80C` starts
/// with `Task_Spawn(1, 0x31, ...)`.
extern RoomFadeStorage D_shelter_b4_upper_sewer_80188D1C;

extern _ShelterB4UpperSewerWaterSurface D_shelter_b4_upper_sewer_80186448[];
extern _ShelterB4UpperSewerWaterSurface D_shelter_b4_upper_sewer_80186454[];
extern u8*                              D_shelter_b4_upper_sewer_80188D30;

extern SVECTOR D_shelter_b4_upper_sewer_80186490[];
extern SVECTOR D_shelter_b4_upper_sewer_801864B0[];
extern SVECTOR D_shelter_b4_upper_sewer_801864D0[];

/// The two points the trail is emitted from, relative to the effect's parent:
/// the first positions the effect's own coordinate, the second is the other
/// end of the trail.
/// The second of those points, which the per-frame state reaches through its
/// own label rather than by indexing the pair.

/// Per-variant right shifts applied to the red, green and blue channels of a
/// disc's brightness.

static void func_shelter_b4_upper_sewer_8017DBA8(Task* task);
static void func_shelter_b4_upper_sewer_8017DC28(Task* task);
static void func_shelter_b4_upper_sewer_8017DD98(Task* task, _ShelterB4UpperSewerWaterSurface* surface, s16 y, u8 c);
static void func_shelter_b4_upper_sewer_8017E55C(Task* arg0);
static void func_shelter_b4_upper_sewer_8017E59C(s32 arg0);
#include "../../shared/room_visual_effects.h"

static RoomFxShade _gRoomEffectHaloShades[3];

/// State handlers of the task `func_shelter_b4_upper_sewer_8017DC30` runs,
/// which copies the table to the stack and calls the entry for the task's
/// state: the room's setup (message table, pointer slot, water level), an idle
/// state, and `taskKill`.
static const TaskFuncTable3 D_shelter_b4_upper_sewer_8017D5C4 = {
    { func_shelter_b4_upper_sewer_8017DBA8, func_shelter_b4_upper_sewer_8017DC28, taskKill }
};

void func_shelter_b4_upper_sewer_8017E4F4(Task*);

s32  func_shelter_b4_upper_sewer_8017D9BC(Task*, s32, s32, s32);
s32  func_shelter_b4_upper_sewer_8017D9C4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b4_upper_sewer_8017DAB0(Task*, s32, s32, s32);
s32  func_shelter_b4_upper_sewer_8017DB50(Task*, s32, s32, s32);
s32  func_shelter_b4_upper_sewer_8017DB58(Task*, s32, s32, s32);
void func_shelter_b4_upper_sewer_8017D660(Task*);
void func_shelter_b4_upper_sewer_8017D80C(Task*);
void func_shelter_b4_upper_sewer_8017DB94(void);

extern TaskDesc D_actor_100400_80147E48;

TaskMessageEntry D_shelter_b4_upper_sewer_801862D0[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b4_upper_sewer_8017D9C4 },
    { 5105, func_shelter_b4_upper_sewer_8017D9BC },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b4_upper_sewer_8017DB50 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b4_upper_sewer_8017DAB0 },
    { ROOM_MESSAGE_SOUND, func_shelter_b4_upper_sewer_8017DB58 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_shelter_b4_upper_sewer_80186300[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b4_upper_sewer_8017D660, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b4_upper_sewer_8017D80C, { .value = 0 } },
};

EvsCommand D_shelter_b4_upper_sewer_80186318[12] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x542C0005 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b4_upper_sewer_8017DB94 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

s16 D_shelter_b4_upper_sewer_80186438 = -1600;

TaskDesc D_shelter_b4_upper_sewer_8018643C[1] = {
    { { { TASK_BODY_NONE, 96 } }, func_shelter_b4_upper_sewer_8017E4F4, { .value = 0 } },
};

_ShelterB4UpperSewerWaterSurface D_shelter_b4_upper_sewer_80186448[1] = {
    { -9700, 5600, 1720, 1800, 16, 0 },
};

_ShelterB4UpperSewerWaterSurface D_shelter_b4_upper_sewer_80186454[5] = {
    { -8000, 5000, 7120, 2900, 32, 0 },
    { -900, 3100, 2800, 4800, 32, 1 },
    { -900, -5900, 1800, 9000, 32, 1 },
    { 900, -5900, 3000, 3800, 32, 1 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END, 0 },
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
#define ROOM_FX_HALO_STORAGE_TYPE  RoomFxShade
#define ROOM_FX_HALO_STORAGE_BOUND [3]
#include "../../shared/room_visual_effects_halo_data.inc.c"
#include "../../shared/room_visual_effects_trail_data.inc.c"
#include "../../shared/room_visual_effects_disc_data.inc.c"

static inline RoomFxShade* RoomFx_GetHaloShades(void)
{
    return _gRoomEffectHaloShades;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

u8* D_shelter_b4_upper_sewer_80186590[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b4_upper_sewer_80186594[1] = { 14 };

DirectionWarpEntry D_shelter_b4_upper_sewer_80186598[4] = {
    { { { .word = 1024 }, -9200, 0, 6080 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -9200, 0, 6850 }, { 0, 0, 0, 0 }, 0x542C0002, 0x542C0001, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 3344, 0, -3054 }, { 0, 0, 0, 0 }, { { .word = 3840 }, 3350, 0, -3930 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, 0x542C0003, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_FADE_DEPARTURE, GAME_FLAG_MAP_MARK_RESERVOIR_1BE },
    { { { .word = 3072 }, 3360, -2000, 9000 }, { 0, 0, 0, 0 }, { { .word = 2816 }, 3360, -2000, 8300 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, 0x542C0003, DIRECTION_WARP_SOUND_NONE, 10, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 0 }, -8960, -2000, 5600 }, { 0, 0, 0, 0 }, { { .word = 512 }, -9600, -2000, 6200 }, { 0, 0, 0, 0 }, 0x542C0007, 0x542C0006, DIRECTION_WARP_SOUND_NONE, 12, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
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

static SVECTOR _gShelterB4UpperSewerCollision09938Normals[7] = {
#include "assets/shelter_b4_upper_sewer_collision_09938_normals.inc"
};

static SVECTOR _gShelterB4UpperSewerCollision09938Verts[87] = {
#include "assets/shelter_b4_upper_sewer_collision_09938_verts.inc"
};

static WorldCollisionGridFace _gShelterB4UpperSewerCollision09938Faces[42] = {
#include "assets/shelter_b4_upper_sewer_collision_09938_faces.inc"
};

static s16 _gShelterB4UpperSewerCollision09938Cells[292] = {
#include "assets/shelter_b4_upper_sewer_collision_09938_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB4UpperSewerCollision09938Cells[i])
static s16* _gShelterB4UpperSewerCollision09938Table[30] = {
#include "assets/shelter_b4_upper_sewer_collision_09938_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b4_upper_sewer_80186EF8 = { NULL, _gShelterB4UpperSewerCollision09938Normals, _gShelterB4UpperSewerCollision09938Verts, _gShelterB4UpperSewerCollision09938Faces, _gShelterB4UpperSewerCollision09938Table, 0x271A, 6000, 6, 5, 4000, 42 };

ViewCamera D_shelter_b4_upper_sewer_80186F1C[14] = {
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

SpriteSource D_shelter_b4_upper_sewer_80187124[4] = {
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

SpriteSource D_shelter_b4_upper_sewer_8018718C[5] = {
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

SpriteSource D_shelter_b4_upper_sewer_80187208[6] = {
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

SpriteSource D_shelter_b4_upper_sewer_80187298[11] = {
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

SpriteSource D_shelter_b4_upper_sewer_801873AC[32] = {
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

SpriteSource D_shelter_b4_upper_sewer_80187644[8] = {
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

SpriteSource D_shelter_b4_upper_sewer_8018770C[3] = {
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -136, 24, 2000, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, -144, 32, 1750, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 8 } }, -144, 40, 1625, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_upper_sewer_80187748[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b4_upper_sewer_80187760[26] = {
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

SpriteSource D_shelter_b4_upper_sewer_80187980[1] = {
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

SpriteView D_shelter_b4_upper_sewer_801879BC[14] = {
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

WorldCoordPointLight D_shelter_b4_upper_sewer_80187A64[19] = {
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

WorldCoordRoomLights D_shelter_b4_upper_sewer_80188184 = { 0, NULL, ARRAY_SIZE(D_shelter_b4_upper_sewer_80187A64), D_shelter_b4_upper_sewer_80187A64, 0, NULL };

WorldCollisionTrigger D_shelter_b4_upper_sewer_8018819C[18] = {
    { NULL, NULL, NULL, { -1938, -1921, -3234, 0 }, { { -3109, -4304, -1110, 0 }, { 2989, -4304, 1073, 0 }, { -3109, 4304, -1110, 0 }, { 2989, 4304, 1073, 0 } }, { 1382, 0, -3863, 0 }, { 0, 0, 4096, 0 }, 5369, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1698, -1920, -3234, 0 }, { { 2834, -4304, 1035, 0 }, { -2923, -4304, -1085, 0 }, { 2834, 4304, 1035, 0 }, { -2923, 4304, -1085, 0 } }, { -1418, 0, 3848, 0 }, { 0, 0, 4096, 0 }, 5271, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -129, -2017, 4319, 0 }, { { 2168, -4304, 303, 0 }, { -2173, -4304, -308, 0 }, { 2168, 4304, 303, 0 }, { -2173, 4304, -308, 0 } }, { -575, 0, 4078, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 159, -2016, 4639, 0 }, { { -1890, -4304, -267, 0 }, { 1883, -4304, 260, 0 }, { -1890, 4304, -267, 0 }, { 1883, 4304, 260, 0 } }, { 566, 0, -4060, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1196, -2400, 6326, 0 }, { { -23, -4304, -1879, 0 }, { 7, -4304, 1860, 0 }, { -23, 4304, -1879, 0 }, { 7, 4304, 1860, 0 } }, { 4099, 0, -34, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1156, -2401, 6284, 0 }, { { 6, -4304, 1882, 0 }, { -40, -4304, -1915, 0 }, { 6, 4304, 1882, 0 }, { -40, 4304, -1915, 0 } }, { -4103, 0, 49, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7553, -2369, 6143, 0 }, { { 169, -4304, 1881, 0 }, { -168, -4304, -1880, 0 }, { 169, 4304, 1881, 0 }, { -168, 4304, -1880, 0 } }, { -4082, 0, 365, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7841, -2368, 6175, 0 }, { { -199, -4304, -1861, 0 }, { 199, -4304, 1861, 0 }, { -199, 4304, -1861, 0 }, { 199, 4304, 1861, 0 } }, { 4073, 0, -436, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4160, -2177, 3264, 0 }, { { 2163, -4304, 302, 0 }, { -2177, -4304, -310, 0 }, { 2163, 4304, 302, 0 }, { -2177, 4304, -310, 0 } }, { -576, 0, 4077, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4175, -2209, 3376, 0 }, { { -2206, -4304, -326, 0 }, { 2200, -4304, 319, 0 }, { -2206, 4304, -326, 0 }, { 2200, 4304, 319, 0 } }, { 594, 0, -4065, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3983, -2368, 6863, 0 }, { { 2098, -4304, 7, 0 }, { -2100, -4304, -8, 0 }, { 2098, 4304, 7, 0 }, { -2100, 4304, -8, 0 } }, { -15, 0, 4101, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 10, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3776, -2369, 6912, 0 }, { { -1876, -4304, 12, 0 }, { 1868, -4304, -20, 0 }, { -1876, 4304, 12, 0 }, { 1868, 4304, -20, 0 } }, { -36, 0, -4098, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 9, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3968, -2369, 9760, 0 }, { { 74, -4304, -1872, 0 }, { -79, -4304, 1868, 0 }, { 74, 4304, -1872, 0 }, { -79, 4304, 1868, 0 } }, { 4092, 0, 167, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 10, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3777, -2336, 9791, 0 }, { { -19, -4304, 1886, 0 }, { 13, -4304, -1891, 0 }, { -19, 4304, 1886, 0 }, { 13, 4304, -1891, 0 } }, { -4099, 0, -36, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 11, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1095, -64, 543, 0 }, { { 2166, -4304, 328, 0 }, { -2168, -4304, -330, 0 }, { 2166, 4304, 328, 0 }, { -2168, 4304, -330, 0 } }, { -619, 0, 4072, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1077, 0, 734, 0 }, { { -2173, -4304, -334, 0 }, { 2163, -4304, 324, 0 }, { -2173, 4304, -334, 0 }, { 2163, 4304, 324, 0 } }, { 614, 0, -4050, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9793, -6208, 6912, 0 }, { { -1855, -4304, 352, 0 }, { 1855, -4304, -352, 0 }, { -1855, 4304, 352, 0 }, { 1855, 4304, -352, 0 } }, { -765, 0, -4027, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 12, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9825, -6240, 6816, 0 }, { { 1855, -4304, -352, 0 }, { -1855, -4304, 352, 0 }, { 1855, 4304, -352, 0 }, { -1855, 4304, 352, 0 } }, { 763, 0, 4025, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 11, 12, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b4_upper_sewer_801886F4[8] = {
    { NULL, NULL, NULL, { 3744, -2096, 9024, 0 }, { { -448, 0, -736, 0 }, { 448, 0, -736, 0 }, { -448, 0, 736, 0 }, { 448, 0, 736, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 861, WORLD_COLLISION_TRIGGER_ACTION_WARP, 46, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3712, -48, -3008, 0 }, { { -448, 0, -464, 0 }, { 448, 0, -464, 0 }, { -448, 0, 464, 0 }, { 448, 0, 464, 0 } }, { 0, 4107, 0, 0 }, { -4096, 0, 0, 0 }, 643, WORLD_COLLISION_TRIGGER_ACTION_WARP, 45, 34, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -9408, -48, 6496, 0 }, { { -448, 0, -1008, 0 }, { 448, 0, -1008, 0 }, { -448, 0, 1008, 0 }, { 448, 0, 1008, 0 } }, { 0, 4105, 0, 0 }, { 4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, 43, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -9024, -2080, 5632, 0 }, { { 1008, 0, -448, 0 }, { 1008, 0, 448, 0 }, { -1008, 0, -448, 0 }, { -1008, 0, 448, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, 43, 68, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3344, -2080, -384, 0 }, { { -528, 0, -1008, 0 }, { 528, 0, -1008, 0 }, { -528, 0, 1008, 0 }, { 528, 0, 1008, 0 } }, { 0, 4119, 0, 0 }, { -4096, 0, 0, 0 }, 1137, WORLD_COLLISION_TRIGGER_ACTION_CAP | WORLD_COLLISION_TRIGGER_OUTSIDE_BATTLE, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 208, -64, -5663, 0 }, { { -944, 0, -1008, 0 }, { 944, 0, -1008, 0 }, { -944, 0, 1008, 0 }, { 944, 0, 1008, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 1378, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -9568, -2080, 6400, 0 }, { { -448, 0, -656, 0 }, { 448, 0, -656, 0 }, { -448, 0, 656, 0 }, { 448, 0, 656, 0 } }, { 0, 4106, 0, 0 }, { 4096, 0, 0, 0 }, 794, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1568, -64, 3696, 0 }, { { -448, 0, -864, 0 }, { 448, 0, -864, 0 }, { -448, 0, 864, 0 }, { 448, 0, 864, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 972, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b4_upper_sewer_80188954[3] = {
    { 70, 70, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_107000_8013F5F0 },
    { 71, 71, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_107000_80139E60 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b4_upper_sewer_80188978[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b4_upper_sewer_80188990[3] = {
    { 24, 24, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102400_8013647C },
    { 4, 4, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_200400_8015FE48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b4_upper_sewer_801889B4[3] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { 49, 49, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_201100_8015F400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b4_upper_sewer_801889D8[2] = {
    { 23, 23, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102300_80147AB8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b4_upper_sewer_801889F0[2] = {
    { 23, 23, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102300_80147AB8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b4_upper_sewer_80188A08[3] = {
    { 3, 3, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_100300_80148110 },
    { 4, 4, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_200400_8015FE48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
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

AreaVariant D_shelter_b4_upper_sewer_80188B9C[12] = {
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

WorldCollisionOccluder D_shelter_b4_upper_sewer_80188BFC[3] = {
    { NULL, NULL, { -4928, -3088, 208, 0 }, { { -3680, 4656, -4624, 0 }, { 3680, 4656, 4624, 0 }, { -3680, -4656, -4624, 0 }, { 3680, -4656, 4624, 0 } }, { -3206, 0, 2551, 0 }, 7507, 1, 0 },
    { NULL, NULL, { 4464, -176, 2784, 0 }, { { -3216, 1920, -4624, 0 }, { 3216, 1920, 4624, 0 }, { -3216, -1920, -4624, 0 }, { 3216, -1920, 4624, 0 } }, { -3368, 0, 2342, 0 }, 5948, 1, 0 },
    { NULL, NULL, { -9041, -1824, 6399, 0 }, { { -1056, 8, -992, 0 }, { -1056, -7, 993, 0 }, { 1056, 8, -992, 0 }, { 1056, -7, 993, 0 } }, { 0, -4096, -32, 0 }, 1448, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionFootstepSounds D_shelter_b4_upper_sewer_80188CB0 = {
    0x10000041,
    0x10000043,
    0x10000041,
};

WorldCollisionFootstepSounds D_shelter_b4_upper_sewer_80188CBC = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

WorldCollisionFootstepSounds D_shelter_b4_upper_sewer_80188CC8 = {
    0x10000015,
    0x10000017,
    0x10000015,
};

WorldCollisionSurfaceProperties D_shelter_b4_upper_sewer_80188CD4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b4_upper_sewer_80188CDC[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b4_upper_sewer_80188CE4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b4_upper_sewer_80188CBC },
};

WorldCollisionSurfaceProperties D_shelter_b4_upper_sewer_80188CEC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b4_upper_sewer_80188CC8 },
};

WorldCollisionSurfaceProperties D_shelter_b4_upper_sewer_80188CF4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b4_upper_sewer_80188CB0 },
};

WorldCollisionSurfaceProperties* D_shelter_b4_upper_sewer_80188CFC[8] = {
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
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_shelter_b4_upper_sewer_80188D2C[0];
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
                Gp_MsgAllyWeapon(1);
                Gp_MsgAlly3F3(1);
                gGameSession->eventState       = 0;
                gGameSession->hideHud          = 0;
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                D_80114D08                     = 0xA;
                break;
            }
            func_800E8614(D_shelter_b4_upper_sewer_80186318, 0);
            gameFlagSetNibble(GAME_FLAG_WATER_HOLE_SHELTER_ROUTE_OPEN, 1);
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_WATER, 0);
            task->state++;
            break;
        case 4:
            if (gGameSession->eventState == 0) {
                D_80114D08                     = 0xA;
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(task);
            }
            break;
    }
}

void func_shelter_b4_upper_sewer_8017D80C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
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
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                D_80114D08                     = 0xA;
                break;
            }
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            Gp_TriggerPeIfArmed();
            D_shelter_b4_upper_sewer_80188D1C.fade.blend      = SCREEN_FADE_SUBTRACT;
            D_shelter_b4_upper_sewer_80188D1C.fade.phase      = SCREEN_FADE_RUNNING;
            D_shelter_b4_upper_sewer_80188D1C.fade.rampFrames = 0x1E;
            Task_Spawn(1, 0x31, 0, &D_shelter_b4_upper_sewer_80188D1C.fade);
            arg0->killCountdown = 0x1E;
            arg0->state++;
            break;
        case 3:
            if (--arg0->killCountdown == 0) {
                SndEvt_EnqueueType6(SOUND_SHELTER_B4_UPPER_SEWER_EXIT_TRANSIT, 0, 0);
                arg0->state++;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(SOUND_SHELTER_B4_UPPER_SEWER_EXIT_TRANSIT) == 0) {
                arg0->state++;
            }
            break;
        case 5:
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = D_shelter_b4_upper_sewer_80188D24.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_shelter_b4_upper_sewer_80188D24.field_4;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ((u8*)&D_shelter_b4_upper_sewer_80188D24.areaId)[1];
            Task_Spawn(0, 0x11, 0x10, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b4_upper_sewer_8017D9BC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b4_upper_sewer_8017D9C4(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    func_map_shelter_80179A04(src, dst);
    if (src->areaId == GAME_AREA_SHELTER_B4_RESERVOIR) {
        if (src->queryOnly == ROOM_EVENT_EXECUTE) {
            D_shelter_b4_upper_sewer_80188D24.warp              = (u8)dst->areaId;
            D_shelter_b4_upper_sewer_80188D24.field_4           = dst->warp;
            ((u8*)&D_shelter_b4_upper_sewer_80188D24.areaId)[1] = dst->room;
            Task_SpawnFromTable(D_shelter_b4_upper_sewer_80186300, 1, 7, 0);
        }
        return 0;
    }
    if (src->areaId == GAME_AREA_SHELTER_B4_WATER_SUPPLY) {
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

s32 func_shelter_b4_upper_sewer_8017DAB0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    u8 temp_a1;

    if (arg2 == 1) {
        if (gameFlagGetNibble(GAME_FLAG_WATER_HOLE_SHELTER_ROUTE_OPEN) == 0) {
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
            Gp_MsgPlayerWeapon(0);
            Gp_MsgAllyWeapon(0);
            gSceneCombatState.actorControl                             = SCENE_COMBAT_ACTORS_HIDDEN;
            temp_a1                                                    = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0xD;
            D_shelter_b4_upper_sewer_80188D2C[0]                       = temp_a1;
            Task_SpawnFromTable(D_shelter_b4_upper_sewer_80186300, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(6);
        }
    }
    return 0;
}

s32 func_shelter_b4_upper_sewer_8017DB50(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b4_upper_sewer_8017DB58(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 4) {
        func_shelter_b4_upper_sewer_8017E59C(1);
        SndEvt_EnqueueType6(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, 4), 0, 0);
    }
    return 0;
}

void func_shelter_b4_upper_sewer_8017DB94(void)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_shelter_b4_upper_sewer_80188D2C[0];
}

static void func_shelter_b4_upper_sewer_8017DBA8(Task* task)
{
    task->msgTable = D_shelter_b4_upper_sewer_801862D0;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) != 0) {
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

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
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

/// Draws each surface in the list at `surface`, up to its terminator, at
/// height `y` as a strip of `segmentCount` flat semi-transparent quads
/// coloured (0, `c` / 4, `c`), projected through the view matrix and each
/// followed by a draw-mode packet selecting blend mode 2.
/// Quads are linked 0x60 deeper in the ordering table than their projected
/// depth, and those the projection flags as invalid are skipped. The
/// per-surface values live in a work block pushed on the scratchpad stack for
/// the duration of the call. `task` is unused.
static void func_shelter_b4_upper_sewer_8017DD98(Task* task, _ShelterB4UpperSewerWaterSurface* surface, s16 y, u8 c)
{
    SVECTOR           v0, v1, v2, v3;
    long              sxy0, sxy1, sxy2, sxy3;
    long              p, flag;
    WaterQuadScratch* scratch;
    WaterQuadScratch* scratchEnd;
    POLY_F4*          poly;
    DR_MODE*          dr;
    s32               otz;
    s32               i;

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    scratchEnd                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    // One scratch reservation holds the values reused across the surface list.
    SCRATCH_STACK_CURSOR(WaterQuadScratch) = scratchEnd - 1;
    scratch                                = scratchEnd - 1;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    scratch->y = y;
    for (; surface->segmentCount != WATER_SURFACE_LIST_END; surface++) {
        if (surface->segmentsAlongZ == 0) {
            scratch->dx = surface->width / surface->segmentCount;
            scratch->dz = surface->depth;
            scratch->x  = surface->x;
            scratch->z  = surface->z;
            for (i = 0; i < surface->segmentCount; i++) {
                v0.vx            = scratch->x + scratch->dx * i;
                v0.vy            = scratch->y;
                v0.vz            = scratch->z;
                v1.vx            = scratch->x + scratch->dx * (i + 1);
                v1.vy            = scratch->y;
                v1.vz            = scratch->z;
                scratch->yOffset = 0;
                v2.vx            = scratch->x + scratch->dx * i;
                v2.vy            = scratch->y + scratch->yOffset;
                v2.vz            = scratch->z + scratch->dz;
                scratch->yOffset = 0;
                v3.vx            = scratch->x + scratch->dx * (i + 1);
                v3.vy            = scratch->y + scratch->yOffset;
                v3.vz            = scratch->z + scratch->dz;
                otz              = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
                if (flag >= 0) {
                    poly                              = (POLY_F4*)D_shelter_b4_upper_sewer_80188D30;
                    D_shelter_b4_upper_sewer_80188D30 = (u8*)(poly + 1);
                    setlen(poly, 5);
                    setcode(poly, 0x2A);
                    GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                    GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                    GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                    GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                    poly->r0                       = 0;
                    poly->g0                       = c >> 2;
                    poly->b0                       = c;
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
            scratch->dx = surface->width;
            scratch->dz = surface->depth / surface->segmentCount;
            scratch->x  = surface->x;
            scratch->z  = surface->z;
            for (i = 0; i < surface->segmentCount; i++) {
                v0.vx            = scratch->x;
                v0.vy            = scratch->y;
                v0.vz            = scratch->z + scratch->dz * i;
                v1.vx            = scratch->x;
                v1.vy            = scratch->y;
                v1.vz            = scratch->z + scratch->dz * (i + 1);
                scratch->yOffset = 0;
                v2.vx            = scratch->x + scratch->dx;
                v2.vy            = scratch->y + scratch->yOffset;
                v2.vz            = scratch->z + scratch->dz * i;
                scratch->yOffset = 0;
                v3.vx            = scratch->x + scratch->dx;
                v3.vy            = scratch->y + scratch->yOffset;
                v3.vz            = scratch->z + scratch->dz * (i + 1);
                otz              = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
                if (flag >= 0) {
                    poly                              = (POLY_F4*)D_shelter_b4_upper_sewer_80188D30;
                    D_shelter_b4_upper_sewer_80188D30 = (u8*)(poly + 1);
                    setlen(poly, 5);
                    setcode(poly, 0x2A);
                    GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                    GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                    GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                    GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                    poly->r0                       = 0;
                    poly->g0                       = c >> 2;
                    poly->b0                       = c;
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
    SCRATCH_STACK_RELEASE_BLOCK(WaterQuadScratch);
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
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
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

    batches = Gp_SprtTables[sess->stage - 1][0].areaViews[sess->area - 1][12].batches;
    if ((arg0 & 0xFF) == 0) {
        batches[1].hidden = 0;
    } else {
        batches[1].hidden = 1;
    }
}

/// Publishes the sewer's effect ids on the task's first tick - two extra ids
/// only while `gameFlagGetNibble(0xB7)` is 1 - then draws the
/// `glowDrawCapsule` capsules the current camera view
/// shows. Views 4 and 13 both end on
/// `D_shelter_b4_upper_sewer_801864F0[12]`, and writing that address off the
/// array (rather than through its own symbol) is what keeps view 4's array
/// base live across the first call while the shared tail is cross-jumped.
void func_shelter_b4_upper_sewer_8017E5F8(Task* arg0)
{
    if (arg0->state == 0) {
        if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) == 1) {
            gRoomEffectWaterRippleId = EFFECT_SHELTER_B4_UPPER_SEWER_WATER_RIPPLE;
            gRoomEffectWaterSprayId  = EFFECT_SHELTER_B4_UPPER_SEWER_WATER_SPRAY;
        }
        gRoomEffectMoteId         = EFFECT_SHELTER_B4_UPPER_SEWER_MOTE;
        gRoomEffectHaloId         = EFFECT_SHELTER_B4_UPPER_SEWER_HALO;
        gRoomEffectOrangeBurstId  = EFFECT_SHELTER_B4_UPPER_SEWER_ORANGE_BURST;
        gRoomEffectSparkEmitterId = EFFECT_SHELTER_B4_UPPER_SEWER_SPARK_EMITTER;
        gRoomEffectFlashId        = EFFECT_SHELTER_B4_UPPER_SEWER_FLASH;
        gRoomEffectTwinTrailId    = EFFECT_SHELTER_B4_UPPER_SEWER_TWIN_TRAIL;
        gRoomEffectSparkBurstId   = EFFECT_SHELTER_B4_UPPER_SEWER_SPARK_BURST;
        gRoomEffectGlowDiscId     = EFFECT_SHELTER_B4_UPPER_SEWER_GLOW_DISC;
        gRoomEffectFlyingSparkId  = EFFECT_SHELTER_B4_UPPER_SEWER_FLYING_SPARK;
        gRoomEffectOrangeBurst2Id = EFFECT_SHELTER_B4_UPPER_SEWER_ORANGE_BURST_2;
        arg0->state               = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 3:
            glowDrawCapsule(D_shelter_b4_upper_sewer_801864F0, 0x200, 0x444);
            break;
        case 4:
            glowDrawCapsule(&D_shelter_b4_upper_sewer_801864F0[0], 0x200, 0x222);
            glowDrawCapsule(&D_shelter_b4_upper_sewer_801864F0[12], 0x200, 0x124);
            break;
        case 8: {
            SVECTOR* p = D_shelter_b4_upper_sewer_801864D0;
            glowDrawCapsule(&p[0], 0x200, 0x444);
            glowDrawCapsule(&p[2], 0x200, 0x444);
            glowDrawCapsule(&p[4], 0x200, 0x222);
            glowDrawCapsule(&p[6], 0x200, 0x444);
            glowDrawCapsule(&p[16], 0x200, 0x124);
            break;
        }
        case 9: {
            SVECTOR* p = D_shelter_b4_upper_sewer_801864D0;
            glowDrawCapsule(&p[0], 0x200, 0x444);
            glowDrawCapsule(&p[2], 0x200, 0x444);
            glowDrawCapsule(&p[4], 0x200, 0x222);
            glowDrawCapsule(&p[6], 0x200, 0x444);
            glowDrawCapsule(&p[16], 0x200, 0x124);
            break;
        }
        case 10: {
            SVECTOR* p = D_shelter_b4_upper_sewer_801864B0;
            glowDrawCapsule(&p[0], 0x200, 0x444);
            glowDrawCapsule(&p[2], 0x200, 0x444);
            glowDrawCapsule(&p[18], 0x200, 0x343);
            break;
        }
        case 11: {
            SVECTOR* p = D_shelter_b4_upper_sewer_80186490;
            glowDrawCapsule(&p[0], 0x200, 0x444);
            glowDrawCapsule(&p[6], 0x200, 0x444);
            glowDrawCapsule(&p[18], 0x200, 0x444);
            glowDrawCapsule(&p[20], 0x200, 0x343);
            break;
        }
        case 12:
            glowDrawCapsule(D_shelter_b4_upper_sewer_80186520, 0x200, 0x444);
            break;
        case 13:
            glowDrawCapsule(&D_shelter_b4_upper_sewer_801864F0[12], 0x200, 0x124);
            break;
    }
}

#include "../../shared/water_ripple_task.inc.c"

void func_shelter_b4_upper_sewer_8017E8B8(Task* task)
{
    waterRippleTask(task);
}

#include "../../shared/water_splash.inc.c"

#include "../../shared/water_drift_task.inc.c"

void func_shelter_b4_upper_sewer_8017ED40(Task* task)
{
    waterDriftTask(task);
}

#include "../../shared/water_spin.inc.c"

#include "../../shared/water_tile.inc.c"

#include "../../shared/glow_draw_capsule.inc.c"

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

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void func_shelter_b4_upper_sewer_80182600(Task* arg0)
{
    RoomFx_SparkEmitterTask(arg0);
}

#include "../../shared/room_visual_effects_flash_task.inc.c"

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
#include "../../shared/room_visual_effects_flying_tasks.inc.c"

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
