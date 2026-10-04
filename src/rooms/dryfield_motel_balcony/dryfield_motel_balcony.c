#include "rooms/dryfield_motel_balcony.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
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

#include "mapui/map_dryfield.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room_visual_effects.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room_events.h"
#include "../../shared/room_variants.h"

extern RoomEventActiveBytes gRoomEventActive;

/// Descriptor of the event task the gate spawns.
extern TaskDesc gRoomEventTaskDesc;

/// The room's message table.
extern TaskMessageEntry D_dryfield_motel_balcony_8018227C[];

/// Per-tint channel shifts for the halo task, indexed by the tint the spawn
/// argument selects.

/// The beam's two anchor offsets, relative to the effect's parent. The second
/// is also reached by its own name.

/// The message and request the event gate latched for the event task.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.

extern WorldCollisionGrid         D_dryfield_motel_balcony_80182B5C[1];
extern WorldCollisionOccluder     D_dryfield_motel_balcony_80186130[2];
extern WorldCollisionTrigger      D_dryfield_motel_balcony_80185DA0[8];
extern WorldCollisionTrigger      D_dryfield_motel_balcony_80186000[4];
extern WorldCoordRoomAmbientEntry D_dryfield_motel_balcony_80186600[23];
extern WorldCoordRoomLights       D_dryfield_motel_balcony_801865E8[1];

extern AreaResource D_dryfield_motel_balcony_801861A8[1];

