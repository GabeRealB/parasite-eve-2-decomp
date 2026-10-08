#include "rooms/neo_ark_r31.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
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
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "../../shared/room_variants.h"

/// Room message handler table installed into `Task::msgTable`.
extern TaskMessageEntry D_neo_ark_r31_8017D9F4[];
extern EvsCommand       D_actor_461800_80133F90[];
extern EvsCommand       D_actor_461800_80134470[];

static s32  _neoArkR31RejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _neoArkR31ResolveRoomTransition(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32  _neoArkR31IgnoreRoomCommand(Task* task, s32 messageId, s32 commandId, s32 commandArg);
static s32  _neoArkR31IgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static void _neoArkR31FramebufferShiftTask(Task* task);

TaskDesc D_neo_ark_r31_8017D9E8 = { { { TASK_BODY_NONE, 192 } }, _neoArkR31FramebufferShiftTask, { .value = 0 } };

TaskMessageEntry D_neo_ark_r31_8017D9F4[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _neoArkR31ResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _neoArkR31RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _neoArkR31IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _neoArkR31IgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

u8* D_neo_ark_r31_8017DA1C[1] = {
    gViewIdentityMap,
};

ViewCount D_neo_ark_r31_8017DA20[1] = { 3 };

DirectionWarpEntry D_neo_ark_r31_8017DA24[1] = {
    { { { .word = 2048 }, 0, 0, 0 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 0, 0, 0 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 1, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

ViewCamera D_neo_ark_r31_8017DA5C[3] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -7510, 0x61A8, -6980 } }, 329 },
    { { { { 2889, 0, -2903 }, { 2898, 236, 2884 }, { 167, -4089, 167 } }, { -8000, -1000, -7000 } }, 289 },
    { { { { -3243, 0, 2501 }, { 1904, 2655, 2469 }, { -1621, 3118, -2102 } }, { -8340, 1050, -7830 } }, 289 },
};

SpriteBatch D_neo_ark_r31_8017DAC8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_r31_8017DAD8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_r31_8017DAE8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_neo_ark_r31_8017DAF8[3] = {
    { { .empty = D_neo_ark_r31_8017DAC8 }, D_neo_ark_r31_8017DAC8, NULL },
    { { .empty = D_neo_ark_r31_8017DAD8 }, D_neo_ark_r31_8017DAD8, NULL },
    { { .empty = D_neo_ark_r31_8017DAE8 }, D_neo_ark_r31_8017DAE8, NULL },
};

WorldCoordPointLight D_neo_ark_r31_8017DB1C[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -0x2710, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 0x186A0, 0x186A0 },
};

WorldCoordRoomLights D_neo_ark_r31_8017DB7C = { 0, NULL, ARRAY_SIZE(D_neo_ark_r31_8017DB1C), D_neo_ark_r31_8017DB1C, 0, NULL };

AreaResource D_neo_ark_r31_8017DB94[3] = {
    { 101, 618, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, D_actor_461800_80139F8C },
    { 132, 618, AREA_RESOURCE_FILE_GROUP_BASE_60, 0, { 0, 0 }, &D_actor_461800_801437EC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_neo_ark_r31_8017DBB8[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017C550, D_neo_ark_r31_8017DB94 },
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
};

s32 D_neo_ark_r31_8017DC20[3] = {
    0x10000011,
    0x10000013,
    0x10000011,
};

WorldCollisionSurfaceProperties D_neo_ark_r31_8017DC2C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties* D_neo_ark_r31_8017DC34[8] = {
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
    D_neo_ark_r31_8017DC2C,
};

s32 D_neo_ark_r31_8017DC54 = 0;

static void func_neo_ark_r31_8017D90C(Task* arg0);
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
        SCREEN_WIDTH         = 320,
        SCREEN_HEIGHT        = 240,
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
        D_neo_ark_r31_8017DC54 = INITIAL_SHIFT_PIXELS;
        task->state++;
    }
    if (D_neo_ark_r31_8017DC54 < 0) {
        taskCallExit(task);
        return;
    }
    // Sample two 16-bit texture strips, clipping their ends to the byte-sized UV range.
    for (sourceX = 0; sourceX < SCREEN_WIDTH; sourceX += TILE_WIDTH) {
        screenX = sourceX - SCREEN_CENTER_X;
        for (sourceY = 0; sourceY < SCREEN_HEIGHT; sourceY += SCREEN_HEIGHT) {
            screenY        = sourceY - SCREEN_CENTER_Y;
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
            poly->tpage    = getTPage(TEXTURE_16_BIT, GPU_BLEND_AVERAGE, sourceX & ~TEXTURE_PAGE_X_MASK, sourceBuffer << TEXTURE_PAGE_Y_SHIFT);
            poly->y0 = poly->y1 = screenY;
            poly->v0 = poly->v1 = (sourceY + (sourceBuffer << FRAMEBUFFER_V_SHIFT)) + gDisplayState.vramYOffset;
            if (poly->v0 < FULL_TILE_V_LIMIT) {
                poly->y2 = poly->y3 = sourceY + SCREEN_CENTER_Y;
                poly->v2 = poly->v3 = poly->v0 + SCREEN_HEIGHT;
            } else {
                s32 remainingRows = TEXTURE_UV_MAX - poly->v0;
                poly->y2 = poly->y3 = screenY + remainingRows;
                poly->v2 = poly->v3 = poly->v0 + remainingRows;
            }
            shiftedX = screenX - D_neo_ark_r31_8017DC54;
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

/// Room task state 0: installs the message table, claims pointer slot 7,
/// sets `gCdCmdQueue.imageMdecMode` to 2 and starts the room script with
/// `evsStartScriptWithSkip`. Advances to state 1.
static void func_neo_ark_r31_8017D90C(Task* arg0)
{
    CdCmdQueue* queue;

    queue          = &gCdCmdQueue;
    arg0->msgTable = D_neo_ark_r31_8017D9F4;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    queue->imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
    evsStartScriptWithSkip(D_actor_461800_80133F90, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_461800_80134470);
    arg0->state = (s32)(arg0->state + 1);
}

/// Re-arms pixel bit 15 for the next RGB16 background decode.
///
/// State 1 refreshes the one-image MDEC mode every tick because decoding consumes
/// it. The task argument is unused.
static void _neoArkR31SetImageMaskMode(Task* unusedTask)
{
    gCdCmdQueue.imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
}

/// State handlers of the room task `neoArkR31RoomTask`, indexed by
/// `Task::state`: the set-up tick, the tick that stores 2 into `gCdCmdQueue.imageMdecMode`,
/// and `taskKill`.
static const TaskFuncTable3 D_neo_ark_r31_8017D5C4 = {
    {
        func_neo_ark_r31_8017D90C,
        _neoArkR31SetImageMaskMode,
        taskKill,
    },
};

void neoArkR31RoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_neo_ark_r31_8017D5C4;
    stateHandlers.funcs[task->state](task);
}
