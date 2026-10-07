#include "rooms/shelter_b3_garbage_incinerator.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "shelter_b3_garbage_incinerator_private.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/cap.h"
#include "gameplay/collision.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/room_common.h"

/// Selects this room's billboard declaration with `s32` frame and size arguments.
///
/// Presence-only configuration for the first inclusion of `effect_sprite.h`;
/// the replacement value is unused. Leave the halfword argument flag undefined
/// and undefine this flag after the header. The drawer narrows frame to `u16`
/// and size to `s16` internally; the call signature remains word-sized.
#define EFFECT_SPRITE_BILLBOARD_WORD_ARGUMENTS
/// Binds the shared debris task to the incinerator's exported `void (Task*)` callback.
///
/// Supply this function identifier before `effect_sprite.h` and keep it defined
/// through the debris fragment, which clears it. No arguments or tokens are built.
#define EFFECT_SPRITE_DEBRIS_TASK shelterB3GarbageIncineratorEffectSpriteDebrisTask
/// Binds the aimed drift fragment to this room's exported `void (Task*)` callback.
///
/// Supply a function identifier before `effect_sprite.h` and retain it through
/// the aimed drift fragment, which clears it. The two Shelter B3 carriers each
/// supply their own export; no arguments, captured locals or tokens are built.
#define EFFECT_SPRITE_DRIFT_AIMED_TASK shelterB3GarbageIncineratorEffectSpriteDriftTaskAimed
#include "../../shared/effect_sprite.h"
#undef EFFECT_SPRITE_BILLBOARD_WORD_ARGUMENTS

#include "../../shared/screen_wave.h"
#include "../../shared/glow_draw.h"

static void _effectSpriteDrawBanked(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);
static void _effectSpriteDrawRotated(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);

#define D_shelter_b3_garbage_incinerator_8018754C (D_shelter_b3_garbage_incinerator_80187544 + 1)
#define D_shelter_b3_garbage_incinerator_80187554 (D_shelter_b3_garbage_incinerator_80187544 + 2)
#define D_shelter_b3_garbage_incinerator_80187564 (D_shelter_b3_garbage_incinerator_80187544 + 4)
#define D_shelter_b3_garbage_incinerator_80187574 (D_shelter_b3_garbage_incinerator_80187544 + 6)
#define D_shelter_b3_garbage_incinerator_8018759C (D_shelter_b3_garbage_incinerator_80187544 + 11)
#define D_shelter_b3_garbage_incinerator_801875AC (D_shelter_b3_garbage_incinerator_80187544 + 13)
#define D_shelter_b3_garbage_incinerator_801875B4 (D_shelter_b3_garbage_incinerator_80187544 + 14)
#define D_shelter_b3_garbage_incinerator_80187614 (D_shelter_b3_garbage_incinerator_80187544 + 26)

static void _shelterB3GarbageIncineratorDrawPulsingDisc(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor, s32 pulseRate);
static void _shelterB3GarbageIncineratorDrawLayeredGlow(const SVECTOR* worldPoint, u16 radiusScale, u16 packedColor, u16 pulseAndRotation);

extern TaskDesc D_actor_341900_80164190[];

/// Four halfwords per entry, read as the two floor-level end points of a wall
/// edge: `[0]`/`[1]` for the first corner and `[2]`/`[3]` for the second.
extern u16 D_shelter_b3_garbage_incinerator_8018FBFC[][4];

/// Same layout as `D_shelter_b3_garbage_incinerator_8018FBFC`, for the wall
/// edges built by `shelterB3GarbageIncineratorSetExitCollisionWalls`.
extern u16 D_shelter_b3_garbage_incinerator_8018FBCC[][4];

// Indexed views below share one contiguous table.
extern TaskDesc D_actor_207000_801575F0;
extern TaskDesc D_shelter_b3_garbage_incinerator_8018FAC0[2];
void            func_shelter_b3_garbage_incinerator_80184D84(Task*);
static void     _shelterB3GarbageIncineratorKillTask(Task* task);

OverlayEncounterSpot D_shelter_b3_garbage_incinerator_801874C4[16] = {
    { 2000, -2500, 300, 2048 },
    { 5000, -2500, 300, 2048 },
    { 8000, -2500, 300, 2048 },
    { 11000, -2500, 300, 2048 },
    { 14000, -2500, 300, 2048 },
    { 16800, -2500, -2500, 3072 },
    { 16800, -2500, -5500, 3072 },
    { 16800, -2500, -8500, 3072 },
    { 16800, -2500, -11500, 3072 },
    { 16800, -2500, -24500, 3072 },
    { 2000, -2500, -5300, 0 },
    { 5000, -2500, -5300, 0 },
    { 8000, -2500, -5300, 0 },
    { 11200, -2500, -8500, 1024 },
    { 11200, -2500, -11500, 1024 },
    { 11200, -2500, -24500, 1024 },
};

SVECTOR D_shelter_b3_garbage_incinerator_80187544[79] = {
    { 16710, -2200, -13430, 0 },
    { 16710, -2200, -22500, 0 },
    { 16720, -2180, -14300, 0 },
    { 16630, -2200, -21580, 0 },
    { -1590, -1060, -140, 0 },
    { 0, -2050, -900, 0 },
    { 14000, -2150, -26000, 0 },
    { 13500, -1980, -26200, 0 },
    { 14500, -1980, -26200, 0 },
    { 17100, -2030, -13900, 0 },
    { 17100, -2030, -14400, 0 },
    { 2000, -1995, 300, 0 },
    { 5000, -1995, 300, 0 },
    { 8000, -1995, 300, 0 },
    { 11000, -1995, 300, 0 },
    { 14000, -1995, 300, 0 },
    { 2000, -1995, -5300, 0 },
    { 5000, -1995, -5300, 0 },
    { 8000, -1995, -5300, 0 },
    { 16800, -1995, -2500, 0 },
    { 16800, -1995, -5500, 0 },
    { 16800, -1995, -8500, 0 },
    { 16800, -1995, -11500, 0 },
    { 16800, -1995, -24500, 0 },
    { 11200, -1995, -8500, 0 },
    { 11200, -1995, -11500, 0 },
    { 11200, -1995, -24500, 0 },
    { 1000, -5050, -850, 0 },
    { 3000, -5050, -850, 0 },
    { 4000, -5050, -850, 0 },
    { 6000, -5050, -850, 0 },
    { 7000, -5050, -850, 0 },
    { 9000, -5050, -850, 0 },
    { 10000, -5050, -850, 0 },
    { 11750, -5050, -850, 0 },
    { 1000, -5050, -4140, 0 },
    { 3000, -5050, -4140, 0 },
    { 4000, -5050, -4140, 0 },
    { 6000, -5050, -4140, 0 },
    { 7000, -5050, -4140, 0 },
    { 9000, -5050, -4140, 0 },
    { 10000, -5050, -4140, 0 },
    { 11750, -5050, -4140, 0 },
    { 13250, -5050, -1760, 0 },
    { 14750, -5050, -1760, 0 },
    { 13250, -5050, -3250, 0 },
    { 14750, -5050, -3250, 0 },
    { 12350, -5050, -4750, 0 },
    { 12350, -5050, -6250, 0 },
    { 12350, -5050, -7500, 0 },
    { 12350, -5050, -9500, 0 },
    { 12350, -5050, -10500, 0 },
    { 12350, -5050, -12500, 0 },
    { 12350, -5050, -13450, 0 },
    { 12350, -5050, -14470, 0 },
    { 12350, -5050, -15500, 0 },
    { 12350, -5050, -17500, 0 },
    { 12350, -5050, -18500, 0 },
    { 12350, -5050, -20500, 0 },
    { 12350, -5050, -21530, 0 },
    { 12350, -5050, -22550, 0 },
    { 12350, -5050, -23500, 0 },
    { 12350, -5050, -25500, 0 },
    { 15640, -5050, -4750, 0 },
    { 15640, -5050, -6250, 0 },
    { 15640, -5050, -7500, 0 },
    { 15640, -5050, -9500, 0 },
    { 15640, -5050, -10500, 0 },
    { 15640, -5050, -12500, 0 },
    { 15640, -5050, -13450, 0 },
    { 15640, -5050, -14470, 0 },
    { 15640, -5050, -15500, 0 },
    { 15640, -5050, -17500, 0 },
    { 15640, -5050, -18500, 0 },
    { 15640, -5050, -20500, 0 },
    { 15640, -5050, -21530, 0 },
    { 15640, -5050, -22550, 0 },
    { 15640, -5050, -23500, 0 },
    { 15640, -5050, -25500, 0 },
};

static SVECTOR _gShelterB3GarbageIncineratorCollision0ADC8Normals[29] = {
#include "assets/shelter_b3_garbage_incinerator_collision_0ADC8_normals.inc"
};

static SVECTOR _gShelterB3GarbageIncineratorCollision0ADC8Verts[84] = {
#include "assets/shelter_b3_garbage_incinerator_collision_0ADC8_verts.inc"
};

static WorldCollisionGridFace _gShelterB3GarbageIncineratorCollision0ADC8Faces[64] = {
#include "assets/shelter_b3_garbage_incinerator_collision_0ADC8_faces.inc"
};