s32 func_dryfield_motel_balcony_8017DB6C(Task*, s32, s32, s32);
s32 func_dryfield_motel_balcony_8017DB74(Task*, s32, s32, s32);
s32 func_dryfield_motel_balcony_8017DB7C(Task*, s32, s32, s32);

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskMessageEntry D_dryfield_motel_balcony_8018227C[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, roomVariantMotelBalconyDoorsMsg },
    { 5105, func_dryfield_motel_balcony_8017DB6C },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_motel_balcony_8017DB7C },
    { ROOM_MESSAGE_COMMAND, func_dryfield_motel_balcony_8017DB74 },
    { ROOM_MESSAGE_SOUND, roomVariantMotelBalconySoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#define ROOM_FX_HALO_STORAGE_INITIALIZER { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 0xF3BC }
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

WorldCollisionRoomResources D_dryfield_motel_balcony_801822D0[1] = {
    { D_dryfield_motel_balcony_80182B5C, D_dryfield_motel_balcony_80185DA0, D_dryfield_motel_balcony_80186000, D_dryfield_motel_balcony_80186130 },
};

u8* D_dryfield_motel_balcony_801822E0[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_motel_balcony_801822E4[1] = { 22 };

WorldCoordRoomLighting D_dryfield_motel_balcony_801822E8[1] = {
    { D_dryfield_motel_balcony_801865E8, D_dryfield_motel_balcony_80186600 },
};

DirectionWarpEntry D_dryfield_motel_balcony_801822F0[4] = {
    { { { .word = 0 }, -0x315E, -3198, 1396 }, { 0, 0, 0, 0 }, { { .word = 0 }, -0x315E, -3198, 1396 }, { 0, 0, 0, 0 }, 0x521D0006, 0x521D0005, 0x521D0009, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -6572, -3200, -2869 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -6572, -3200, -2869 }, { 0, 0, 0, 0 }, 0x521D0002, 0x521D0001, 0x521D0009, 6, DIRECTION_WARP_FLAG_NONE, 464 },
    { { { .word = 1024 }, -6669, -3200, 4477 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -6669, -3200, 4477 }, { 0, 0, 0, 0 }, 0x521D0002, 0x521D0001, 0x521D0009, 5, DIRECTION_WARP_FLAG_NONE, 465 },
    { { { .word = 2048 }, -1990, -3200, 0x29A8 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -1990, -3200, 0x29A8 }, { 0, 0, 0, 0 }, 0x521D0002, 0x521D0001, 0x521D0009, 4, DIRECTION_WARP_FLAG_NONE, 466 },
};

static SVECTOR _gDryfieldMotelBalconyCollision0559CNormals[7] = {
#include "assets/dryfield_motel_balcony_collision_0559C_normals.inc"
};

static SVECTOR _gDryfieldMotelBalconyCollision0559CVerts[69] = {
#include "assets/dryfield_motel_balcony_collision_0559C_verts.inc"
};

static WorldCollisionGridFace _gDryfieldMotelBalconyCollision0559CFaces[32] = {
#include "assets/dryfield_motel_balcony_collision_0559C_faces.inc"
};

static s16 _gDryfieldMotelBalconyCollision0559CCells[342] = {
#include "assets/dryfield_motel_balcony_collision_0559C_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldMotelBalconyCollision0559CCells[i])
static s16* _gDryfieldMotelBalconyCollision0559CTable[64] = {
#include "assets/dryfield_motel_balcony_collision_0559C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_motel_balcony_80182B5C[1] = {
    { NULL, _gDryfieldMotelBalconyCollision0559CNormals, _gDryfieldMotelBalconyCollision0559CVerts, _gDryfieldMotelBalconyCollision0559CFaces, _gDryfieldMotelBalconyCollision0559CTable, 0x32BE, 0x341C, 8, 8, 4000, 32 },
};

ViewCamera D_dryfield_motel_balcony_80182B80[22] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 4112, 0x88B8, -2748 } }, 380 },
    { { { { -559, 0, -4057 }, { -963, 3978, 132 }, { 3941, 972, -543 } }, { 0x4437, 4754, -1733 } }, 230 },
    { { { { 1192, 0, 3918 }, { 526, 4058, -160 }, { -3883, 550, 1181 } }, { 3977, 4660, -1257 } }, 230 },
    { { { { 3968, 0, -1013 }, { -310, 3898, -1217 }, { 964, 1256, 3777 } }, { 5913, 5512, -3603 } }, 230 },
    { { { { 3980, 0, -964 }, { 9, 4095, 40 }, { 964, -42, 3980 } }, { 6137, 4498, 2582 } }, 230 },
    { { { { 3960, 0, -1044 }, { 83, 4082, 317 }, { 1040, -328, 3947 } }, { 6205, 4066, 7032 } }, 230 },
    { { { { 257, 0, -4087 }, { -569, 4056, -35 }, { 4048, 570, 255 } }, { 0x3EDF, 4634, -1649 } }, 230 },
    { { { { 543, 0, -4059 }, { 386, 4077, 51 }, { 4041, -389, 540 } }, { 0x2C23, 4037, -1767 } }, 230 },
    { { { { -1629, 0, 3757 }, { 362, 4076, 157 }, { -3740, 395, -1622 } }, { -2180, 4664, -0x298C } }, 230 },
    { { { { 3791, 0, -1548 }, { 139, 4079, 341 }, { 1542, -369, 3776 } }, { 6606, 4183, 7528 } }, 230 },
    { { { { 3674, 0, -1810 }, { -1285, 2883, -2609 }, { 1274, 2908, 2586 } }, { 5542, 9403, -3532 } }, 230 },
    { { { { 3475, 0, -2168 }, { -803, 3804, -1288 }, { 2013, 1518, 3227 } }, { 4659, 7233, 2080 } }, 230 },
    { { { { 2289, 0, -3396 }, { -1745, 3513, -1176 }, { 2913, 2104, 1963 } }, { 6624, 8520, 6055 } }, 230 },
    { { { { -3895, 0, -1265 }, { 436, 3844, -1344 }, { 1187, -1413, -3656 } }, { 6016, 3919, -0x2A09 } }, 230 },
    { { { { 3347, 0, -2360 }, { 1258, 3465, 1784 }, { 1997, -2183, 2831 } }, { 6606, 3324, 1736 } }, 230 },
    { { { { 3538, 0, -2062 }, { 822, 3756, 1411 }, { 1891, -1633, 3244 } }, { 6146, 3144, 8006 } }, 230 },
    { { { { -4087, 0, 270 }, { -89, 3867, -1344 }, { -255, -1347, -3859 } }, { 5661, 3313, -134 } }, 289 },
    { { { { -3824, 0, 1466 }, { -419, 3924, -1094 }, { -1405, -1172, -3664 } }, { 1367, 798, 24 } }, 230 },
    { { { { 3637, 0, -1883 }, { 328, 4033, 634 }, { 1854, -714, 3581 } }, { 6173, 491, 4712 } }, 230 },
    { { { { 3836, 0, -1434 }, { -416, 3919, -1114 }, { 1372, 1189, 3671 } }, { 6259, 5350, 2341 } }, 230 },
    { { { { 3850, 0, -1396 }, { -587, 3715, -1621 }, { 1266, 1725, 3492 } }, { 6333, 5485, 6887 } }, 230 },
    { { { { -3931, 0, -1147 }, { 191, 4038, -654 }, { 1131, -682, -3876 } }, { 6478, 3912, -2373 } }, 230 },
};

