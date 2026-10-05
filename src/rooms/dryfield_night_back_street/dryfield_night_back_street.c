#include "rooms/dryfield_night_back_street.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"
#include "../../shared/back_street.h"

#define D_dryfield_night_back_street_8018036C (D_dryfield_night_back_street_8018034C + 4)
#define D_dryfield_night_back_street_8018037C (D_dryfield_night_back_street_8018034C + 6)
#define D_dryfield_night_back_street_8018038C (D_dryfield_night_back_street_8018034C + 8)

/// The room's message table, installed on the room entry task.
extern TaskMessageEntry D_dryfield_night_back_street_80180324[];

// Indexed views below share one contiguous table.
s32 func_dryfield_night_back_street_8017D724(Task*, s32, s32, s32);
s32 func_dryfield_night_back_street_8017D72C(Task*, s32, s32, s32);
s32 func_dryfield_night_back_street_8017D734(Task*, s32, s32, s32);

extern WorldCollisionGrid         D_dryfield_night_back_street_80180B34[1];
extern WorldCollisionTrigger      D_dryfield_night_back_street_80180D70[6];
extern WorldCollisionTrigger      D_dryfield_night_back_street_80180F38[10];
extern WorldCoordRoomAmbientEntry D_dryfield_night_back_street_801815C8[6];
extern WorldCoordRoomLights       D_dryfield_night_back_street_80181470[1];

extern TaskDesc Actor00100_D1BA84;

TaskMessageEntry D_dryfield_night_back_street_80180324[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, backStreetEventMsg },
    { 5105, func_dryfield_night_back_street_8017D724 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_back_street_8017D734 },
    { ROOM_MESSAGE_COMMAND, func_dryfield_night_back_street_8017D72C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

SVECTOR D_dryfield_night_back_street_8018034C[10] = {
    { -9920, -2300, 5110, 0 },
    { -9920, -2300, 3900, 0 },
    { -9850, -2330, 5110, 0 },
    { -9850, -2330, 3900, 0 },
    { -8190, -1800, 5850, 0 },
    { -6800, -1800, 5850, 0 },
    { -2110, -1800, 6000, 0 },
    { -820, -1800, 6000, 0 },
    { 8870, -1800, 6000, 0 },
    { 10150, -1800, 6000, 0 },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources D_dryfield_night_back_street_801803AC[1] = {
    { D_dryfield_night_back_street_80180B34, D_dryfield_night_back_street_80180D70, D_dryfield_night_back_street_80180F38, NULL },
};

WorldCoordRoomLighting D_dryfield_night_back_street_801803BC[1] = {
    { D_dryfield_night_back_street_80181470, D_dryfield_night_back_street_801815C8 },
};

u8* D_dryfield_night_back_street_801803C4[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_night_back_street_801803C8[1] = { 5 };

DirectionWarpEntry D_dryfield_night_back_street_801803CC[4] = {
    { { { .word = 1024 }, -9597, 0, 4667 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -8696, 0, 5479 }, { 0, 0, 0, 0 }, 0x53050002, 0x53050001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 470 },
    { { { .word = 2048 }, -7539, 0, 5536 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -8028, 0, 5175 }, { 0, 0, 0, 0 }, 0x53050004, 0x53050003, 0x53050005, 2, DIRECTION_WARP_FLAG_NONE, 469 },
    { { { .word = 2048 }, -1423, 0, 5449 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -2346, 0, 5223 }, { 0, 0, 0, 0 }, 0x53050002, 0x53050001, 0x53050005, 3, DIRECTION_WARP_FLAG_NONE, 468 },
    { { { .word = 2048 }, 9463, 2, 5532 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 9800, 2, 4668 }, { 0, 0, 0, 0 }, 0x53050002, 0x53050001, 0x53050005, 5, DIRECTION_WARP_FLAG_NONE, 467 },
};

static SVECTOR _gDryfieldNightBackStreetCollision03574Normals[19] = {
#include "assets/dryfield_night_back_street_collision_03574_normals.inc"
};

static SVECTOR _gDryfieldNightBackStreetCollision03574Verts[72] = {
#include "assets/dryfield_night_back_street_collision_03574_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightBackStreetCollision03574Faces[32] = {
#include "assets/dryfield_night_back_street_collision_03574_faces.inc"
};

static s16 _gDryfieldNightBackStreetCollision03574Cells[224] = {
#include "assets/dryfield_night_back_street_collision_03574_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightBackStreetCollision03574Cells[i])
static s16* _gDryfieldNightBackStreetCollision03574Table[28] = {
#include "assets/dryfield_night_back_street_collision_03574_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_back_street_80180B34[1] = {
    { NULL, _gDryfieldNightBackStreetCollision03574Normals, _gDryfieldNightBackStreetCollision03574Verts, _gDryfieldNightBackStreetCollision03574Faces, _gDryfieldNightBackStreetCollision03574Table, 0x2AFE, 100, 7, 4, 4000, 32 },
};

ViewCamera D_dryfield_night_back_street_80180B58[5] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -850, 0x7148, -4850 } }, 329 },
    { { { { 696, 0, 4036 }, { -1070, 3949, 184 }, { -3892, -1085, 671 } }, { 4016, 385, -4128 } }, 230 },
    { { { { 681, 0, 4038 }, { 402, 4075, -67 }, { -4018, 408, 677 } }, { -2416, 1408, -4201 } }, 257 },
    { { { { 615, 0, -4049 }, { -873, 3999, -132 }, { 3954, 883, 600 } }, { 2448, 1600, -4230 } }, 230 },
    { { { { 173, 0, -4092 }, { -1464, 3824, -62 }, { 3821, 1465, 162 } }, { -2046, 2454, -4167 } }, 257 },
};

