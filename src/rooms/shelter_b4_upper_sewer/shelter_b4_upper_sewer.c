#include "rooms/shelter_b4_upper_sewer.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/companion_load.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/player_actor.h"
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

static void _waterDrawSpin(const GfxCoord* coord, s16 textureColumn, s16 radiusScale, s16 spinAngle);
static void _waterDrawTile(const GfxCoord* coord, s16 frameIndex, s16 radiusScale);

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
/// with `taskSpawn(1, 0x31, ...)`.
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
static void _shelterB4UpperSewerIdleRoom(Task* task);
static void _shelterB4UpperSewerDrawWaterSurfaces(Task* task, const _ShelterB4UpperSewerWaterSurface* surface, s16 waterY, u8 blueIntensity);
static void _shelterB4UpperSewerInitializeWater(Task* task);
static void _shelterB4UpperSewerSetView13SpriteHidden(s32 hidden);
#include "../../shared/room_visual_effects.h"

static RoomFxShade _gRoomEffectHaloShades[3];

/// State handlers of the task `shelterB4UpperSewerRoomTask` runs,
/// which copies the table to the stack and calls the entry for the task's
/// state: the room's setup (message table, pointer slot, water level), an idle
/// state, and `taskKill`.
static const TaskFuncTable3 D_shelter_b4_upper_sewer_8017D5C4 = {
    { func_shelter_b4_upper_sewer_8017DBA8, _shelterB4UpperSewerIdleRoom, taskKill }
};

static void _shelterB4UpperSewerWaterTask(Task* task);

static s32  _shelterB4UpperSewerRejectKeyItemMessage(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg);
s32         func_shelter_b4_upper_sewer_8017D9C4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32         func_shelter_b4_upper_sewer_8017DAB0(Task*, s32, s32, s32);
static s32  _shelterB4UpperSewerIgnoreRoomActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static s32  _shelterB4UpperSewerHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unusedArg);
void        func_shelter_b4_upper_sewer_8017D660(Task*);
void        func_shelter_b4_upper_sewer_8017D80C(Task*);
static void _shelterB4UpperSewerRestoreSavedView(void);

extern TaskDesc D_actor_100400_80147E48;

enum { SHELTER_B4_UPPER_SEWER_MESSAGE_USE_KEY_ITEM = 0x13F1 };

TaskMessageEntry D_shelter_b4_upper_sewer_801862D0[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b4_upper_sewer_8017D9C4 },
    { SHELTER_B4_UPPER_SEWER_MESSAGE_USE_KEY_ITEM, _shelterB4UpperSewerRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB4UpperSewerIgnoreRoomActionMessage },
    { ROOM_MESSAGE_COMMAND, func_shelter_b4_upper_sewer_8017DAB0 },
    { ROOM_MESSAGE_SOUND, _shelterB4UpperSewerHandleSoundCue },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _shelterB4UpperSewerRestoreSavedView }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

s16 D_shelter_b4_upper_sewer_80186438 = -1600;

