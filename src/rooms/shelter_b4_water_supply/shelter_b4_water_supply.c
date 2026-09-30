#include "rooms/shelter_b4_water_supply.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
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
#include "main/mc.h"
#include "main/mc_types.h"
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

#define D_shelter_b4_water_supply_801826A0 (D_shelter_b4_water_supply_80182690 + 2)
#define D_shelter_b4_water_supply_801826C0 (D_shelter_b4_water_supply_80182690 + 6)
#define D_shelter_b4_water_supply_801826D0 (D_shelter_b4_water_supply_80182690 + 8)

/// One water surface: a rectangle at (`x`, `z`) spanning `width` along X and
/// `depth` along Z. A list of them ends at an entry whose `end` is -1; `end`
/// is not otherwise read.
typedef struct ShelterB4WaterSupplySurface {
    s16 x;
    s16 z;
    s16 width;
    s16 depth;
    s16 end;
} ShelterB4WaterSupplySurface;

/// Block the room task `func_shelter_b4_water_supply_8017EE54` receives as
/// `spawnArg2`. Only the halfword at 0x26 is touched there: an effect
/// strength, set from how far a tracked part moved this frame and used as the
/// odds of spawning each of the two effects.
typedef struct _ShelterB4WaterSupplySplash {
    byte pad_0[0x26];
    s16  strength;
} _ShelterB4WaterSupplySplash;

/// Descriptor of the departure task spawned once the event block is staged.
extern TaskDesc D_shelter_b4_water_supply_801825E4;

/// The room's message table, installed by the room task's first state.
extern GpMsgEntry D_shelter_b4_water_supply_801825F0[];

/// Task table spawned by `func_shelter_b4_water_supply_8017DA30` once the
/// valve script has run.
extern TaskDesc D_shelter_b4_water_supply_80182620[];

/// Height of the water surfaces.
extern s16 D_shelter_b4_water_supply_80182638;

/// Tasks the room task's first state spawns.
extern TaskDesc D_shelter_b4_water_supply_8018263C[];

/// The room's water surfaces whose strips run along Z.
extern ShelterB4WaterSupplySurface D_shelter_b4_water_supply_80182648[];

/// The room's water surfaces whose strips run along X.
extern ShelterB4WaterSupplySurface D_shelter_b4_water_supply_8018265C[];

/// End-point pairs of the light beams the room task draws per view.
extern SVECTOR D_shelter_b4_water_supply_80182670[];
extern SVECTOR D_shelter_b4_water_supply_80182680[];

/// Last-frame world positions of the two tracked parts of the slot-3 task's
/// model, compared against this frame's to measure how far each moved.
extern SVECTOR D_shelter_b4_water_supply_801826E0[];

/// Per-tint channel shifts for the glowing disc, indexed by the tint the spawn
/// argument selects.
extern RoomHaloShade D_shelter_b4_water_supply_801826F0[];

/// Spawn argument for the task `func_shelter_b4_water_supply_8017D7C0` starts
/// with `Task_Spawn(1, 0x31, ...)`.
extern RoomFadeStorage D_shelter_b4_water_supply_80184E34;

/// Staging save location the spawned task reads: `field_2` / `field_4` /
/// `field_1` receive the outgoing location's `field_0` / `field_2` / `field_3`.
extern GpSaveLoc D_shelter_b4_water_supply_80184E3C;

/// The staged event block, read by the departure task.
extern RoomDeparture D_shelter_b4_water_supply_80184E44;

/// Cursor into the primitive area the water surface is written to.
extern u8* D_shelter_b4_water_supply_80184E50;