SpriteBatch D_dryfield_motel_balcony_80182E98[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_balcony_80182EA8[155] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -120, 2261, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -120, 2394, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -112, 1332, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -112, 1338, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -112, 1344, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -112, 1351, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -112, 1357, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -112, 1364, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -112, 1370, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -112, 1377, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -112, 1383, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 88, -104, 1353, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -104, 1360, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -104, 1366, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -104, 1373, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -104, 1379, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -104, 1386, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -104, 1393, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -104, 1398, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -104, 1405, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -96, 1364, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -96, 1371, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -96, 1377, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -96, 1384, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -96, 1391, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -96, 1397, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -96, 1404, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -96, 1411, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -96, 1418, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -88, 1376, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -88, 1382, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -88, 1389, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -88, 1396, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -88, 1402, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -88, 1409, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -88, 1416, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 144, -88, 1423, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, -88, 1430, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 88, -80, 1387, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 96, -80, 1394, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 104, -80, 1400, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 112, -80, 1407, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 120, -80, 1410, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 128, -80, 1416, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, -80, 1428, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 144, -80, 1435, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, -80, 1443, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 88, -72, 1399, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 96, -72, 1405, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 104, -72, 1412, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 112, -72, 1419, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 120, -72, 1432, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 128, -72, 1443, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, -72, 1450, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 144, -72, 1456, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, -72, 1455, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 88, -64, 1410, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 96, -64, 1417, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 104, -64, 1424, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 112, -64, 1431, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 120, -64, 1446, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 128, -64, 1456, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, -64, 1463, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 144, -64, 1468, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, -64, 1468, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 88, -56, 1422, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 96, -56, 1429, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 104, -56, 1437, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 112, -56, 1444, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 120, -56, 1461, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 128, -56, 1468, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, -56, 1476, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -56, 1478, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -56, 1481, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -48, 1435, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -48, 1442, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -48, 1449, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -48, 1456, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -48, 1483, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -48, 1491, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -48, 1499, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -48, 1487, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -48, 1494, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -40, 1447, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -40, 1454, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -40, 1462, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -40, 1469, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -40, 1496, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -40, 1504, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -40, 1512, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -40, 1495, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -40, 1508, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -32, 1460, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -32, 1467, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -32, 1475, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -32, 1482, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -32, 1451, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -32, 1427, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -32, 1456, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -32, 1513, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -32, 1521, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -24, 1472, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -24, 1480, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -24, 1488, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -24, 1495, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -24, 1502, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -24, 1418, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -24, 1511, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -24, 1519, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -24, 1535, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -16, 1486, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -16, 1493, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -16, 1501, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -16, 1509, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -16, 1516, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -16, 1524, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -16, 1533, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -16, 1541, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -16, 1550, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -8, 1507, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -8, 1515, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -8, 1522, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -8, 1527, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -8, 1532, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -8, 1537, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -8, 1543, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -8, 1552, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 0, 1513, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 0, 1518, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 0, 1524, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 0, 1533, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 0, 1541, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 0, 1549, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 0, 1558, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 0, 1566, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 8, 1533, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 8, 1532, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 8, 1547, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 8, 1555, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 8, 1555, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 8, 1571, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 8, 1578, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 8, 1579, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -64, -88, 2408, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -72, -96, 2077, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -80, -104, 1750, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -88, -112, 1500, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 112 } }, -160, -120, 1250, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, -88, 2400, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, -40, 2400, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -16, -120, 2087, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -8, -120, 1510, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 112 } }, 0, -120, 1400, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 72 } }, -104, -32, 1000, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 0, -32, 1000, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_balcony_80183AC4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 153, 0, 0, { 1, 0 } },
    { 153, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_balcony_80183AE4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_balcony_80183AF4[12] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 0, 1479, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, 0, 1295, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, 0, 1481, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 8, -8, 1508, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, -8, 1537, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, -8, 1567, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, -8, 1600, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, -8, 1633, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, -8, 1669, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, -16, 1682, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, -16, 1719, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 144, -8, 1755, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_balcony_80183BE4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_balcony_80183BFC[30] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 112, 478, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 16, 2861, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 16, 2146, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, 24, 1679, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, 32, 1381, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, 32, 1203, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, 40, 1034, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, 48, 909, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, 48, 857, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 24, 56, 746, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 32, 64, 686, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, 72, 635, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, 72, 618, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, 80, 591, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 88, 558, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 96, 528, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 96, 511, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 104, 494, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 88 } }, -160, -120, 675, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -128, -120, 750, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, -160, -32, 675, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -128, -32, 750, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -160, 48, 675, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -128, 48, 750, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, -120, 495, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, -112, 451, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -128, -96, 526, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 16, 2862, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -24, 16, 2861, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 16, 16, 2861, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_balcony_80183E54[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 3, 0 } },
    { 18, 6, 0, 0, { 0, 0 } },
    { 24, 3, 0, 0, { 2, 0 } },
    { 27, 3, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_balcony_80183E84[30] = {
    { 143, 0x3FC0, { .fields = { 56, 40 } }, -160, 80, 351, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 48 } }, -104, 72, 369, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 48 } }, -40, 72, 394, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 56 } }, 24, 64, 427, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 80, 451, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 24, 3466, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, 24, 2408, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 24, 1884, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -24, 24, 1517, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, 32, 1267, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -8, 32, 1089, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 0, 40, 957, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, 40, 847, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, 40, 821, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 48, 752, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 32, 48, 683, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, 48, 637, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 56, 592, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 56, 574, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 56, 519, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -48, 24, 3724, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -16, 24, 3720, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 8, 24, 3720, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 176 } }, -160, -120, 951, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 176 } }, -120, -120, 1344, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 168 } }, -112, -112, 1416, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 152 } }, -104, -96, 1714, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -96, -72, 1739, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -88, -48, 1794, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -32, 1632, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_balcony_801840DC[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { 5, 15, 0, 0, { 3, 0 } },
    { 20, 0, 0, 0, { 2, 0 } },
    { 20, 3, 0, 0, { 4, 0 } },
    { 23, 7, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_balcony_80184114[19] = {
    { 141, 0x3FC0, { .fields = { 16, 80 } }, -88, 0, 750, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 72 } }, -72, 0, 750, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 80 } }, -32, 0, 750, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 72 } }, -24, 0, 750, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 72 } }, 24, 0, 750, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 88 } }, 16, 0, 750, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 208 } }, -160, -112, 1750, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 200 } }, -56, -104, 1750, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 112, 216 } }, 48, -120, 1750, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 56 } }, 24, -112, 1750, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 136 } }, 24, -40, 1750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, -56, 1750, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 176 } }, -32, -80, 2075, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -24, -16, 2750, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 8, -16, 2750, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -24, 32, 2500, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 8, 32, 2500, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -24, 56, 2250, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 8, 56, 2250, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_balcony_80184290[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 1, 0 } },
    { 6, 7, 0, 0, { 2, 0 } },
    { 13, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_balcony_801842B8[24] = {
    { 142, 0x3FC0, { .fields = { 112, 240 } }, -160, -120, 1000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -48, -104, 1000, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, -80, 1000, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -48, 24, 1000, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -48, 80, 1000, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 64, -96, 1000, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 240 } }, 72, -120, 1000, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -48, -64, 1000, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -48, -24, 1000, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -40, 32, 1500, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 0, 32, 1500, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 40, 32, 1500, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -40, 80, 1250, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 0, 80, 1250, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 40, 80, 1250, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -40, 96, 1250, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 0, 96, 1250, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, 96, 1250, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 16, 96, 1250, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 24, 96, 1250, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -24, 96, 1250, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -16, 96, 1250, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 56, 96, 1250, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 64, 96, 1250, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_balcony_80184498[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_balcony_801844B8[62] = {
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -160, 72, 300, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -112, 72, 300, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 64, 300, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 64, 300, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 56, 300, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 48, 300, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, 40, 300, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 32, 300, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 24, 300, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 16, 300, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 8, 300, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 0, 300, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, -16, 300, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, -24, 300, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, -32, 300, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -48, 300, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -56, 300, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, -64, 300, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, -80, 300, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, -88, 300, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -104, 300, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, -112, 300, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, 72, 389, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, 80, 426, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 80, 471, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, 88, 507, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -64, 64, 682, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -24, 64, 682, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 8, 72, 646, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 48, 80, 613, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 96, 88, 474, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 96, 371, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 64, 797, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 56, 914, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 56, 1022, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 48, 1243, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -24, 40, 1385, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, 40, 1385, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -8, 32, 1520, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 0, 32, 1513, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, 8, 1750, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 8, 1875, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, 24, 1519, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, 16, 1536, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 16, 1575, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, 8, 1625, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 8, 1875, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 80, 875, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 16, 1750, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 8, 40, 1502, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 0, 48, 1493, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 56, 1243, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 56, 1243, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -16, 64, 1031, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 64, 1034, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -32, 72, 881, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 16, 72, 881, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 32, 1518, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 24, 1625, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, 80, 616, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -72, 88, 584, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 104, 532, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_balcony_80184990[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 3, 0 } },
    { 22, 4, 0, 0, { 0, 0 } },
    { 26, 6, 0, 0, { 5, 0 } },
    { 32, 14, 0, 0, { 1, 0 } },
    { 46, 13, 0, 0, { 4, 0 } },
    { 59, 3, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_balcony_801849D0[34] = {
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -160, 80, 446, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -112, 80, 469, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -72, 72, 502, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -24, 72, 540, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 16, 64, 585, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 64, 72, 620, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 32, 4027, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 24, 3360, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 24, 2581, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, 32, 2045, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 32, 1763, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, 32, 1522, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -32, 40, 1299, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 56, 775, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 64, 624, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 64, 633, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 64, 639, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -16, 40, 1189, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -24, 40, 1197, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, 48, 985, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, 48, 906, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, 48, 838, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, 56, 823, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 56, 776, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 32, 3273, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 40, 2358, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -80, 48, 1771, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 56, 1418, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -48, 56, 1376, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -80, 64, 1182, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -40, 64, 1156, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -72, 72, 1013, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -32, 72, 1025, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 80, 1000, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_balcony_80184C78[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 1, 0 } },
    { 6, 18, 0, 0, { 2, 0 } },
    { 24, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_balcony_80184CA0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_balcony_80184CB0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_balcony_80184CC0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_balcony_80184CD0[20] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 112, 478, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 112, 506, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 112, 522, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 112, 553, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 104, 602, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 104, 657, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 104, 724, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, 96, 811, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, 96, 897, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 96, 963, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 96, 1129, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, 88, 1380, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, 88, 1687, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 88, 2347, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 64, 88, 3478, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 48, 96, 2719, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 32, 112, 1369, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 80, 112, 1345, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 40, 104, 1821, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 104, 1821, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_balcony_80184E60[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 1, 0 } },
    { 15, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_balcony_80184E80[6] = {
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 112, 742, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -16, 104, 587, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 24, 96, 477, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 72, 88, 381, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 112, 80, 333, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, 72, 311, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_balcony_80184EF8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_balcony_80184F10[5] = {
    { 143, 0x3FC0, { .fields = { 48, 80 } }, -160, 40, 529, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, -112, 48, 571, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, -72, 48, 620, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -32, 56, 667, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 96, 644, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_balcony_80184F74[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_balcony_80184F8C[38] = {
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -144, -120, 1000, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, -120, 1000, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -48, -112, 1000, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, -112, 1000, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 48, -104, 1000, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 136, -120, 1000, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, -72, 1000, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, -120, 1000, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -144, -96, 1000, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, -96, 1000, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -64, -88, 1000, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -88, 1000, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, 32, -80, 1000, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -160, -24, 750, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -112, -24, 750, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -64, -24, 750, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -16, -24, 750, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 32, -24, 750, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 80, -24, 848, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 120, -24, 633, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, -16, 975, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -160, 16, 280, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -144, 24, 319, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -128, 32, 370, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -112, 40, 441, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -96, 56, 546, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 56 } }, -80, 64, 784, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 40 } }, -56, 80, 1000, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 40, -40, 975, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, -72, 975, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 64, -104, 950, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 80, -120, 811, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 120, -120, 607, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, 64, -40, 915, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 72 } }, 112, -40, 648, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 88 } }, 48, 32, 973, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 88 } }, 112, 32, 640, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 56 } }, 48, 56, 925, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_balcony_80185284[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 1, 0 } },
    { 20, 17, 0, 0, { 2, 0 } },
    { 37, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_balcony_801852AC[111] = {
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -104, 104, 1153, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -128, -120, 1338, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -144, -104, 1292, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, -136, -80, 1276, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -144, -64, 1219, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -152, -32, 1153, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, 16, 1087, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, 64, 1038, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -96, 56, 1236, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -96, 32, 1297, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -88, -24, 1363, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -88, -40, 1430, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -80, -96, 1425, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, -120, 1425, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -88, -120, 1425, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -48, -104, 1425, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -32, -96, 1425, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, 0, -88, 1425, { .fields = { 40, 8 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, 40, -80, 1425, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, 72, -72, 1425, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, 104, -64, 1425, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 144, -56, 1425, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 32, -120, 1425, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, 56, -112, 1425, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 72, -104, 1425, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 96, -96, 1425, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 120, -88, 1425, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 144, -80, 1425, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 80, -120, 1425, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, 104, -112, 1425, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 120, -104, 1425, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 144, -96, 957, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 112, -120, 1425, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 40 } }, 88, -104, 1090, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 56 } }, 72, -64, 1361, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -48, 1425, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, -40, 1425, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 48, -16, 1425, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, -8, 1425, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 88 } }, 48, 16, 1429, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, 96, -8, 1425, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, 104, 16, 1425, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 48 } }, 80, 56, 1418, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 16 } }, 64, 104, 1200, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -120, 16, 1391, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -104, 24, 1428, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -120, 40, 1348, { .fields = { 80, 8 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -64, 40, 1425, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 40, 1425, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -112, 48, 1367, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -160, 64, 1050, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -112, 64, 1335, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -16, 88, 1425, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -144, 8, 1425, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -152, 40, 1305, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, 72, 1425, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -136, 80, 1425, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 104, 1425, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -120, 88, 1425, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -88, 24, 1425, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -96, 48, 1425, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -96, 64, 1410, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -56, 16, 1452, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -80, 80, 1385, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -72, 96, 1188, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -40, 24, 1425, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -40, 48, 1425, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -40, 64, 1425, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -40, 80, 1425, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 16, 24, 1425, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 40, 32, 1425, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 48, 1425, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 40, 64, 1425, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, 40, 80, 1425, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 64, 24, 1425, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 72, 56, 1425, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 80, 80, 1425, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 80, 32, 1425, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 80, 48, 1425, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 88, 64, 1425, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, 96, 80, 1425, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 112, 32, 1425, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 56, 1425, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, 32, 1425, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 136, 80, 1260, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 64, 16 } }, -160, 104, 1088, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 88, 24 } }, -96, 96, 1149, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 80, 24 } }, -8, 96, 1128, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 40, 16 } }, 72, 104, 1043, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 32, 8 } }, 112, 112, 797, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 16 } }, -136, 80, 2775, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 24 } }, -80, 72, 2775, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, -16, 64, 2775, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 24 } }, 0, 72, 2775, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 24, 16 } }, 32, 80, 2775, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -72, 104, 1318, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 16 } }, -136, 96, 1172, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -104, 80, 1247, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 24 } }, -88, 80, 1425, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 40 } }, -48, 80, 1425, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 32 } }, 16, 72, 1425, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 24 } }, 56, 72, 1313, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 8 } }, 104, 80, 1425, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 72, 32 } }, -160, 80, 2775, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 88, 24 } }, -88, 80, 2775, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 72, 24 } }, 0, 80, 2775, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 88, 24 } }, 72, 72, 2775, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 16 } }, -128, 88, 1210, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 32 } }, -96, 80, 1163, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 32 } }, -72, 72, 1425, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 24 } }, 32, 80, 1425, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_balcony_80185B58[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 32, 0, 0, { 6, 0 } },
    { 32, 12, 0, 0, { 1, 0 } },
    { 44, 41, 0, 0, { 4, 0 } },
    { 85, 5, 0, 0, { 0, 0 } },
    { 90, 5, 0, 0, { 5, 0 } },
    { 95, 8, 0, 0, { 3, 0 } },
    { 103, 4, 0, 0, { 7, 0 } },
    { 107, 4, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_balcony_80185BA8[8] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -40, 56, 525, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -64, 64, 550, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 8, 64, 500, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 136, 64, 450, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 48, 72, 475, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 96, 72, 450, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -72, -96, 1110, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -72, -32, 1088, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_balcony_80185C48[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 1, 0 } },
    { 6, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_balcony_80185C68[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_balcony_80185C78[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_balcony_80185C88[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_motel_balcony_80185C98[22] = {
    { { .empty = D_dryfield_motel_balcony_80182E98 }, D_dryfield_motel_balcony_80182E98, NULL },
    { { .elements = D_dryfield_motel_balcony_80182EA8 }, D_dryfield_motel_balcony_80183AC4, NULL },
    { { .empty = D_dryfield_motel_balcony_80183AE4 }, D_dryfield_motel_balcony_80183AE4, NULL },
    { { .elements = D_dryfield_motel_balcony_80183AF4 }, D_dryfield_motel_balcony_80183BE4, NULL },
    { { .elements = D_dryfield_motel_balcony_80183BFC }, D_dryfield_motel_balcony_80183E54, NULL },
    { { .elements = D_dryfield_motel_balcony_80183E84 }, D_dryfield_motel_balcony_801840DC, NULL },
    { { .elements = D_dryfield_motel_balcony_80184114 }, D_dryfield_motel_balcony_80184290, NULL },
    { { .elements = D_dryfield_motel_balcony_801842B8 }, D_dryfield_motel_balcony_80184498, NULL },
    { { .elements = D_dryfield_motel_balcony_801844B8 }, D_dryfield_motel_balcony_80184990, NULL },
    { { .elements = D_dryfield_motel_balcony_801849D0 }, D_dryfield_motel_balcony_80184C78, NULL },
    { { .empty = D_dryfield_motel_balcony_80184CA0 }, D_dryfield_motel_balcony_80184CA0, NULL },
    { { .empty = D_dryfield_motel_balcony_80184CB0 }, D_dryfield_motel_balcony_80184CB0, NULL },
    { { .empty = D_dryfield_motel_balcony_80184CC0 }, D_dryfield_motel_balcony_80184CC0, NULL },
    { { .elements = D_dryfield_motel_balcony_80184CD0 }, D_dryfield_motel_balcony_80184E60, NULL },
    { { .elements = D_dryfield_motel_balcony_80184E80 }, D_dryfield_motel_balcony_80184EF8, NULL },
    { { .elements = D_dryfield_motel_balcony_80184F10 }, D_dryfield_motel_balcony_80184F74, NULL },
    { { .elements = D_dryfield_motel_balcony_80184F8C }, D_dryfield_motel_balcony_80185284, NULL },
    { { .elements = D_dryfield_motel_balcony_801852AC }, D_dryfield_motel_balcony_80185B58, NULL },
    { { .elements = D_dryfield_motel_balcony_80185BA8 }, D_dryfield_motel_balcony_80185C48, NULL },
    { { .empty = D_dryfield_motel_balcony_80185C68 }, D_dryfield_motel_balcony_80185C68, NULL },
    { { .empty = D_dryfield_motel_balcony_80185C78 }, D_dryfield_motel_balcony_80185C78, NULL },
    { { .empty = D_dryfield_motel_balcony_80185C88 }, D_dryfield_motel_balcony_80185C88, NULL },
};

WorldCollisionTrigger D_dryfield_motel_balcony_80185DA0[8] = {
    { NULL, NULL, NULL, { -6032, -4416, 320, 0 }, { { -1360, 3008, 0, 0 }, { 1360, 3008, 0, 0 }, { -1360, -3008, 0, 0 }, { 1360, -3008, 0, 0 } }, { 0, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 3298, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6176, -3968, 480, 0 }, { { 1360, 2272, 0, 0 }, { -1360, 2272, 0, 0 }, { 1360, -2272, 0, 0 }, { -1360, -2272, 0, 0 } }, { 0, 0, -4110, 0 }, { 0, 0, 4096, 0 }, 2635, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7168, -4000, 1952, 0 }, { { 66, 2304, 1358, 0 }, { -67, 2304, -1359, 0 }, { 66, -2304, 1358, 0 }, { -67, -2304, -1359, 0 } }, { 4102, 0, -202, 0 }, { 0, 0, 4096, 0 }, 2672, 0, 5, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7073, -4096, 1855, 0 }, { { -66, 2528, -1358, 0 }, { 67, 2528, 1359, 0 }, { -66, -2528, -1358, 0 }, { 67, -2528, 1359, 0 } }, { -4091, 0, 200, 0 }, { 0, 0, 4096, 0 }, 2862, 0, 3, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6181, -4400, 6107, 0 }, { { -1319, 2576, 330, 0 }, { 1320, 2576, -329, 0 }, { -1319, -2576, 330, 0 }, { 1320, -2576, -329, 0 } }, { 992, 0, 3980, 0 }, { 0, 0, 4096, 0 }, 2907, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6176, -4656, 6238, 0 }, { { 1316, 2704, -332, 0 }, { -1323, 2704, 327, 0 }, { 1316, -2704, -332, 0 }, { -1323, -2704, 327, 0 } }, { -996, 0, -3982, 0 }, { 0, 0, 4096, 0 }, 3018, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -0x2EB7, -4032, 2043, 0 }, { { 1, 2480, 1361, 0 }, { 0, 2480, -1361, 0 }, { 1, -2480, 1361, 0 }, { 0, -2480, -1361, 0 } }, { 4103, 0, -3, 0 }, { 0, 0, 4096, 0 }, 2827, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -0x2E15, -3936, 2013, 0 }, { { 0, 2480, -1361, 0 }, { 1, 2480, 1361, 0 }, { 0, -2480, -1361, 0 }, { 1, -2480, 1361, 0 } }, { -4106, 0, 1, 0 }, { 0, 0, 4096, 0 }, 2827, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_motel_balcony_80186000[4] = {
    { NULL, NULL, NULL, { -0x3100, -3264, 1248, 0 }, { { -608, 0, -224, 0 }, { 608, 0, -224, 0 }, { -608, 0, 224, 0 }, { 608, 0, 224, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, 4076, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 21, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6816, -3264, -2816, 0 }, { { 224, 0, -608, 0 }, { 224, 0, 608, 0 }, { -224, 0, -608, 0 }, { -224, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { 4076, 0, -401, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_WARP, 28, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6752, -3280, 4416, 0 }, { { 224, 0, -608, 0 }, { 224, 0, 608, 0 }, { -224, 0, -608, 0 }, { -224, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { 4076, 0, -401, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_WARP, 30, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1984, -3248, 0x2970, 0 }, { { 928, 0, 368, 0 }, { -928, 0, 368, 0 }, { 928, 0, -368, 0 }, { -928, 0, -368, 0 } }, { 0, 4101, 0, 0 }, { -51, 0, -4096, 0 }, 997, WORLD_COLLISION_TRIGGER_ACTION_WARP, 31, 65, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_dryfield_motel_balcony_80186130[2] = {
    { NULL, NULL, { -0x2840, -4064, 7440, 0 }, { { -3104, -2624, 4336, 0 }, { 3104, -2624, -4336, 0 }, { -3104, 2624, 4336, 0 }, { 3104, 2624, -4336, 0 } }, { -3332, 0, -2385, 0 }, 5926, 1, 0 },
    { NULL, NULL, { -0x2B61, -4016, -2561, 0 }, { { 4011, -2640, 3514, 0 }, { -4010, -2640, -3513, 0 }, { 4011, 2640, 3514, 0 }, { -4010, 2640, -3513, 0 } }, { -2701, 0, 3082, 0 }, 5948, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

AreaResource D_dryfield_motel_balcony_801861A8[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_motel_balcony_801861B4[2] = {
    { 18, 18, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401800_80155AC4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_motel_balcony_801861CC[2] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_801379A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_motel_balcony_801861E4[2] = {
    { 3, 3, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_80148110 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_motel_balcony_801861FC[3] = {
    { 56, 56, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_105600_801482C0 },
    { 57, 57, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205700_801611F8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_motel_balcony_80186220[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017BA34, D_dryfield_motel_balcony_801861A8 },
    { D_map_dryfield_8017BA44, D_dryfield_motel_balcony_801861B4 },
    { D_map_dryfield_8017BA84, D_dryfield_motel_balcony_801861CC },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_8017BAF4, D_dryfield_motel_balcony_801861E4 },
    { D_map_dryfield_8017BB24, D_dryfield_motel_balcony_801861FC },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCoordPointLight D_dryfield_motel_balcony_80186288[9] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3546, -5261, 222 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4003, 3928, 3859 }, { 0, 0 } }, 4002, 7298 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x3377, -5250, 2697 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4918, 4096, 4097 }, { 0, 0 } }, 500, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3303, -5000, 3148 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4122, 3865, 4223 }, { 0, 0 } }, 2940, 5583 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2000, -5000, -6710 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 5740, 5668, 5680 }, { 0, 0 } }, 5399, 0x2EE0 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3200, -5250, 8480 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4944, 4100, 4099 }, { 0, 0 } }, 3323, 5204 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9995, -5250, 2697 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4940, 4096, 4096 }, { 0, 0 } }, 500, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4050, -5250, 5400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4902, 4064, 4085 }, { 0, 0 } }, 3565, 5064 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2EC7, -5250, 2697 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4805, 4204, 4145 }, { 0, 0 } }, 500, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7035, -5250, 2697 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 5401, 4327, 4285 }, { 0, 0 } }, 1121, 3000 },
};