static s16 _gShelterB3GarbageIncineratorCollision0ADC8Cells[604] = {
#include "assets/shelter_b3_garbage_incinerator_collision_0ADC8_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB3GarbageIncineratorCollision0ADC8Cells[i])
static s16* _gShelterB3GarbageIncineratorCollision0ADC8Table[35] = {
#include "assets/shelter_b3_garbage_incinerator_collision_0ADC8_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b3_garbage_incinerator_80188388[1] = {
    { NULL, _gShelterB3GarbageIncineratorCollision0ADC8Normals, _gShelterB3GarbageIncineratorCollision0ADC8Verts, _gShelterB3GarbageIncineratorCollision0ADC8Faces, _gShelterB3GarbageIncineratorCollision0ADC8Table, 0, 0x6590, 5, 7, 4000, 64 },
};

ViewCamera D_shelter_b3_garbage_incinerator_801883AC[40] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x2710, 0x6D60, 0x32C8 } }, 207 },
    { { { { 453, 0, 4070 }, { -980, 3975, 109 }, { -3951, -986, 439 } }, { -8876, 504, 2982 } }, 257 },
    { { { { 558, 0, 4057 }, { -776, 4020, 106 }, { -3982, -783, 547 } }, { -0x3544, 864, 3002 } }, 230 },
    { { { { 4046, 0, 635 }, { -41, 4087, 264 }, { -634, -267, 4037 } }, { -0x374C, 1334, 0x2A78 } }, 230 },
    { { { { 4055, 0, -575 }, { 4, 4095, 29 }, { 575, -30, 4055 } }, { -0x33E1, 1404, 0x3BAD } }, 257 },
    { { { { 4049, 0, -615 }, { -11, 4095, -72 }, { 615, 73, 4048 } }, { -0x3337, 1779, 0x4BD4 } }, 257 },
    { { { { 4025, 0, -756 }, { -16, 4095, -87 }, { 756, 88, 4024 } }, { -0x32C9, 1909, 0x5D13 } }, 257 },
    { { { { 3997, 0, -892 }, { -49, 4089, -220 }, { 890, 225, 3991 } }, { -0x322E, 1984, 0x64DE } }, 199 },
    { { { { -4031, 0, -724 }, { -69, 4076, 388 }, { 720, 395, -4012 } }, { -0x32B0, 2164, 0x4684 } }, 257 },
    { { { { 2811, 0, -2978 }, { 675, 3989, 637 }, { 2901, -928, 2738 } }, { -0x3B20, 1362, 0x3D5C } }, 329 },
    { { { { 3233, 0, 2514 }, { 1855, 2763, -2386 }, { -1696, 3022, 2181 } }, { -1336, 4404, 4802 } }, 230 },
    { { { { 336, 0, -4082 }, { 454, 4070, 37 }, { 4056, -455, 334 } }, { -2456, 1144, 2572 } }, 289 },
    { { { { 591, 0, 4053 }, { -591, 4052, 86 }, { -4009, -597, 585 } }, { -9836, 384, 4072 } }, 911 },
    { { { { 1113, 0, -3941 }, { -1781, 3653, -503 }, { 3516, 1851, 993 } }, { -0x351C, 2804, 0x3A9A } }, 257 },
    { { { { 1113, 0, -3941 }, { -1781, 3653, -503 }, { 3516, 1851, 993 } }, { -0x351C, 2804, 0x5A02 } }, 257 },
    { { { { 4055, 0, -575 }, { -255, 3668, -1803 }, { 515, 1821, 3632 } }, { -0x3337, 4529, 0x4FC6 } }, 257 },
    { { { { 453, 0, 4070 }, { -980, 3975, 109 }, { -3951, -986, 439 } }, { -8876, 504, 2982 } }, 257 },
    { { { { 4096, 0, 0 }, { 0, 3778, -1580 }, { 0, 1580, 3778 } }, { -0x351C, 2884, 0x6560 } }, 329 },
    { { { { -2520, 0, 3228 }, { -1199, 3803, -935 }, { -2997, -1521, -2340 } }, { -0x3D5A, -40, 0x4AF8 } }, 257 },
    { { { { 3847, 0, 1405 }, { 1293, 1604, -3539 }, { -550, 3768, 1506 } }, { -0x379C, 3029, 0x55AC } }, 329 },
    { { { { -4095, 0, 5 }, { -2, 3777, -1584 }, { -5, -1584, -3777 } }, { -0x35BC, 1004, 0x5070 } }, 257 },
    { { { { 453, 0, 4070 }, { -980, 3975, 109 }, { -3951, -986, 439 } }, { -8876, 504, 2982 } }, 257 },
    { { { { 558, 0, 4057 }, { -776, 4020, 106 }, { -3982, -783, 547 } }, { -0x3544, 864, 3002 } }, 230 },
    { { { { 4046, 0, 635 }, { -41, 4087, 264 }, { -633, -267, 4037 } }, { -0x374C, 1334, 0x2A78 } }, 230 },
    { { { { 4055, 0, -575 }, { 4, 4095, 29 }, { 575, -30, 4055 } }, { -0x33E1, 1404, 0x3BAD } }, 257 },
    { { { { 4049, 0, -615 }, { -11, 4095, -72 }, { 615, 73, 4048 } }, { -0x3337, 1779, 0x4BD4 } }, 257 },
    { { { { 4025, 0, -756 }, { -16, 4095, -87 }, { 756, 88, 4024 } }, { -0x32C9, 1909, 0x5D13 } }, 257 },
    { { { { 3997, 0, -892 }, { -49, 4089, -220 }, { 890, 225, 3991 } }, { -0x322E, 1984, 0x64DE } }, 199 },
    { { { { -4031, 0, -724 }, { -69, 4076, 388 }, { 720, 395, -4012 } }, { -0x32B0, 2164, 0x4684 } }, 257 },
    { { { { 1113, 0, -3941 }, { -1781, 3653, -503 }, { 3516, 1851, 993 } }, { -0x351C, 2804, 0x3A9A } }, 257 },
    { { { { 1113, 0, -3941 }, { -1781, 3653, -503 }, { 3516, 1851, 993 } }, { -0x351C, 2804, 0x5A02 } }, 257 },
    { { { { 4055, 0, -575 }, { -255, 3668, -1803 }, { 515, 1821, 3632 } }, { -0x3337, 4529, 0x4FC6 } }, 257 },
    { { { { 4096, 0, 0 }, { 0, 3778, -1580 }, { 0, 1580, 3778 } }, { -0x351C, 2884, 0x6560 } }, 329 },
    { { { { 3997, 0, -892 }, { -49, 4089, -220 }, { 890, 225, 3991 } }, { -0x322E, 1984, 0x64DE } }, 199 },
    { { { { 4049, 0, -615 }, { -11, 4095, -72 }, { 615, 73, 4048 } }, { -0x3337, 1779, 0x4BD4 } }, 257 },
    { { { { 4025, 0, -756 }, { -16, 4095, -87 }, { 756, 88, 4024 } }, { -0x32C9, 1909, 0x5D13 } }, 257 },
    { { { { 3997, 0, -892 }, { -49, 4089, -220 }, { 890, 225, 3991 } }, { -0x322E, 1984, 0x64DE } }, 199 },
    { { { { 1113, 0, -3941 }, { -1781, 3653, -503 }, { 3516, 1851, 993 } }, { -0x351C, 2804, 0x3A9A } }, 257 },
    { { { { 1113, 0, -3941 }, { -1781, 3653, -503 }, { 3516, 1851, 993 } }, { -0x351C, 2804, 0x5A02 } }, 257 },
    { { { { 4096, 0, 0 }, { 0, 3778, -1580 }, { 0, 1580, 3778 } }, { -0x351C, 2884, 0x6560 } }, 329 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018894C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018895C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018896C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_garbage_incinerator_8018897C[14] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 80, 1050, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -120, 56, 1050, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -120, -56, 1087, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -120, -120, 1100, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -128, -64, 1087, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -128, -120, 1112, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -128, -8, 1075, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -128, 48, 1050, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -136, -120, 1075, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -136, -72, 1050, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -136, -16, 1025, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -136, 40, 1000, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 0, 1075, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 24, 1068, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_80188A94[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b3_garbage_incinerator_80188AAC[2] = {
    { { 27, 0, 292, 239 }, 1012 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_shelter_b3_garbage_incinerator_80188AC0[11] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 56, 1900, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 40, 1950, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -104, -120, 2000, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -104, -64, 1975, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -104, 0, 1950, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -112, 0, 1937, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -120, -8, 1925, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -112, -64, 1962, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -120, -72, 1950, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -112, -120, 1987, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, -120, 1975, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_80188B9C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b3_garbage_incinerator_80188BB4[2] = {
    { { 42, 0, 278, 239 }, 1962 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_shelter_b3_garbage_incinerator_80188BC8[8] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, 56, 1150, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 144, 64, 1175, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, 72, 1150, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 144, 0, 1175, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 8, 1150, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, -40, 1150, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 144, -40, 1175, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, -40, 1150, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_80188C68[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b3_garbage_incinerator_80188C80[2] = {
    { { 83, 0, 237, 239 }, 2875 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_shelter_b3_garbage_incinerator_80188C94[5] = {
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 48, -32, 2300, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 56, -40, 2250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 64, -40, 2200, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 80, -48, 2100, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 72, -48, 2150, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_80188CF8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b3_garbage_incinerator_80188D10[2] = {
    { { 85, 0, 235, 239 }, 4000 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_shelter_b3_garbage_incinerator_80188D24[47] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 56, 1450, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 112, 825, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 48, 72, 1225, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 112, 72, 1250, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 56, 80, 1100, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 120, 80, 1175, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 88, 1125, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 64, 88, 1125, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 8, 96, 950, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 24, 112, 825, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 72, 96, 950, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 80, 104, 900, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 88, 112, 825, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 96, 950, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 104, 900, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 0, 80, 1087, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -8, 72, 1200, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 0, 88, 1000, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -64, 88, 1000, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 16, 104, 875, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, 72, 1187, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -56, 80, 1075, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -64, 96, 925, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 0, 112, 825, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -48, 104, 875, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 104, 875, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -64, 112, 825, { .fields = { 56, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -120, 80, 1075, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -128, 88, 1000, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -128, 96, 925, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -112, 72, 1162, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 104, 875, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -48, 64, 1275, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -16, 64, 1275, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 104, 64, 1325, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 96, 56, 1450, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 48, 1450, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -128, 104, 875, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -128, 112, 825, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, 56, 1387, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 56, 1375, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, 64, 1312, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 64, 1300, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 16, -24, 2725, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, -32, 2675, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 32, -32, 2625, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 40, -32, 2575, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_801890D0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 43, 0, 0, { 1, 0 } },
    { 43, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b3_garbage_incinerator_801890F0[2] = {
    { { 98, 0, 222, 239 }, 4250 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteBatch D_shelter_b3_garbage_incinerator_80189104[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_garbage_incinerator_80189114[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_garbage_incinerator_80189124[47] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -120, 1125, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -104, 1125, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -88, 1125, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 24, 232, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 56, 225, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 112, 450, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -160, 104, 337, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, 104, 425, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -160, 96, 300, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 96, 400, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 88, 275, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -120, 88, 375, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 80, 225, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, 80, 350, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 72, 225, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 72, 325, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 64, 225, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -144, 64, 225, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -152, 56, 225, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 48, 250, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 40, 250, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 32, 243, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, -16, 1500, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, -112, 1125, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 88, -112, 1000, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 96, -120, 1000, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 88, -104, 1000, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 88, -96, 1000, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, -96, 1125, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, -88, 1000, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, -80, 1125, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, -80, 1000, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, -72, 1000, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, -72, 1125, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 80, -64, 1075, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -64, 1200, { .fields = { 112, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, -56, 1075, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -56, 1200, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, -48, 1200, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, -48, 1325, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, -40, 1325, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, -40, 1450, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, -32, 1375, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, -32, 1500, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, -24, 1437, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, -24, 1562, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -160, 112, 375, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_801894D0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 47, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_garbage_incinerator_801894E8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_garbage_incinerator_801894F8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_garbage_incinerator_80189508[35] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -64, 825, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -64, 712, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 8, 825, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 16, 937, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, 16, 937, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, -16, 825, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, -16, 800, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, -8, 800, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -8, 800, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, -56, 775, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, -56, 750, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, -48, 750, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -48, 750, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, 24, 875, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, 24, 875, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, 32, 875, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 32, 875, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 64, 925, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, 64, 925, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 72, 925, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 72, 925, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, 8, 917, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 24, 937, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, -64, 812, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -64, 812, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, -24, 850, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -24, 850, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, 16, 950, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, 16, 950, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, 56, 950, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 56, 950, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -32, 825, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 40, 48, 937, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -48, 825, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, -56, 825, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_801897C4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 35, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_garbage_incinerator_801897DC[58] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -32, 820, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 0, 812, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 8, 812, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 16, 812, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 24, 925, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 88, 987, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, 40, 925, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 64, 8, 925, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, -72, 750, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, -24, 762, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, 24, 912, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, -72, 750, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, -24, 762, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, 24, 912, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 120, -64, 787, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 136, -64, 787, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 120, -16, 800, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 136, -16, 800, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 120, 32, 950, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 136, 32, 950, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, 40, 987, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, -8, 837, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, -56, 825, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -32, 812, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, 48, 925, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, 56, 925, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, 56, 950, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 72, 64, 950, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 72, 925, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 104, 72, 925, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 80, 962, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 136, 80, 962, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -24, 812, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 24, 925, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 32, 925, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -80, 750, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -32, 787, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, 16, 950, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, -80, 700, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 56, -56, 812, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 56, -32, 812, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 72, 1025, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 56, 1075, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 64, 1037, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 80, 962, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -104, 88, 925, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -112, 96, 887, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 104, 875, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -120, 112, 850, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 96, 912, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 112, 875, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 112, 900, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -88, 104, 900, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 80, 987, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 88, 950, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 48, 1143, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 40, 1143, { .fields = { 40, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 72, 1000, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_80189C64[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 41, 0, 0, { 1, 0 } },
    { 41, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_garbage_incinerator_80189C84[62] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -120, 3250, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -120, 3125, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -112, 3125, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -112, 3125, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -104, 2975, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -104, 3187, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -96, 2987, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -96, 3193, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -88, 2987, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -88, 3237, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -80, 2987, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -80, 3237, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -72, 2987, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -72, 3375, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -64, 3250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -64, 3350, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, -64, 3350, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -56, 3250, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -56, 3418, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, -56, 3450, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -48, 3000, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -48, 3500, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, -48, 3450, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -40, 2875, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -40, 3450, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, -40, 3450, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -32, 2750, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -32, 3125, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, -32, 3450, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -32, 1575, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -24, 1575, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -24, 1583, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -16, 1675, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -16, 1675, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -8, 1687, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -8, 1675, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 0, 1687, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 8, 1687, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 8, 1687, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 48, 1750, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 32, 1800, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 32, 1750, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 128, -8, 1675, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, 0, 1675, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 32, 1750, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 40, 1750, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 144, 8, 1600, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 16, 1600, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 16, 1787, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, 40, 1725, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, 0, 1675, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, 24, 1750, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -48, 1575, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, -40, 1575, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, -16, 1625, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 8, 1625, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, 32, 1800, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 16, 1800, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, -32, 1625, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, -24, 1575, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -16, 1550, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, -8, 1525, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018A15C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 29, 0, 0, { 1, 0 } },
    { 29, 33, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b3_garbage_incinerator_8018A17C[2] = {
    { { 75, 0, 245, 239 }, 2625 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018A190[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_garbage_incinerator_8018A1A0[45] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, -49, 2000, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 23, 1500, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, -65, 2000, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, -41, 1750, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, -33, 1500, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, -17, 1425, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 39, 1425, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, -65, 1350, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -160, -1, 1350, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -144, -1, 1350, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, -65, 2375, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 144, -25, 1500, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, -49, 1750, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, -1, 1625, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, -33, 2125, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, -57, 2000, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, -17, 1875, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -144, -65, 1350, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -128, -65, 1425, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, -65, 1500, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, -65, 1750, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, -65, 2250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, -65, 2000, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, -65, 1750, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -65, 1500, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 40, 1325, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 40, 1325, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 40, 1325, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 40, 1325, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 40, 1325, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, 40, 1325, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 40, 1325, { .fields = { 32, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 40, 1325, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 96, 40, 1325, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 40, 1325, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 48, 1350, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 96, 48, 1350, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 48, 1350, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, 48, 1350, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 48, 1350, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 48, 1350, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 48, 1350, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 48, 1350, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 48, 1350, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 48, 1350, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018A524[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 25, 0, 0, { 1, 0 } },
    { 25, 20, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b3_garbage_incinerator_8018A544[2] = {
    { { 31, 1, 287, 167 }, 1337 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018A558[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_garbage_incinerator_8018A568[74] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, -32, 737, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 0, 718, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 24, 706, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 24, 687, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, -24, 737, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -136, -16, 731, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -112, -8, 731, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 0, 718, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 8, 700, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -48, 16, 700, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -24, 24, 693, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 32, 687, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 40, 675, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 48, 662, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, 56, 662, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 96, 64, 650, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 120, 72, 650, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 80, 650, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, -16, 731, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -144, -8, 731, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -120, 0, 718, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 8, 706, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 16, 706, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, 24, 693, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 32, 681, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 40, 681, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 16, 48, 668, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 56, 662, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 64, 662, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 88, 72, 643, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 112, 80, 650, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 80, 637, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 104, 88, 650, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 88, 643, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 72, 656, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, 64, 656, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 8, 56, 662, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, 48, 675, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 40, 681, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 32, 687, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 24, 693, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -112, 16, 706, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 8, 712, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -152, 0, 725, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, -8, 725, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, 88, 637, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 80, 643, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 72, 650, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 64, 656, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -24, 56, 668, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -48, 48, 668, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 40, 681, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 32, 681, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -120, 24, 693, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 88, 631, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 16, 80, 643, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 72, 650, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -144, 16, 706, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 8, 718, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 64, 662, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, 56, 662, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 48, 675, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -104, 40, 681, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 32, 687, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -152, 24, 700, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 16, 712, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 8, 88, 637, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, 80, 643, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 72, 656, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 64, 656, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 56, 668, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -112, 48, 668, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -136, 40, 681, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 32, 693, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018AB30[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 74, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b3_garbage_incinerator_8018AB48[2] = {
    { { 0, 0, 319, 208 }, 700 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018AB5C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018AB6C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018AB7C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_garbage_incinerator_8018AB8C[14] = {
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -136, -120, 1075, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -128, -120, 1112, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -120, -120, 1100, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 80, 1050, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -136, -72, 1050, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -136, -16, 1025, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -136, 40, 1000, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -128, -64, 1087, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -128, -8, 1075, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -128, 48, 1050, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -120, 56, 1050, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -120, -56, 1087, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 24, 1068, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 0, 1075, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018ACA4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b3_garbage_incinerator_8018ACBC[2] = {
    { { 30, 0, 290, 239 }, 962 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_shelter_b3_garbage_incinerator_8018ACD0[15] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 56, 1887, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 56, 1900, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -128, -120, 1962, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, -120, 1975, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -112, -120, 1987, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -104, -120, 1975, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -128, -72, 1937, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -120, -72, 1950, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -112, -64, 1962, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -104, -64, 1950, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -104, -8, 1925, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -112, -8, 1937, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 40, 1950, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -120, -16, 1925, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -128, -16, 1912, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018ADFC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b3_garbage_incinerator_8018AE14[2] = {
    { { 35, 0, 285, 239 }, 1875 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_shelter_b3_garbage_incinerator_8018AE28[8] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, -40, 1150, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 144, -40, 1175, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, -40, 1150, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 144, 0, 1175, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 8, 1150, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, 72, 1150, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 144, 64, 1175, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, 56, 1150, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018AEC8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b3_garbage_incinerator_8018AEE0[2] = {
    { { 83, 0, 237, 239 }, 2875 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_shelter_b3_garbage_incinerator_8018AEF4[5] = {
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 64, -40, 2200, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 56, -40, 2250, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 48, -32, 2300, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 72, -48, 2150, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 80, -48, 2100, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018AF58[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b3_garbage_incinerator_8018AF70[2] = {
    { { 85, 1, 234, 238 }, 4000 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_shelter_b3_garbage_incinerator_8018AF84[133] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 16, 2050, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 16, 1875, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 24, 2500, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 72, 750, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 72, 750, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, 8, 2500, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -88, 8, 2175, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 8, 1700, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 8, 1400, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 32, 1700, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -104, 32, 1400, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 8, 1300, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 8, 1150, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -112, 40, 1300, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -120, 40, 1150, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, 8, 1050, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, 48, 1050, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -136, 8, 925, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 56, 925, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -144, 8, 875, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -144, 64, 875, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, 8, 750, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, 8, 750, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, 40, 750, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, 40, 750, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 16, 2712, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, 16, 2712, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 16, 2050, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 16, 2050, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 24, 2075, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 24, 1950, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 16, 1600, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 16, 1600, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 32, 1725, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 32, 1600, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 16, 1487, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 16, 1487, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 40, 1487, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 40, 1487, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, 24, 1487, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, 104, 24, 1487, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 32, -32, 2625, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, -32, 2675, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 16, -24, 2725, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 96, 925, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 48, 1450, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 56, 1450, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 96, 56, 1450, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, 56, 1400, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 56, 1400, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 64, 1325, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 104, 64, 1325, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, 64, 1300, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 64, 1300, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 16, 64, 1275, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, 64, 1275, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -48, 64, 1275, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 72, 1250, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 112, 72, 1250, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 72, 1225, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 72, 1225, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 16, 72, 1187, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -8, 72, 1187, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -32, 72, 1187, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, 72, 1187, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 72, 1162, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 72, 1162, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -144, 72, 1162, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 72, 1162, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 80, 1075, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 88, 1000, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 96, 925, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 104, 875, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 112, 825, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 80, 1075, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 80, 1075, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 80, 1075, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 80, 1075, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 88, 1000, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 88, 1000, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 88, 1000, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 88, 1000, { .fields = { 40, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 104, 875, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -112, 104, 875, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 104, 875, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -48, 104, 875, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 80, 1075, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 88, 1000, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 80, 1100, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 16, 80, 1100, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -24, 96, 925, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 112, 825, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 112, 825, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 112, 825, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 112, 825, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 112, 825, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, 112, 825, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 112, 825, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 96, 112, 825, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 112, 825, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, 104, 875, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 16, 104, 875, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 104, 875, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 104, 900, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 112, 104, 900, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 104, 900, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 8, 96, 950, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, 96, 950, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 72, 96, 950, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 104, 96, 950, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 96, 950, { .fields = { 16, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 88, 1125, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 96, 88, 1125, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 88, 1125, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, 88, 1000, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 88, 1000, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 80, 1175, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 120, 80, 1175, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 88, 80, 1100, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, 80, 1100, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 80, 1100, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 64, 1250, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 64, 1250, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -112, 80, 1040, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 80, 1075, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 88, 1000, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -112, 88, 1000, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -112, 96, 925, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 96, 925, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 96, 925, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 96, 925, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 96, 925, { .fields = { 0, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 96, 925, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018B9E8[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 41, 0, 0, { 1, 0 } },
    { 41, 3, 0, 0, { 2, 0 } },
    { 44, 89, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b3_garbage_incinerator_8018BA10[2] = {
    { { 98, 0, 222, 239 }, 4250 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018BA24[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_garbage_incinerator_8018BA34[34] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -64, 712, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 8, 825, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, 16, 937, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 40, 48, 937, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, 8, 917, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, -56, 775, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, -56, 750, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, -48, 750, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -48, 750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, -16, 825, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, -16, 800, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, -8, 800, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -8, 800, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, 24, 875, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, 24, 875, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, 32, 875, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 32, 875, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 64, 925, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, 64, 925, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 72, 925, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 72, 925, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -56, 825, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -64, 812, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, -64, 812, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -24, 850, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, -24, 850, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, 16, 950, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, 16, 950, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 56, 950, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, 56, 950, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -32, 825, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, 16, 937, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, 48, 937, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -64, 825, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018BCDC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 34, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_garbage_incinerator_8018BCF4[40] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -32, 820, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 24, 925, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 88, 987, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, 40, 925, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 24, 925, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -32, 812, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 64, 8, 925, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 32, 925, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, 56, 925, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, 56, 950, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 72, 925, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, 24, 912, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, -24, 762, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, -72, 750, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, -72, 750, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, -24, 762, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, 24, 912, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 104, 72, 925, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 80, 962, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 136, 80, 962, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 120, 32, 950, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 136, 32, 950, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 120, -16, 800, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 136, -16, 800, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 120, -64, 787, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 136, -64, 787, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, -56, 825, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, -8, 837, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, 40, 987, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 72, 64, 950, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, -56, 812, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, -56, 812, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 8, 812, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 0, 812, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -80, 825, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -32, 787, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, 16, 950, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -88, 700, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -24, 812, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, 48, 925, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018C014[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 40, 0, 0, { 1, 0 } },
    { 40, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_garbage_incinerator_8018C034[54] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -120, 2875, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -120, 3250, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -112, 3125, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -104, 2975, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -104, 3187, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -96, 3193, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -72, 3375, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -64, 3350, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, -64, 3350, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -56, 3418, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -48, 3000, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -48, 3500, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -40, 2875, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -40, 3450, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -32, 2750, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -32, 3125, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -112, 2625, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -88, 2625, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -64, 2625, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, -40, 2625, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, -56, 3450, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -120, 3125, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -88, 3237, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, -64, 3250, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, -96, 2987, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -24, 1583, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -16, 1675, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 8, 1687, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 40, 1750, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 48, 1750, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 32, 1800, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 16, 1800, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, 32, 1800, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 16, 1787, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, 40, 1725, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, 24, 1750, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, 0, 1675, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -48, 1575, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, -40, 1575, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, -16, 1625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 8, 1625, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 32, 1750, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 16, 1600, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 144, 8, 1600, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 136, 0, 1675, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 128, -8, 1675, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 32, 1750, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, -32, 1625, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, -24, 1575, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -16, 1550, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, -8, 1525, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -8, 1687, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, -32, 1575, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, -16, 1675, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018C46C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 25, 0, 0, { 1, 0 } },
    { 25, 29, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b3_garbage_incinerator_8018C48C[2] = {
    { { 68, 0, 252, 239 }, 2500 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_shelter_b3_garbage_incinerator_8018C4A0[41] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, -48, 2767, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 24, 1500, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 48, 1425, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, -64, 2000, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, -40, 1750, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, -32, 1500, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -128, -24, 1425, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -144, 0, 1350, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -160, 0, 1350, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, -64, 2375, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 0, 1750, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 144, -24, 1500, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, -48, 1750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, -56, 2000, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, -16, 1875, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, -32, 2125, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, -64, 1750, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, -64, 1500, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, -64, 1425, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -144, -64, 1350, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, -64, 1350, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, -64, 2250, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, -64, 2000, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, -64, 1750, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -64, 1500, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -160, 40, 1350, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -96, 40, 1350, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -96, 48, 1325, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -160, 48, 1325, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 96, 40, 1350, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 40, 1350, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 96, 48, 1325, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 48, 1325, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, 40, 1350, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 40, 1350, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, 48, 1325, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 48, 1325, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 40, 1350, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 48, 1325, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 40, 1350, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 48, 1325, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018C7D4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 25, 0, 0, { 1, 0 } },
    { 25, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b3_garbage_incinerator_8018C7F4[2] = {
    { { 32, 1, 286, 169 }, 1337 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018C808[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_garbage_incinerator_8018C818[36] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -64, 712, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 8, 825, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 16, 937, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, 16, 937, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 40, 48, 937, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 24, 937, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, 8, 917, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, -56, 775, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, -56, 750, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, -48, 750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -48, 750, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, -16, 825, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, -16, 800, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, -8, 800, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -8, 800, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, 24, 875, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, 24, 875, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 128, 32, 875, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 32, 875, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 64, 925, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, 64, 925, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 72, 925, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 72, 925, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -56, 825, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, 48, 937, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, 48, 937, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -64, 812, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, -64, 812, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -24, 850, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, -24, 850, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, 16, 950, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, 16, 950, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 56, 950, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 56, 950, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -32, 825, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -64, 825, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018CAE8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 36, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_garbage_incinerator_8018CB00[53] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -32, 820, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 24, 925, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 88, 987, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, 40, 925, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, 48, 925, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 24, 925, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -32, 812, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, -24, 812, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -24, 812, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 64, 8, 925, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 32, 925, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, 56, 925, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, 56, 950, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, -104, 700, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, -104, 700, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, -104, 700, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 72, -80, 750, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, -80, 750, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 72, -32, 787, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, -32, 787, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 72, 16, 950, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, 16, 950, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 72, 925, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, 24, 912, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, -24, 762, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, -72, 750, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, -104, 712, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, -104, 712, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, -72, 750, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, -24, 762, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, 24, 912, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 104, 72, 925, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 80, 962, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 136, 80, 962, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 120, 32, 950, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 136, 32, 950, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 120, -16, 800, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 136, -16, 800, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 120, -64, 787, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 136, -64, 787, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, -104, 750, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 136, -104, 750, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, -104, 787, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, -56, 825, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, -8, 837, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, 40, 987, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 72, 64, 950, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -88, 700, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, -56, 812, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -80, 700, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, -56, 812, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 8, 812, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 0, 812, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018CF24[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 53, 0, 0, { 1, 0 } },
    { 53, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b3_garbage_incinerator_8018CF44[20] = {
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 40, 1350, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 40, 1350, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 40, 1350, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 40, 1350, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 40, 1350, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 40, 1350, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, 40, 1350, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 40, 1350, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 96, 40, 1350, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 40, 1350, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 48, 1325, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 48, 1325, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 48, 1325, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 48, 1325, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 48, 1325, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 0, 48, 1325, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, 48, 1325, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 48, 1325, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 96, 48, 1325, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 48, 1325, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b3_garbage_incinerator_8018D0D4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b3_garbage_incinerator_8018D0EC[2] = {
    { { 1, 1, 318, 168 }, 1337 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteView D_shelter_b3_garbage_incinerator_8018D100[40] = {
    { { .empty = D_shelter_b3_garbage_incinerator_8018894C }, D_shelter_b3_garbage_incinerator_8018894C, NULL },
    { { .empty = D_shelter_b3_garbage_incinerator_8018895C }, D_shelter_b3_garbage_incinerator_8018895C, NULL },
    { { .empty = D_shelter_b3_garbage_incinerator_8018896C }, D_shelter_b3_garbage_incinerator_8018896C, NULL },
    { { .elements = D_shelter_b3_garbage_incinerator_8018897C }, D_shelter_b3_garbage_incinerator_80188A94, D_shelter_b3_garbage_incinerator_80188AAC },
    { { .elements = D_shelter_b3_garbage_incinerator_80188AC0 }, D_shelter_b3_garbage_incinerator_80188B9C, D_shelter_b3_garbage_incinerator_80188BB4 },
    { { .elements = D_shelter_b3_garbage_incinerator_80188BC8 }, D_shelter_b3_garbage_incinerator_80188C68, D_shelter_b3_garbage_incinerator_80188C80 },
    { { .elements = D_shelter_b3_garbage_incinerator_80188C94 }, D_shelter_b3_garbage_incinerator_80188CF8, D_shelter_b3_garbage_incinerator_80188D10 },
    { { .elements = D_shelter_b3_garbage_incinerator_80188D24 }, D_shelter_b3_garbage_incinerator_801890D0, D_shelter_b3_garbage_incinerator_801890F0 },
    { { .empty = D_shelter_b3_garbage_incinerator_80189104 }, D_shelter_b3_garbage_incinerator_80189104, NULL },
    { { .empty = D_shelter_b3_garbage_incinerator_80189114 }, D_shelter_b3_garbage_incinerator_80189114, NULL },
    { { .elements = D_shelter_b3_garbage_incinerator_80189124 }, D_shelter_b3_garbage_incinerator_801894D0, NULL },
    { { .empty = D_shelter_b3_garbage_incinerator_801894E8 }, D_shelter_b3_garbage_incinerator_801894E8, NULL },
    { { .empty = D_shelter_b3_garbage_incinerator_801894F8 }, D_shelter_b3_garbage_incinerator_801894F8, NULL },
    { { .elements = D_shelter_b3_garbage_incinerator_80189508 }, D_shelter_b3_garbage_incinerator_801897C4, NULL },
    { { .elements = D_shelter_b3_garbage_incinerator_801897DC }, D_shelter_b3_garbage_incinerator_80189C64, NULL },
    { { .elements = D_shelter_b3_garbage_incinerator_80189C84 }, D_shelter_b3_garbage_incinerator_8018A15C, D_shelter_b3_garbage_incinerator_8018A17C },
    { { .empty = D_shelter_b3_garbage_incinerator_8018A190 }, D_shelter_b3_garbage_incinerator_8018A190, NULL },
    { { .elements = D_shelter_b3_garbage_incinerator_8018A1A0 }, D_shelter_b3_garbage_incinerator_8018A524, D_shelter_b3_garbage_incinerator_8018A544 },
    { { .empty = D_shelter_b3_garbage_incinerator_8018A558 }, D_shelter_b3_garbage_incinerator_8018A558, NULL },
    { { .elements = D_shelter_b3_garbage_incinerator_8018A568 }, D_shelter_b3_garbage_incinerator_8018AB30, D_shelter_b3_garbage_incinerator_8018AB48 },
    { { .empty = D_shelter_b3_garbage_incinerator_8018AB5C }, D_shelter_b3_garbage_incinerator_8018AB5C, NULL },
    { { .empty = D_shelter_b3_garbage_incinerator_8018AB6C }, D_shelter_b3_garbage_incinerator_8018AB6C, NULL },
    { { .empty = D_shelter_b3_garbage_incinerator_8018AB7C }, D_shelter_b3_garbage_incinerator_8018AB7C, NULL },
    { { .elements = D_shelter_b3_garbage_incinerator_8018AB8C }, D_shelter_b3_garbage_incinerator_8018ACA4, D_shelter_b3_garbage_incinerator_8018ACBC },
    { { .elements = D_shelter_b3_garbage_incinerator_8018ACD0 }, D_shelter_b3_garbage_incinerator_8018ADFC, D_shelter_b3_garbage_incinerator_8018AE14 },
    { { .elements = D_shelter_b3_garbage_incinerator_8018AE28 }, D_shelter_b3_garbage_incinerator_8018AEC8, D_shelter_b3_garbage_incinerator_8018AEE0 },
    { { .elements = D_shelter_b3_garbage_incinerator_8018AEF4 }, D_shelter_b3_garbage_incinerator_8018AF58, D_shelter_b3_garbage_incinerator_8018AF70 },
    { { .elements = D_shelter_b3_garbage_incinerator_8018AF84 }, D_shelter_b3_garbage_incinerator_8018B9E8, D_shelter_b3_garbage_incinerator_8018BA10 },
    { { .empty = D_shelter_b3_garbage_incinerator_8018BA24 }, D_shelter_b3_garbage_incinerator_8018BA24, NULL },
    { { .elements = D_shelter_b3_garbage_incinerator_8018BA34 }, D_shelter_b3_garbage_incinerator_8018BCDC, NULL },
    { { .elements = D_shelter_b3_garbage_incinerator_8018BCF4 }, D_shelter_b3_garbage_incinerator_8018C014, NULL },
    { { .elements = D_shelter_b3_garbage_incinerator_8018C034 }, D_shelter_b3_garbage_incinerator_8018C46C, D_shelter_b3_garbage_incinerator_8018C48C },
    { { .elements = D_shelter_b3_garbage_incinerator_8018C4A0 }, D_shelter_b3_garbage_incinerator_8018C7D4, D_shelter_b3_garbage_incinerator_8018C7F4 },
    { { .empty = D_shelter_b3_garbage_incinerator_8018C808 }, D_shelter_b3_garbage_incinerator_8018C808, NULL },
    { { .elements = D_shelter_b3_garbage_incinerator_8018AE28 }, D_shelter_b3_garbage_incinerator_8018AEC8, D_shelter_b3_garbage_incinerator_8018AEE0 },
    { { .elements = D_shelter_b3_garbage_incinerator_8018AEF4 }, D_shelter_b3_garbage_incinerator_8018AF58, D_shelter_b3_garbage_incinerator_8018AF70 },
    { { .elements = D_shelter_b3_garbage_incinerator_8018AF84 }, D_shelter_b3_garbage_incinerator_8018B9E8, D_shelter_b3_garbage_incinerator_8018BA10 },
    { { .elements = D_shelter_b3_garbage_incinerator_8018C818 }, D_shelter_b3_garbage_incinerator_8018CAE8, NULL },
    { { .elements = D_shelter_b3_garbage_incinerator_8018CB00 }, D_shelter_b3_garbage_incinerator_8018CF24, NULL },
    { { .elements = D_shelter_b3_garbage_incinerator_8018CF44 }, D_shelter_b3_garbage_incinerator_8018D0D4, D_shelter_b3_garbage_incinerator_8018D0EC },
};

WorldCoordLight D_shelter_b3_garbage_incinerator_8018D2E0[2] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1010, -2010, -2010 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3072, 3072 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -990, -2010, 990 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3072, 3072 }, { 0, 0 } },
};

WorldCoordPointLight D_shelter_b3_garbage_incinerator_8018D390[25] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x36B0, -2150, -0x6464 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 0, 0 }, { 0, 0 } }, 1119, 1413 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x34BC, -1760, -0x4CDF } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, 20, { 0, 0, 0, 0 }, 0, NULL } }, { 1641, 1496, 1355 }, { 0, 0 } }, 1504, 1825 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x38A4, -1760, -0x6658 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2868, 2725, 2582 }, { 0, 0 } }, 1040, 1361 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2BC0, -2000, -8500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2BC0, -2000, -0x2CEC } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2BC0, -2000, -0x5FB4 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x41A0, -2000, -0x5FB4 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x41A0, -2000, -0x2CEC } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x41A0, -2000, -8500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x41A0, -2000, -5500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x41A0, -2000, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8000, -2000, -5300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5000, -2000, -5300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -2000, -5300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -2000, 300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5000, -2000, 300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8000, -2000, 300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x36B0, -2000, 300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2AF8, -2000, 300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 40, -2049, -901 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0, 2048, 716 }, { 0, 0 } }, 100, 750 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4268, -2000, -0x36B0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2868, 2724, 2581 }, { 0, 0 } }, 1412, 2092 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x44FD, 127, -0x55F0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2871, 2724, 2581 }, { 0, 0 } }, 1682, 2378 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x36AE, -1038, -0x4C68 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, 19, { 0, 0, 0, 0 }, 0, NULL } }, { 2871, 2724, 2583 }, { 0, 0 } }, 1421, 3321 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2498, -678, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, 12, { 0, 0, 0, 0 }, 0, NULL } }, { 1845, 1765, 1682 }, { 0, 0 } }, 1363, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3712, -1038, -0x4C68 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, 21, { 0, 0, 0, 0 }, 0, NULL } }, { 2871, 2724, 2583 }, { 0, 0 } }, 1421, 3321 },
};

WorldCoordRoomLights D_shelter_b3_garbage_incinerator_8018DCF0[1] = {
    { ARRAY_SIZE(D_shelter_b3_garbage_incinerator_8018D2E0), D_shelter_b3_garbage_incinerator_8018D2E0, ARRAY_SIZE(D_shelter_b3_garbage_incinerator_8018D390), D_shelter_b3_garbage_incinerator_8018D390, 0, NULL },
};

WorldCoordLight D_shelter_b3_garbage_incinerator_8018DD08[2] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1010, -2010, -2010 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 1843, 1843 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -990, -2010, 990 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 1843, 1843 }, { 0, 0 } },
};

WorldCoordPointLight D_shelter_b3_garbage_incinerator_8018DDB8[21] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x36B0, -2150, -0x6464 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 0, 0 }, { 0, 0 } }, 1119, 1413 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x38A4, -1760, -0x6658 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2868, 2725, 2582 }, { 0, 0 } }, 1040, 1361 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2BC0, -2000, -8500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2BC0, -2000, -0x2CEC } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2BC0, -2000, -0x5FB4 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x41A0, -2000, -0x5FB4 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x41A0, -2000, -0x2CEC } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x41A0, -2000, -8500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x41A0, -2000, -5500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x41A0, -2000, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8000, -2000, -5300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5000, -2000, -5300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -2000, -5300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -2000, 300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5000, -2000, 300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8000, -2000, 300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x36B0, -2000, 300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2AF8, -2000, 300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 0, 0 }, { 0, 0 } }, 1300, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 40, -2049, -901 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0, 2048, 716 }, { 0, 0 } }, 100, 750 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4268, -2000, -0x36B0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2868, 2724, 2581 }, { 0, 0 } }, 1412, 2092 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x44FD, 127, -0x55F0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2871, 2724, 2581 }, { 0, 0 } }, 1682, 2378 },
};

WorldCoordRoomLights D_shelter_b3_garbage_incinerator_8018E598[1] = {
    { ARRAY_SIZE(D_shelter_b3_garbage_incinerator_8018DD08), D_shelter_b3_garbage_incinerator_8018DD08, ARRAY_SIZE(D_shelter_b3_garbage_incinerator_8018DDB8), D_shelter_b3_garbage_incinerator_8018DDB8, 0, NULL },
};

WorldCollisionTrigger D_shelter_b3_garbage_incinerator_8018E5B0[22] = {
    { NULL, NULL, NULL, { 5376, -2160, -2864, 0 }, { { 0, -4720, 4464, 0 }, { 0, -4720, -4368, 0 }, { 0, 4720, 4368, 0 }, { 0, 4720, -4464, 0 } }, { -4113, 0, 0, 0 }, { 0, 0, 4096, 0 }, 6496, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5168, -2176, -2881, 0 }, { { -16, -4720, -3856, 0 }, { 16, -4720, 3952, 0 }, { -16, 4720, -3952, 0 }, { 16, 4720, 3856, 0 } }, { 4101, -1, -17, 0 }, { 0, 0, 4096, 0 }, 6144, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3800, -1888, -6848, 0 }, { { 3808, -4720, 992, 0 }, { -3808, -4720, -992, 0 }, { 3808, 4720, 992, 0 }, { -3808, 4720, -992, 0 } }, { -1036, 0, 3974, 0 }, { 0, 0, 4096, 0 }, 6144, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3801, -1888, -6624, 0 }, { { -3904, -4720, -1008, 0 }, { 3904, -4720, 1008, 0 }, { -3904, 4720, -1008, 0 }, { 3904, 4720, 1008, 0 } }, { 1025, 0, -3974, 0 }, { 0, 0, 4096, 0 }, 6207, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x377F, -1952, -0x3042, 0 }, { { 3899, -4720, 328, 0 }, { -3898, -4720, -327, 0 }, { 3899, 4720, 328, 0 }, { -3898, 4720, -327, 0 } }, { -345, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 6122, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x374F, -1856, -0x2EE1, 0 }, { { -3724, -4720, -353, 0 }, { 3725, -4720, 354, 0 }, { -3724, 4720, -353, 0 }, { 3725, 4720, 354, 0 } }, { 389, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 6014, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x413F, -2048, -0x3161, 0 }, { { 1144, -4720, 1298, 0 }, { -1143, -4720, -1297, 0 }, { 1144, 4720, 1298, 0 }, { -1143, 4720, -1297, 0 } }, { -3076, 0, 2709, 0 }, { 0, 0, 4096, 0 }, 5016, 0, 6, 14, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3F50, -2080, -0x3241, 0 }, { { -914, -4720, -1070, 0 }, { 915, -4720, 1071, 0 }, { -914, 4720, -1070, 0 }, { 915, 4720, 1071, 0 } }, { 3119, 0, -2666, 0 }, { 0, 0, 4096, 0 }, 4910, 0, 14, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9984, -2016, -2592, 0 }, { { -394, -4720, -3836, 0 }, { 402, -4720, 3931, 0 }, { -403, 4720, -3932, 0 }, { 393, 4720, 3835, 0 } }, { 4079, -1, -419, 0 }, { 0, 0, 4096, 0 }, 6144, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2780, -1984, -2432, 0 }, { { 437, -4720, 4442, 0 }, { -428, -4720, -4347, 0 }, { 427, 4720, 4346, 0 }, { -438, 4720, -4443, 0 } }, { -4093, -1, 402, 0 }, { 0, 0, 4096, 0 }, 6496, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3680, -1440, -0x5AE0, 0 }, { { 3887, -4720, -439, 0 }, { -3887, -4720, 439, 0 }, { 3887, 4720, -439, 0 }, { -3887, 4720, 439, 0 } }, { 461, 0, 4083, 0 }, { 0, 0, 4096, 0 }, 6122, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3720, -1312, -0x5A40, 0 }, { { -3887, -4720, 439, 0 }, { 3887, -4720, -439, 0 }, { -3887, 4720, 439, 0 }, { 3887, 4720, -439, 0 } }, { -462, 0, -4084, 0 }, { 0, 0, 4096, 0 }, 6122, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3F3F, -1440, -0x50D1, 0 }, { { 765, -4720, 1229, 0 }, { -764, -4720, -1228, 0 }, { 765, 4720, 1229, 0 }, { -764, 4720, -1228, 0 } }, { -3485, 0, 2167, 0 }, { 0, 0, 4096, 0 }, 4910, 0, 8, 15, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3DEF, -1313, -0x51A1, 0 }, { { -654, -4720, -1050, 0 }, { 654, -4720, 1051, 0 }, { -654, 4720, -1050, 0 }, { 654, 4720, 1051, 0 } }, { 3477, 0, -2166, 0 }, { 0, 0, 4096, 0 }, 4857, 0, 15, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x37C1, -1696, -0x3F00, 0 }, { { 4000, -4720, 0, 0 }, { -4000, -4720, 0, 0 }, { 4000, 4720, 0, 0 }, { -4000, 4720, 0, 0 } }, { 0, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 6186, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3870, -1568, -0x3E64, 0 }, { { -4496, -4720, 6, 0 }, { 4496, -4720, -5, 0 }, { -4496, 4720, 6, 0 }, { 4496, 4720, -5, 0 } }, { -6, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 6516, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3640, -1632, -0x4CF0, 0 }, { { -4496, -4720, -330, 0 }, { 4496, -4720, 331, 0 }, { -4496, 4720, -330, 0 }, { 4496, 4720, 331, 0 } }, { 301, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 6516, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x36C1, -1792, -0x4DC1, 0 }, { { 4000, -4720, 320, 0 }, { -4000, -4720, -320, 0 }, { 4000, 4720, 320, 0 }, { -4000, 4720, -320, 0 } }, { -328, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 6186, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x41FF, 0, -0x3AA0, 0 }, { { -1360, -4720, 1117, 0 }, { 1360, -4720, -1117, 0 }, { -1360, 4720, 1117, 0 }, { 1360, 4720, -1117, 0 } }, { -2600, 0, -3166, 0 }, { 0, 0, 4096, 0 }, 5016, 0, 6, 14, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x4100, 0, -0x3AA1, 0 }, { { 1360, -4720, -1117, 0 }, { -1360, -4720, 1117, 0 }, { 1360, 4720, -1117, 0 }, { -1360, 4720, 1117, 0 } }, { 2604, 0, 3170, 0 }, { 0, 0, 4096, 0 }, 5016, 0, 14, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x4050, 0, -0x5991, 0 }, { { -1024, -4720, 1069, 0 }, { 1024, -4720, -1069, 0 }, { -1024, 4720, 1069, 0 }, { 1024, 4720, -1069, 0 } }, { -2963, 0, -2838, 0 }, { 0, 0, 4096, 0 }, 4937, 0, 8, 15, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3F80, 0, -0x5991, 0 }, { { 1168, -4720, -1101, 0 }, { -1168, -4720, 1101, 0 }, { 1168, 4720, -1101, 0 }, { -1168, 4720, 1101, 0 } }, { 2814, 0, 2985, 0 }, { 0, 0, 4096, 0 }, 4964, 0, 15, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b3_garbage_incinerator_8018EC38[20] = {
    { NULL, NULL, NULL, { 5376, -2160, -2864, 0 }, { { 0, -4720, 4464, 0 }, { 0, -4720, -4368, 0 }, { 0, 4720, 4368, 0 }, { 0, 4720, -4464, 0 } }, { -4113, 0, 0, 0 }, { 0, 0, 4096, 0 }, 6496, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5168, -2176, -2881, 0 }, { { -16, -4720, -3856, 0 }, { 16, -4720, 3952, 0 }, { -16, 4720, -3952, 0 }, { 16, 4720, 3856, 0 } }, { 4101, -1, -17, 0 }, { 0, 0, 4096, 0 }, 6144, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3800, -1888, -6848, 0 }, { { 3808, -4720, 992, 0 }, { -3808, -4720, -992, 0 }, { 3808, 4720, 992, 0 }, { -3808, 4720, -992, 0 } }, { -1036, 0, 3974, 0 }, { 0, 0, 4096, 0 }, 6144, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3801, -1888, -6624, 0 }, { { -3904, -4720, -1008, 0 }, { 3904, -4720, 1008, 0 }, { -3904, 4720, -1008, 0 }, { 3904, 4720, 1008, 0 } }, { 1025, 0, -3974, 0 }, { 0, 0, 4096, 0 }, 6207, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x377F, -1952, -0x3042, 0 }, { { 3899, -4720, 328, 0 }, { -3898, -4720, -327, 0 }, { 3899, 4720, 328, 0 }, { -3898, 4720, -327, 0 } }, { -345, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 6122, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x374F, -1856, -0x2EE1, 0 }, { { -3724, -4720, -353, 0 }, { 3725, -4720, 354, 0 }, { -3724, 4720, -353, 0 }, { 3725, 4720, 354, 0 } }, { 389, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 6014, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x4140, -2080, -0x32C1, 0 }, { { -1410, -4720, -942, 0 }, { 1411, -4720, 943, 0 }, { -1410, 4720, -942, 0 }, { 1411, 4720, 943, 0 } }, { 2275, 0, -3407, 0 }, { 0, 0, 4096, 0 }, 4990, 0, 14, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9984, -2016, -2592, 0 }, { { -394, -4720, -3836, 0 }, { 402, -4720, 3931, 0 }, { -403, 4720, -3932, 0 }, { 393, 4720, 3835, 0 } }, { 4079, -1, -419, 0 }, { 0, 0, 4096, 0 }, 6144, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2780, -1984, -2432, 0 }, { { 437, -4720, 4442, 0 }, { -428, -4720, -4347, 0 }, { 427, 4720, 4346, 0 }, { -438, 4720, -4443, 0 } }, { -4093, -1, 402, 0 }, { 0, 0, 4096, 0 }, 6496, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3680, -1440, -0x5AE0, 0 }, { { 3887, -4720, -439, 0 }, { -3887, -4720, 439, 0 }, { 3887, 4720, -439, 0 }, { -3887, 4720, 439, 0 } }, { 461, 0, 4083, 0 }, { 0, 0, 4096, 0 }, 6122, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3720, -1312, -0x5A40, 0 }, { { -3887, -4720, 439, 0 }, { 3887, -4720, -439, 0 }, { -3887, 4720, 439, 0 }, { 3887, 4720, -439, 0 } }, { -462, 0, -4084, 0 }, { 0, 0, 4096, 0 }, 6122, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3E3F, -1440, -0x5241, 0 }, { { 637, -4720, 701, 0 }, { -636, -4720, -700, 0 }, { 637, 4720, 701, 0 }, { -636, 4720, -700, 0 } }, { -3044, 0, 2764, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 8, 15, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3D8F, -1313, -0x5261, 0 }, { { -782, -4720, -826, 0 }, { 782, -4720, 827, 0 }, { -782, 4720, -826, 0 }, { 782, 4720, 827, 0 } }, { 2987, 0, -2828, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 15, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x37C1, -1696, -0x3F00, 0 }, { { 4000, -4720, 0, 0 }, { -4000, -4720, 0, 0 }, { 4000, 4720, 0, 0 }, { -4000, 4720, 0, 0 } }, { 0, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 6186, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3870, -1568, -0x3E64, 0 }, { { -4496, -4720, 6, 0 }, { 4496, -4720, -5, 0 }, { -4496, 4720, 6, 0 }, { 4496, 4720, -5, 0 } }, { -6, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 6516, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3640, -1632, -0x4E40, 0 }, { { -4496, -4720, 6, 0 }, { 4496, -4720, -5, 0 }, { -4496, 4720, 6, 0 }, { 4496, 4720, -5, 0 } }, { -6, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 6516, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x36C1, -1792, -0x4F01, 0 }, { { 4000, -4720, 0, 0 }, { -4000, -4720, 0, 0 }, { 4000, 4720, 0, 0 }, { -4000, 4720, 0, 0 } }, { 0, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 6186, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x4100, 0, -0x3AA1, 0 }, { { 1360, -4720, -1117, 0 }, { -1360, -4720, 1117, 0 }, { 1360, 4720, -1117, 0 }, { -1360, 4720, 1117, 0 } }, { 2604, 0, 3170, 0 }, { 0, 0, 4096, 0 }, 5016, 0, 14, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x40F0, 0, -0x5921, 0 }, { { -1344, -4720, 1117, 0 }, { 1344, -4720, -1117, 0 }, { -1344, 4720, 1117, 0 }, { 1344, 4720, -1117, 0 } }, { -2620, 0, -3152, 0 }, { 0, 0, 4096, 0 }, 5016, 0, 8, 15, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x4000, 0, -0x5901, 0 }, { { 1360, -4720, -1117, 0 }, { -1360, -4720, 1117, 0 }, { 1360, 4720, -1117, 0 }, { -1360, 4720, 1117, 0 } }, { 2604, 0, 3170, 0 }, { 0, 0, 4096, 0 }, 5016, 0, 15, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b3_garbage_incinerator_8018F228[17] = {
    { NULL, NULL, NULL, { 5376, -2160, -2864, 0 }, { { 0, -4720, 4464, 0 }, { 0, -4720, -4368, 0 }, { 0, 4720, 4368, 0 }, { 0, 4720, -4464, 0 } }, { -4113, 0, 0, 0 }, { 0, 0, 4096, 0 }, 6496, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5168, -2176, -2881, 0 }, { { -16, -4720, -3856, 0 }, { 16, -4720, 3952, 0 }, { -16, 4720, -3952, 0 }, { 16, 4720, 3856, 0 } }, { 4101, -1, -17, 0 }, { 0, 0, 4096, 0 }, 6144, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3800, -1888, -6848, 0 }, { { 3808, -4720, 992, 0 }, { -3808, -4720, -992, 0 }, { 3808, 4720, 992, 0 }, { -3808, 4720, -992, 0 } }, { -1036, 0, 3974, 0 }, { 0, 0, 4096, 0 }, 6144, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3801, -1888, -6624, 0 }, { { -3904, -4720, -1008, 0 }, { 3904, -4720, 1008, 0 }, { -3904, 4720, -1008, 0 }, { 3904, 4720, 1008, 0 } }, { 1025, 0, -3974, 0 }, { 0, 0, 4096, 0 }, 6207, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x377F, -1952, -0x3042, 0 }, { { 3899, -4720, 328, 0 }, { -3898, -4720, -327, 0 }, { 3899, 4720, 328, 0 }, { -3898, 4720, -327, 0 } }, { -345, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 6122, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x374F, -1856, -0x2EE1, 0 }, { { -3724, -4720, -353, 0 }, { 3725, -4720, 354, 0 }, { -3724, 4720, -353, 0 }, { 3725, 4720, 354, 0 } }, { 389, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 6014, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 9984, -2016, -2592, 0 }, { { -394, -4720, -3836, 0 }, { 402, -4720, 3931, 0 }, { -403, 4720, -3932, 0 }, { 393, 4720, 3835, 0 } }, { 4079, -1, -419, 0 }, { 0, 0, 4096, 0 }, 6144, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2780, -1984, -2432, 0 }, { { 437, -4720, 4442, 0 }, { -428, -4720, -4347, 0 }, { 427, 4720, 4346, 0 }, { -438, 4720, -4443, 0 } }, { -4093, -1, 402, 0 }, { 0, 0, 4096, 0 }, 6496, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3680, -1440, -0x5AE0, 0 }, { { 3887, -4720, -439, 0 }, { -3887, -4720, 439, 0 }, { 3887, 4720, -439, 0 }, { -3887, 4720, 439, 0 } }, { 461, 0, 4083, 0 }, { 0, 0, 4096, 0 }, 6122, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3720, -1312, -0x5A40, 0 }, { { -3887, -4720, 439, 0 }, { 3887, -4720, -439, 0 }, { -3887, 4720, 439, 0 }, { 3887, 4720, -439, 0 } }, { -462, 0, -4084, 0 }, { 0, 0, 4096, 0 }, 6122, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3D9E, -1313, -0x5061, 0 }, { { -980, -4720, -1461, 0 }, { 980, -4720, 1460, 0 }, { -980, 4720, -1461, 0 }, { 980, 4720, 1460, 0 } }, { 3404, 0, -2286, 0 }, { 0, 0, 4096, 0 }, 5016, 0, 15, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x37C1, -1696, -0x3F00, 0 }, { { 4000, -4720, 0, 0 }, { -4000, -4720, 0, 0 }, { 4000, 4720, 0, 0 }, { -4000, 4720, 0, 0 } }, { 0, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 6186, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3870, -1568, -0x3E64, 0 }, { { -4496, -4720, 6, 0 }, { 4496, -4720, -5, 0 }, { -4496, 4720, 6, 0 }, { 4496, 4720, -5, 0 } }, { -6, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 6516, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3640, -1632, -0x4E40, 0 }, { { -4496, -4720, 6, 0 }, { 4496, -4720, -5, 0 }, { -4496, 4720, 6, 0 }, { 4496, 4720, -5, 0 } }, { -6, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 6516, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x36C1, -1792, -0x4F01, 0 }, { { 4000, -4720, 0, 0 }, { -4000, -4720, 0, 0 }, { 4000, 4720, 0, 0 }, { -4000, 4720, 0, 0 } }, { 0, 0, 4096, 0 }, { 0, 0, 4096, 0 }, 6186, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3A1F, -1632, -0x56D2, 0 }, { { 86, -4720, -525, 0 }, { -85, -4720, 526, 0 }, { 86, 4720, -525, 0 }, { -85, 4720, 526, 0 } }, { 4062, 0, 660, 0 }, { 0, 0, 4096, 0 }, 4748, 0, 15, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3DE0, -1504, -0x59F1, 0 }, { { 980, -4720, -355, 0 }, { -980, -4720, 355, 0 }, { 980, 4720, -355, 0 }, { -980, 4720, 355, 0 } }, { 1395, 0, 3852, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 15, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b3_garbage_incinerator_8018F734[6] = {
    { NULL, NULL, NULL, { 0x3710, -96, -0x64B0, 0 }, { { -1072, 0, -496, 0 }, { 1072, 0, -528, 0 }, { -1072, 0, 528, 0 }, { 1072, 0, 496, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 1193, WORLD_COLLISION_TRIGGER_ACTION_WARP, 41, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 528, -80, -800, 0 }, { { 480, 0, -544, 0 }, { 448, 0, 544, 0 }, { -448, 0, -544, 0 }, { -480, 0, 544, 0 } }, { 0, 4099, 0, 0 }, { 4096, 0, 0, 0 }, 724, WORLD_COLLISION_TRIGGER_ACTION_WARP, 39, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4380, -64, -0x38E1, 0 }, { { -288, 0, -592, 0 }, { 288, 0, -560, 0 }, { -288, 0, 560, 0 }, { 288, 0, 592, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 655, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 1, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4360, -64, -0x5760, 0 }, { { -240, 0, -528, 0 }, { 240, 0, -496, 0 }, { -240, 0, 496, 0 }, { 240, 0, 528, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 579, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 1, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x422F, -64, -0x35C1, 0 }, { { 341, 0, -1249, 0 }, { 1337, 0, 226, 0 }, { -1336, 0, -225, 0 }, { -340, 0, 1250, 0 } }, { 0, 4096, 0, 0 }, { -3858, 0, -1380, 0 }, 1354, WORLD_COLLISION_TRIGGER_ACTION_CAP, 20, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4240, -64, -0x5580, 0 }, { { 217, 0, -1277, 0 }, { 1352, 0, 94, 0 }, { -1352, 0, -94, 0 }, { -216, 0, 1277, 0 } }, { 0, 4096, 0, 0 }, { -4018, 0, -800, 0 }, 1354, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 1, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b3_garbage_incinerator_8018F8FC[4] = {
    { 32, 440, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_444000_80161854 },
    { 103, 419, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_shelter_b3_garbage_incinerator_8018FAC0 },
    { 44, 422, AREA_RESOURCE_FILE_GROUP_BASE_60, 1, { 0, 0 }, D_shelter_b3_garbage_incinerator_8018FAC0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b3_garbage_incinerator_8018F92C[4] = {
    { 32, 1, 0, 5500, 1, -2400, 1024, 0, 0, 2, 0 },
    { 103, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 44, 1, 0, 0x38C0, 0, -5888, 2048, 0, 4, 6, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaResource D_shelter_b3_garbage_incinerator_8018F96C[4] = {
    { 70, 70, AREA_RESOURCE_FILE_GROUP_BASE_20, 2, { 0, 0 }, &D_actor_207000_801575F0 },
    { 71, 71, AREA_RESOURCE_FILE_GROUP_BASE_20, 1, { 0, 0 }, &D_actor_207000_80151E60 },
    { 44, 424, AREA_RESOURCE_FILE_GROUP_BASE_30, 1, { 0, 0 }, D_actor_342400_80173A54 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b3_garbage_incinerator_8018F99C[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaResource D_shelter_b3_garbage_incinerator_8018F9AC[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b3_garbage_incinerator_8018F9B8[4] = {
    { 44, 1, 0, 0x38C0, 4000, -5888, 2048, 0, 0, 2, 0 },
    { 70, 1, 0, 0x2B5C, 0, -8200, 2048, 0, 2, 4, 0 },
    { 71, 1, 1, 5000, 0, -5900, 2048, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaResource D_shelter_b3_garbage_incinerator_8018F9F8[4] = {
    { 32, 440, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_444000_80161854 },
    { 103, 419, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_shelter_b3_garbage_incinerator_8018FAC0 },
    { 44, 422, AREA_RESOURCE_FILE_GROUP_BASE_60, 1, { 0, 0 }, D_shelter_b3_garbage_incinerator_8018FAC0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b3_garbage_incinerator_8018FA28[3] = {
    { 32, 1, 0, 5500, 1, -2400, 1024, 0, 0, 2, 0 },
    { 44, 1, 0, 0x38C0, 0, -5888, 2048, 0, 4, 6, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b3_garbage_incinerator_8018FA58[13] = {
    { NULL, NULL },
    { D_shelter_b3_garbage_incinerator_8018F92C, D_shelter_b3_garbage_incinerator_8018F8FC },
    { D_shelter_b3_garbage_incinerator_8018F9B8, D_shelter_b3_garbage_incinerator_8018F96C },
    { D_shelter_b3_garbage_incinerator_8018F99C, D_shelter_b3_garbage_incinerator_8018F9AC },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b3_garbage_incinerator_8018FA28, D_shelter_b3_garbage_incinerator_8018F9F8 },
    { NULL, NULL },
    { NULL, NULL },
};

TaskDesc D_shelter_b3_garbage_incinerator_8018FAC0[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b3_garbage_incinerator_80184D84, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _shelterB3GarbageIncineratorKillTask, { .value = 0 } },
};

WorldCollisionOccluder D_shelter_b3_garbage_incinerator_8018FAD8[1] = {
    { NULL, NULL, { 4624, -2864, -5504, 0 }, { { -5200, 3888, 0, 0 }, { 5200, 3888, 0, 0 }, { -5200, -3888, 0, 0 }, { 5200, -3888, 0, 0 } }, { 0, 0, 4116, 0 }, 6476, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionFootstepSounds D_shelter_b3_garbage_incinerator_8018FB14 = {
    0x10000015,
    0x10000017,
    0x10000019,
};

WorldCollisionFootstepSounds D_shelter_b3_garbage_incinerator_8018FB20 = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionSurfaceProperties D_shelter_b3_garbage_incinerator_8018FB2C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b3_garbage_incinerator_8018FB20 },
};

WorldCollisionSurfaceProperties D_shelter_b3_garbage_incinerator_8018FB34[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b3_garbage_incinerator_8018FB20 },
};

WorldCollisionSurfaceProperties D_shelter_b3_garbage_incinerator_8018FB3C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b3_garbage_incinerator_8018FB14 },
};

WorldCollisionSurfaceProperties D_shelter_b3_garbage_incinerator_8018FB44[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b3_garbage_incinerator_8018FB14 },
};

WorldCollisionSurfaceProperties* D_shelter_b3_garbage_incinerator_8018FB4C[8] = {
    D_shelter_b3_garbage_incinerator_8018FB2C,
    D_shelter_b3_garbage_incinerator_8018FB34,
    D_shelter_b3_garbage_incinerator_8018FB2C,
    D_shelter_b3_garbage_incinerator_8018FB3C,
    D_shelter_b3_garbage_incinerator_8018FB44,
    D_shelter_b3_garbage_incinerator_8018FB2C,
    D_shelter_b3_garbage_incinerator_8018FB2C,
    D_shelter_b3_garbage_incinerator_8018FB2C,
};

AreaApplyRec D_shelter_b3_garbage_incinerator_8018FB6C[23] = {
    { 4, 1, 3, 17 },
    { 4, 1, 7, 33 },
    { 4, 2, 2, 1 },
    { 4, 3, 3, 1 },
    { 4, 4, 3, 1 },
    { 4, 5, 3, 1 },
    { 4, 7, 3, 1 },
    { 4, 9, 2, 1 },
    { 4, 10, 2, 1 },
    { 4, 11, 2, 1 },
    { 4, 12, 2, 1 },
    { 4, 14, 2, 1 },
    { 4, 15, 2, 1 },
    { 4, 17, 3, 1 },
    { 4, 25, 2, 1 },
    { 4, 30, 3, 1 },
    { 4, 32, 3, 1 },
    { 4, 33, 3, 1 },
    { 4, 34, 3, 1 },
    { 4, 35, 3, 1 },
    { 4, 39, 2, 0 },
    { 4, 40, 2, 0 },
    { 255, 0, 0, 0 },
};

// Low half initializes the successor scene clock; the high half remains unexplained.
u16 D_shelter_b3_garbage_incinerator_8018FBC8[2] = { 0x2292, 0xDECA };

u16 D_shelter_b3_garbage_incinerator_8018FBCC[6][4] = {
    { 0x32C8, 0xC568, 0x2710, 0xC568 },
    { 0x32C8, 0xADF8, 0x32C8, 0xC568 },
    { 0x2710, 0xADF8, 0x32C8, 0xADF8 },
    { 0x4268, 0xC568, 0x3A98, 0xC568 },
    { 0x3A98, 0xC568, 0x3A98, 0xADF8 },
    { 0x3A98, 0xADF8, 0x4268, 0xADF8 },
};

u16 D_shelter_b3_garbage_incinerator_8018FBFC[6][4] = {
    { 0x32C8, 0xC568, 0x2710, 0xC568 },
    { 0x32C8, 0xADF8, 0x32C8, 0xC568 },
    { 0x2710, 0xADF8, 0x32C8, 0xADF8 },
    { 0x4268, 0xC568, 0x3A98, 0xC568 },
    { 0x4268, 0xC568, 0x2710, 0xC568 },
    { 0x2710, 0xADF8, 0x4268, 0xADF8 },
};

RoomEventMsg D_shelter_b3_garbage_incinerator_8018FC2C = { 0 };

Task* D_shelter_b3_garbage_incinerator_8018FC34 = NULL;

ScreenWaveCtx* gScreenWaveCtx = NULL;

Task* D_shelter_b3_garbage_incinerator_8018FC3C = NULL;

CapCommandRef* CapCaption_Data_8015E650 = NULL;

TextGlyphCell* CapCaption_Data_8015E654 = NULL;

CapSequenceRecord* CapCaption_Data_8015E658 = NULL;

s16 CapCaption_Data_8015E65C = 0;

s16 CapCaption_Data_8015E65E = 0;

s16 CapCaption_Data_8015E660 = 0;

s16 CapCaption_Data_8015E662 = 0;

s16 CapCaption_Data_8015E664 = 0;

s16 CapCaption_Data_8015E666 = 0;

u16 CapCaption_Data_8015E668 = 0;

u16 CapCaption_Data_8015E66A = 0;

u8 CapCaption_Data_8015E66C[4] = {
    0,
    196,
    94,
    51,
};

ScreenWaveGridOscillator gScreenWaveColumns[10];

ScreenWaveGridOscillator gScreenWaveRows[30];

POLY_FT4 gScreenWaveGrid[2][30][8];

/// Draws the layered warning glow and two grey lamp discs beside it.
///
/// `warningColor` packs RGB nibbles in bits 8..11, 4..7 and 0..3; a nonzero
/// high nibble selects the odd-frame intensity shift, while zero selects a
/// sine pulse at 128 angle units per animation frame (4096 per turn).
/// All three glows use radius scale 512: the disc/outer glow radius is
/// `512 * 64 / depth` pixels, where depth is camera Z / 4.
/// Requires composed view matrices, initialized scratch and a current packet
/// arena/ordering table. Each accepted point must have nonzero projected depth;
/// queued additive packets live in the frame arena until GPU completion.
static inline void _shelterB3GarbageIncineratorDrawWarningAndGreyLamps(u16 warningColor)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_WARNING_LAMP_POINT     = 0,
        SHELTER_B3_GARBAGE_INCINERATOR_FIRST_GREY_LAMP_POINT  = 9,
        SHELTER_B3_GARBAGE_INCINERATOR_SECOND_GREY_LAMP_POINT = 10,
        SHELTER_B3_GARBAGE_INCINERATOR_LAMP_RADIUS_SCALE      = 512,
        SHELTER_B3_GARBAGE_INCINERATOR_WARNING_GLOW_RATE      = 0x80,
        SHELTER_B3_GARBAGE_INCINERATOR_GREY_LAMP              = 0x3444,
    };
    _shelterB3GarbageIncineratorDrawLayeredGlow(&D_shelter_b3_garbage_incinerator_80187544[SHELTER_B3_GARBAGE_INCINERATOR_WARNING_LAMP_POINT], SHELTER_B3_GARBAGE_INCINERATOR_LAMP_RADIUS_SCALE, warningColor, SHELTER_B3_GARBAGE_INCINERATOR_WARNING_GLOW_RATE);
    glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[SHELTER_B3_GARBAGE_INCINERATOR_FIRST_GREY_LAMP_POINT], SHELTER_B3_GARBAGE_INCINERATOR_LAMP_RADIUS_SCALE, SHELTER_B3_GARBAGE_INCINERATOR_GREY_LAMP);
    glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[SHELTER_B3_GARBAGE_INCINERATOR_SECOND_GREY_LAMP_POINT], SHELTER_B3_GARBAGE_INCINERATOR_LAMP_RADIUS_SCALE, SHELTER_B3_GARBAGE_INCINERATOR_GREY_LAMP);
}

void shelterB3GarbageIncineratorDrawLightsTask(Task* task)
{
    // RGB nibbles occupy bits 8..11, 4..7 and 0..3. Flickering colours also
    // encode the odd-frame intensity shift in bits 12..15.
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_WARNING_ORANGE          = 0x3C40,
        SHELTER_B3_GARBAGE_INCINERATOR_WARNING_BLUE            = 0x304C,
        SHELTER_B3_GARBAGE_INCINERATOR_WARNING_SWAP_MASK       = 2,
        SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_1           = 0x5100,
        SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_2           = 0x5200,
        SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_3           = 0x5300,
        SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4           = 0x5400,
        SHELTER_B3_GARBAGE_INCINERATOR_RED_LAMP                = 0x3400,
        SHELTER_B3_GARBAGE_INCINERATOR_GREY_LAMP               = 0x3444,
        SHELTER_B3_GARBAGE_INCINERATOR_DIM_GREY_LAMP           = 0x3333,
        SHELTER_B3_GARBAGE_INCINERATOR_GREEN_LAMP              = 0x3040,
        SHELTER_B3_GARBAGE_INCINERATOR_CYAN_LAMP               = 0x0044,
        SHELTER_B3_GARBAGE_INCINERATOR_GREEN_PULSE             = 0x03F6,
        SHELTER_B3_GARBAGE_INCINERATOR_EXIT_GLOW               = 0x0F63,
        SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_OFF           = 0x0000,
        SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_1             = 0x0100,
        SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2             = 0x0200,
        SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3             = 0x0300,
        SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4             = 0x0400,
        SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE          = 0x40, // Angle units per animation frame; 4096 per turn
        SHELTER_B3_GARBAGE_INCINERATOR_WARNING_GLOW_RATE       = 0x80,
        SHELTER_B3_GARBAGE_INCINERATOR_GREEN_PULSE_RATE        = 0xC0,
        SHELTER_B3_GARBAGE_INCINERATOR_EXIT_PULSE_AND_ROTATION = 0x10C0, // Rate 0xC0, rotation enabled by bit 12
    };

    EffectWork* work;
    u32         liftPhase;
    u8          mappedViewIndex;

    work                             = task->spawnArg2.pointer;
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    liftPhase                        = gGameSession->incineratorDescentPhase;

    // Keep the warning colour in the effect work for the view-specific draws.
    if (liftPhase != GAME_SESSION_INCINERATOR_DESCENT_WAITING) {
        if (liftPhase < GAME_SESSION_INCINERATOR_DESCENT_LANDED && (gDisplayState.animFrame & SHELTER_B3_GARBAGE_INCINERATOR_WARNING_SWAP_MASK) == 0) {
            work->scale = SHELTER_B3_GARBAGE_INCINERATOR_WARNING_ORANGE;
        } else {
            work->scale = SHELTER_B3_GARBAGE_INCINERATOR_WARNING_BLUE;
        }
    } else {
        work->scale = SHELTER_B3_GARBAGE_INCINERATOR_WARNING_ORANGE;
    }

    // Logical views can share a camera; select lights by the mapped index.
    mappedViewIndex = viewGetMappedIndex();
    switch (mappedViewIndex) {
        case 0x02:
        case 0x16:
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_8018759C[0], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_8018759C[1], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_8018759C[5], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_8018759C[16], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_8018759C[17], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_8018759C[24], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_8018759C[25], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            break;
        case 0x03:
        case 0x17:
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_8018759C[0], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_2);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_8018759C[1], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_3);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_8018759C[2], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_8018759C[5], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_2);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_8018759C[6], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_3);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_8018759C[7], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_8018759C[16], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_8018759C[17], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_8018759C[18], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_8018759C[19], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_8018759C[20], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_8018759C[24], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_8018759C[25], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_8018759C[26], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_8018759C[27], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_8018759C[28], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            break;
        case 0x04:
        case 0x18:
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_801875AC[0], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_2);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_801875AC[1], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_2);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_801875AC[2], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_2);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_801875AC[6], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_3);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_801875AC[7], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[18], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[19], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[20], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[21], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[28], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[29], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[30], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[31], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[32], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[33], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            break;
        case 0x05:
        case 0x19:
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_801875B4[0], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_1);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_801875B4[1], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_1);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_801875B4[5], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_2);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_801875B4[6], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_3);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_801875B4[7], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_801875B4[10], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875B4[19], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_1, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875B4[20], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_1, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875B4[28], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_1, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875B4[29], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_1, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875B4[30], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875B4[31], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875B4[32], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875B4[33], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875B4[34], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875B4[49], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875B4[50], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875B4[51], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            break;
        case 0x06:
        case 0x1A:
        case 0x23:
            _shelterB3GarbageIncineratorDrawWarningAndGreyLamps(work->scale);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[19], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_1);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[20], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_2);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[21], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_3);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[22], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[24], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_3);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[25], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[42], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_1, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[45], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_1, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[46], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_1, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[47], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_1, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[48], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[49], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[50], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[51], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[63], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_1, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[64], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[65], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[66], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[67], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[68], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            break;
        case 0x07:
        case 0x1B:
        case 0x24:
            _shelterB3GarbageIncineratorDrawWarningAndGreyLamps(work->scale);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[20], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_1);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[21], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_2);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[22], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_3);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[24], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_2);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[25], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_3);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[47], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_1, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[48], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_1, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[49], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[50], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[51], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[52], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[53], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[54], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[55], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[64], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_1, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[65], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_1, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[66], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[67], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[68], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[69], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[70], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[71], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            break;
        case 0x08:
        case 0x1C:
        case 0x22:
        case 0x25:
            _shelterB3GarbageIncineratorDrawLayeredGlow(&D_shelter_b3_garbage_incinerator_80187544[0], 0x200, work->scale, SHELTER_B3_GARBAGE_INCINERATOR_WARNING_GLOW_RATE);
            _shelterB3GarbageIncineratorDrawLayeredGlow(&D_shelter_b3_garbage_incinerator_80187544[1], 0x200, work->scale, SHELTER_B3_GARBAGE_INCINERATOR_WARNING_GLOW_RATE);
            if (gGameSession->incineratorExitPhase == GAME_SESSION_INCINERATOR_EXIT_WARP) {
                _shelterB3GarbageIncineratorDrawLayeredGlow(&D_shelter_b3_garbage_incinerator_80187544[3], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_EXIT_GLOW, SHELTER_B3_GARBAGE_INCINERATOR_EXIT_PULSE_AND_ROTATION);
            }
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[20], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[21], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[22], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[24], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[25], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[49], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_1, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[50], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_OFF, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[51], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[52], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[53], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[54], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[55], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[56], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[57], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[67], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_1, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[68], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_1, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[69], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[70], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[71], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[72], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[73], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187544[74], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            break;
        case 0x09:
        case 0x1D:
            _shelterB3GarbageIncineratorDrawLayeredGlow(&D_shelter_b3_garbage_incinerator_8018754C[0], 0x200, work->scale, SHELTER_B3_GARBAGE_INCINERATOR_WARNING_GLOW_RATE);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_8018754C[5], 0x280, SHELTER_B3_GARBAGE_INCINERATOR_GREEN_LAMP);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_8018754C[6], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_GREY_LAMP);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_8018754C[7], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_GREY_LAMP);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_8018754C[22], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_8018754C[25], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            break;
        case 0x0A:
            _shelterB3GarbageIncineratorDrawLayeredGlow(&D_shelter_b3_garbage_incinerator_80187554[0], 0x180, SHELTER_B3_GARBAGE_INCINERATOR_GREEN_PULSE, SHELTER_B3_GARBAGE_INCINERATOR_GREEN_PULSE_RATE);
            /* fallthrough */
        case 0x1E:
        case 0x26:
            _shelterB3GarbageIncineratorDrawWarningAndGreyLamps(work->scale);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[22], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            break;
        case 0x0B:
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187564[0], 0x280, SHELTER_B3_GARBAGE_INCINERATOR_CYAN_LAMP);
            break;
        case 0x0C:
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_801875AC[0], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_801875AC[1], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_3);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_801875AC[2], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_2);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_801875AC[5], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_801875AC[6], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_1);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_801875AC[7], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_1);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[20], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[21], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[28], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[29], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[30], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[31], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[32], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[33], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_2, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[34], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[50], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_1, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_801875AC[51], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_1, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            break;
        case 0x0E:
            _shelterB3GarbageIncineratorDrawWarningAndGreyLamps(work->scale);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[22], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            break;
        case 0x0F:
        case 0x1F:
        case 0x27:
            _shelterB3GarbageIncineratorDrawLayeredGlow(&D_shelter_b3_garbage_incinerator_8018754C[0], 0x200, work->scale, SHELTER_B3_GARBAGE_INCINERATOR_WARNING_GLOW_RATE);
            if (gGameSession->incineratorExitPhase == GAME_SESSION_INCINERATOR_EXIT_WARP) {
                _shelterB3GarbageIncineratorDrawLayeredGlow(&D_shelter_b3_garbage_incinerator_8018754C[2], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_EXIT_GLOW, SHELTER_B3_GARBAGE_INCINERATOR_EXIT_PULSE_AND_ROTATION);
            }
            break;
        case 0x10:
        case 0x20:
            _shelterB3GarbageIncineratorDrawLayeredGlow(&D_shelter_b3_garbage_incinerator_80187544[0], 0x200, work->scale, SHELTER_B3_GARBAGE_INCINERATOR_WARNING_GLOW_RATE);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[9], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_DIM_GREY_LAMP);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[10], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_DIM_GREY_LAMP);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[20], 0x100, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[21], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[22], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[24], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187544[25], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            break;
        case 0x11:
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_8018759C[0], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_3);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_8018759C[1], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_8018759C[5], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_3);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_8018759C[16], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_8018759C[17], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_8018759C[24], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_8018759C[25], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_3, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            break;
        case 0x12:
        case 0x21:
            _shelterB3GarbageIncineratorDrawLayeredGlow(&D_shelter_b3_garbage_incinerator_80187544[0], 0x200, work->scale, SHELTER_B3_GARBAGE_INCINERATOR_WARNING_GLOW_RATE);
            break;
        case 0x13:
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187614[0], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_LAMP);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187614[35], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187614[36], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            break;
        case 0x15:
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187574[0], 0x280, SHELTER_B3_GARBAGE_INCINERATOR_RED_LAMP);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187574[1], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_GREY_LAMP);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187574[2], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_GREY_LAMP);
            glowDrawDisc(&D_shelter_b3_garbage_incinerator_80187574[20], 0x200, SHELTER_B3_GARBAGE_INCINERATOR_RED_FLICKER_4);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187574[56], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            _shelterB3GarbageIncineratorDrawPulsingDisc(&D_shelter_b3_garbage_incinerator_80187574[72], 0x300, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_4, SHELTER_B3_GARBAGE_INCINERATOR_RED_PULSE_RATE);
            break;
    }
}

