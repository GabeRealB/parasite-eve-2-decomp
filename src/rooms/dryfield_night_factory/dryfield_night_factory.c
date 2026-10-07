#include "dryfield_night_factory_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "common.h"

#include "rooms/dryfield_night_factory.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/sound.h"
#include "main/sound.h"
#include "gameplay/collision.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/dryfield_factory.h"

#include "rooms/room_common.h"
#include "../../shared/room_events.h"
/// Selects the nighttime factory instance for shared room declarations and code.
///
/// Keep this binding through all factory implementation fragments.
#define DRYFIELD_TIME DRYFIELD_NIGHT
#include "../../shared/factory_lift.h"

/// State handlers of the factory model task: set-up, the per-frame state and
/// `taskKill`.
static const TaskFuncTable3 _gFactoryLiftStates = {
    { factoryLiftInit, factoryLiftUpdate, taskKill },
};

/// State handlers of the cutscene task: set-up, the cutscene sequence and
/// `taskKill`.
static const TaskFuncTable3 _gFactoryHatchTaskStates = {
    { factoryHatchInit, factoryHatchUpdate, taskKill },
};

/// The cutscene sequence's handler table: the flag watcher of state 0 and the
/// two movements it arms.
static const FactoryHatchStateFuncTable _gFactoryHatchStates = {
    { factoryHatchWatch, factoryHatchOpen, factoryHatchClose },
};

SpriteBatch D_dryfield_night_factory_80189A14[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_night_factory_80189A24[19] = {
    { { .empty = D_dryfield_night_factory_80187EC0 }, D_dryfield_night_factory_80187EC0, NULL },
    { { .elements = D_dryfield_night_factory_80187ED0 }, D_dryfield_night_factory_80187FD4, NULL },
    { { .elements = D_dryfield_night_factory_80188004 }, D_dryfield_night_factory_801880F4, NULL },
    { { .elements = D_dryfield_night_factory_8018811C }, D_dryfield_night_factory_80188784, NULL },
    { { .empty = D_dryfield_night_factory_801887BC }, D_dryfield_night_factory_801887BC, NULL },
    { { .elements = D_dryfield_night_factory_801887CC }, D_dryfield_night_factory_80188A4C, NULL },
    { { .elements = D_dryfield_night_factory_80188A84 }, D_dryfield_night_factory_80188D90, NULL },
    { { .elements = D_dryfield_night_factory_80188DD0 }, D_dryfield_night_factory_80188EC0, NULL },
    { { .elements = D_dryfield_night_factory_80188ED8 }, D_dryfield_night_factory_80188F28, NULL },
    { { .empty = D_dryfield_night_factory_80188F40 }, D_dryfield_night_factory_80188F40, NULL },
    { { .empty = D_dryfield_night_factory_80188F50 }, D_dryfield_night_factory_80188F50, NULL },
    { { .empty = D_dryfield_night_factory_80188F60 }, D_dryfield_night_factory_80188F60, NULL },
    { { .empty = D_dryfield_night_factory_80188F70 }, D_dryfield_night_factory_80188F70, NULL },
    { { .empty = D_dryfield_night_factory_80188F80 }, D_dryfield_night_factory_80188F80, NULL },
    { { .elements = D_dryfield_night_factory_80188F90 }, D_dryfield_night_factory_801895A8, NULL },
    { { .elements = D_dryfield_night_factory_801895D8 }, D_dryfield_night_factory_80189858, NULL },
    { { .empty = D_dryfield_night_factory_80189880 }, D_dryfield_night_factory_80189880, NULL },
    { { .elements = D_dryfield_night_factory_80189890 }, D_dryfield_night_factory_801899E4, NULL },
    { { .elements = D_dryfield_night_factory_80189890 }, D_dryfield_night_factory_801899E4, NULL },
};

WorldCoordPointLight D_dryfield_night_factory_80189B08[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3051, -1864, 8762 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1419, 4022 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5657, -1403, 9444 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1759, 1784, 1784 }, { 0, 0 } }, 1000, 1319 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3051, -1864, 2398 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1702, 3699 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3051, -1864, 5767 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1522, 4180 },
};