TaskDesc D_shelter_b4_upper_sewer_8018643C[1] = {
    { { { TASK_BODY_NONE, 96 } }, _shelterB4UpperSewerWaterTask, { .value = 0 } },
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

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
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

static void _shelterB4UpperSewerUpdateWater(Task* task);

void func_shelter_b4_upper_sewer_8017D660(Task* task)
{
    switch (task->state) {
        case 0:
            _shelterB4UpperSewerSetView13SpriteHidden(0);
            gGameSession->eventState = 1;
            task->state++;
            break;
        case 1:
            gGameSession->hideHud = 1;
            capRunCommand(1, CAP_PLAYBACK_IN_PLACE);
            D_80115690 = 1;
            D_80115680 = 5;
            task->state++;
            break;
        case 2:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case 3:
            if (capGetVariantKey() == 0xC) {
                taskKill(task);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_shelter_b4_upper_sewer_80188D2C[0];
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
                companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                companionSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
                gGameSession->eventState       = 0;
                gGameSession->hideHud          = 0;
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                D_80114D08                     = 0xA;
                break;
            }
            evsStartScript(D_shelter_b4_upper_sewer_80186318, EVENT_SCRIPT_HUD_HIDE_RESTORE);
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
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommand(arg0->spawnArg1.value, CAP_PLAYBACK_IN_PLACE);
            arg0->state++;
            break;
        case 1:
            if (capIsBusy() == 0) {
                arg0->state++;
            }
            break;
        case 2:
            if (capGetVariantKey() != 0xA) {
                taskKill(arg0);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                D_80114D08                     = 0xA;
                break;
            }
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            sceneQueueBattleEscapeResult();
            D_shelter_b4_upper_sewer_80188D1C.fade.blend      = SCREEN_FADE_SUBTRACT;
            D_shelter_b4_upper_sewer_80188D1C.fade.phase      = SCREEN_FADE_RUNNING;
            D_shelter_b4_upper_sewer_80188D1C.fade.rampFrames = 0x1E;
            taskSpawn(1, 0x31, 0, &D_shelter_b4_upper_sewer_80188D1C.fade);
            arg0->killCountdown = 0x1E;
            arg0->state++;
            break;
        case 3:
            if (--arg0->killCountdown == 0) {
                sndEvtRequestScriptStart(SOUND_SHELTER_B4_UPPER_SEWER_EXIT_TRANSIT, 0, 0);
                arg0->state++;
            }
            break;
        case 4:
            if (sndScriptHasActiveId(SOUND_SHELTER_B4_UPPER_SEWER_EXIT_TRANSIT) == 0) {
                arg0->state++;
            }
            break;
        case 5:
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = D_shelter_b4_upper_sewer_80188D24.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_shelter_b4_upper_sewer_80188D24.field_4;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ((u8*)&D_shelter_b4_upper_sewer_80188D24.areaId)[1];
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_SKIP_BATTLE_ESCAPE, 0);
            taskKill(arg0);
            break;
    }
}

/// Refuses key-item use in this room without consuming the item.
///
/// All arguments are ignored. The zero result selects the item menu's refusal notice.
static s32 _shelterB4UpperSewerRejectKeyItemMessage(Task* task, s32 messageId, s32 keyItemId, s32 unusedArg)
{
    enum { SHELTER_B4_UPPER_SEWER_KEY_ITEM_REFUSED = 0 };

    return SHELTER_B4_UPPER_SEWER_KEY_ITEM_REFUSED;
}

s32 func_shelter_b4_upper_sewer_8017D9C4(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    mapShelterRoomVariantResolve(src, dst);
    if (src->areaId == GAME_AREA_SHELTER_B4_RESERVOIR) {
        if (src->queryOnly == ROOM_EVENT_EXECUTE) {
            D_shelter_b4_upper_sewer_80188D24.warp              = (u8)dst->areaId;
            D_shelter_b4_upper_sewer_80188D24.field_4           = dst->warp;
            ((u8*)&D_shelter_b4_upper_sewer_80188D24.areaId)[1] = dst->room;
            taskSpawnFromTable(D_shelter_b4_upper_sewer_80186300, 1, 7, 0);
        }
        return 0;
    }
    if (src->areaId == GAME_AREA_SHELTER_B4_WATER_SUPPLY) {
        if (src->queryOnly == ROOM_EVENT_EXECUTE) {
            D_shelter_b4_upper_sewer_80188D24.warp              = (u8)dst->areaId;
            D_shelter_b4_upper_sewer_80188D24.field_4           = dst->warp;
            ((u8*)&D_shelter_b4_upper_sewer_80188D24.areaId)[1] = dst->room;
            taskSpawnFromTable(D_shelter_b4_upper_sewer_80186300, 1, 8, 0);
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
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            companionSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            gSceneCombatState.actorControl                             = SCENE_COMBAT_ACTORS_HIDDEN;
            temp_a1                                                    = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0xD;
            D_shelter_b4_upper_sewer_80188D2C[0]                       = temp_a1;
            taskSpawnFromTable(D_shelter_b4_upper_sewer_80186300, 0, 0, 0);
        } else {
            capRunCommandWithTransition(6);
        }
    }
    return 0;
}