#define GLOW_DRAW_DISC_SHIFTED_FLICKER 1
#include "../../shared/glow_draw_disc.inc.c"

#include "../../shared/effect_sprite_drift_aimed.inc.c"

#define EFFECT_SPRITE_BANKED_FIRST_TEXEL_ROW 112
#include "../../shared/effect_sprite_draw_banked.inc.c"

#include "../../shared/effect_sprite_draw_rotated.inc.c"

#include "../../shared/effect_sprite_debris.inc.c"

#include "../../shared/effect_sprite_draw_chip.inc.c"

#include "../../shared/effect_sprite_draw_billboard.inc.c"

/// Draws a sine-pulsed additive disc around a world point.
///
/// The signed low halfword of `radiusScale` gives a pixel radius of
/// `radiusScale * 64 / depth`, with depth equal to camera Z / 4. RGB nibbles
/// in `packedColor` bits 8..11, 4..7 and 0..3 scale a centre intensity of
/// `rsin(animFrame * pulseRate) / 34 + 120`; the rim is black. `pulseRate`
/// uses its signed low halfword, in 4096 angle units per animation frame.
/// Rejects negative GTE flags and requires nonzero depth. Borrows `worldPoint`
/// during the call, reserves one scratch block, and queues four Gouraud quads
/// plus additive blend commands in the current frame's packet arena. View
/// matrices, scratch stack and arena must be ready.
static void _shelterB3GarbageIncineratorDrawPulsingDisc(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor, s32 pulseRate)
{
    GlowCentreScratch* block;
    POLY_G4*           wedge;
    s32                screenRadius;
    s32                angle;
    s32                halfStepAngle;
    s32                nextAngle;
    u8                 pulseLevel;
    u8                 red;
    u8                 green;
    u8                 blue;

    block = SCRATCH_STACK_RESERVE_BLOCK(GlowCentreScratch);

    // Project once; all wedges share the screen centre and sorting depth.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        pulseLevel    = rsin(gDisplayState.animFrame * (s16)pulseRate) / GLOW_PULSE_DIVISOR + GLOW_PULSE_BASE_INTENSITY;
        screenRadius  = ((s16)radiusScale * GLOW_RADIUS_SCALE) / block->otz;
        red           = pulseLevel * (((s16)packedColor >> 8) & 0xF) / 15;
        green         = pulseLevel * (((s16)packedColor >> 4) & 0xF) / 15;
        blue          = pulseLevel * (packedColor & 0xF) / 15;
        angle         = 0;
        block->radius = screenRadius;
        do {
            wedge         = _glowAllocateDiscWedge(red, green, blue);
            wedge->x0     = block->sx + ((block->radius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            halfStepAngle = angle + GLOW_EIGHTH_TURN;
            wedge->y0     = block->sy + ((block->radius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            wedge->x1     = block->sx + ((block->radius * rsin(halfStepAngle)) >> GLOW_TRIG_SHIFT);
            wedge->y1     = block->sy + ((block->radius * rcos(halfStepAngle)) >> GLOW_TRIG_SHIFT);
            nextAngle     = angle + GLOW_QUARTER_TURN;
            wedge->x2     = block->sx;
            wedge->y2     = block->sy;
            wedge->x3     = block->sx + ((block->radius * rsin(nextAngle)) >> GLOW_TRIG_SHIFT);
            wedge->y3     = block->sy + ((block->radius * rcos(nextAngle)) >> GLOW_TRIG_SHIFT);
            angle         = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    wedge);
            gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, block->otz);
        } while (angle < GLOW_FULL_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowCentreScratch);
}

/// Draws two tinted glow fans and four blades around a world point.
///
/// `radiusScale` gives outer/inner pixel radii of `radiusScale * 64 / depth`
/// and `radiusScale * 8 / depth`, with depth equal to camera Z / 4. RGB nibbles
/// are in `packedColor` bits 8..11, 4..7 and 0..3. A nonzero high nibble adds
/// `1 << nibble` on odd frames to each nibble scaled by 16; bytes wrap.
/// With a zero high nibble, intensity is the byte-wrapped result of
/// `rsin(animFrame * rate) / 68 - 76`, ranging from 120 to 240.
///
/// `pulseAndRotation` bits 0..11 are the rate, in 4096 angle units per frame;
/// bit 12 rotates by 128 units per frame. Higher bits are ignored. Eight dim
/// outer wedges have bright copies at half radius; half-bright blades alternate
/// between the outer radius and twice it. Rejects negative GTE flags and requires
/// nonzero depth. Borrows `worldPoint` during the call, reserves one scratch
/// block, and queues twenty additive quads plus blend commands in the current
/// frame's packet arena. View matrices, scratch stack and arena must be ready.
static void _shelterB3GarbageIncineratorDrawLayeredGlow(const SVECTOR* worldPoint, u16 radiusScale, u16 packedColor, u16 pulseAndRotation)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_GLOW_ROTATE         = 0x1000,
        SHELTER_B3_GARBAGE_INCINERATOR_GLOW_PHASE_MASK     = GLOW_FULL_TURN - 1,
        SHELTER_B3_GARBAGE_INCINERATOR_GLOW_ROTATION_SHIFT = 7,
        SHELTER_B3_GARBAGE_INCINERATOR_GLOW_ROTATION_MASK  = 0xF80,
        SHELTER_B3_GARBAGE_INCINERATOR_GLOW_FLICKER_MASK   = 0xF000,
        SHELTER_B3_GARBAGE_INCINERATOR_GLOW_PULSE_DIVISOR  = 68,
        SHELTER_B3_GARBAGE_INCINERATOR_GLOW_PULSE_SUBTRACT = 76,
    };

    GlowCentreRadiiScratch* block;
    POLY_G4*                wedge;
    s32                     angle;
    s32                     halfStepAngle;
    s32                     nextAngle;
    s32                     previousBladeAngle;
    s32                     nextBladeAngle;
    s32                     oppositeBladeAngle;
    u16                     animationFrame;
    u16                     startAngle;
    s32                     flicker;
    u32                     colorWord;
    u8                      pulseLevel;
    u8                      red;
    u8                      green;
    u8                      blue;

    /// Initializes a glow quad with a coloured vertex 2 and a black rim.
    ///
    /// `packet` must be a side-effect-free pointer to writable POLY_G4 storage;
    /// it is evaluated repeatedly. Each colour is evaluated once and narrows to
    /// a byte. Captures no locals; geometry, DMA linkage and blend remain unset.
#define SHELTER_B3_GARBAGE_INCINERATOR_INIT_GLOW_WEDGE(packet, red, green, blue) \
    (setPolyG4(packet),                                                          \
     setRGB0(packet, 0, 0, 0),                                                   \
     setRGB1(packet, 0, 0, 0),                                                   \
     setRGB2(packet, (red), (green), (blue)),                                    \
     setRGB3(packet, 0, 0, 0))

    block = SCRATCH_STACK_RESERVE_BLOCK(GlowCentreRadiiScratch);

    // Project once; the two fans and four blades share the centre and depth.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        animationFrame = gDisplayState.animFrame;
        if (pulseAndRotation & SHELTER_B3_GARBAGE_INCINERATOR_GLOW_ROTATE) {
            startAngle = (animationFrame << SHELTER_B3_GARBAGE_INCINERATOR_GLOW_ROTATION_SHIFT) & SHELTER_B3_GARBAGE_INCINERATOR_GLOW_ROTATION_MASK;
        } else {
            startAngle = 0;
        }
        pulseAndRotation &= SHELTER_B3_GARBAGE_INCINERATOR_GLOW_PHASE_MASK;
        // A nonzero high nibble selects frame flicker; zero selects a wrapped pulse.
        if (packedColor & SHELTER_B3_GARBAGE_INCINERATOR_GLOW_FLICKER_MASK) {
            flicker   = animationFrame & 1;
            colorWord = packedColor;
            flicker <<= colorWord >> 12;
            red       = flicker + ((colorWord >> 4) & 0xF0);
            green     = flicker + (colorWord & 0xF0);
            blue      = flicker + ((packedColor & 0xF) << 4);
        } else {
            pulseLevel = rsin((animationFrame * pulseAndRotation) & SHELTER_B3_GARBAGE_INCINERATOR_GLOW_PHASE_MASK) / SHELTER_B3_GARBAGE_INCINERATOR_GLOW_PULSE_DIVISOR - SHELTER_B3_GARBAGE_INCINERATOR_GLOW_PULSE_SUBTRACT;
            red        = pulseLevel * ((packedColor >> 8) & 0xF) / 15;
            green      = pulseLevel * ((packedColor >> 4) & 0xF) / 15;
            blue       = pulseLevel * (packedColor & 0xF) / 15;
        }
        block->outerRadius = (radiusScale * GLOW_RADIUS_SCALE) / block->otz;
        block->innerRadius = (radiusScale * GLOW_INNER_RADIUS_SCALE) / block->otz;
        // Eight dim outer wedges receive bright copies at half radius.
        for (angle = startAngle; angle < startAngle + GLOW_FULL_TURN; angle = nextAngle) {
            wedge          = gGpuPrimCursor;
            gGpuPrimCursor = wedge + 1;
            SHELTER_B3_GARBAGE_INCINERATOR_INIT_GLOW_WEDGE(wedge, (u8)red >> 1, (u8)green >> 1, (u8)blue >> 1);
            wedge->x0     = block->sx + ((block->outerRadius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            halfStepAngle = angle + GLOW_SIXTEENTH_TURN;
            wedge->y0     = block->sy + ((block->outerRadius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            wedge->x1     = block->sx + ((block->outerRadius * rsin(halfStepAngle)) >> GLOW_TRIG_SHIFT);
            wedge->y1     = block->sy + ((block->outerRadius * rcos(halfStepAngle)) >> GLOW_TRIG_SHIFT);
            nextAngle     = angle + GLOW_EIGHTH_TURN;
            wedge->x2     = block->sx;
            wedge->y2     = block->sy;
            wedge->x3     = block->sx + ((block->outerRadius * rsin(nextAngle)) >> GLOW_TRIG_SHIFT);
            wedge->y3     = block->sy + ((block->outerRadius * rcos(nextAngle)) >> GLOW_TRIG_SHIFT);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    wedge);
            gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, block->otz);

            wedge          = gGpuPrimCursor;
            gGpuPrimCursor = wedge + 1;
            SHELTER_B3_GARBAGE_INCINERATOR_INIT_GLOW_WEDGE(wedge, red, green, blue);
            wedge->x0 = block->sx + ((block->outerRadius * rsin(angle)) >> (GLOW_TRIG_SHIFT + 1));
            wedge->y0 = block->sy + ((block->outerRadius * rcos(angle)) >> (GLOW_TRIG_SHIFT + 1));
            wedge->x1 = block->sx + ((block->outerRadius * rsin(halfStepAngle)) >> (GLOW_TRIG_SHIFT + 1));
            wedge->y1 = block->sy + ((block->outerRadius * rcos(halfStepAngle)) >> (GLOW_TRIG_SHIFT + 1));
            wedge->x2 = block->sx;
            wedge->y2 = block->sy;
            wedge->x3 = block->sx + ((block->outerRadius * rsin(nextAngle)) >> (GLOW_TRIG_SHIFT + 1));
            wedge->y3 = block->sy + ((block->outerRadius * rcos(nextAngle)) >> (GLOW_TRIG_SHIFT + 1));
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    wedge);
            gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, block->otz);
        }

        // Overlay four half-bright blades, alternating full and double outer radius.
        red   = (u8)red >> 1;
        green = (u8)green >> 1;
        blue  = (u8)blue >> 1;
        for (angle = startAngle + GLOW_EIGHTH_TURN; angle < startAngle + GLOW_FULL_TURN; angle = oppositeBladeAngle) {
            previousBladeAngle = angle - GLOW_QUARTER_TURN;
            wedge              = gGpuPrimCursor;
            gGpuPrimCursor     = wedge + 1;
            SHELTER_B3_GARBAGE_INCINERATOR_INIT_GLOW_WEDGE(wedge, red, green, blue);
            wedge->x0      = block->sx + ((block->innerRadius * rsin(previousBladeAngle)) >> (GLOW_TRIG_SHIFT + 1));
            wedge->y0      = block->sy + ((block->innerRadius * rcos(previousBladeAngle)) >> (GLOW_TRIG_SHIFT + 1));
            wedge->x1      = block->sx + ((block->outerRadius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            wedge->y1      = block->sy + ((block->outerRadius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            nextBladeAngle = angle + GLOW_QUARTER_TURN;
            wedge->x2      = block->sx;
            wedge->y2      = block->sy;
            wedge->x3      = block->sx + ((block->innerRadius * rsin(nextBladeAngle)) >> (GLOW_TRIG_SHIFT + 1));
            wedge->y3      = block->sy + ((block->innerRadius * rcos(nextBladeAngle)) >> (GLOW_TRIG_SHIFT + 1));
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    wedge);
            gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, block->otz);

            wedge          = gGpuPrimCursor;
            gGpuPrimCursor = wedge + 1;
            SHELTER_B3_GARBAGE_INCINERATOR_INIT_GLOW_WEDGE(wedge, red, green, blue);
            wedge->x0          = block->sx + ((block->innerRadius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            wedge->y0          = block->sy + ((block->innerRadius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            wedge->x1          = block->sx + ((block->outerRadius * rsin(nextBladeAngle)) >> (GLOW_TRIG_SHIFT - 1));
            wedge->y1          = block->sy + ((block->outerRadius * rcos(nextBladeAngle)) >> (GLOW_TRIG_SHIFT - 1));
            oppositeBladeAngle = angle + GLOW_HALF_TURN;
            wedge->x2          = block->sx;
            wedge->y2          = block->sy;
            wedge->x3          = block->sx + ((block->innerRadius * rsin(oppositeBladeAngle)) >> GLOW_TRIG_SHIFT);
            wedge->y3          = block->sy + ((block->innerRadius * rcos(oppositeBladeAngle)) >> GLOW_TRIG_SHIFT);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    wedge);
            gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, block->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(GlowCentreRadiiScratch);

#undef SHELTER_B3_GARBAGE_INCINERATOR_INIT_GLOW_WEDGE
}

/// Retains an unused no-op function in this overlay's text.
static void _shelterB3GarbageIncineratorNoop(void)
{
}

void func_shelter_b3_garbage_incinerator_80184D84(Task* arg0)
{
    union {
        s32 msg[5];
        struct {
            u8 param1[8];
            u8 param2[8];
        } cd;
    } buf;
    s32 out;
    s32 v;

    switch (arg0->state) {
        case 0:
            v = gPlayerStatus.weapon;
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) {
                v = v + 1;
            } else {
                v = v + 0x22;
            }
            buf.msg[0] = v;
            buf.msg[1] = 1;
            buf.msg[2] = 0;
            buf.msg[3] = 0;
            buf.msg[4] = 0;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, buf.msg, 0);
            arg0->spawnArg2.pointer = taskSpawnFromTable(D_actor_341900_80164190, 0, 0, 0);
            arg0->state++;
            return;
        case 1:
            if (taskPollKill(arg0->spawnArg2.pointer, &out) != 0) {
                arg0->state++;
            }
            return;
        case 2:
            arg0->state = 3;
            return;
        case 3:
            buf.cd.param1[2] = 0x22;
            buf.cd.param1[3] = 0;
            buf.cd.param1[0] = 0;
            buf.cd.param2[0] = 0x16;
            buf.cd.param2[1] = 0;
            buf.cd.param2[2] = 0;
            buf.cd.param2[3] = 0;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, buf.cd.param1, buf.cd.param2);
            taskKill(arg0);
            break;
    }
}

/// Ends this resource-table task immediately when it is dispatched.
static void _shelterB3GarbageIncineratorKillTask(Task* task)
{
    taskKill(task);
}

/// Rebuilds the two coincident collision quads used by room variant 2.
///
/// Requires this room's active writable grid: faces/normals 6..7 and vertices
/// 24..31. Each wall spans x=10000..13000, y=200..1000 at z=-15000 in grid-local
/// game coordinates, with a normalized +Z normal and room surface class 1
/// (passes probes, ignores weapon impacts, applies pushback). Other variants
/// leave the grid unchanged; cell lists remain valid and are not rebuilt.
static void _shelterB3GarbageIncineratorSetVariant2Walls(void)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_EXTRA_WALL_VARIANT        = 2,
        SHELTER_B3_GARBAGE_INCINERATOR_FIRST_EXTRA_WALL          = 6,
        SHELTER_B3_GARBAGE_INCINERATOR_EXTRA_WALL_END            = 8,
        SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT         = 4,
        SHELTER_B3_GARBAGE_INCINERATOR_PASS_PROBES_SURFACE_CLASS = 1,
        SHELTER_B3_GARBAGE_INCINERATOR_NORMAL_SCALE              = 4096,
    };

    SVECTOR                 normal;
    SVECTOR*                normals;
    SVECTOR*                vertices;
    WorldCollisionGridFace* faces;
    s16                     faceIndex;
    SVECTOR*                normalPointer;

    /// Initializes a wall quad's four vertex indices, surface and normal index.
    ///
    /// The pool must contain `index`; `index * vertexCount` starts four grid
    /// vertices, and `index` also addresses its normal. Pool, index and stride
    /// expressions are evaluated repeatedly and must have no side effects.
    /// Surface class is evaluated once. Captures no locals; use as a statement.
#define SHELTER_B3_GARBAGE_INCINERATOR_INIT_WALL_FACE(pool, index, vertexCount, propertyIndex) \
    ((pool)[(index)].vertexIndices[1] = (index) * (vertexCount) + 1,                           \
     (pool)[(index)].vertexIndices[0] = (index) * (vertexCount),                               \
     (pool)[(index)].vertexIndices[2] = (index) * (vertexCount) + 2,                           \
     (pool)[(index)].vertexIndices[3] = (index) * (vertexCount) + 3,                           \
     (pool)[(index)].surfaceClass     = (propertyIndex),                                       \
     (pool)[(index)].normalIndex      = (index))

    normals  = Gp_GridParams->normals;
    vertices = Gp_GridParams->vertices;
    faces    = Gp_GridParams->faces;
    if (gGameSession->location.loc.variant == SHELTER_B3_GARBAGE_INCINERATOR_EXTRA_WALL_VARIANT) {
        faceIndex = SHELTER_B3_GARBAGE_INCINERATOR_FIRST_EXTRA_WALL;
        do {
            vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT].vx = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 2].vx = 13000;
            vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT].vy = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 2].vy = 1000;
            vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT].vz = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 2].vz = -15000;
            vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 1].vx = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 3].vx = 10000;
            vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 1].vy = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 3].vy = 1000;
            vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 1].vz = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 3].vz = -15000;
            vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT].vy                                                                                     = 200;
            vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 1].vy                                                                                 = 200;
            SHELTER_B3_GARBAGE_INCINERATOR_INIT_WALL_FACE(faces, faceIndex, SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT,
                                                          SHELTER_B3_GARBAGE_INCINERATOR_PASS_PROBES_SURFACE_CLASS);
            normalPointer = &normal;
            normal.vx     = 0;
            normal.vy     = 0;
            normal.vz     = SHELTER_B3_GARBAGE_INCINERATOR_NORMAL_SCALE;
            VectorNormalSS(normalPointer, normalPointer);
            normals[faceIndex] = normal;
            faceIndex++;
        } while (faceIndex < SHELTER_B3_GARBAGE_INCINERATOR_EXTRA_WALL_END);
    }