WorldCoordRoomLights D_dryfield_motel_balcony_801865E8[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_motel_balcony_80186288), D_dryfield_motel_balcony_80186288, 0, NULL },
};

WorldCoordRoomAmbientEntry D_dryfield_motel_balcony_80186600[23] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_motel_balcony_80186600) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 823, 820, 838, 823 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 1481, 1461, 1422, 1463 } },
    { .color = { 676, 659, 633, 662 } },
    { .color = { 558, 480, 533, 515 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_dryfield_motel_balcony_801866B8 = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

WorldCollisionFootstepSounds D_dryfield_motel_balcony_801866C4 = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

WorldCollisionFootstepSounds D_dryfield_motel_balcony_801866D0 = {
    0x10000051,
    0x10000053,
    0x10000055,
};

WorldCollisionSurfaceProperties D_dryfield_motel_balcony_801866DC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_motel_balcony_801866B8 },
};

WorldCollisionSurfaceProperties D_dryfield_motel_balcony_801866E4[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_motel_balcony_801866B8 },
};

WorldCollisionSurfaceProperties D_dryfield_motel_balcony_801866EC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_motel_balcony_801866C4 },
};

WorldCollisionSurfaceProperties D_dryfield_motel_balcony_801866F4[2] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_motel_balcony_801866D0 },
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_motel_balcony_801866B8 },
};