static void func_shelter_b4_water_supply_8017DB18(void);
static void func_shelter_b4_water_supply_8017DD40(Task* arg0);
static void func_shelter_b4_water_supply_8017DD9C(Task* task);
static s32  func_shelter_b4_water_supply_8017DDFC(RoomEventMsg* in, RoomEventMsg* out);
static void func_shelter_b4_water_supply_8017DE74(Task* task);
static void func_shelter_b4_water_supply_8017E5D8(Task* task);
static void func_shelter_b4_water_supply_8017ED90(Task* arg0);
static void func_shelter_b4_water_supply_8017EDD0(Task* task);
static void func_shelter_b4_water_supply_8017F3A0(GfxCoord* arg0, s32 arg1, s32 arg2);
static void func_shelter_b4_water_supply_8017FB90(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_shelter_b4_water_supply_8017FF7C(GfxCoord* arg0, s16 arg1, s16 arg2);
static void func_shelter_b4_water_supply_80180260(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b4_water_supply_80181158(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_shelter_b4_water_supply_801813DC(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_shelter_b4_water_supply_80181800(GfxCoord* arg0, s16 arg1, u8* rgb);
static void func_shelter_b4_water_supply_80181D40(GfxCoord* coord, s16 size);
static void func_shelter_b4_water_supply_8018226C(GfxCoord* arg0, s32 arg1);

void func_shelter_b4_water_supply_8017D650(Task*);
void func_shelter_b4_water_supply_8017D7C0(Task*);
s32  func_shelter_b4_water_supply_8017D970(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b4_water_supply_8017D978(Task*, s32, GpSaveLoc*, GpSaveLoc*);
s32  func_shelter_b4_water_supply_8017DA28(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b4_water_supply_8017DA30(Task*, s32, GpMsg13EF*, s32);
s32  func_shelter_b4_water_supply_8017DAE4(Task*, s32, s32, s32);
void func_shelter_b4_water_supply_8017DC28(Task*);
void func_shelter_b4_water_supply_8017ED28(Task*);

extern TaskDesc D_80147E48;

TaskDesc D_shelter_b4_water_supply_801825E4 = { 0, 32, func_shelter_b4_water_supply_8017D650, { .model = NULL } };

GpMsgEntry D_shelter_b4_water_supply_801825F0[6] = {
    { 5102, func_shelter_b4_water_supply_8017D978 },
    { 5105, func_shelter_b4_water_supply_8017D970 },
    { 5103, func_shelter_b4_water_supply_8017DA30 },
    { 5104, func_shelter_b4_water_supply_8017DA28 },
    { 5106, func_shelter_b4_water_supply_8017DAE4 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_shelter_b4_water_supply_80182620[2] = {
    { 0, 32, func_shelter_b4_water_supply_8017DC28, { .model = NULL } },
    { 0, 32, func_shelter_b4_water_supply_8017D7C0, { .model = NULL } },
};

s16 D_shelter_b4_water_supply_80182638 = -2500;

TaskDesc D_shelter_b4_water_supply_8018263C[1] = {
    { 0, 96, func_shelter_b4_water_supply_8017ED28, { .model = NULL } },
};

ShelterB4WaterSupplySurface D_shelter_b4_water_supply_80182648[2] = {
    { 9100, -0x364C, 1800, 0x2EE0, 0 },
    { 0, 0, 0, 0, -1 },
};

ShelterB4WaterSupplySurface D_shelter_b4_water_supply_8018265C[2] = {
    { 4400, -1900, 8150, 1800, 0 },
    { 0, 0, 0, 0, -1 },
};

SVECTOR D_shelter_b4_water_supply_80182670[2] = {
    { 0x36C4, -5130, -1910, 0 },
    { 0x337C, -4540, -1910, 0 },
};

SVECTOR D_shelter_b4_water_supply_80182680[2] = {
    { 6520, -3790, -1910, 0 },
    { 5500, -3790, -1910, 0 },
};

// Indexed views below share one contiguous table.
SVECTOR D_shelter_b4_water_supply_80182690[10] = {
    { 10480, -3840, -100, 0 },
    { 9500, -3750, -100, 0 },
    { 1990, -5790, -80, 0 },
    { 1000, -5790, -80, 0 },
    { 10920, -3790, -3560, 0 },
    { 10920, -3790, -4550, 0 },
    { 10920, -3790, -11570, 0 },
    { 10920, -3790, -12550, 0 },
    { 9130, -3790, -7950, 0 },
    { 9130, -3790, -6970, 0 },
};

SVECTOR D_shelter_b4_water_supply_801826E0[2] = { 0 };

RoomHaloShade D_shelter_b4_water_supply_801826F0[2] = {
    { 1, 0, 0 },
    { 0, 1, 0 },
};

// Four 16-byte direction-facing rows used by Gp_MsgPlayerDirFacing.
u8 D_shelter_b4_water_supply_801826FC[64] = {
    2,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    0,
    2,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    0,
    0,
    0,
};

u8* D_shelter_b4_water_supply_8018273C[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_b4_water_supply_80182740[1] = {
    { { .bytes = { 11, 0 } } },
};

GpWarpRec D_shelter_b4_water_supply_80182744[3] = {
    { { .words = { 1024, 640, -4000, -1024 } }, { 0, 0, 0, 0 }, { .words = { 768, 5350, -2000, -1600 } }, { 0, 0, 0, 0 }, 0, 0x542E0005, 0, 7, 0, 0 },
    { { .words = { 1024, 9450, -2000, -0x32C8 } }, { 0, 0, 0, 0 }, { .words = { 3840, 0x283C, -2000, -0x3520 } }, { 0, 0, 0, 0 }, 0x542E0002, 0x542E0001, 0, 11, 0, 0 },
    { { .words = { 3072, 0x3CF0, -3600, -1000 } }, { 0, 0, 0, 0 }, { .words = { 3072, 0x3CF0, -3600, -1000 } }, { 0, 0, 0, 0 }, 0x542E0004, 0, 0x542E0006, 2, 0, 445 },
};

SVECTOR D_shelter_b4_water_supply_801827EC[12] = {
#include "assets/shelter_b4_water_supply_collision_0587C_normals.inc"
};

SVECTOR D_shelter_b4_water_supply_8018284C[59] = {
#include "assets/shelter_b4_water_supply_collision_0587C_verts.inc"
};

GpGridFace D_shelter_b4_water_supply_80182A24[45] = {
#include "assets/shelter_b4_water_supply_collision_0587C_faces.inc"
};

s16 D_shelter_b4_water_supply_80182C40[222] = {
#include "assets/shelter_b4_water_supply_collision_0587C_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b4_water_supply_80182C40[i])
s16* D_shelter_b4_water_supply_80182DFC[16] = {
#include "assets/shelter_b4_water_supply_collision_0587C_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b4_water_supply_80182E3C = { NULL, D_shelter_b4_water_supply_801827EC, D_shelter_b4_water_supply_8018284C, D_shelter_b4_water_supply_80182A24, D_shelter_b4_water_supply_80182DFC, -400, 0x364C, 4, 4, 4000, 45 };

GpViewRec D_shelter_b4_water_supply_80182E60[11] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -6828, 0x7D00, 6785 } }, 447 },
    { { { { -1049, 0, -3959 }, { 841, 4002, -223 }, { 3868, -870, -1025 } }, { -0x2E41, 3890, 333 } }, 230 },
    { { { { -770, 0, -4022 }, { 44, 4095, -8 }, { 4022, -45, -770 } }, { -9469, 2988, 580 } }, 230 },
    { { { { -1205, 0, -3914 }, { -191, 4091, 58 }, { 3909, 200, -1204 } }, { -6858, 2988, 402 } }, 230 },
    { { { { -943, 0, 3985 }, { 65, 4095, 15 }, { -3985, 67, -943 } }, { -0x291C, 2925, 450 } }, 230 },
    { { { { -1040, 0, 3961 }, { -254, 4087, -66 }, { -3953, -263, -1038 } }, { -7900, 2925, 420 } }, 257 },
    { { { { -608, 0, -4050 }, { -1926, 3602, 289 }, { 3562, 1948, -535 } }, { 1769, 6119, 698 } }, 230 },
    { { { { 3988, 0, 933 }, { 42, 4091, -182 }, { -932, 187, 3983 } }, { -0x2898, 2969, 7455 } }, 230 },
    { { { { -4028, 0, 739 }, { 15, 4095, 83 }, { -739, 85, -4027 } }, { -0x28D4, 2936, 3528 } }, 230 },
    { { { { -4027, 0, 745 }, { 0, 4095, 1 }, { -745, 1, -4027 } }, { -0x28BD, 2952, 6769 } }, 230 },
    { { { { -3911, 0, 1216 }, { 140, 4068, 451 }, { -1208, 473, -3884 } }, { -0x2913, 3254, 9638 } }, 230 },
};

GpSprtCmd D_shelter_b4_water_supply_80182FEC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b4_water_supply_80182FFC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b4_water_supply_8018300C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_water_supply_8018301C[34] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 72, 646, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 537, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, -72, 625, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, -88, 624, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -120, 647, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -120, 601, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -104, 628, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, -56, 650, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, -24, 650, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, 8, 650, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 40, 650, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 56, 650, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 72, 650, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, -120, 578, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, -88, 650, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, -120, 545, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, -88, 575, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, -56, 600, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, -24, 600, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, 8, 600, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, 40, 600, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, 72, 600, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, -120, 575, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, -80, 575, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, -40, 575, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, 0, 575, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, 40, 575, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 104, 80, 575, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -120, 500, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -80, 500, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -40, 500, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 0, 500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 40, 500, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 80, 500, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b4_water_supply_801832C4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 34, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b4_water_supply_801832DC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b4_water_supply_801832EC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_water_supply_801832FC[108] = {
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, -8, 1200, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, -8, 1200, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, -8, 1200, { .fields = { 120, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, -8, 1200, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, -8, 1212, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, -8, 1225, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, -8, 1225, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 0, 1200, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 0, 1200, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -24, 0, 1200, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 0, 0, 1212, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 0, 1212, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 8, 1200, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, 8, 1200, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -24, 8, 1200, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 0, 8, 1200, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 24, 8, 1200, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, 16, 1175, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 16, 1177, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -32, 16, 1177, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -8, 16, 1177, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, 16, 1177, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 16, 1177, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, 32, 1162, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 32, 1162, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -32, 32, 1162, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -8, 32, 1162, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, 32, 1162, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 32, 1162, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -56, 2517, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -56, 2500, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -48, 2750, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -48, 2525, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -40, 2800, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -40, 2500, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -32, 2875, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -32, 2500, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -24, 2565, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -24, 2500, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -16, 2750, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -16, 2500, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -8, 2500, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, -120, 2500, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, -120, 1225, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, -96, 2500, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, -96, 1268, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, -72, 1576, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -72, 1503, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -120, 2125, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -96, 2125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -72, 2250, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -48, 2250, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -24, 2325, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, -24, 1950, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -56, 1875, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -88, 1250, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -120, 1250, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 40, -120, 1125, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 40, -88, 1125, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 40, -56, 1125, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, -24, 1125, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 72, -120, 850, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 72, -88, 850, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 72, -56, 850, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 72, -24, 850, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 104, -120, 750, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 104, -88, 750, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 104, -56, 750, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -24, 750, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -120, 675, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -88, 675, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -56, 675, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, -56, 1500, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -48, 2500, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, -64, 1656, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, -64, 1375, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, -64, 1375, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -64, 2250, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -64, 2250, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, -64, 1459, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, -48, 1524, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, -32, 1689, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, -80, 1281, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -80, 1341, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -80, 1495, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, -80, 1468, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -80, 1372, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, -80, 1256, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -64, -104, 1129, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -40, -104, 1225, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -16, -104, 1217, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -104, 1200, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -48, 2500, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -32, 2500, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -16, 2500, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 48, 538, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 56, 530, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 72, 509, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 72, 675, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 88, 506, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, 40, 550, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, 48, 575, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, 56, 575, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 56, 535, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, 72, 575, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 72, 523, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 64, 575, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 88, 522, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b4_water_supply_80183B6C[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 29, 0, 0, { 3, 0 } },
    { 29, 43, 0, 0, { 0, 0 } },
    { 72, 23, 0, 0, { 2, 0 } },
    { 95, 13, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_water_supply_80183B9C[47] = {
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -16, -88, 1100, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -16, -56, 1308, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -16, -24, 1324, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -16, 8, 1125, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 24, 1283, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 80, -88, 903, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 80, -56, 1050, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 80, -24, 1050, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 80, 8, 1050, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, -88, 1000, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, -56, 1000, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, -24, 1000, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, 8, 1000, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, 48, 1000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 112, -88, 1000, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 136, -88, 1000, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 136, -56, 1000, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 112, -56, 1000, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 112, -24, 1000, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 136, -24, 1000, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, 8, 1000, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 8, 1000, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, 48, 1000, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 136, 48, 1000, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 112, 88, 1000, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 136, 88, 1000, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -32, -88, 1125, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -32, -56, 1125, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -32, -24, 1125, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, 8, 1125, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -56, -88, 1000, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -56, -56, 1000, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -56, -24, 1000, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -56, 8, 1000, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -56, 40, 1000, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -88, -88, 1000, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -88, -40, 1000, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -88, 8, 1000, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -88, 56, 1000, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -120, -88, 1000, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -120, -40, 1000, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -120, 8, 1000, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -120, 56, 1000, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, -88, 1000, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, -40, 1000, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, 8, 1000, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -160, 56, 1000, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_b4_water_supply_80183F48[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 47, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b4_water_supply_80183F60[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b4_water_supply_80183F70[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_b4_water_supply_80183F80[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b4_water_supply_80183F90[11] = {
    { { .empty = D_shelter_b4_water_supply_80182FEC }, D_shelter_b4_water_supply_80182FEC, NULL },
    { { .empty = D_shelter_b4_water_supply_80182FFC }, D_shelter_b4_water_supply_80182FFC, NULL },
    { { .empty = D_shelter_b4_water_supply_8018300C }, D_shelter_b4_water_supply_8018300C, NULL },
    { { .elements = D_shelter_b4_water_supply_8018301C }, D_shelter_b4_water_supply_801832C4, NULL },
    { { .empty = D_shelter_b4_water_supply_801832DC }, D_shelter_b4_water_supply_801832DC, NULL },
    { { .empty = D_shelter_b4_water_supply_801832EC }, D_shelter_b4_water_supply_801832EC, NULL },
    { { .elements = D_shelter_b4_water_supply_801832FC }, D_shelter_b4_water_supply_80183B6C, NULL },
    { { .elements = D_shelter_b4_water_supply_80183B9C }, D_shelter_b4_water_supply_80183F48, NULL },
    { { .empty = D_shelter_b4_water_supply_80183F60 }, D_shelter_b4_water_supply_80183F60, NULL },
    { { .empty = D_shelter_b4_water_supply_80183F70 }, D_shelter_b4_water_supply_80183F70, NULL },
    { { .empty = D_shelter_b4_water_supply_80183F80 }, D_shelter_b4_water_supply_80183F80, NULL },
};

GpPointLight D_shelter_b4_water_supply_80184014[10] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2710, -3800, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3358, 2949, 2539, { 0, 0 } }, 2000, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x393A, -5245, -1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3358, 2949, 2539, { 0, 0 } }, 1750, 2250 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1930, -7250, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3276, 3194, 2785, { 0, 0 } }, 3000, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2710, -3800, -4000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3358, 2949, 2539, { 0, 0 } }, 2000, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9500, -3800, -7500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3358, 2949, 2539, { 0, 0 } }, 2000, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2904, -3800, -0x2EE0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3358, 2949, 2539, { 0, 0 } }, 2000, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3520, -4845, -1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3358, 2949, 2539, { 0, 0 } }, 2000, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6000, -3800, -1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3358, 2949, 2539, { 0, 0 } }, 2000, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1500, -5785, -500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3358, 2949, 2539, { 0, 0 } }, 2000, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2C24, -3800, -500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3358, 2949, 2539, { 0, 0 } }, 2000, 2500 },
};

GpRoomCoordSet D_shelter_b4_water_supply_801843D4 = { 0, NULL, 10, D_shelter_b4_water_supply_80184014, 0, NULL };

GpObj4C D_shelter_b4_water_supply_801843EC[18] = {
    { NULL, NULL, NULL, { 0x359F, -3328, -1152, 0 }, { { 0, -4160, 1376, 0 }, { 0, -4160, -1376, 0 }, { 0, 4160, 1376, 0 }, { 0, 4160, -1376, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 0x3541, -3392, -1089, 0 }, { { 0, -4160, -1504, 0 }, { 0, -4160, 1504, 0 }, { 0, 4160, -1504, 0 }, { 0, 4160, 1504, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 0x2DE0, -3361, -1056, 0 }, { { 0, -4160, 1376, 0 }, { 0, -4160, -1376, 0 }, { 0, 4160, 1376, 0 }, { 0, 4160, -1376, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 0x2D60, -3297, -992, 0 }, { { 0, -4160, -1504, 0 }, { 0, -4160, 1504, 0 }, { 0, 4160, -1504, 0 }, { 0, 4160, 1504, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 8608, -3457, -1024, 0 }, { { 0, -4160, 1376, 0 }, { 0, -4160, -1376, 0 }, { 0, 4160, 1376, 0 }, { 0, 4160, -1376, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 8449, -3328, -992, 0 }, { { 0, -4160, -1504, 0 }, { 0, -4160, 1504, 0 }, { 0, 4160, -1504, 0 }, { 0, 4160, 1504, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 5760, -3329, -1152, 0 }, { { 0, -4160, 1376, 0 }, { 0, -4160, -1376, 0 }, { 0, 4160, 1376, 0 }, { 0, 4160, -1376, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { 5632, -3424, -1152, 0 }, { { 0, -4160, -1504, 0 }, { 0, -4160, 1504, 0 }, { 0, 4160, -1504, 0 }, { 0, 4160, 1504, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { 3264, -3392, -992, 0 }, { { 0, -4160, 1376, 0 }, { 0, -4160, -1376, 0 }, { 0, 4160, 1376, 0 }, { 0, 4160, -1376, 0 } }, { -4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 7, 6, 1, 0 },
    { NULL, NULL, NULL, { 3104, -3393, -960, 0 }, { { 0, -4160, -1504, 0 }, { 0, -4160, 1504, 0 }, { 0, 4160, -1504, 0 }, { 0, 4160, 1504, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 6, 7, 1, 0 },
    { NULL, NULL, NULL, { 0x2730, -3201, -2064, 0 }, { { -1232, -4160, 16, 0 }, { 1232, -4160, -16, 0 }, { -1232, 4160, 16, 0 }, { 1232, 4160, -16, 0 } }, { -54, 0, -4107, 0 }, { 0, 0, 4096, 0 }, 4314, 0, 8, 4, 1, 0 },
    { NULL, NULL, NULL, { 0x2741, -3424, -2144, 0 }, { { 1200, -4160, -16, 0 }, { -1200, -4160, 16, 0 }, { 1200, 4160, -16, 0 }, { -1200, 4160, 16, 0 } }, { 54, 0, 4110, 0 }, { 0, 0, 4096, 0 }, 4314, 0, 4, 8, 1, 0 },
    { NULL, NULL, NULL, { 0x2711, -3424, -5617, 0 }, { { 1344, -4160, 32, 0 }, { -1344, -4160, -32, 0 }, { 1344, 4160, 32, 0 }, { -1344, 4160, -32, 0 } }, { -98, 0, 4108, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 8, 9, 1, 0 },
    { NULL, NULL, NULL, { 0x2720, -3552, -5488, 0 }, { { -1408, -4160, -16, 0 }, { 1408, -4160, 16, 0 }, { -1408, 4160, -16, 0 }, { 1408, 4160, 16, 0 } }, { 46, 0, -4109, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 9, 8, 1, 0 },
    { NULL, NULL, NULL, { 9984, -3425, -8576, 0 }, { { -1408, -4160, -16, 0 }, { 1408, -4160, 16, 0 }, { -1408, 4160, -16, 0 }, { 1408, 4160, 16, 0 } }, { 46, 0, -4109, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 10, 9, 1, 0 },
    { NULL, NULL, NULL, { 9952, -3296, -8768, 0 }, { { 1344, -4160, 32, 0 }, { -1344, -4160, -32, 0 }, { 1344, 4160, 32, 0 }, { -1344, 4160, -32, 0 } }, { -98, 0, 4108, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 9, 10, 1, 0 },
    { NULL, NULL, NULL, { 9984, -3617, -0x2EE0, 0 }, { { 1344, -4160, 32, 0 }, { -1344, -4160, -32, 0 }, { 1344, 4160, 32, 0 }, { -1344, 4160, -32, 0 } }, { -98, 0, 4108, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 10, 11, 1, 0 },
    { NULL, NULL, NULL, { 9984, -3616, -0x2E40, 0 }, { { -1408, -4160, -16, 0 }, { 1408, -4160, 16, 0 }, { -1408, 4160, -16, 0 }, { 1408, 4160, 16, 0 } }, { 46, 0, -4109, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 11, 10, 129, 0 },
};

GpObj4C D_shelter_b4_water_supply_80184944[7] = {
    { NULL, NULL, NULL, { 2496, -4063, -1056, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1101, 257, 170, 64, 2, 0 },
    { NULL, NULL, NULL, { 4800, -2056, -1056, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4095, 0, 0, 0 }, 1101, 1, 170, 192, 2, 0 },
    { NULL, NULL, NULL, { 0x2FA0, -2064, -1056, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4097, 0, -39, 0 }, 1101, 1, 184, 64, 2, 0 },
    { NULL, NULL, NULL, { 0x3640, -3647, -1024, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, 257, 184, 192, 2, 0 },
    { NULL, NULL, NULL, { 9536, -2072, -0x32D0, 0 }, { { -416, 0, -944, 0 }, { 416, 0, -944, 0 }, { -416, 0, 944, 0 }, { 416, 0, 944, 0 } }, { 0, 4099, 0, 0 }, { 4096, 0, 0, 0 }, 1024, 0, 45, 33, 2, 0 },
    { NULL, NULL, NULL, { 544, -4062, -1120, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, 0, 44, 19, 2, 0 },
    { NULL, NULL, NULL, { 0x3D40, -3645, -1024, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1101, 5, 10, 32, 130, 0 },
};

GpAreaTmdRec D_shelter_b4_water_supply_80184B58[2] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_water_supply_80184B70[3] = {
    { 70, 70, 0, 0, { 0, 0 }, D_8013F5F0 },
    { 72, 72, 1, 0, { 0, 0 }, D_80153EC8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_water_supply_80184B94[2] = {
    { 24, 24, 0, 0, { 0, 0 }, D_8013647C },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_water_supply_80184BAC[2] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b4_water_supply_80184BC4[2] = {
    { 4, 0, 0, 7000, -2000, -1000, 1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_water_supply_80184BE4[5] = {
    { 72, 0, 0, 0x2710, -2000, -1900, 0, 0, 2, 3, 0 },
    { 72, 0, 0, 7800, -2000, -1000, 2600, 0, 2, 3, 0 },
    { 70, 0, 0, 9500, -2000, -900, 2048, 0, 0, 2, 0 },
    { 70, 0, 0, 0x27D8, -2000, -5400, -200, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_water_supply_80184C34[4] = {
    { 24, 0, 0, 0x2710, -2000, -1000, 0, 0, 0, 2, 0 },
    { 24, 0, 0, 0x2710, -2000, -7000, 0, 0, 0, 2, 0 },
    { 24, 0, 0, 0x2710, -2000, -4000, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_water_supply_80184C74[3] = {
    { 4, 0, 0, 0x2710, -2000, -1000, 2048, 0, 0, 2, 0 },
    { 4, 0, 0, 0x2710, -2000, -6500, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_shelter_b4_water_supply_80184CA4[12] = {
    { NULL, NULL },
    { D_shelter_b4_water_supply_80184BC4, D_shelter_b4_water_supply_80184B58 },
    { D_shelter_b4_water_supply_80184BE4, D_shelter_b4_water_supply_80184B70 },
    { D_shelter_b4_water_supply_80184C34, D_shelter_b4_water_supply_80184B94 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b4_water_supply_80184C74, D_shelter_b4_water_supply_80184BAC },
};

GpObj3A D_shelter_b4_water_supply_80184D04[2] = {
    { NULL, NULL, { 4608, -3408, -7120, 0 }, { { -4256, 4336, -4880, 0 }, { 4256, 4336, 4880, 0 }, { -4256, -4336, -4880, 0 }, { 4256, -4336, 4880, 0 } }, { -3089, 0, 2693, 0 }, { 101, 30 }, 1, 0 },
    { NULL, NULL, { 0x371F, -3712, -7649, 0 }, { { 3162, 4336, -5651, 0 }, { -3161, 4336, 5652, 0 }, { 3162, -4336, -5651, 0 }, { -3161, -4336, 5652, 0 } }, { -3578, 0, -2002, 0 }, { 101, 30 }, 129, 0 },
};

GpRoomBoundVec D_shelter_b4_water_supply_80184D7C[12] = {
    { 11, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 520, 596, 596, 567 },
    { 516, 557, 558, 541 },
    { 513, 529, 544, 524 },
    { 435, 519, 535, 489 },
    { 520, 561, 556, 545 },
    { 517, 555, 540, 538 },
    { 519, 548, 538, 535 },
    { 420, 480, 498, 459 },
};

s32 D_shelter_b4_water_supply_80184DDC[3] = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

s32 D_shelter_b4_water_supply_80184DE8[3] = {
    0x10000025,
    0x10000027,
    0x10000029,
};

GpRoomParamRec D_shelter_b4_water_supply_80184DF4[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b4_water_supply_80184DFC[1] = {
    { 0, 0, 1, 0, D_shelter_b4_water_supply_80184DDC },
};

GpRoomParamRec D_shelter_b4_water_supply_80184E04[1] = {
    { 0, 0, 1, 0, D_shelter_b4_water_supply_80184DE8 },
};

GpRoomParamRec D_shelter_b4_water_supply_80184E0C[1] = {
    { 0, 1, 0, 0, D_shelter_b4_water_supply_80184DDC },
};

GpRoomParamRec* D_shelter_b4_water_supply_80184E14[8] = {
    D_shelter_b4_water_supply_80184DF4,
    D_shelter_b4_water_supply_80184DFC,
    D_shelter_b4_water_supply_80184E04,
    D_shelter_b4_water_supply_80184E0C,
    D_shelter_b4_water_supply_80184DF4,
    D_shelter_b4_water_supply_80184DF4,
    D_shelter_b4_water_supply_80184DF4,
    D_shelter_b4_water_supply_80184DF4,
};

RoomFadeStorage D_shelter_b4_water_supply_80184E34 = { 0 };

GpSaveLoc D_shelter_b4_water_supply_80184E3C = { 0 };

RoomDeparture D_shelter_b4_water_supply_80184E44 = { 0 };

u8* D_shelter_b4_water_supply_80184E50;

/// The task the staged event block `D_shelter_b4_water_supply_80184E44`
/// spawns. State 0 sends the block's `facing` to the slot-3 game pointer as
/// message 0x3EE, skipping to state 2 when it is -1; state 1 waits until
/// that pointer answers 0x3F0 with 0. States 2 and 3 play the block's sound
/// event `sndEvent`, if any, and wait for its voice to go quiet. State 4 queues
/// type-7 sound event 0x80000000, commits the save location in the block's
/// first four bytes (stage, area, warp, room), re-spawns the player task as
/// type 0x11 and kills itself.
void func_shelter_b4_water_supply_8017D650(Task* arg0)
{
    GpXformArg msg;
    void*      slot;

    slot = gameGetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            msg.rot.vy = D_shelter_b4_water_supply_80184E44.facing;
            if (msg.rot.vy == -1) {
                arg0->state = 2;
                break;
            }
            Gp_DispatchMsgPtr(slot, 0x3EE, &msg, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 1:
            if (Gp_DispatchMsg(slot, 0x3F0, 0, 0) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 2:
            if (D_shelter_b4_water_supply_80184E44.sndEvent == 0) {
                arg0->state = 4;
                break;
            }
            SndEvt_EnqueueType6(D_shelter_b4_water_supply_80184E44.sndEvent, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 3:
            if (SndVoice_HasActiveId(D_shelter_b4_water_supply_80184E44.sndEvent) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 4:
            SndEvt_EnqueueType7((s32)0x80000000, 0);
            gDisplayState.spriteVariant        = 1;
            Mc_SaveData[0].state.at4.loc.stage = D_shelter_b4_water_supply_80184E44.stage;
            Mc_SaveData[0].state.at4.loc.area  = D_shelter_b4_water_supply_80184E44.area;
            Mc_SaveData[0].state.at4.loc.warp  = D_shelter_b4_water_supply_80184E44.warp;
            Mc_SaveData[0].state.at4.loc.room  = D_shelter_b4_water_supply_80184E44.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
        default:
            break;
    }
}

/// The room task's state table, dispatched by
/// `func_shelter_b4_water_supply_8017DDA4` from a stack copy: install the
/// message table and spawn the room's tasks, idle, then kill.
static const TaskFuncTable3 D_shelter_b4_water_supply_8017D5D8 = {
    {
        func_shelter_b4_water_supply_8017DD40,
        func_shelter_b4_water_supply_8017DD9C,
        taskKill,
    },
};

void func_shelter_b4_water_supply_8017D7C0(Task* arg0)
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
            D_shelter_b4_water_supply_80184E34.fade.field_0 = 0;
            D_shelter_b4_water_supply_80184E34.fade.field_1 = 0;
            D_shelter_b4_water_supply_80184E34.fade.field_2 = 0x1E;
            Task_Spawn(1, 0x31, 0, &D_shelter_b4_water_supply_80184E34.fade);
            arg0->killCountdown = 0x1E;
            arg0->state++;
            break;
        case 3:
            if (--arg0->killCountdown == 0) {
                SndEvt_EnqueueType6(0x542E0005, 0, 0);
                arg0->state++;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(0x542E0005) == 0) {
                arg0->state++;
            }
            break;
        case 5:
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_shelter_b4_water_supply_80184E3C.field_2;
            Mc_SaveData[0].state.at4.loc.warp = D_shelter_b4_water_supply_80184E3C.field_4;
            Mc_SaveData[0].state.at4.loc.room = D_shelter_b4_water_supply_80184E3C.prefix.bytes.field_1;
            Task_Spawn(0, 0x11, 0x10, 0);
            taskKill(arg0);
            break;
    }
}

s32 func_shelter_b4_water_supply_8017D970(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Copies the location at `src` into `dst` and passes both to `func_map_shelter_80179A04`.
/// When the leading halfword of `src` is 0x2C it returns 0, first staging three
/// bytes of `dst` and spawning from the task table unless `src->field_5` is set;
/// any other location returns 1.
s32 func_shelter_b4_water_supply_8017D978(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    func_map_shelter_80179A04(src, dst);
    if (*(u16*)src == 0x2C) {
        if (src->field_5 == 0) {
            D_shelter_b4_water_supply_80184E3C.field_2              = dst->prefix.bytes.field_0;
            D_shelter_b4_water_supply_80184E3C.field_4              = dst->field_2;
            D_shelter_b4_water_supply_80184E3C.prefix.bytes.field_1 = dst->field_3;
            Task_SpawnFromTable(D_shelter_b4_water_supply_80182620, 1, 4, 0);
        }
        return 0;
    }
    return 1;
}

s32 func_shelter_b4_water_supply_8017DA28(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Handler for slot-7 msg `0x13EF` in `D_shelter_b4_water_supply_801825F0`:
/// the directed action on the water-supply valve (`field_2` 0xA / `field_3`
/// 0x20).
s32 func_shelter_b4_water_supply_8017DA30(Task* task, s32 msgId, GpMsg13EF* arg2, s32 arg3)
{
    if (arg2->field_2 == 0xA) {
        if (arg2->field_3 == 0x20) {
            if (GameFlag_GetNibble(0xB8) != 0) {
                if (GameFlag_GetNibble(0x139) != 0) {
                    func_shelter_b4_water_supply_8017DB18();
                } else {
                    GameFlag_SetNibble(0x139, 1);
                    Gp_MsgPlayerWeapon(0);
                    Gp_RunCapCmd1(3);
                    Task_SpawnFromTable(D_shelter_b4_water_supply_80182620, 0, 0, 0);
                }
            } else {
                Gp_RunCapCmd1(1);
                GameFlag_SetNibble(0x1BD, 2);
            }
        }
    }
    return 0;
}

s32 func_shelter_b4_water_supply_8017DAE4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 6) {
        SndEvt_EnqueueType6(0x542E0000 | 6, 0, 0);
    }
    return 0;
}

static void func_shelter_b4_water_supply_8017DB18(void)
{
    RoomDeparture  work;
    RoomEventMsg   param;
    RoomDeparture* wp;
    s32            (*resolve)(RoomEventMsg*, RoomEventMsg*) = func_shelter_b4_water_supply_8017DDFC;

    work.stage    = 3;
    work.area     = 0x20;
    work.warp     = 3;
    work.room     = 1;
    work.sndEvent = 0x542E0003;
    work.facing   = 0x400;
    Gp_MsgPlayerWeapon(0);
    wp                  = &work;
    param.prefix.packed = wp->area;
    param.field_2       = wp->warp;
    param.field_3       = wp->room;
    param.field_5       = 0;
    resolve(&param, &param);
    wp->area                           = param.prefix.packed;
    wp->warp                           = param.field_2;
    wp->room                           = param.field_3;
    D_shelter_b4_water_supply_80184E44 = work;
    Task_SpawnFromTable(&D_shelter_b4_water_supply_801825E4, 0, 0, 0);
    if (gameGetPtrSlot(0xA) != NULL && GameFlag_GetNibble(0xCF) == 0) {
        GameFlag_SetNibble(0x4C, 6);
    }
}

void func_shelter_b4_water_supply_8017DC28(Task* arg0)
{
    RoomDeparture work;
    RoomEventMsg  param;
    s32           (*resolve)(RoomEventMsg*, RoomEventMsg*);

    if (Gp_CapBusy() == 0) {
        resolve       = func_shelter_b4_water_supply_8017DDFC;
        work.stage    = 3;
        work.area     = 0x20;
        work.warp     = 3;
        work.room     = 1;
        work.sndEvent = 0x542E0003;
        work.facing   = 0x400;
        Gp_MsgPlayerWeapon(0);
        param.prefix.packed = work.area;
        param.field_2       = work.warp;
        param.field_3       = work.room;
        param.field_5       = 0;
        resolve(&param, &param);
        work.area                          = param.prefix.packed;
        work.warp                          = param.field_2;
        work.room                          = param.field_3;
        D_shelter_b4_water_supply_80184E44 = work;
        Task_SpawnFromTable(&D_shelter_b4_water_supply_801825E4, 0, 0, 0);
        if (gameGetPtrSlot(0xA) != NULL && GameFlag_GetNibble(0xCF) == 0) {
            GameFlag_SetNibble(0x4C, 6);
        }
        taskKill(arg0);
    }
}

/// The room task's first state: installs the room's message table, registers
/// the task in game pointer slot 7, spawns the room's tasks and advances.
static void func_shelter_b4_water_supply_8017DD40(Task* arg0)
{
    arg0->msgTable = D_shelter_b4_water_supply_801825F0;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_shelter_b4_water_supply_8018263C, 0, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

/// The room task's idle state.
static void func_shelter_b4_water_supply_8017DD9C(Task* task)
{
}

/// The room task: copies the three-state table
/// `D_shelter_b4_water_supply_8017D5D8` onto the stack and runs the entry for
/// the task's current state.
void func_shelter_b4_water_supply_8017DDA4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b4_water_supply_8017D5D8;
    sp.funcs[task->state](task);
}

/// Message 0x20: unless a report-only query, answers in `field_3` from
/// nibbles 0x51 (1 when set, 2 when clear) and 0x53 (adds 2 when set).
/// Always returns 1 (not consumed).
static s32 func_shelter_b4_water_supply_8017DDFC(RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->prefix.packed == 0x20 && in->field_5 == 0) {
        if (GameFlag_GetNibble(0x51) == 0) {
            out->field_3 = 2;
        } else {
            out->field_3 = 1;
        }
        if (GameFlag_GetNibble(0x53) != 0) {
            out->field_3 = (u8)out->field_3 + 2;
        }
    }
    return 1;
}

/// Draws each surface in `D_shelter_b4_water_supply_80182648` at height
/// `D_shelter_b4_water_supply_80182638` as two strips of 16 semi-transparent
/// Gouraud quads laid side by side along X, each strip running along Z and
/// projected through the view matrix. The seam between the strips is lifted by
/// a sine wave that runs along Z and scrolls with the display frame counter.
/// The outer edges are coloured (0x80, 0, 0) and the seam (0x20, 0x20, 0x20);
/// each quad is followed by a draw-mode packet selecting blend mode 2. Quads
/// the projection flags as invalid are skipped. The per-surface values live in
/// a work block pushed on the scratchpad stack for the duration of the call.
/// `task`, the water task whose drawing state calls it, is unused.
static void func_shelter_b4_water_supply_8017DE74(Task* task)
{
    SVECTOR                      v0, v1, v2, v3;
    long                         sxy0, sxy1, sxy2, sxy3;
    long                         p, flag;
    s32                          phase;
    ShelterB4WaterSupplySurface* e;
    RoomWaterScratch*            w;
    u8*                          head;
    POLY_G4*                     poly;
    DR_MODE*                     dr;
    s32                          otz;
    s32                          i;

    e                          = D_shelter_b4_water_supply_80182648;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    head                       = SCRATCH_HEAD(u8);
    phase                      = -(gDisplayState.animFrame * 16);
    SCRATCH_HEAD(u8)           = head - 0xC;
    w                          = (RoomWaterScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    w->y = D_shelter_b4_water_supply_80182638;
    for (; e->end != -1; e++) {
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
            w->wave = (u32)rsin(phase + (i << 9)) >> 6;
            v2.vx   = w->x + w->dx;
            v2.vy   = w->y + w->wave;
            v2.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v3.vx   = w->x + w->dx;
            v3.vy   = w->y + w->wave;
            v3.vz   = w->z + w->dz * (i + 1);
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                               = (POLY_G4*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                PRIM_XY_WORD(poly, 0) = sxy0;
                PRIM_XY_WORD(poly, 1) = sxy1;
                PRIM_XY_WORD(poly, 2) = sxy2;
                PRIM_XY_WORD(poly, 3) = sxy3;
                poly->r0              = 0x80;
                poly->r1              = 0x80;
                poly->g0              = 0;
                poly->b0              = 0;
                poly->g1              = 0;
                poly->b1              = 0;
                poly->r2              = 0x20;
                poly->g2              = 0x20;
                poly->b2              = 0x20;
                poly->r3              = 0x20;
                poly->g3              = 0x20;
                poly->b3              = 0x20;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        poly);
                dr                                 = (DR_MODE*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        dr);
            }
        }
        for (i = 0; i < 16; i++) {
            w->wave = (u32)rsin(phase + (i << 9)) >> 6;
            v0.vx   = w->x + w->dx;
            v0.vy   = w->y + w->wave;
            v0.vz   = w->z + w->dz * i;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
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
                poly                               = (POLY_G4*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                PRIM_XY_WORD(poly, 0) = sxy0;
                PRIM_XY_WORD(poly, 1) = sxy1;
                PRIM_XY_WORD(poly, 2) = sxy2;
                PRIM_XY_WORD(poly, 3) = sxy3;
                poly->r2              = 0x80;
                poly->r3              = 0x80;
                poly->g2              = 0;
                poly->b2              = 0;
                poly->g3              = 0;
                poly->b3              = 0;
                poly->r0              = 0x20;
                poly->g0              = 0x20;
                poly->b0              = 0x20;
                poly->r1              = 0x20;
                poly->g1              = 0x20;
                poly->b1              = 0x20;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        poly);
                dr                                 = (DR_MODE*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        dr);
            }
        }
    }
    SCRATCH_POP_BYTES(0xC);
}

/// Draws each surface in `D_shelter_b4_water_supply_8018265C` at height
/// `D_shelter_b4_water_supply_80182638` as two strips of 16 semi-transparent
/// Gouraud quads laid side by side along Z, projected through the view matrix.
/// The seam between the strips is lifted by a sine wave that runs along X and
/// scrolls with the display frame counter. The outer edges are coloured
/// (0x80, 0, 0) and the seam (0x20, 0x20, 0x20); each quad is followed by a
/// draw-mode packet selecting blend mode 2. Quads the projection flags as
/// invalid are skipped. The per-surface values live in a work block pushed on
/// the scratchpad stack for the duration of the call. `task`, the water task
/// whose drawing state calls it, is unused.
static void func_shelter_b4_water_supply_8017E5D8(Task* task)
{
    SVECTOR                      v0, v1, v2, v3;
    long                         sxy0, sxy1, sxy2, sxy3;
    long                         p, flag;
    s32                          phase;
    ShelterB4WaterSupplySurface* e;
    RoomWaterScratch*            w;
    u8*                          head;
    POLY_G4*                     poly;
    DR_MODE*                     dr;
    s32                          otz;
    s32                          i;

    e                          = D_shelter_b4_water_supply_8018265C;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    head                       = SCRATCH_HEAD(u8);
    phase                      = -(gDisplayState.animFrame * 16);
    SCRATCH_HEAD(u8)           = head - 0xC;
    w                          = (RoomWaterScratch*)(head - 0xC);
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    w->y = D_shelter_b4_water_supply_80182638;
    for (; e->end != -1; e++) {
        w->dx = e->width / 16;
        w->dz = e->depth / 2;
        w->x  = e->x;
        w->z  = e->z;
        for (i = 0; i < 16; i++) {
            v0.vx   = w->x + w->dx * i;
            v0.vy   = w->y;
            v0.vz   = w->z;
            v1.vx   = w->x + w->dx * (i + 1);
            v1.vy   = w->y;
            v1.vz   = w->z;
            w->wave = (u32)rsin(phase + (i << 9)) >> 6;
            v2.vx   = w->x + w->dx * i;
            v2.vy   = w->y + w->wave;
            v2.vz   = w->z + w->dz;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v3.vx   = w->x + w->dx * (i + 1);
            v3.vy   = w->y + w->wave;
            v3.vz   = w->z + w->dz;
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                               = (POLY_G4*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                PRIM_XY_WORD(poly, 0) = sxy0;
                PRIM_XY_WORD(poly, 1) = sxy1;
                PRIM_XY_WORD(poly, 2) = sxy2;
                PRIM_XY_WORD(poly, 3) = sxy3;
                poly->r0              = 0x80;
                poly->r1              = 0x80;
                poly->g0              = 0;
                poly->b0              = 0;
                poly->g1              = 0;
                poly->b1              = 0;
                poly->r2              = 0x20;
                poly->g2              = 0x20;
                poly->b2              = 0x20;
                poly->r3              = 0x20;
                poly->g3              = 0x20;
                poly->b3              = 0x20;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        poly);
                dr                                 = (DR_MODE*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        dr);
            }
        }
        for (i = 0; i < 16; i++) {
            w->wave = (u32)rsin(phase + (i << 9)) >> 6;
            v0.vx   = w->x + w->dx * i;
            v0.vy   = w->y + w->wave;
            v0.vz   = w->z + w->dz;
            w->wave = (u32)rsin(phase + ((i + 1) << 9)) >> 6;
            v1.vx   = w->x + w->dx * (i + 1);
            v1.vy   = w->y + w->wave;
            v1.vz   = w->z + w->dz;
            v2.vx   = w->x + w->dx * i;
            v2.vy   = w->y;
            v2.vz   = w->z + w->dz * 2;
            v3.vx   = w->x + w->dx * (i + 1);
            v3.vy   = w->y;
            v3.vz   = w->z + w->dz * 2;
            otz     = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                               = (POLY_G4*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                PRIM_XY_WORD(poly, 0) = sxy0;
                PRIM_XY_WORD(poly, 1) = sxy1;
                PRIM_XY_WORD(poly, 2) = sxy2;
                PRIM_XY_WORD(poly, 3) = sxy3;
                poly->r2              = 0x80;
                poly->r3              = 0x80;
                poly->g2              = 0;
                poly->b2              = 0;
                poly->g3              = 0;
                poly->b3              = 0;
                poly->r0              = 0x20;
                poly->g0              = 0x20;
                poly->b0              = 0x20;
                poly->r1              = 0x20;
                poly->g1              = 0x20;
                poly->b1              = 0x20;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        poly);
                dr                                 = (DR_MODE*)D_shelter_b4_water_supply_80184E50;
                D_shelter_b4_water_supply_80184E50 = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        dr);
            }
        }
    }
    SCRATCH_POP_BYTES(0xC);
}

/// The water task: runs its current state - `func_shelter_b4_water_supply_8017ED90`
/// once, then `func_shelter_b4_water_supply_8017EDD0`, which draws the surfaces -
/// and each tick publishes the room's water height to the session.
void func_shelter_b4_water_supply_8017ED28(Task* task)
{
    TaskFunc states[2] = { func_shelter_b4_water_supply_8017ED90, func_shelter_b4_water_supply_8017EDD0 };

    states[task->state](task);
    gGameSession->waterY = D_shelter_b4_water_supply_80182638;
}

/// The water task's opening state: clears the session's `field_80` or
/// `field_7E`, chosen by `Mc_SaveData[0].state.companionType`, and advances the task to its next state.
static void func_shelter_b4_water_supply_8017ED90(Task* arg0)
{
    if (Mc_SaveData[0].state.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// The water task's drawing state: points the primitive cursor
/// `D_shelter_b4_water_supply_80184E50` at the current buffer's 0xC000-byte
/// slice of one of two primitive areas, chosen by `Mc_SaveData[0].state.companionType`, then draws both
/// lists of water surfaces.
static void func_shelter_b4_water_supply_8017EDD0(Task* task)
{
    if (Mc_SaveData[0].state.companionType == 0) {
        D_shelter_b4_water_supply_80184E50 = (u8*)Fs_ActorLoadBase2 + gDisplayState.otBuffer * 0xC000;
    } else {
        D_shelter_b4_water_supply_80184E50 = (u8*)Fs_ActorLoadBase1 + gDisplayState.otBuffer * 0xC000;
    }
    func_shelter_b4_water_supply_8017DE74(task);
    func_shelter_b4_water_supply_8017E5D8(task);
}

/// Room task. State 0 installs five effect ids in the shared effect-id slots,
/// records the world positions of parts 14 and 17 of the slot-3 task's model,
/// and advances. Later states, while no event is running and `waterY` is below
/// that model's root, spawn each of two effects at water level under each part
/// with odds that grow with how far the part moved since last frame. Every
/// frame it then draws the light beams the current view selects, through
/// `func_shelter_b4_water_supply_80180260`.
void func_shelter_b4_water_supply_8017EE54(Task* arg0)
{
    Task*                        ctl;
    _ShelterB4WaterSupplySplash* splash;
    GfxCoord*                    ctlCoords;
    GfxCoord*                    part;
    GfxCoord                     surface;
    s32                          i;
    u32                          rnd;

    splash    = arg0->spawnArg2.pointer;
    ctl       = gameGetPtrSlot(3);
    ctlCoords = ctl->extra.tmd->coords;
    if (arg0->state == 0) {
        D_8011574C  = 0x60174;
        D_80115738  = 0x60175;
        D_80115734  = 0x60226;
        D_80115730  = 0x60231;
        D_80115754  = 0x6023C;
        arg0->state = 1;
        for (i = 0; i < 2; i++) {
            part                                     = &ctl->extra.tmd->coords[14 + i * 3];
            D_shelter_b4_water_supply_801826E0[i].vx = part->workm.t[0];
            D_shelter_b4_water_supply_801826E0[i].vy = part->workm.t[1];
            D_shelter_b4_water_supply_801826E0[i].vz = part->workm.t[2];
        }
    } else if (Gp_State1C->eventState == 0 && gGameSession->waterY < ctlCoords->coord.t[1]) {
        i = 0;
        for (; i < 2; i++) {
            part = &ctl->extra.tmd->coords[14 + i * 3];
            Gp_UpdateCoord(part);
            splash->strength = ABS(D_shelter_b4_water_supply_801826E0[i].vx - part->workm.t[0]) +
                               ABS(D_shelter_b4_water_supply_801826E0[i].vy - part->workm.t[1]) +
                               ABS(D_shelter_b4_water_supply_801826E0[i].vz - part->workm.t[2]) + 0x20;
            Gp_WorldToLocal(&gGfxViewCoord.workm, &part->workm, &surface.coord);
            surface.parent       = &gGfxViewCoord;
            surface.coord.t[1]   = gGameSession->waterY;
            surface.composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(&surface);
            rnd = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911);
            if ((s32)((rnd >> 16) & 0x1FF) < splash->strength) {
                Gp_SpawnEff(D_8011574C, &surface, 0x40, 0);
            }
            splash->strength -= 0x20;
            rnd               = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911);
            if ((s32)((rnd >> 16) & 0x1FF) < splash->strength) {
                Gp_SpawnEff(D_80115738, &surface, 0x1202180, 0);
            }
            D_shelter_b4_water_supply_801826E0[i].vx = part->workm.t[0];
            D_shelter_b4_water_supply_801826E0[i].vy = part->workm.t[1];
            D_shelter_b4_water_supply_801826E0[i].vz = part->workm.t[2];
        }
    }
    switch ((u8)Gp_GetViewIndex()) {
        case 4:
            func_shelter_b4_water_supply_80180260(&D_shelter_b4_water_supply_80182690[0], 0x200, 0);
            func_shelter_b4_water_supply_80180260(&D_shelter_b4_water_supply_80182690[4], 0x200, 0x800);
        case 2:
        case 3:
            func_shelter_b4_water_supply_80180260(D_shelter_b4_water_supply_80182670, 0x200, 0x800);
            break;
        case 6:
            func_shelter_b4_water_supply_80180260(D_shelter_b4_water_supply_801826A0, 0x200, 0);
        case 5:
            func_shelter_b4_water_supply_80180260(D_shelter_b4_water_supply_80182680, 0x200, 0x800);
            break;
        case 7:
            func_shelter_b4_water_supply_80180260(D_shelter_b4_water_supply_801826A0, 0x200, 0);
            break;
        case 8:
            func_shelter_b4_water_supply_80180260(&D_shelter_b4_water_supply_80182690[0], 0x200, 0);
            func_shelter_b4_water_supply_80180260(&D_shelter_b4_water_supply_80182690[4], 0x200, 0x800);
            break;
        case 9:
            func_shelter_b4_water_supply_80180260(D_shelter_b4_water_supply_801826D0, 0x200, 0x800);
        case 10:
        case 11:
            func_shelter_b4_water_supply_80180260(D_shelter_b4_water_supply_801826C0, 0x200, 0x800);
            break;
    }
}

/// Per-frame driver of an expanding, fading flash. While the room's event
/// state is 0 it updates the task's coordinate, ticks the age counter and
/// draws the flash through `func_shelter_b4_water_supply_8017F3A0` at size
/// `angle`, seeded from the spawn argument and grown by 0x20 a frame, and
/// brightness `scale`, which starts at 0x40 and drops by 2 a frame; the
/// first frame also turns the coordinate about Y by a random angle. The work
/// block is released once the brightness falls under 2. Once the event state
/// is non-zero it only draws, releasing the block from event state 4 on.
void func_shelter_b4_water_supply_8017F24C(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.disp2d->coord;
    if (Gp_State1C->eventState != 0) {
        func_shelter_b4_water_supply_8017F3A0(coord, work->angle, work->scale);
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
        func_shelter_b4_water_supply_8017F3A0(coord, work->angle, work->scale);
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
static void func_shelter_b4_water_supply_8017F3A0(GfxCoord* arg0, s32 arg1, s32 arg2)
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

/// Per-frame driver of a particle, drawn as the spinning sprite of
/// `func_shelter_b4_water_supply_8017FB90` or, when the spawn argument's top
/// nibble is set, the upright sprite of `func_shelter_b4_water_supply_8017FF7C`.
/// The first frame takes the size from the argument's low 12 bits, a random
/// angle, and the ticks per animation frame from bits 12-15. Unless the work
/// block already carries a velocity it picks one by the kind in bits 24-27 -
/// none, a random upward burst, a random spray, a narrow upward jet, or the
/// work block's stored direction - scaled to the speed in bits 16-23 (0x40
/// when zero). Every later tick draws, moves the coordinate by the velocity
/// with gravity pulling it down, and releases the block after animation frame
/// 7. While the room's event state is non-zero it only draws, releasing the
/// block from event state 4 on.
void func_shelter_b4_water_supply_8017F6D4(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    SVECTOR*   vec;
    s32        kind;
    s32        step;
    s32        state;
    s32        level;

    work  = task->spawnArg2.pointer;
    coord = task->extra.disp2d->coord;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            if (task->state < 2) {
                func_shelter_b4_water_supply_8017FB90(coord, work->index, work->scale, work->angle);
            } else {
                func_shelter_b4_water_supply_8017FF7C(coord, work->index, work->scale);
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
            func_shelter_b4_water_supply_8017FB90(coord, work->index, work->scale, work->angle);
            break;
        case 2:
            func_shelter_b4_water_supply_8017FF7C(coord, work->index, work->scale);
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
static void func_shelter_b4_water_supply_8017FB90(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
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

/// Draws an upright sprite at the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, one
/// semi-transparent, unshaded `POLY_FT4` (tpage 0x2B, clut 0x43D2) is queued
/// as an axis-aligned square of half-side `r = arg2 * 55 / otz`, raised so the
/// projected point sits three quarters of the way down it. `arg1` picks one of
/// eight 56-texel frames in a grid four wide, starting at v 0x70.
static void func_shelter_b4_water_supply_8017FF7C(GfxCoord* arg0, s16 arg1, s16 arg2)
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
    SCRATCH_POP(GpRingScratch);
}

/// Draws a flickering grey light beam from `arg0[0]` to `arg0[1]`. Both points
/// are projected through the view matrix; unless the far end is nearer than
/// OTZ 0x11, gouraud `POLY_G4` wedges around each end (radius
/// `(s16)arg1 * 64 / otz` at that end) are joined by quads between the two,
/// each fading from grey on the axis to black at the rim. `arg2` turns the
/// wedges about the axis. The grey alternates between 0x20 and 0x28 with the
/// display frame counter.
static void func_shelter_b4_water_supply_80180260(SVECTOR* arg0, s32 arg1, s32 arg2)
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

/// Per-frame driver of a glowing disc anchored to its parent at the work
/// block's position. State 1 grows the disc and, every fourth tick, spawns the
/// effect `D_80115730` names at a random one of joints 3-18 of the slot-3
/// task's model and adopts it as a child task; state 2 keeps growing it and
/// adds a half-bright second disc on odd ticks; state 3 drifts the disc away
/// while it fades inside an expanding ring, then releases the work block, as
/// does state 4. The spawn argument picks the disc's tint from
/// `D_shelter_b4_water_supply_801826F0`. Nothing runs while the room's event
/// state is set, and the block is released once that state reaches 4.
void func_shelter_b4_water_supply_801809DC(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    GpEffWork* spawned;
    MATRIX*    mtx;
    u8         col[4];

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.disp2d->coord;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    mem->age++;
    switch (arg0->state) {
        case 0:
            coord->parent                    = mem->parent;
            mtx                              = &coord->coord;
            MATRIX_PAIR(&coord->coord, 0, 0) = 0x1000;
            MATRIX_PAIR(mtx, 0, 2)           = 0;
            MATRIX_PAIR(mtx, 1, 1)           = 0x1000;
            MATRIX_PAIR(mtx, 2, 0)           = 0;
            mtx->m[2][2]                     = 0x1000;
            coord->coord.t[0]                = mem->pos.vx;
            coord->coord.t[1]                = mem->pos.vy;
            coord->coord.t[2]                = mem->pos.vz;
            coord->composeStamp              = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            break;
        case 1:
            Gp_UpdateCoord(coord);
            if (!(mem->age & 3)) {
                Task* player = gameGetPtrSlot(3);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                spawned      = Gp_SpawnEff(D_80115730, &player->extra.tmd->coords[(((u32)Gp_LcgState >> 16) & 0xF) + 3], coord, NULL);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->task);
                }
            }
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b4_water_supply_801826F0[arg0->spawnArg1.value].r;
            col[1] = mem->scale >> D_shelter_b4_water_supply_801826F0[arg0->spawnArg1.value].g;
            col[2] = mem->scale >> D_shelter_b4_water_supply_801826F0[arg0->spawnArg1.value].b;
            func_shelter_b4_water_supply_80181800(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b4_water_supply_801826F0[arg0->spawnArg1.value].r;
            col[1] = mem->scale >> D_shelter_b4_water_supply_801826F0[arg0->spawnArg1.value].g;
            col[2] = mem->scale >> D_shelter_b4_water_supply_801826F0[arg0->spawnArg1.value].b;
            func_shelter_b4_water_supply_80181800(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_shelter_b4_water_supply_80181800(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_shelter_b4_water_supply_801826F0[arg0->spawnArg1.value].r;
            col[1] = mem->scale >> D_shelter_b4_water_supply_801826F0[arg0->spawnArg1.value].g;
            col[2] = mem->scale >> D_shelter_b4_water_supply_801826F0[arg0->spawnArg1.value].b;
            func_shelter_b4_water_supply_80181800(coord, mem->angle, col);
            col[0] = mem->scale;
            col[1] = mem->scale >> 1;
            col[2] = mem->scale >> 2;
            if (mem->period == 0) {
                mem->move.vy = -0x100;
                mem->move.vz = 0x100;
                mem->move.vx = 0;
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
            }
            mem->period       += 8;
            coord->workm.t[0] += mem->move.vx;
            coord->workm.t[1] += mem->move.vy;
            coord->workm.t[2] += mem->move.vz;
            func_shelter_b4_water_supply_801813DC(coord, (s16)(mem->period + 0x80), 0x100, col);
            mem->angle -= 0x10;
            if (mem->scale > 0x10) {
                mem->scale -= 0x10;
                break;
            }
            Gp_ReleaseState1CMem(mem, arg0);
            break;
        case 4:
            Gp_ReleaseState1CMem(mem, arg0);
            break;
    }
}

/// Per-frame driver of a sprite drifting toward the coordinate passed as the
/// spawn argument. The first frame takes the displacement from the task's
/// coordinate to that target, brings it into the coordinate's parent frame
/// and scales it to 0xCC/0x1000 of its length; each later frame adds that step
/// to the coordinate and, on every other tick, draws the sprite through
/// `func_shelter_b4_water_supply_80181158` at the next animation frame. The
/// work block is released at tick 20, or once the room's event state reaches
/// 4; from event state 1 on the sprite is neither moved nor drawn.
void func_shelter_b4_water_supply_80180F34(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    GfxCoord*  target;
    VECTOR     delta;

    work   = task->spawnArg2.pointer;
    coord  = task->extra.disp2d->coord;
    target = task->spawnArg1.pointer;
    if (Gp_State1C->eventState == 0) {
        work->age++;
        switch (task->state) {
            case 0:
                delta.vx = target->workm.t[0] - coord->workm.t[0];
                delta.vy = target->workm.t[1] - coord->workm.t[1];
                delta.vz = target->workm.t[2] - coord->workm.t[2];
                ApplyTransposeMatrixLV(&coord->workm, &delta, &delta);
                work->pos.vx = delta.vx;
                work->pos.vy = delta.vy;
                work->pos.vz = delta.vz;
                gte_SetRotMatrix(&coord->coord);
                gte_ldv0(&work->pos);
                gte_rtv0();
                gte_stsv(&work->pos);
                gte_lddp(0xCC);
                gte_ldsv(&work->pos);
                gte_gpf12();
                gte_stsv(&work->pos);
                task->state = 1;
                break;
            case 1:
                coord->coord.t[0]  += work->pos.vx;
                coord->coord.t[1]  += work->pos.vy;
                coord->coord.t[2]  += work->pos.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    func_shelter_b4_water_supply_80181158(coord, ++work->index, 0x200, 0x80);
                }
                if (work->age >= 20) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    } else if (Gp_State1C->eventState >= 4) {
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Queues a semi-transparent textured square centred on the projected world
/// position of `arg0`, of half-size `arg2` scaled by depth. `arg1 & 3` picks
/// the animation frame from a row of four 24-texel frames and `arg3` is the
/// grey level. Nothing is drawn when the projection overflows.
static void func_shelter_b4_water_supply_80181158(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    DisplayState*  ds;
    s32            tex;
    s32            sarg;
    s32            t;
    s16            xy;
    u16            vz;

    tex                                     = arg1;
    scratch                                 = (void**)G_SCRATCH_HEAD;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = arg0->workm.t[1];
    vz                                      = arg0->workm.t[2];
    *scratch                                = block;
    block->vec.vz                           = vz;
    vec                                     = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        prim->clut  = 0x42CB;
        t           = (tex & 3) * 24;
        setRGB0(prim, arg3, arg3, arg3);
        setUVWH(prim, t + 0x60, 0, 0x17, 0x17);
        sarg        = (s16)arg2;
        t           = sarg * 24;
        block->step = (t - sarg) / block->otz;
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
    SCRATCH_POP_BYTES_AT(scratch, 0x18);
}

/// Draws a ring around the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, sixteen gouraud
/// `POLY_G4` segments are queued between on-screen radii `(s16)arg1 * 64 /
/// (otz + 1)` and `(s16)(arg1 + arg2) * 64 / (otz + 1)`, black at the first
/// and coloured `rgb` at the second.
static void func_shelter_b4_water_supply_801813DC(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    GpArcScratch* block;
    POLY_G4*      prim;
    s32           ang;
    s32           next;
    s32           outer;

    block         = SCRATCH_PUSH(GpArcScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    outer         = arg1 + arg2;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->inner = ((s16)arg1 * 64) / block->otz;
        block->outer = ((s16)outer * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang = next) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->inner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->inner * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->sx + ((block->inner * rsin(next)) >> 12);
            prim->y1 = block->sy + ((block->inner * rcos(next)) >> 12);
            prim->x2 = block->sx + ((block->outer * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->outer * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->outer * rsin(next)) >> 12);
            prim->y3 = block->sy + ((block->outer * rcos(next)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(GpArcScratch);
}

/// Draws a round glow at the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, eight gouraud
/// `POLY_G4` wedges of on-screen radius `arg1 * 64 / (otz + 1)` are queued
/// around the projected point, coloured `rgb` at the centre and black at the
/// rim.
static void func_shelter_b4_water_supply_80181800(GfxCoord* arg0, s16 arg1, u8* rgb)
{
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;
    s32            otz;

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
        otz         = block->otz + 1;
        block->otz  = otz;
        block->step = (arg1 * 64) / otz;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->step * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->step * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->step * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->step * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->step * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->step * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Per-frame driver of a flare: every tick it draws an orange round glow
/// through `func_shelter_b4_water_supply_80181800` and the textured glow of
/// `func_shelter_b4_water_supply_80181D40`, both growing by 0x10 a tick. While
/// the ring's brightness stays above 0x18 an expanding orange ring is drawn
/// around them, fading by 0x18 a tick; after that the glow's own brightness
/// fades by 0x18 a tick and the work block is released once it drops under
/// 0x18. Nothing runs while the room's event state is set, and the block is
/// released once that state reaches 4.
void func_shelter_b4_water_supply_80181B94(Task* arg0)
{
    u8         rgb[3];
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        flag;
    s16        step;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.disp2d->coord;
    if (flag != 0) {
        if (flag < 4) {
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
        func_shelter_b4_water_supply_80181800(coord, (s16)(step * 2), rgb);
        func_shelter_b4_water_supply_80181D40(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            func_shelter_b4_water_supply_801813DC(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
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
static void func_shelter_b4_water_supply_80181D40(GfxCoord* coord, s16 size)
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
            func_shelter_b4_water_supply_8018226C(&ground, outerSize);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Queues one semi-transparent textured quad lying flat at the coordinate's
/// world position: the unit quad `D_80111E38` is scaled by `arg1`, turned by
/// the view matrix and projected through `GsWSMATRIX`. Unless the GTE flags
/// the projection, the quad is coloured (0x30, 0x20, 0x20) and its texture
/// alternates between two 32-pixel columns on successive frames.
static void func_shelter_b4_water_supply_8018226C(GfxCoord* arg0, s32 arg1)
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

    scratch  = (void**)G_SCRATCH_HEAD;
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