#undef SHELTER_B3_GARBAGE_INCINERATOR_INIT_WALL_FACE
}

/// Links one exit collision quad to its consecutive vertices and normal.
///
/// Borrows one writable face. `faceIndex` is the wall's grid slot (0..5): its
/// vertices are `4 * faceIndex` through `4 * faceIndex + 3`, and its normal
/// index is `faceIndex`. The caller initializes those vertices and the unit
/// normal separately in the room's live grid. Surface class 1 passes probes,
/// ignores weapon impacts and applies pushback. Leaves vertex/normal pools and
/// cell lists untouched.
static inline void _shelterB3GarbageIncineratorInitExitWallFace(WorldCollisionGridFace* face, s16 faceIndex)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT         = 4,
        SHELTER_B3_GARBAGE_INCINERATOR_PASS_PROBES_SURFACE_CLASS = 1,
    };
    face->vertexIndices[1] = faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 1;
    face->vertexIndices[0] = faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT;
    face->vertexIndices[2] = faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 2;
    face->vertexIndices[3] = faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 3;
    face->surfaceClass     = SHELTER_B3_GARBAGE_INCINERATOR_PASS_PROBES_SURFACE_CLASS;
    face->normalIndex      = faceIndex;
}

void shelterB3GarbageIncineratorSetExitCollisionWalls(void)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT = 4,
        SHELTER_B3_GARBAGE_INCINERATOR_LOW_WALL_HEIGHT   = 400,
    };

    SVECTOR                 normal;
    SVECTOR*                normals;
    SVECTOR*                vertices;
    WorldCollisionGridFace* faces;
    s16                     faceIndex;
    SVECTOR*                normalPointer;

    faceIndex     = 0;
    normals       = Gp_GridParams->normals;
    vertices      = Gp_GridParams->vertices;
    faces         = Gp_GridParams->faces;
    normalPointer = &normal;
    // Extra variant walls use separate slots; the existing cell lists stay valid.
    _shelterB3GarbageIncineratorSetVariant2Walls();
    do {
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT].vx = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 2].vx = D_shelter_b3_garbage_incinerator_8018FBCC[faceIndex][0];
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT].vy = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 2].vy = 0;
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT].vz = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 2].vz = D_shelter_b3_garbage_incinerator_8018FBCC[faceIndex][1];
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 1].vx = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 3].vx = D_shelter_b3_garbage_incinerator_8018FBCC[faceIndex][2];
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 1].vy = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 3].vy = 0;
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 1].vz = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 3].vz = D_shelter_b3_garbage_incinerator_8018FBCC[faceIndex][3];
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT].vy                                                                                    -= SHELTER_B3_GARBAGE_INCINERATOR_LOW_WALL_HEIGHT;
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 1].vy                                                                                -= SHELTER_B3_GARBAGE_INCINERATOR_LOW_WALL_HEIGHT;
        _shelterB3GarbageIncineratorInitExitWallFace(&faces[faceIndex], faceIndex);
        normal.vx = D_shelter_b3_garbage_incinerator_8018FBCC[faceIndex][3] - D_shelter_b3_garbage_incinerator_8018FBCC[faceIndex][1];
        normal.vy = 0;
        normal.vz = D_shelter_b3_garbage_incinerator_8018FBCC[faceIndex][0] - D_shelter_b3_garbage_incinerator_8018FBCC[faceIndex][2];
        VectorNormalSS(normalPointer, normalPointer);
        normals[faceIndex] = normal;
        faceIndex++;
    } while (faceIndex < (s32)ARRAY_SIZE(D_shelter_b3_garbage_incinerator_8018FBCC));
}