WorldCollisionSurfaceProperties* D_dryfield_motel_balcony_80186704[8] = {
    D_dryfield_motel_balcony_801866DC,
    D_dryfield_motel_balcony_801866E4,
    D_dryfield_motel_balcony_801866EC,
    D_dryfield_motel_balcony_801866F4,
    D_dryfield_motel_balcony_801866DC,
    D_dryfield_motel_balcony_801866DC,
    D_dryfield_motel_balcony_801866DC,
    D_dryfield_motel_balcony_801866DC,
};

RoomEventMsg gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 238, 254, 37 } };

RoomEventReq gRoomEventReq;

static void func_dryfield_motel_balcony_8017DB84(Task* task);
static void func_dryfield_motel_balcony_8017DBC8(Task* arg0);

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

#include "../../shared/room_variants_motel_balcony_doors.inc.c"

#include "../../shared/room_variants_motel_balcony_sound.inc.c"

s32 func_dryfield_motel_balcony_8017DB6C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_dryfield_motel_balcony_8017DB74(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_dryfield_motel_balcony_8017DB7C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Room task state 0: installs the room's message table, registers the task
/// in pointer slot 7 and advances to the next state.
static void func_dryfield_motel_balcony_8017DB84(Task* task)
{
    task->msgTable = D_dryfield_motel_balcony_8018227C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// Room task state 1: idles.
static void func_dryfield_motel_balcony_8017DBC8(Task* arg0)
{
}

/// The room task's three states: setup, idle and exit.
static const TaskFuncTable3 D_dryfield_motel_balcony_8017D5DC = {
    func_dryfield_motel_balcony_8017DB84,
    func_dryfield_motel_balcony_8017DBC8,
    taskKill,
};

/// Runs the room task's current state from its state table, dispatching
/// through a copy of the table taken onto the stack.
void func_dryfield_motel_balcony_8017DBD0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_balcony_8017D5DC;
    sp.funcs[task->state](task);
}

/// On its first tick, stores the room's seven effect ids into the gameplay
/// slots `gRoomEffectSparkEmitterId`..`gRoomEffectFlashId`, then idles.
void func_dryfield_motel_balcony_8017DC28(Task* arg0)
{
    if (arg0->state == 0) {
        gRoomEffectMoteId         = EFFECT_DRYFIELD_MOTEL_BALCONY_MOTE;
        gRoomEffectHaloId         = EFFECT_DRYFIELD_MOTEL_BALCONY_HALO;
        gRoomEffectOrangeBurstId  = EFFECT_DRYFIELD_MOTEL_BALCONY_ORANGE_BURST;
        gRoomEffectSparkEmitterId = EFFECT_DRYFIELD_MOTEL_BALCONY_SPARK_EMITTER;
        gRoomEffectFlashId        = EFFECT_DRYFIELD_MOTEL_BALCONY_FLASH;
        gRoomEffectTwinTrailId    = EFFECT_DRYFIELD_MOTEL_BALCONY_TWIN_TRAIL;
        gRoomEffectSparkBurstId   = EFFECT_DRYFIELD_MOTEL_BALCONY_SPARK_BURST;
        arg0->state               = 1;
    }
}

#include "../../shared/room_visual_effects.inc.c"

void func_dryfield_motel_balcony_8017DCB8(Task* task)
{
    RoomFx_MoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void func_dryfield_motel_balcony_8017EA00(Task* arg0)
{
    RoomFx_HaloTask(arg0);
}

void func_dryfield_motel_balcony_8017ED98(Task* arg0)
{
    RoomFx_OrangeBurstTask(arg0);
}

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void func_dryfield_motel_balcony_801801A8(Task* arg0)
{
    RoomFx_SparkEmitterTask(arg0);
}

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_dryfield_motel_balcony_801802DC(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_dryfield_motel_balcony_80180D40(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_dryfield_motel_balcony_80181628(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
