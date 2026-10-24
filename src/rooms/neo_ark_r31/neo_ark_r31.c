#include "rooms/neo_ark_r31.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor_461800.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/session.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "../../shared/room/room_variants.h"

static s32  _neoArkR31RejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _neoArkR31ResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _neoArkR31IgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32  _neoArkR31IgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static void _neoArkR31FramebufferShiftTask(Task* task);

TaskDesc gNeoArkR31FramebufferShiftTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_CONTROL } },
    .callback = _neoArkR31FramebufferShiftTask,
    .data     = { .value = 0 },
};

/// R31 handlers for exit resolution, key-item refusal and ignored actions/commands.
///
/// Four handlers and a `TASK_MESSAGE_TABLE_END` record; the room task borrows
/// this read-only table and its callbacks while the overlay remains loaded.
static const TaskMessageEntry _gNeoArkR31RoomMessages[5] SECTION(".data") = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _neoArkR31ResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _neoArkR31RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkR31IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _neoArkR31IgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u8* gNeoArkR31ViewMaps[1] = {
    gViewIdentityMap,
};

// Logical entries searched in each room map, independent of its storage capacity.
enum {
    NEO_ARK_R31_LOGICAL_VIEW_COUNT = 3,
};

ViewCount gNeoArkR31ViewCounts[1] = { NEO_ARK_R31_LOGICAL_VIEW_COUNT };