/// Links one lift collision quad to its consecutive vertices and normal.
///
/// Borrows one writable face for either the low or arrival-height lift layout.
/// `faceIndex` is the wall's grid slot (0..5): its vertices are `4 * faceIndex`
/// through `4 * faceIndex + 3`, and its normal index is `faceIndex`. The caller
/// initializes those vertices and the unit normal separately in the live grid.
/// Surface class 1 passes probes, ignores weapon impacts and applies pushback.
/// Leaves vertex/normal pools and cell lists untouched.
static inline void _shelterB3GarbageIncineratorInitLiftWallFace(WorldCollisionGridFace* face, s16 faceIndex)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT         = 4,
        SHELTER_B3_GARBAGE_INCINERATOR_PASS_PROBES_SURFACE_CLASS = 1,
    };
    face->vertexIndices[1] = faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 1;
    face->vertexIndices[0] = faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT;
    face->vertexIndices[2] = faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 2;
    face->vertexIndices[3] = faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 3;
    face->normalIndex      = faceIndex;
    face->surfaceClass     = SHELTER_B3_GARBAGE_INCINERATOR_PASS_PROBES_SURFACE_CLASS;
}

void shelterB3GarbageIncineratorSetLiftCollisionWalls(void)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT = 4,
        SHELTER_B3_GARBAGE_INCINERATOR_LOW_WALL_HEIGHT   = 400,
    };

    SVECTOR                 normal;
    SVECTOR*                normals;
    SVECTOR*                vertices;
    WorldCollisionGridFace* faces;
    s16                     faceIndex;
    SVECTOR*                normalPointer;

    faceIndex     = 0;
    normals       = Gp_GridParams->normals;
    vertices      = Gp_GridParams->vertices;
    faces         = Gp_GridParams->faces;
    normalPointer = &normal;
    // Extra variant walls use separate slots; the existing cell lists stay valid.
    _shelterB3GarbageIncineratorSetVariant2Walls();
    do {
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT].vx = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 2].vx = D_shelter_b3_garbage_incinerator_8018FBFC[faceIndex][0];
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT].vy = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 2].vy = 0;
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT].vz = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 2].vz = D_shelter_b3_garbage_incinerator_8018FBFC[faceIndex][1];
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 1].vx = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 3].vx = D_shelter_b3_garbage_incinerator_8018FBFC[faceIndex][2];
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 1].vy = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 3].vy = 0;
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 1].vz = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 3].vz = D_shelter_b3_garbage_incinerator_8018FBFC[faceIndex][3];
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT].vy                                                                                    -= SHELTER_B3_GARBAGE_INCINERATOR_LOW_WALL_HEIGHT;
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 1].vy                                                                                -= SHELTER_B3_GARBAGE_INCINERATOR_LOW_WALL_HEIGHT;
        _shelterB3GarbageIncineratorInitLiftWallFace(&faces[faceIndex], faceIndex);
        normal.vx = D_shelter_b3_garbage_incinerator_8018FBFC[faceIndex][3] - D_shelter_b3_garbage_incinerator_8018FBFC[faceIndex][1];
        normal.vy = 0;
        normal.vz = D_shelter_b3_garbage_incinerator_8018FBFC[faceIndex][0] - D_shelter_b3_garbage_incinerator_8018FBFC[faceIndex][2];
        VectorNormalSS(normalPointer, normalPointer);
        normals[faceIndex] = normal;
        faceIndex++;
    } while (faceIndex < (s32)ARRAY_SIZE(D_shelter_b3_garbage_incinerator_8018FBFC));
}