WorldCoordRoomLights D_dryfield_night_factory_80189C88[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_night_factory_80189B08), D_dryfield_night_factory_80189B08, 0, NULL },
};

WorldCollisionTrigger D_dryfield_night_factory_80189CA0[14] = {
    { NULL, NULL, NULL, { 2925, -2368, 1935, 0 }, { { -144, -2976, -2288, 0 }, { -144, 2976, -2288, 0 }, { 144, -2976, 2288, 0 }, { 144, 2976, 2288, 0 } }, { -4092, 0, 257, 0 }, { 0, 0, 4096, 0 }, 3753, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2704, -2368, 1920, 0 }, { { 144, -2976, 2400, 0 }, { 144, 2976, 2400, 0 }, { -144, -2976, -2400, 0 }, { -144, 2976, -2400, 0 } }, { 4090, 0, -246, 0 }, { 0, 0, 4096, 0 }, 3822, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1312, -2304, 5840, 0 }, { { 1616, -2976, 160, 0 }, { 1616, 2976, 160, 0 }, { -1616, -2976, -160, 0 }, { -1616, 2976, -160, 0 } }, { 405, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 3386, 0, 4, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1280, -2368, 5712, 0 }, { { -1632, -2976, -144, 0 }, { -1632, 2976, -144, 0 }, { 1632, -2976, 144, 0 }, { 1632, 2976, 144, 0 } }, { -362, 0, 4091, 0 }, { 0, 0, 4096, 0 }, 3396, 0, 7, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1712, -2336, 8096, 0 }, { { -1520, -2976, 1280, 0 }, { -1520, 2976, 1280, 0 }, { 1520, -2976, -1280, 0 }, { 1520, 2976, -1280, 0 } }, { 2640, 0, 3134, 0 }, { 0, 0, 4096, 0 }, 3574, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1760, -2336, 8289, 0 }, { { 1520, -2976, -1280, 0 }, { 1520, 2976, -1280, 0 }, { -1520, -2976, 1280, 0 }, { -1520, 2976, 1280, 0 } }, { -2641, 0, -3136, 0 }, { 0, 0, 4096, 0 }, 3574, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3232, -2336, 5040, 0 }, { { 64, -2976, -752, 0 }, { 64, 2976, -752, 0 }, { -64, -2976, 752, 0 }, { -64, 2976, 752, 0 } }, { -4094, 0, -349, 0 }, { 0, 0, 4096, 0 }, 3061, 0, 4, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3088, -2368, 5087, 0 }, { { -64, -2976, 752, 0 }, { -64, 2976, 752, 0 }, { 64, -2976, -752, 0 }, { 64, 2976, -752, 0 } }, { 4091, 0, 348, 0 }, { 0, 0, 4096, 0 }, 3061, 0, 6, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3360, -2336, 9648, 0 }, { { 112, -2976, -2032, 0 }, { 112, 2976, -2032, 0 }, { -112, -2976, 2032, 0 }, { -112, 2976, 2032, 0 } }, { -4097, 0, -227, 0 }, { 0, 0, 4096, 0 }, 3602, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3184, -2272, 9648, 0 }, { { -128, -2976, 2112, 0 }, { -128, 2976, 2112, 0 }, { 128, -2976, -2112, 0 }, { 128, 2976, -2112, 0 } }, { 4091, 0, 247, 0 }, { 0, 0, 4096, 0 }, 3647, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1104, -2144, 4080, 0 }, { { -1152, -2976, 16, 0 }, { -1152, 2976, 16, 0 }, { 1152, -2976, -16, 0 }, { 1152, 2976, -16, 0 } }, { 56, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 3187, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1040, -2144, 4192, 0 }, { { 1360, -2976, -48, 0 }, { 1360, 2976, -48, 0 }, { -1360, -2976, 48, 0 }, { -1360, 2976, 48, 0 } }, { -146, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 3268, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4992, -2080, 4275, 0 }, { { 1360, -2976, 0, 0 }, { 1360, 2976, 0, 0 }, { -1360, -2976, 0, 0 }, { -1360, 2976, 0, 0 } }, { 0, 0, -4100, 0 }, { 0, 0, 4096, 0 }, 3268, 0, 2, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5008, -2112, 4160, 0 }, { { -1600, -2976, 0, 0 }, { -1600, 2976, 0, 0 }, { 1600, -2976, 0, 0 }, { 1600, 2976, 0, 0 } }, { 0, 0, 4107, 0 }, { 0, 0, 4096, 0 }, 3376, 0, 6, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordRoomAmbientEntry D_dryfield_night_factory_8018A0C8[20] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_night_factory_8018A0C8) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 763, 757, 658, 746 } },
    { .color = { 870, 852, 838, 857 } },
    { .color = { 638, 625, 617, 628 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 458, 450, 448, 452 } },
    { .color = { 534, 526, 518, 528 } },
    { .color = { 519, 513, 500, 513 } },
    { .color = { 479, 477, 477, 477 } },
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

