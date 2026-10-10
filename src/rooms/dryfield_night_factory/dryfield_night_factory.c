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
#include "main/sound_types.h"
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

static void _factoryLiftInit(Task* task);
static void _factoryLiftUpdate(Task* task);

static void _factoryHatchInit(Task* task);
static void _factoryHatchUpdate(Task* task);
static void _factoryLiftExit(Task* task);
static void _factoryLiftSyncCollision(Task* task, s32 remapFaces, s32 useTurnedTemplate);
static s32  _factoryLiftTurnOut(Task* task);
static s32  _factoryLiftTurnBack(Task* task);
static s32  _factoryLiftRaise(Task* task);
static s32  _factoryLiftLower(Task* task);
static s32  _factoryLiftJamTurnOut(Task* task);
static s32  _factoryLiftJamTurnBack(Task* task);
static void _factoryLiftNotifyPanel(Task* panelTask);

#include "../../shared/factory_lift_motion.inc.c"

/// State handlers of the factory model task: set-up, the per-frame state and
/// `taskKill`.
static const TaskFuncTable3 _gFactoryLiftStates = {
    { _factoryLiftInit, _factoryLiftUpdate, taskKill },
};

/// State handlers of the cutscene task: set-up, the cutscene sequence and
/// `taskKill`.
static const TaskFuncTable3 _gFactoryHatchTaskStates = {
    { _factoryHatchInit, _factoryHatchUpdate, taskKill },
};

/// The cutscene sequence's handler table: the flag watcher of state 0 and the
/// two movements it arms.
static const FactoryHatchStateFuncTable _gFactoryHatchStates = {
    { _factoryHatchWatch, _factoryHatchOpen, _factoryHatchClose },
};

SpriteBatch D_dryfield_night_factory_80189A14[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_night_factory_80189A24[19] = {
    { { .empty = gDryfieldNightFactoryView1SpriteBatches }, gDryfieldNightFactoryView1SpriteBatches, NULL },
    { { .elements = gDryfieldNightFactoryView2SpriteSources }, gDryfieldNightFactoryView2SpriteBatches, NULL },
    { { .elements = gDryfieldNightFactoryView3SpriteSources }, gDryfieldNightFactoryView3SpriteBatches, NULL },
    { { .elements = gDryfieldNightFactoryView4SpriteSources }, gDryfieldNightFactoryView4SpriteBatches, NULL },
    { { .empty = gDryfieldNightFactoryView5SpriteBatches }, gDryfieldNightFactoryView5SpriteBatches, NULL },
    { { .elements = gDryfieldNightFactoryView6SpriteSources }, gDryfieldNightFactoryView6SpriteBatches, NULL },
    { { .elements = gDryfieldNightFactoryView7SpriteSources }, gDryfieldNightFactoryView7SpriteBatches, NULL },
    { { .elements = gDryfieldNightFactoryView8SpriteSources }, gDryfieldNightFactoryView8SpriteBatches, NULL },
    { { .elements = gDryfieldNightFactoryView9SpriteSources }, gDryfieldNightFactoryView9SpriteBatches, NULL },
    { { .empty = gDryfieldNightFactoryView10SpriteBatches }, gDryfieldNightFactoryView10SpriteBatches, NULL },
    { { .empty = gDryfieldNightFactoryView11SpriteBatches }, gDryfieldNightFactoryView11SpriteBatches, NULL },
    { { .empty = gDryfieldNightFactoryView12SpriteBatches }, gDryfieldNightFactoryView12SpriteBatches, NULL },
    { { .empty = gDryfieldNightFactoryView13SpriteBatches }, gDryfieldNightFactoryView13SpriteBatches, NULL },
    { { .empty = gDryfieldNightFactoryView14SpriteBatches }, gDryfieldNightFactoryView14SpriteBatches, NULL },
    { { .elements = gDryfieldNightFactoryView15SpriteSources }, gDryfieldNightFactoryView15SpriteBatches, NULL },
    { { .elements = gDryfieldNightFactoryView16SpriteSources }, D_dryfield_night_factory_80189858, NULL },
    { { .empty = D_dryfield_night_factory_80189880 }, D_dryfield_night_factory_80189880, NULL },
    { { .elements = D_dryfield_night_factory_80189890 }, D_dryfield_night_factory_801899E4, NULL },
    { { .elements = D_dryfield_night_factory_80189890 }, D_dryfield_night_factory_801899E4, NULL },
};

/// Authored point lights for the room, eligible in every logical view.
///
/// Positions and falloff radii use world-coordinate units; RGB uses Q12.
/// Coordinate setup parents and composes these mutable lights, and queries
/// overwrite attenuation. Pointers remain valid while this room is loaded.
static WorldCoordPointLight _gDryfieldNightFactoryPointLights[4] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3051, -1864, 8762 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1419,
        .outer = 4022,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5657, -1403, 9444 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 1759, 1784, 1784 },
        },
        .inner = 1000,
        .outer = 1319,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3051, -1864, 2398 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1702,
        .outer = 3699,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3051, -1864, 5767 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1522,
        .outer = 4180,
    },
};