void shelterB3GarbageIncineratorSetLiftArrivalCollisionWalls(void)
{
    enum {
        SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT = 4,
        SHELTER_B3_GARBAGE_INCINERATOR_ARRIVAL_WALL_Y0   = 1000,
        SHELTER_B3_GARBAGE_INCINERATOR_ARRIVAL_WALL_Y1   = 2000,
    };

    SVECTOR                 normal;
    SVECTOR*                normals;
    SVECTOR*                vertices;
    WorldCollisionGridFace* faces;
    s16                     faceIndex;

    faceIndex = 0;
    normals   = Gp_GridParams->normals;
    vertices  = Gp_GridParams->vertices;
    faces     = Gp_GridParams->faces;
    // Extra variant walls use separate slots; the existing cell lists stay valid.
    _shelterB3GarbageIncineratorSetVariant2Walls();
    do {
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT].vx = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 2].vx = D_shelter_b3_garbage_incinerator_8018FBFC[faceIndex][0];
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT].vy = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 2].vy = 0;
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT].vz = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 2].vz = D_shelter_b3_garbage_incinerator_8018FBFC[faceIndex][1];
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 1].vx = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 3].vx = D_shelter_b3_garbage_incinerator_8018FBFC[faceIndex][2];
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 1].vy = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 3].vy = 0;
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 1].vz = vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 3].vz = D_shelter_b3_garbage_incinerator_8018FBFC[faceIndex][3];
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT].vy                                                                                    += SHELTER_B3_GARBAGE_INCINERATOR_ARRIVAL_WALL_Y0;
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 1].vy                                                                                += SHELTER_B3_GARBAGE_INCINERATOR_ARRIVAL_WALL_Y0;
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 2].vy                                                                                += SHELTER_B3_GARBAGE_INCINERATOR_ARRIVAL_WALL_Y1;
        vertices[faceIndex * SHELTER_B3_GARBAGE_INCINERATOR_WALL_VERTEX_COUNT + 3].vy                                                                                += SHELTER_B3_GARBAGE_INCINERATOR_ARRIVAL_WALL_Y1;
        _shelterB3GarbageIncineratorInitLiftWallFace(&faces[faceIndex], faceIndex);
        normal.vx = D_shelter_b3_garbage_incinerator_8018FBFC[faceIndex][3] - D_shelter_b3_garbage_incinerator_8018FBFC[faceIndex][1];
        normal.vy = 0;
        normal.vz = D_shelter_b3_garbage_incinerator_8018FBFC[faceIndex][0] - D_shelter_b3_garbage_incinerator_8018FBFC[faceIndex][2];
        VectorNormalSS(&normal, &normal);
        normals[faceIndex] = normal;
        faceIndex++;
    } while (faceIndex < (s32)ARRAY_SIZE(D_shelter_b3_garbage_incinerator_8018FBFC));
}

/// Retains an unused wrapper that restores the lift's low collision walls.
///
/// Requires the same active writable room grid as
/// `shelterB3GarbageIncineratorSetLiftCollisionWalls`; owns no storage.
static void _shelterB3GarbageIncineratorResetLiftCollisionWalls(void)
{
    shelterB3GarbageIncineratorSetLiftCollisionWalls();
}