/// Ignores the room action supplied by a direction trigger and returns zero.
///
/// All arguments are unused; the borrowed request is neither read nor retained.
static s32 _shelterB4UpperSewerIgnoreRoomActionMessage(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Hides view 13's switchable sprite and starts the room sound for cue 4.
///
/// Handles `ROOM_MESSAGE_SOUND`; other cue IDs have no effect. Requires this
/// room's loaded sprite directory and sound bank. The receiver, message ID and
/// second payload are unused, and every cue returns zero.
static s32 _shelterB4UpperSewerHandleSoundCue(Task* task, s32 messageId, s32 cueId, s32 unusedArg)
{
    enum {
        SHELTER_B4_UPPER_SEWER_SOUND_CUE_HIDE_SPRITE = 4,
        SHELTER_B4_UPPER_SEWER_HIDE_SPRITE_SOUND     = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B4_UPPER_SEWER, 4)
    };

    if (cueId == SHELTER_B4_UPPER_SEWER_SOUND_CUE_HIDE_SPRITE) {
        _shelterB4UpperSewerSetView13SpriteHidden(1);
        sndEvtRequestScriptStart(SHELTER_B4_UPPER_SEWER_HIDE_SPRITE_SOUND, 0, 0);
    }
    return 0;
}

/// Restores the live save's view after the water-hole access scene's temporary view.
///
/// Requires the outgoing view captured before that scene starts. This event-script
/// callback changes only the saved view selector; it does not load a camera.
static void _shelterB4UpperSewerRestoreSavedView(void)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_shelter_b4_upper_sewer_80188D2C[0];
}

static void func_shelter_b4_upper_sewer_8017DBA8(Task* task)
{
    task->msgTable = D_shelter_b4_upper_sewer_801862D0;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) != 0) {
        D_shelter_b4_upper_sewer_80186438 = -0x708;
        taskSpawnFromTable(D_shelter_b4_upper_sewer_8018643C, 0, 0, 0);
    } else {
        D_shelter_b4_upper_sewer_80186438 = 0;
    }
    task->state = (s32)(task->state + 1);
}

/// Keeps the initialized room task available for messages without per-frame work.
///
/// This is room state 1; the task is unused and its state is left intact.
static void _shelterB4UpperSewerIdleRoom(Task* task)
{
}

void shelterB4UpperSewerRoomTask(Task* task)
{
    TaskFuncTable3 handlers = D_shelter_b4_upper_sewer_8017D5C4;

    handlers.funcs[task->state](task);
}

/// Clamps and publishes the water height, then draws the current view's flat water.
///
/// Height is in world units, limited to -1600..0. Blue intensity is
/// (-height * 16) / 225. The current display half of an unused actor-load
/// buffer supplies packet storage; view 12 omits the first water rectangle.
static void _shelterB4UpperSewerUpdateWater(Task* task)
{
    enum {
        SHELTER_B4_UPPER_SEWER_WATER_PACKET_HALF_BYTES     = 0xC000,
        SHELTER_B4_UPPER_SEWER_WATER_MIN_Y                 = -1600,
        SHELTER_B4_UPPER_SEWER_WATER_MAX_Y                 = 0,
        SHELTER_B4_UPPER_SEWER_WATER_VIEW_SKIP_FIRST_PATCH = 12
    };
    s16 clampedWaterY;
    u8  blueIntensity;
    s32 waterY;

    // Borrow the unused actor-load buffer, with one packet area per display buffer.
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        D_shelter_b4_upper_sewer_80188D30 = (u8*)Fs_ActorLoadBase2 + gDisplayState.otBuffer * SHELTER_B4_UPPER_SEWER_WATER_PACKET_HALF_BYTES;
    } else {
        D_shelter_b4_upper_sewer_80188D30 = (u8*)Fs_ActorLoadBase1 + gDisplayState.otBuffer * SHELTER_B4_UPPER_SEWER_WATER_PACKET_HALF_BYTES;
    }
    if (D_shelter_b4_upper_sewer_80186438 < SHELTER_B4_UPPER_SEWER_WATER_MIN_Y) {
        D_shelter_b4_upper_sewer_80186438 = SHELTER_B4_UPPER_SEWER_WATER_MIN_Y;
    } else if (D_shelter_b4_upper_sewer_80186438 > SHELTER_B4_UPPER_SEWER_WATER_MAX_Y) {
        D_shelter_b4_upper_sewer_80186438 = SHELTER_B4_UPPER_SEWER_WATER_MAX_Y;
    }
    clampedWaterY        = D_shelter_b4_upper_sewer_80186438;
    gGameSession->waterY = clampedWaterY;
    waterY               = clampedWaterY;
    blueIntensity        = (-waterY * 16) / 225;
    if (gGameSession->location.loc.view != SHELTER_B4_UPPER_SEWER_WATER_VIEW_SKIP_FIRST_PATCH) {
        _shelterB4UpperSewerDrawWaterSurfaces(task, D_shelter_b4_upper_sewer_80186448, waterY, blueIntensity);
    } else {
        _shelterB4UpperSewerDrawWaterSurfaces(task, D_shelter_b4_upper_sewer_80186454, waterY, blueIntensity);
    }
}