WorldCollisionTrigger D_dryfield_night_factory_8018A168[19] = {
    { NULL, NULL, NULL, { 5328, -48, 1296, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_WARP, 25, 17, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5712, -64, 0x2B70, 0 }, { { -672, 0, -928, 0 }, { 672, 0, -928, 0 }, { -672, 0, 928, 0 }, { 672, 0, 928, 0 } }, { 0, 4108, 0, 0 }, { -3784, 0, -1567, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 336, -64, 544, 0 }, { { -688, 0, -672, 0 }, { 688, 0, -672, 0 }, { -688, 0, 672, 0 }, { 688, 0, 672, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 960, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5856, -64, 5600, 0 }, { { -496, 0, -576, 0 }, { 496, 0, -576, 0 }, { -496, 0, 576, 0 }, { 496, 0, 576, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 759, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6224, -64, 3968, 0 }, { { -1264, 0, -416, 0 }, { 1264, 0, -416, 0 }, { -1264, 0, 416, 0 }, { 1264, 0, 416, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 1330, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6240, -64, 4816, 0 }, { { -1264, 0, -448, 0 }, { 1264, 0, -448, 0 }, { -1264, 0, 448, 0 }, { 1264, 0, 448, 0 } }, { 0, 4114, 0, 0 }, { 0, 0, 4096, 0 }, 1336, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 384, -64, 7328, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_WARP, 22, 34, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5440, -64, 7088, 0 }, { { -528, 0, -992, 0 }, { 528, 0, -992, 0 }, { -528, 0, 992, 0 }, { 528, 0, 992, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_WARP, 24, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1568, -64, 7104, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3712, -64, 2528, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3424, -64, 0, 0 }, { { -1872, 0, -304, 0 }, { 464, 0, -304, 0 }, { -1072, 0, 1296, 0 }, { 464, 0, 1296, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1894, WORLD_COLLISION_TRIGGER_ACTION_CAP, 16, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4816, -64, 928, 0 }, { { -768, 0, -368, 0 }, { 768, 0, -368, 0 }, { -768, 0, 368, 0 }, { 768, 0, 368, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, 4096, 0 }, 851, WORLD_COLLISION_TRIGGER_ACTION_CAP, 17, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 320, -64, 2336, 0 }, { { -512, 0, -1488, 0 }, { 1408, 0, -912, 0 }, { -512, 0, 880, 0 }, { 1408, 0, 880, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, 4096, 0 }, 1673, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1799, -64, 0x2787, 0 }, { { -775, 0, 52, 0 }, { -269, 0, -621, 0 }, { 322, 0, 592, 0 }, { 724, 0, -20, 0 } }, { 0, 4099, 0, 0 }, { 3406, 0, -2276, 0 }, 775, WORLD_COLLISION_TRIGGER_ACTION_CAP, 20, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3456, -64, 7440, 0 }, { { -2208, 0, -2688, 0 }, { 1760, 0, -2688, 0 }, { -2208, 0, 2688, 0 }, { 1760, 0, 2688, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, 4096, 0 }, 3472, WORLD_COLLISION_TRIGGER_ACTION_CAP, 24, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1248, -64, 8960, 0 }, { { -368, 0, -720, 0 }, { 368, 0, -720, 0 }, { -368, 0, 720, 0 }, { 368, 0, 720, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_CAP, 22, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4816, -64, 1744, 0 }, { { -928, 0, -384, 0 }, { 928, 0, -384, 0 }, { -928, 0, 384, 0 }, { 928, 0, 384, 0 } }, { 0, 4099, 0, 0 }, { 201, 0, -4091, 0 }, 1003, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4751, -64, 3455, 0 }, { { -947, 0, -557, 0 }, { 1098, 0, -195, 0 }, { -1097, 0, 196, 0 }, { 948, 0, 558, 0 } }, { 0, 4105, 0, 0 }, { -201, 0, 4091, 0 }, 1108, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5632, -64, 9808, 0 }, { { -672, 0, -368, 0 }, { 672, 0, -368, 0 }, { -672, 0, 368, 0 }, { 672, 0, 368, 0 } }, { 0, 4102, 0, 0 }, { -3612, 0, 1931, 0 }, 765, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaVariant D_dryfield_night_factory_8018A70C[11] = { 0 };

WorldCollisionFootstepSounds D_dryfield_night_factory_8018A764 = {
    0x10000051,
    0x10000053,
    0x10000051,
};

WorldCollisionFootstepSounds D_dryfield_night_factory_8018A770 = {
    0x10000015,
    0x10000017,
    0x10000015,
};

WorldCollisionSurfaceProperties D_dryfield_night_factory_8018A77C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_factory_8018A784[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_factory_8018A78C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_factory_8018A764 },
};

WorldCollisionSurfaceProperties D_dryfield_night_factory_8018A794[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_factory_8018A770 },
};

WorldCollisionSurfaceProperties* D_dryfield_night_factory_8018A79C[8] = {
    D_dryfield_night_factory_8018A77C,
    D_dryfield_night_factory_8018A784,
    D_dryfield_night_factory_8018A78C,
    D_dryfield_night_factory_8018A794,
    D_dryfield_night_factory_8018A77C,
    D_dryfield_night_factory_8018A77C,
    D_dryfield_night_factory_8018A77C,
    D_dryfield_night_factory_8018A77C,
};

PadScriptCmd gFactoryNightJoltCmds[3] = { { 0x201, 1 }, { 0, 0x101 }, { 0, 0 } };

PadScriptVibrationSegment gFactoryNightJoltRecs[3] = { { 255, 255, 8, 1 }, { 150, 80, 20, 1 }, { 0, 0, 5, 0 } };

RoomEventMsg gRoomEventMsg = { 0 };

u8 gRoomEventActive = 0;

TaskDesc* gFactoryPanelDesc = NULL;

TaskDesc* gFactorySpawnTable = NULL;

Task** gFactoryPanelSlot = NULL;

RoomEventReq gRoomEventReq = { 0 };

#include "../../shared/factory_lift_init.inc.c"

#include "../../shared/factory_lift_sync_collision.inc.c"

#include "../../shared/factory_lift_turn_out.inc.c"

#include "../../shared/factory_lift_turn_back.inc.c"

#include "../../shared/factory_lift_raise.inc.c"

#include "../../shared/factory_lift_lower.inc.c"

#include "../../shared/factory_lift_jam_turn_out.inc.c"

#include "../../shared/factory_lift_jam_turn_back.inc.c"

#include "../../shared/factory_hatch_open.inc.c"

#include "../../shared/factory_hatch_close.inc.c"

#include "../../shared/factory_power_scene.inc.c"

#include "../../shared/factory_whiteout_scene.inc.c"

#include "../../shared/factory_barrier_collision.inc.c"

#include "../../shared/factory_lift_update.inc.c"

#include "../../shared/factory_lift_exit.inc.c"

#include "../../shared/factory_lift_bind_lighting.inc.c"

#include "../../shared/factory_lift_notify_panel.inc.c"

#include "../../shared/factory_hatch_init.inc.c"

#include "../../shared/factory_hatch_update.inc.c"

#include "../../shared/factory_hatch_watch.inc.c"

#include "../../shared/factory_lift_run.inc.c"

#include "../../shared/factory_hatch_run.inc.c"

#include "../../shared/factory_lamp_scene.inc.c"

#include "../../shared/factory_cap_scene.inc.c"

#include "../../shared/factory_hatch_scene.inc.c"