SpriteBatch D_dryfield_night_back_street_80180C0C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_back_street_80180C1C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_back_street_80180C2C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_back_street_80180C3C[5] = {
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 64, -64, 1295, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 72, -72, 1258, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 152 } }, 80, -72, 1125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 200 } }, 96, -88, 1022, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 216 } }, 120, -96, 785, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_back_street_80180CA0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_back_street_80180CB8[5] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -16, 1714, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -16, 1680, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, -24, 1678, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, -16, 1669, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, -8, 1663, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_back_street_80180D1C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_night_back_street_80180D34[5] = {
    { { .empty = D_dryfield_night_back_street_80180C0C }, D_dryfield_night_back_street_80180C0C, NULL },
    { { .empty = D_dryfield_night_back_street_80180C1C }, D_dryfield_night_back_street_80180C1C, NULL },
    { { .empty = D_dryfield_night_back_street_80180C2C }, D_dryfield_night_back_street_80180C2C, NULL },
    { { .elements = D_dryfield_night_back_street_80180C3C }, D_dryfield_night_back_street_80180CA0, NULL },
    { { .elements = D_dryfield_night_back_street_80180CB8 }, D_dryfield_night_back_street_80180D1C, NULL },
};

WorldCollisionTrigger D_dryfield_night_back_street_80180D70[6] = {
    { NULL, NULL, NULL, { -6272, -3296, 4752, 0 }, { { 0, -4320, -1584, 0 }, { 0, -4320, 1584, 0 }, { 0, 4320, -1584, 0 }, { 0, 4320, 1584, 0 } }, { 4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5888, -3504, 4672, 0 }, { { 0, -4528, 1584, 0 }, { 0, -4528, -1584, 0 }, { 0, 4528, 1584, 0 }, { 0, 4528, -1584, 0 } }, { -4098, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -192, -4160, 4576, 0 }, { { -236, -5184, 1562, 0 }, { 228, -5184, -1572, 0 }, { -236, 5184, 1562, 0 }, { 228, 5184, -1572, 0 } }, { -4054, 0, -601, 0 }, { 0, 0, 4096, 0 }, 5418, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -320, -4128, 4608, 0 }, { { 229, -5152, -1571, 0 }, { -235, -5152, 1563, 0 }, { 229, 5152, -1571, 0 }, { -235, 5152, 1563, 0 } }, { 4053, 0, 600, 0 }, { 0, 0, 4096, 0 }, 5369, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4256, -4096, 4704, 0 }, { { 0, -5120, -1584, 0 }, { 0, -5120, 1584, 0 }, { 0, 5120, -1584, 0 }, { 0, 5120, 1584, 0 } }, { 4097, 0, 0, 0 }, { 0, 0, 4096, 0 }, 5345, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4416, -4352, 4736, 0 }, { { 0, -5376, 1584, 0 }, { 0, -5376, -1584, 0 }, { 0, 5376, 1584, 0 }, { 0, 5376, -1584, 0 } }, { -4126, 0, 0, 0 }, { 0, 0, 4096, 0 }, 5585, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_back_street_80180F38[10] = {
    { NULL, NULL, NULL, { -9648, -55, 4496, 0 }, { { -400, 0, -560, 0 }, { 400, 0, -560, 0 }, { -400, 0, 560, 0 }, { 400, 0, 560, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 686, WORLD_COLLISION_TRIGGER_ACTION_WARP, 3, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7872, -55, 5776, 0 }, { { 832, 0, -352, 0 }, { 832, 0, 352, 0 }, { -832, 0, -352, 0 }, { -832, 0, 352, 0 } }, { 0, 4117, 0, 0 }, { 0, 0, -4096, 0 }, 902, WORLD_COLLISION_TRIGGER_ACTION_WARP, 6, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1616, -48, 5744, 0 }, { { 688, 0, -352, 0 }, { 688, 0, 352, 0 }, { -688, 0, -352, 0 }, { -688, 0, 352, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 770, WORLD_COLLISION_TRIGGER_ACTION_WARP, 7, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9424, -48, 5712, 0 }, { { 784, 0, -384, 0 }, { 784, 0, 384, 0 }, { -784, 0, -384, 0 }, { -784, 0, 384, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_WARP, 9, 65, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2CA0, -64, 5024, 0 }, { { -1072, 0, -416, 0 }, { 1072, 0, -416, 0 }, { -1072, 0, 416, 0 }, { 1072, 0, 416, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6113, -64, 5569, 0 }, { { -752, 0, -432, 0 }, { 752, 0, -432, 0 }, { -752, 0, 432, 0 }, { 752, 0, 432, 0 } }, { 0, 4098, 0, 0 }, { 201, 0, -4092, 0 }, 865, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8288, -64, 2144, 0 }, { { -1696, 0, -208, 0 }, { 1760, 0, -208, 0 }, { -800, 0, 1008, 0 }, { 736, 0, 1008, 0 } }, { 0, 4104, 0, 0 }, { 201, 0, -4092, 0 }, 1768, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4576, -64, 5536, 0 }, { { -752, 0, -432, 0 }, { 752, 0, -432, 0 }, { -752, 0, 432, 0 }, { 752, 0, 432, 0 } }, { 0, 4098, 0, 0 }, { 201, 0, -4092, 0 }, 865, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5152, -64, 5664, 0 }, { { 1056, 0, -384, 0 }, { 1056, 0, 384, 0 }, { -1056, 0, -384, 0 }, { -1056, 0, 384, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2740, -64, 5360, 0 }, { { -304, 0, -720, 0 }, { 656, 0, -720, 0 }, { -656, 0, 720, 0 }, { 304, 0, 720, 0 } }, { 0, 4112, 0, 0 }, { -4091, 0, 200, 0 }, 972, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// The night back street's six point lights for model shading in every view.
///
/// Positions and falloff radii use integer world units: full strength within
/// 1000 units, fading with squared distance to zero at 3000. RGB intensities use
/// 12 fractional bits (`ONE` is full intensity). The loaded room overlay owns
/// these writable records; coordinate updates parent and compose their
/// transforms, and lighting queries overwrite attenuation. Borrowed pointers
/// must not survive unloading the overlay.
static WorldCoordPointLight _gDryfieldNightBackStreetPointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6797, -1800, 5711 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3522, 3522, 3112 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8205, -1800, 5708 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3522, 3522, 3112 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -807, -1800, 5862 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3522, 3522, 3112 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8855, -1800, 5862 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3522, 3522, 3112 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 10160, -1800, 5862 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3522, 3522, 3112 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -9762, -2100, 4476 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3522, 3522, 3112 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 3000,
    },
};

WorldCoordRoomLights D_dryfield_night_back_street_80181470[1] = {
    { 0, NULL, ARRAY_SIZE(_gDryfieldNightBackStreetPointLights), _gDryfieldNightBackStreetPointLights, 0, NULL },
};

AreaResource D_dryfield_night_back_street_80181488[2] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102500_801379A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_back_street_801814A0[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { 15, 15, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &Actor01500_D0A008 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_back_street_801814C4[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { 8, 7, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &D_actor_300700_80165B88 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_back_street_801814E8[2] = {
    { 57, 57, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor05700GolemPawnRookTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_back_street_80181500[2] = {
    { 37, 37, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103700_80139DAC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_night_back_street_80181518[22] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017B048, D_dryfield_night_back_street_80181488 },
    { NULL, NULL },
    { D_map_dryfield_full_8017B0B8, D_dryfield_night_back_street_801814A0 },
    { D_map_dryfield_full_8017B128, D_dryfield_night_back_street_801814C4 },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017B1A8, D_dryfield_night_back_street_801814E8 },
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
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017B1C8, D_dryfield_night_back_street_80181500 },
};

WorldCoordRoomAmbientEntry D_dryfield_night_back_street_801815C8[6] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_night_back_street_801815C8) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 618, 618, 615, 617 } },
    { .color = { 616, 618, 617, 617 } },
    { .color = { 616, 618, 618, 617 } },
    { .color = { 616, 617, 617, 616 } },
};

WorldCollisionFootstepSounds D_dryfield_night_back_street_801815F8 = {
    0x1000001D,
    0x1000001F,
    0x1000001D,
};

WorldCollisionSurfaceProperties D_dryfield_night_back_street_80181604[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_back_street_8018160C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_back_street_801815F8 },
};