DirectionWarpEntry gNeoArkR31WarpEntries[1] = {
    {
        .player         = { .yaw = { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, .x = 0, .y = 0, .z = 0 },
        .unknown_10     = { 0 },
        .companion      = { .yaw = { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, .x = 0, .y = 0, .z = 0 },
        .unknown_24     = { 0 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 1,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

ViewCamera gNeoArkR31ViewCameras[3] = {
    { .transform = { .m = { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, .t = { -7510, 0x61A8, -6980 } }, .screenDistance = 329 },
    { .transform = { .m = { { 2889, 0, -2903 }, { 2898, 236, 2884 }, { 167, -4089, 167 } }, .t = { -8000, -1000, -7000 } }, .screenDistance = 289 },
    { .transform = { .m = { { -3243, 0, 2501 }, { 1904, 2655, 2469 }, { -1621, 3118, -2102 } }, .t = { -8340, 1050, -7830 } }, .screenDistance = 289 },
};

/// Keeps decoded image strips without source sprites for mapped view 1.
///
/// A zero-count prefix selects the background; SPRITE_BATCH_END terminates
/// the list. The descriptor also retains this list in sources.empty, which
/// is never dereferenced as a SpriteSource pool.
static SpriteBatch _gNeoArkR31View1SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// Keeps decoded image strips without source sprites for mapped view 2.
///
/// A zero-count prefix selects the background; SPRITE_BATCH_END terminates
/// the list. The descriptor also retains this list in sources.empty, which
/// is never dereferenced as a SpriteSource pool.
static SpriteBatch _gNeoArkR31View2SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

/// Keeps decoded image strips without source sprites for mapped view 3.
///
/// A zero-count prefix selects the background; SPRITE_BATCH_END terminates
/// the list. The descriptor also retains this list in sources.empty, which
/// is never dereferenced as a SpriteSource pool.
static SpriteBatch _gNeoArkR31View3SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END, .spriteCount = 0, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
};

SpriteView gNeoArkR31SpriteViews[3] = {
    { { .empty = _gNeoArkR31View1SpriteBatches }, _gNeoArkR31View1SpriteBatches, NULL },
    { { .empty = _gNeoArkR31View2SpriteBatches }, _gNeoArkR31View2SpriteBatches, NULL },
    { { .empty = _gNeoArkR31View3SpriteBatches }, _gNeoArkR31View3SpriteBatches, NULL },
};

/// Authored point lights for the room, eligible in every logical view.
///
/// Positions and falloff radii use world-coordinate units; RGB uses Q12.
/// Coordinate setup parents and composes these mutable lights, and queries
/// overwrite attenuation. Pointers remain valid while this room is loaded.
static WorldCoordPointLight _gNeoArkR31PointLights[1] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -10000, 0 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 100000,
        .outer = 100000,
    },
};

WorldCoordRoomLights gNeoArkR31RoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gNeoArkR31PointLights),
    .pointLights           = _gNeoArkR31PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Placement-entry keys, decimal file numbers and table selectors for these layouts.
enum {
    NEO_ARK_R31_RESOURCE_KYLE = 101,
    NEO_ARK_R31_RESOURCE_AYA  = 132,
    NEO_ARK_R31_FILE_CAST     = 618,
};

/// Kyle and Aya-model resource recipes for the default placement variant (1).
///
/// Keys 101 and 132 select files 461800 and 661800 respectively; both use
/// descriptor zero in actor_461800. The base-60 file supplies Aya textures.
/// Paired with `gMapNeoArkR31DefaultPlacements`; AREA_PLACEMENT_END ends the list.
/// Retain room recipes, placements and actor descriptors through loading,
/// spawning and model-flag updates.
static AreaResource _gNeoArkR31DefaultResources[3] = {
    { .entryId = NEO_ARK_R31_RESOURCE_KYLE, .fileNumber = NEO_ARK_R31_FILE_CAST, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_40, .taskIndex = 0, .taskTable = gActor461800ScriptedWalkerTasks },
    { .entryId = NEO_ARK_R31_RESOURCE_AYA, .fileNumber = NEO_ARK_R31_FILE_CAST, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_60, .taskIndex = 0, .taskTable = &gActor461800FootstepWalkerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gNeoArkR31AreaVariants[NEO_ARK_R31_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapNeoArkR31DefaultPlacements, .resources = _gNeoArkR31DefaultResources },
};

enum {
    NEO_ARK_R31_UNUSED_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x11,
    NEO_ARK_R31_UNUSED_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x13,
};

/// Retained walking, running and scripted-jump cue bases, unused by this room.
///
/// These type-1 requests select the loaded sound bank. No surface record
/// borrows them; every surface class uses a NULL footstep-sound pointer.
static WorldCollisionFootstepSounds _gNeoArkR31UnusedFootstepSounds = {
    .walk         = NEO_ARK_R31_UNUSED_WALK_CUE_BASE,
    .run          = NEO_ARK_R31_UNUSED_RUN_CUE_BASE,
    .scriptedJump = NEO_ARK_R31_UNUSED_WALK_CUE_BASE,
};

/// Collision policy for surface classes 0..7: probes block, impacts are enabled and pushback applies.
///
/// No footstep cues; the loaded room overlay owns this record.
static WorldCollisionSurfaceProperties _gNeoArkR31SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

WorldCollisionSurfaceProperties* gNeoArkR31SurfacePropertiesByClass[8] = {
    &_gNeoArkR31SurfaceProperties,
    &_gNeoArkR31SurfaceProperties,
    &_gNeoArkR31SurfaceProperties,
    &_gNeoArkR31SurfaceProperties,
    &_gNeoArkR31SurfaceProperties,
    &_gNeoArkR31SurfaceProperties,
    &_gNeoArkR31SurfaceProperties,
    &_gNeoArkR31SurfaceProperties,
};

s32 gNeoArkR31FramebufferShiftPixels = 0;

static void _neoArkR31InitializeRoom(Task* task);
static void _neoArkR31SetImageMaskMode(Task* unusedTask);

/// Brackets the shifted framebuffer draw with GPU mask-bit writes enabled and disabled.
///
/// Borrows two DR_STP packets from the frame arena. Reverse OT traversal runs
/// the enable command first at the last tag and the disable command before the
/// shifted quads at `drawDepth`: preceding draws set pixel bit 15, while the
/// shifted overlay does not force that bit in its output.
/// `drawDepth` is a tag index in 0..1022, not a byte offset. Queue the shifted
/// quads there before calling; the packets remain live until GPU consumption.
static inline void _neoArkR31QueueFramebufferMaskModes(s32 drawDepth)
{
    enum { NEO_ARK_R31_MASK_WRITE_DISABLE = 0,
           NEO_ARK_R31_MASK_WRITE_ENABLE  = 1 };

    DR_STP* maskMode;

    maskMode       = gGpuPrimCursor;
    gGpuPrimCursor = maskMode + 1;
    SetDrawStp(maskMode, NEO_ARK_R31_MASK_WRITE_DISABLE);
    addPrim(gGpuCurrentOt + drawDepth, maskMode);
    maskMode       = gGpuPrimCursor;
    gGpuPrimCursor = maskMode + 1;
    SetDrawStp(maskMode, NEO_ARK_R31_MASK_WRITE_ENABLE);
    addPrim(gGpuCurrentOt + GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*gGpuCurrentOt), maskMode);
}

/// Overlays the current draw framebuffer shifted left by the scene's pixel displacement.
///
/// State 0 initializes the displacement to three pixels; subsequent calls use
/// the scene controller's nonnegative value. A negative displacement requests
/// task exit before drawing. Requires the 320x240 double-buffered display, OT
/// tags 6 and 1023 and word-aligned arena space for two POLY_FT4 and two DR_STP
/// packets. Packets borrow that storage until GPU drawing completes.
static void _neoArkR31FramebufferShiftTask(Task* task)
{
    enum {
        INITIALIZE           = 0,
        INITIAL_SHIFT_PIXELS = 3,
        DRAW_DEPTH           = 6,
        TILE_WIDTH           = 160,
        SCREEN_CENTER_X      = 160,
        SCREEN_CENTER_Y      = 120,
        TEXTURE_16_BIT       = 2,
        TEXTURE_PAGE_X_MASK  = 63,
        TEXTURE_PAGE_Y_SHIFT = 8,
        FRAMEBUFFER_V_SHIFT  = 4,
        FULL_TILE_V_LIMIT    = 16,
        FULL_TILE_U_LIMIT    = 96,
        TEXTURE_UV_MAX       = 255,
    };
    POLY_FT4* poly;
    s32       sourceBuffer;
    s32       drawDepth;
    s32       sourceX;
    s32       sourceY;
    s32       screenX;
    s32       screenY;
    s32       shiftedX;

    drawDepth    = DRAW_DEPTH;
    sourceBuffer = gDisplayState.otBuffer;
    if (task->state == INITIALIZE) {
        gNeoArkR31FramebufferShiftPixels = INITIAL_SHIFT_PIXELS;
        task->state++;
    }
    if (gNeoArkR31FramebufferShiftPixels < 0) {
        taskCallExit(task);
        return;
    }
    // Sample two 16-bit texture strips, clipping their ends to the byte-sized UV range.
    for (sourceX = 0; sourceX < DISPLAY_SCREEN_WIDTH; sourceX += TILE_WIDTH) {
        screenX = sourceX - SCREEN_CENTER_X;
        for (sourceY = 0; sourceY < DISPLAY_SCREEN_HEIGHT; sourceY += DISPLAY_SCREEN_HEIGHT) {
            screenY        = sourceY - SCREEN_CENTER_Y;
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
            poly->tpage    = getTPage(TEXTURE_16_BIT, GPU_BLEND_AVERAGE, sourceX & ~TEXTURE_PAGE_X_MASK, sourceBuffer << TEXTURE_PAGE_Y_SHIFT);
            poly->y0 = poly->y1 = screenY;
            poly->v0 = poly->v1 = (sourceY + (sourceBuffer << FRAMEBUFFER_V_SHIFT)) + gDisplayState.vramYOffset;
            if (poly->v0 < FULL_TILE_V_LIMIT) {
                poly->y2 = poly->y3 = sourceY + SCREEN_CENTER_Y;
                poly->v2 = poly->v3 = poly->v0 + DISPLAY_SCREEN_HEIGHT;
            } else {
                s32 remainingRows = TEXTURE_UV_MAX - poly->v0;
                poly->y2 = poly->y3 = screenY + remainingRows;
                poly->v2 = poly->v3 = poly->v0 + remainingRows;
            }
            shiftedX = screenX - gNeoArkR31FramebufferShiftPixels;
            poly->x0 = poly->x2 = shiftedX;
            poly->u0 = poly->u2 = sourceX & TEXTURE_PAGE_X_MASK;
            if (poly->u0 < FULL_TILE_U_LIMIT) {
                poly->x1 = poly->x3 = poly->x0 + TILE_WIDTH;
                poly->u1 = poly->u3 = poly->u0 + TILE_WIDTH;
            } else {
                s32 remainingColumns = TEXTURE_UV_MAX - poly->u0;
                poly->x1 = poly->x3 = poly->x0 + remainingColumns;
                poly->u1 = poly->u3 = poly->u0 + remainingColumns;
            }
            setPolyFT4(poly);
            setSemiTrans(poly, 1);
            setShadeTex(poly, 1);
            addPrim(gGpuCurrentOt + drawDepth, poly);
        }
    }
    _neoArkR31QueueFramebufferMaskModes(drawDepth);
}

/// Refuses every key item, selecting the inventory's cannot-use notice.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; all arguments are unread and unretained.
static s32 _neoArkR31RejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves a Neo Ark destination and permits the ordinary room transition.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; task and messageId are unused.
/// Request and reply borrow complete eight-byte records and may alias. Copies
/// the request before resolving its room selector; queries keep the requested
/// destination. Destination selectors must be valid in the active stage.
/// Neither pointer is retained. Returns `ROOM_VARIANT_TRANSITION_DIRECT`.
static s32 _neoArkR31ResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    return ROOM_VARIANT_TRANSITION_DIRECT;
}

/// Ignores `ROOM_MESSAGE_COMMAND`, returning zero without changing room state.
///
/// Both integer payloads and the receiver and message ID are unused.
static s32 _neoArkR31IgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg)
{
    return 0;
}