/// Draws a terminated list of flat water rectangles at a shared world-unit height.
///
/// Borrows the list through `WATER_SURFACE_LIST_END`; every drawable entry
/// needs a positive segment count. Each valid projected strip quad has RGB
/// (0, blueIntensity / 4, blueIntensity), subtractive blending and an ordering
/// depth bias of 96. The byte cursor must have aligned storage for one POLY_F4
/// and one DR_MODE per drawn quad (36 bytes, sending only its first command).
/// Scratch storage is released before returning. `task` is unused, retained in the matched call interface.
static void _shelterB4UpperSewerDrawWaterSurfaces(Task* task, const _ShelterB4UpperSewerWaterSurface* surface, s16 waterY, u8 blueIntensity)
{
    enum {
        SHELTER_B4_UPPER_SEWER_WATER_DEPTH_BIAS      = 96,
        SHELTER_B4_UPPER_SEWER_WATER_QUAD_CODE       = 0x2A,
        SHELTER_B4_UPPER_SEWER_WATER_DRAW_MODE_WORDS = 1,
        // GPU draw-mode opcode, subtractive blend and retained texture page 10.
        SHELTER_B4_UPPER_SEWER_WATER_DRAW_MODE = 0xE1000000 | (GPU_BLEND_SUBTRACT << 5) | 10
    };

    /// Queues one flat water quad followed by its subtractive draw-mode command.
    ///
    /// Captures screenXY0..3, orderingDepth, blueIntensity, waterQuad and drawMode.
    /// Uses the aligned byte cursor for 36 bytes of frame storage, retaining the
    /// DR_MODE's unused second slot while sending one word. Call as a statement
    /// after a valid projection inside braces; expands to a statement list that
    /// updates the cursor and links both packets, with no additional control flow.
#define SHELTER_B4_UPPER_SEWER_EMIT_WATER_QUAD()                                                                                                                                                   \
    waterQuad                         = (POLY_F4*)D_shelter_b4_upper_sewer_80188D30;                                                                                                               \
    D_shelter_b4_upper_sewer_80188D30 = (u8*)(waterQuad + 1);                                                                                                                                      \
    setlen(waterQuad, sizeof(*waterQuad) / sizeof(u32) - 1);                                                                                                                                       \
    setcode(waterQuad, SHELTER_B4_UPPER_SEWER_WATER_QUAD_CODE);                                                                                                                                    \
    GPU_PRIMITIVE_XY_WORD(waterQuad, 0) = screenXY0;                                                                                                                                               \
    GPU_PRIMITIVE_XY_WORD(waterQuad, 1) = screenXY1;                                                                                                                                               \
    GPU_PRIMITIVE_XY_WORD(waterQuad, 2) = screenXY2;                                                                                                                                               \
    GPU_PRIMITIVE_XY_WORD(waterQuad, 3) = screenXY3;                                                                                                                                               \
    waterQuad->r0                       = 0;                                                                                                                                                       \
    waterQuad->g0                       = blueIntensity >> 2;                                                                                                                                      \
    waterQuad->b0                       = blueIntensity;                                                                                                                                           \
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(orderingDepth + SHELTER_B4_UPPER_SEWER_WATER_DEPTH_BIAS) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), \
            waterQuad);                                                                                                                                                                            \
    drawMode                          = (DR_MODE*)D_shelter_b4_upper_sewer_80188D30;                                                                                                               \
    D_shelter_b4_upper_sewer_80188D30 = (u8*)(drawMode + 1);                                                                                                                                       \
    setlen(drawMode, SHELTER_B4_UPPER_SEWER_WATER_DRAW_MODE_WORDS);                                                                                                                                \
    drawMode->code[0] = SHELTER_B4_UPPER_SEWER_WATER_DRAW_MODE;                                                                                                                                    \
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(orderingDepth + SHELTER_B4_UPPER_SEWER_WATER_DEPTH_BIAS) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), \
            drawMode);

    SVECTOR           corner0, corner1, corner2, corner3;
    long              screenXY0, screenXY1, screenXY2, screenXY3;
    long              depthCue, projectionFlags;
    WaterQuadScratch* scratch;
    WaterQuadScratch* scratchEnd;
    POLY_F4*          waterQuad;
    DR_MODE*          drawMode;
    s32               orderingDepth;
    s32               segmentIndex;

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    scratchEnd                 = SCRATCH_STACK_CURSOR(WaterQuadScratch);
    // One scratch reservation holds the values reused across the surface list.
    SCRATCH_STACK_CURSOR(WaterQuadScratch) = scratchEnd - 1;
    scratch                                = scratchEnd - 1;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    scratch->y = waterY;
    for (; surface->segmentCount != WATER_SURFACE_LIST_END; surface++) {
        if (surface->segmentsAlongZ == 0) {
            scratch->dx = surface->width / surface->segmentCount;
            scratch->dz = surface->depth;
            scratch->x  = surface->x;
            scratch->z  = surface->z;
            for (segmentIndex = 0; segmentIndex < surface->segmentCount; segmentIndex++) {
                corner0.vx       = scratch->x + scratch->dx * segmentIndex;
                corner0.vy       = scratch->y;
                corner0.vz       = scratch->z;
                corner1.vx       = scratch->x + scratch->dx * (segmentIndex + 1);
                corner1.vy       = scratch->y;
                corner1.vz       = scratch->z;
                scratch->yOffset = 0;
                corner2.vx       = scratch->x + scratch->dx * segmentIndex;
                corner2.vy       = scratch->y + scratch->yOffset;
                corner2.vz       = scratch->z + scratch->dz;
                scratch->yOffset = 0;
                corner3.vx       = scratch->x + scratch->dx * (segmentIndex + 1);
                corner3.vy       = scratch->y + scratch->yOffset;
                corner3.vz       = scratch->z + scratch->dz;
                orderingDepth    = RotTransPers4(&corner0, &corner1, &corner2, &corner3, &screenXY0, &screenXY1, &screenXY2, &screenXY3, &depthCue, &projectionFlags);
                if (projectionFlags >= 0) {
                    SHELTER_B4_UPPER_SEWER_EMIT_WATER_QUAD();
                }
            }
        } else {
            scratch->dx = surface->width;
            scratch->dz = surface->depth / surface->segmentCount;
            scratch->x  = surface->x;
            scratch->z  = surface->z;
            for (segmentIndex = 0; segmentIndex < surface->segmentCount; segmentIndex++) {
                corner0.vx       = scratch->x;
                corner0.vy       = scratch->y;
                corner0.vz       = scratch->z + scratch->dz * segmentIndex;
                corner1.vx       = scratch->x;
                corner1.vy       = scratch->y;
                corner1.vz       = scratch->z + scratch->dz * (segmentIndex + 1);
                scratch->yOffset = 0;
                corner2.vx       = scratch->x + scratch->dx;
                corner2.vy       = scratch->y + scratch->yOffset;
                corner2.vz       = scratch->z + scratch->dz * segmentIndex;
                scratch->yOffset = 0;
                corner3.vx       = scratch->x + scratch->dx;
                corner3.vy       = scratch->y + scratch->yOffset;
                corner3.vz       = scratch->z + scratch->dz * (segmentIndex + 1);
                orderingDepth    = RotTransPers4(&corner0, &corner1, &corner2, &corner3, &screenXY0, &screenXY1, &screenXY2, &screenXY3, &depthCue, &projectionFlags);
                if (projectionFlags >= 0) {
                    SHELTER_B4_UPPER_SEWER_EMIT_WATER_QUAD();
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(WaterQuadScratch);
#undef SHELTER_B4_UPPER_SEWER_EMIT_WATER_QUAD
}

/// Draws the room's water and publishes its height for water-dependent actors.
///
/// Start in state 0 with this room loaded: initialization clears the selected
/// actor-buffer marker and advances to state 1. Drawing ticks borrow that buffer
/// for packets and clamp height to -1600..0 world units. The first tick publishes
/// the configured height before clamping. Only states 0 and 1 are valid; this
/// bodyless task ignores spawn arguments and does not retire itself.
static void _shelterB4UpperSewerWaterTask(Task* task)
{
    TaskFunc states[] = { _shelterB4UpperSewerInitializeWater, _shelterB4UpperSewerUpdateWater };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b4_upper_sewer_80186438;
}

/// Clears the selected actor-buffer session marker before water rendering begins.
///
/// No companion selects actor buffer 2; a companion selects buffer 1.
/// The marker's nonzero meaning is unproven. Advances the water task to drawing.
static void _shelterB4UpperSewerInitializeWater(Task* task)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    task->state++;
}

/// Sets whether the current area's view-13 sprite batch is hidden.
///
/// Only the low byte of `hidden` is tested (0 visible, nonzero hidden).
/// Requires the loaded sprite directory for this room and its three-entry
/// view-13 batch list; batch 1 covers the view's single source sprite.
static void _shelterB4UpperSewerSetView13SpriteHidden(s32 hidden)
{
    enum {
        SHELTER_B4_UPPER_SEWER_TOGGLE_SPRITE_VIEW  = 13,
        SHELTER_B4_UPPER_SEWER_TOGGLE_SPRITE_BATCH = 1
    };
    const GameLocationKey* location = &gGameSession->location.loc;
    SpriteBatch*           batches;

    batches = Gp_SprtTables[location->stage - 1]->areaViews[location->area - 1][SHELTER_B4_UPPER_SEWER_TOGGLE_SPRITE_VIEW - 1].batches;
    if ((hidden & 0xFF) == 0) {
        batches[SHELTER_B4_UPPER_SEWER_TOGGLE_SPRITE_BATCH].hidden = 0;
    } else {
        batches[SHELTER_B4_UPPER_SEWER_TOGGLE_SPRITE_BATCH].hidden = 1;
    }
}

void shelterB4UpperSewerDrawGlowsTask(Task* task)
{
    enum {
        SHELTER_B4_UPPER_SEWER_GLOW_TASK_INITIALIZE = 0,
        SHELTER_B4_UPPER_SEWER_GLOW_TASK_ACTIVE     = 1,
        SHELTER_B4_UPPER_SEWER_RESERVOIR_COMPLETE   = 1,
        SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE    = 0x200,
        // RGB nibbles; the capsule drawer multiplies each channel by 16.
        SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY = 0x444,
        SHELTER_B4_UPPER_SEWER_GLOW_DIM_GRAY    = 0x222,
        SHELTER_B4_UPPER_SEWER_GLOW_BLUE        = 0x124,
        SHELTER_B4_UPPER_SEWER_GLOW_GREEN       = 0x343
    };

    // Install the package's effect callbacks before drawing the visible glow pairs.
    if (task->state == SHELTER_B4_UPPER_SEWER_GLOW_TASK_INITIALIZE) {
        if (gameFlagGetNibble(GAME_FLAG_B4_RESERVOIR_EVENT_DONE) == SHELTER_B4_UPPER_SEWER_RESERVOIR_COMPLETE) {
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
        task->state               = SHELTER_B4_UPPER_SEWER_GLOW_TASK_ACTIVE;
    }

    switch (viewGetMappedIndex() & 0xFF) {
        case 3:
            _glowDrawCapsule(D_shelter_b4_upper_sewer_801864F0, SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            break;
        case 4:
            _glowDrawCapsule(&D_shelter_b4_upper_sewer_801864F0[0], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_DIM_GRAY);
            _glowDrawCapsule(&D_shelter_b4_upper_sewer_801864F0[12], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BLUE);
            break;
        case 8: {
            const SVECTOR* glowPoints = D_shelter_b4_upper_sewer_801864D0;
            _glowDrawCapsule(&glowPoints[0], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[2], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[4], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_DIM_GRAY);
            _glowDrawCapsule(&glowPoints[6], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[16], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BLUE);
            break;
        }
        case 9: {
            const SVECTOR* glowPoints = D_shelter_b4_upper_sewer_801864D0;
            _glowDrawCapsule(&glowPoints[0], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[2], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[4], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_DIM_GRAY);
            _glowDrawCapsule(&glowPoints[6], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[16], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BLUE);
            break;
        }
        case 10: {
            const SVECTOR* glowPoints = D_shelter_b4_upper_sewer_801864B0;
            _glowDrawCapsule(&glowPoints[0], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[2], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[18], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_GREEN);
            break;
        }
        case 11: {
            const SVECTOR* glowPoints = D_shelter_b4_upper_sewer_80186490;
            _glowDrawCapsule(&glowPoints[0], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[6], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[18], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            _glowDrawCapsule(&glowPoints[20], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_GREEN);
            break;
        }
        case 12:
            _glowDrawCapsule(&D_shelter_b4_upper_sewer_801864F0[6], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BRIGHT_GRAY);
            break;
        case 13:
            _glowDrawCapsule(&D_shelter_b4_upper_sewer_801864F0[12], SHELTER_B4_UPPER_SEWER_GLOW_RADIUS_SCALE, SHELTER_B4_UPPER_SEWER_GLOW_BLUE);
            break;
    }
}

#include "../../shared/water_ripple_task.inc.c"

void shelterB4UpperSewerWaterRippleTask(Task* task)
{
    _waterRippleTask(task);
}

#include "../../shared/water_splash.inc.c"

#include "../../shared/water_drift_task.inc.c"

void shelterB4UpperSewerWaterDriftTask(Task* task)
{
    _waterDriftTask(task);
}

#include "../../shared/water_spin.inc.c"

#include "../../shared/water_tile.inc.c"

#include "../../shared/glow_draw_capsule.inc.c"

#include "../../shared/room_visual_effects.inc.c"

void shelterB4UpperSewerRoomVisualEffectsMoteTask(Task* task)
{
    _roomVisualEffectsMoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void shelterB4UpperSewerRoomVisualEffectsHaloTask(Task* task)
{
    _roomVisualEffectsHaloTask(task);
}

void shelterB4UpperSewerRoomVisualEffectsHaloOrangeBurstTask(Task* task)
{
    _roomVisualEffectsHaloOrangeBurstTask(task);
}

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void shelterB4UpperSewerRoomVisualEffectsSparkEmitterTask(Task* task)
{
    _roomVisualEffectsSparkEmitterTask(task);
}

#include "../../shared/room_visual_effects_flash_task.inc.c"

void shelterB4UpperSewerRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void shelterB4UpperSewerRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void shelterB4UpperSewerRoomVisualEffectsSparkBurstTask(Task* task)
{
    _roomVisualEffectsSparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
#include "../../shared/room_visual_effects_flying_tasks.inc.c"

void shelterB4UpperSewerRoomVisualEffectsGlowDiscTask(Task* task)
{
    _roomVisualEffectsGlowDiscTask(task);
}

void shelterB4UpperSewerRoomVisualEffectsFlyingSparkTask(Task* task)
{
    _roomVisualEffectsFlyingSparkTask(task);
}

#include "../../shared/room_visual_effects_burst.inc.c"

void shelterB4UpperSewerRoomVisualEffectsFlyingOrangeBurstTask(Task* task)
{
    _roomVisualEffectsFlyingOrangeBurstTask(task);
}

#include "../../shared/room_visual_effects_burst_draw.inc.c"