WorldCoordRoomLights gDryfieldNightFactoryRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightFactoryPointLights),
    .pointLights           = _gDryfieldNightFactoryPointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
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

WorldCoordRoomAmbientEntry gDryfieldNightFactoryAmbientByView[20] = {
    { .viewCount = ARRAY_SIZE(gDryfieldNightFactoryAmbientByView) - 1 },
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

/// Cue-2 sound requests in the loaded type-1 bank; cue 1 selects the next entry.
enum {
    DRYFIELD_NIGHT_FACTORY_SURFACE_2_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x51,
    DRYFIELD_NIGHT_FACTORY_SURFACE_2_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x53
};

/// Footstep cue bases for surface class 2.
///
/// Walking and scripted jumps share one pair; running uses the other.
/// Cue 1 adds one to each base, and companion playback adds 100.
/// Requests borrow the loaded type-1 bank while the room is active.
static WorldCollisionFootstepSounds _gDryfieldNightFactorySurface2FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_FACTORY_SURFACE_2_WALK_CUE_BASE,
    .run          = DRYFIELD_NIGHT_FACTORY_SURFACE_2_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_NIGHT_FACTORY_SURFACE_2_WALK_CUE_BASE,
};

/// Cue-2 sound requests in the loaded type-1 bank; cue 1 selects the next entry.
enum {
    DRYFIELD_NIGHT_FACTORY_SURFACE_3_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x15,
    DRYFIELD_NIGHT_FACTORY_SURFACE_3_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x17
};

/// Footstep cue bases for surface class 3.
///
/// Walking and scripted jumps share one pair; running uses the other.
/// Cue 1 adds one to each base, and companion playback adds 100.
/// Requests borrow the loaded type-1 bank while the room is active.
static WorldCollisionFootstepSounds _gDryfieldNightFactorySurface3FootstepSounds = {
    .walk         = DRYFIELD_NIGHT_FACTORY_SURFACE_3_WALK_CUE_BASE,
    .run          = DRYFIELD_NIGHT_FACTORY_SURFACE_3_RUN_CUE_BASE,
    .scriptedJump = DRYFIELD_NIGHT_FACTORY_SURFACE_3_WALK_CUE_BASE,
};

/// Collision policy for surface classes 0 and 4..7: probes block, impacts are enabled and pushback applies.
///
/// No footstep cues; the loaded room overlay owns this record.
static WorldCollisionSurfaceProperties _gDryfieldNightFactoryDefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Collision policy for surface class 1: probes pass and weapon impacts are ignored.
///
/// Grid contacts still apply pushback; this class supplies no footstep cues.
static WorldCollisionSurfaceProperties _gDryfieldNightFactorySurface1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Collision policy for surface class 2: probes block, impacts are enabled and pushback applies.
///
/// Borrows room-owned walking, running and scripted-jump cue bases while loaded.
static WorldCollisionSurfaceProperties _gDryfieldNightFactorySurface2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightFactorySurface2FootstepSounds,
};

/// Collision policy for surface class 3: probes block, impacts are enabled and pushback applies.
///
/// Borrows room-owned walking, running and scripted-jump cue bases while loaded.
static WorldCollisionSurfaceProperties _gDryfieldNightFactorySurface3Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightFactorySurface3FootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightFactorySurfacePropertiesByClass[8] = {
    &_gDryfieldNightFactoryDefaultSurfaceProperties,
    &_gDryfieldNightFactorySurface1Properties,
    &_gDryfieldNightFactorySurface2Properties,
    &_gDryfieldNightFactorySurface3Properties,
    &_gDryfieldNightFactoryDefaultSurfaceProperties,
    &_gDryfieldNightFactoryDefaultSurfaceProperties,
    &_gDryfieldNightFactoryDefaultSurfaceProperties,
    &_gDryfieldNightFactoryDefaultSurfaceProperties,
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