/// Ignores `DIRECTION_MESSAGE_ROOM_ACTION`, returning zero without changing room state.
///
/// The request is borrowed for synchronous dispatch and is neither read nor
/// retained. All other arguments are unused.
static s32 _neoArkR31IgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Registers the room receiver and starts its skippable actor scene.
///
/// State 0 installs room messages, publishes the borrowed task in the room slot
/// and arms RGB16 background decoding with pixel bit 15 set. Starts the loaded
/// actor-461800 scene and skip scripts with HUD hold/restoration, then enters
/// state 1, which re-arms that decoder policy for each image. Allocates no work.
static void _neoArkR31InitializeRoom(Task* task)
{
    CdCmdQueue* queue;

    queue          = &gCdCmdQueue;
    task->msgTable = _gNeoArkR31RoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    queue->imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
    evsStartScriptWithSkip(gActor461800NeoArkSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gActor461800NeoArkSceneSkipScript);
    task->state = task->state + 1;
}

/// Re-arms pixel bit 15 for the next RGB16 background decode.
///
/// State 1 refreshes the one-image MDEC mode every tick because decoding consumes
/// it. The task argument is unused.
static void _neoArkR31SetImageMaskMode(Task* unusedTask)
{
    gCdCmdQueue.imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
}

/// R31 room states: 0 initialize, 1 re-arm RGB16 image mask decoding, 2 release.
///
/// The dispatcher copies all three callbacks before indexing by `Task::state`.
/// There is no terminator or bounds check; keep the room loaded through dispatch.
static const TaskFuncTable3 _gNeoArkR31RoomStateHandlers = {
    {
        _neoArkR31InitializeRoom,
        _neoArkR31SetImageMaskMode,
        taskKill,
    },
};

void neoArkR31RoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gNeoArkR31RoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}