WorldCollisionSurfaceProperties D_dryfield_night_back_street_80181614[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_back_street_801815F8 },
};

WorldCollisionSurfaceProperties* D_dryfield_night_back_street_8018161C[8] = {
    D_dryfield_night_back_street_80181604,
    D_dryfield_night_back_street_8018160C,
    D_dryfield_night_back_street_80181614,
    D_dryfield_night_back_street_80181604,
    D_dryfield_night_back_street_80181604,
    D_dryfield_night_back_street_80181604,
    D_dryfield_night_back_street_80181604,
    D_dryfield_night_back_street_80181604,
};

static void func_dryfield_night_back_street_8017D73C(Task* task);
static void func_dryfield_night_back_street_8017D780(Task* task);

#include "../../shared/back_street_event_msg.inc.c"

static void _glowDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);
static void _glowDrawShaft(const SVECTOR worldPoints[2], s32 radiusScale);

s32 func_dryfield_night_back_street_8017D724(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_dryfield_night_back_street_8017D72C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_dryfield_night_back_street_8017D734(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// The room entry task's first state: installs the room's message table, hands
/// the task to pointer slot 7 and moves on to the next state.
static void func_dryfield_night_back_street_8017D73C(Task* task)
{
    task->msgTable = D_dryfield_night_back_street_80180324;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// The room entry task's idle state.
static void func_dryfield_night_back_street_8017D780(Task* task)
{
}

/// The room entry task's three states: set the room up, idle, end.
static const TaskFuncTable3 D_dryfield_night_back_street_8017D5C4 = {
    { func_dryfield_night_back_street_8017D73C, func_dryfield_night_back_street_8017D780, taskKill },
};

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_dryfield_night_back_street_8017D788(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_back_street_8017D5C4;
    sp.funcs[task->state](task);
}

/// The room's light points. Two shafts come first, each a pair of ends at
/// 0x8018034C and 0x8018035C, then glow points in pairs: those of camera view
/// 2 at 0x8018036C, of view 3 at 0x8018037C and of views 4 and 5 at
/// 0x8018038C. The code names only the glow pairs, so the shafts are reached
/// as elements -4 and -2 of the view-2 array.
///
/// Per-frame room task. On its first run it stores the effect ids 0x6000A,
/// 0x60097 and 0x600E4 in three gameplay globals. Each run it sets
/// `roomEffectMode` to 2 and draws the lights of the current camera view
/// (`gGameSession->location.loc.view`): view 2 draws a sprite on each of its two
/// glow points with `_glowDrawFlare` and a shaft
/// between each pair of shaft ends with
/// `_glowDrawShaft`; view 3 adds its own two glows
/// to view 2's set; views 4 and 5 draw their shared two glows; every other
/// view draws nothing.
void func_dryfield_night_back_street_8017D7E0(Task* arg0)
{
    if (arg0->state == 0) {
        gRoomEffectFlashId      = EFFECT_DRYFIELD_NIGHT_BACK_STREET_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_DRYFIELD_NIGHT_BACK_STREET_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_DRYFIELD_NIGHT_BACK_STREET_SPARK_BURST;
    }
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    switch (gGameSession->location.loc.view) {
        case 3:
            _glowDrawFlare(&D_dryfield_night_back_street_8018037C[0], 1, 0x300);
            _glowDrawFlare(&D_dryfield_night_back_street_8018037C[1], 1, 0x300);
            /* fallthrough */
        case 2:
            _glowDrawFlare(&D_dryfield_night_back_street_8018036C[0], 0, 0x300);
            _glowDrawFlare(&D_dryfield_night_back_street_8018036C[1], 0, 0x300);
            _glowDrawShaft(&D_dryfield_night_back_street_8018036C[-4], 0x100);
            _glowDrawShaft(&D_dryfield_night_back_street_8018036C[-2], 0x100);
            break;
        case 4:
        case 5:
            _glowDrawFlare(&D_dryfield_night_back_street_8018038C[0], 1, 0x300);
            _glowDrawFlare(&D_dryfield_night_back_street_8018038C[1], 1, 0x300);
            break;
    }
}

#include "../../shared/glow_draw_shaft.inc.c"

#include "../../shared/glow_draw_flare.inc.c"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_dryfield_night_back_street_8017E390(Task* arg0)
{
    _roomVisualEffectsFlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_dryfield_night_back_street_8017EDF4(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_dryfield_night_back_street_8017F6DC(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
