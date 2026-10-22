#include "rooms/acropolis_fountain.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "acropolis_fountain_private.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
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
#include "main/fs_types.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/stream_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_akropolis.h"

#include "rooms/room_common.h"

#include "assets/rooms/akropolis/fountain/declarations.inc"

/// Phases of `_AcropolisFountainWaterLoop`.
///
/// Wait watches the streamed movie. The first time its frame is inside the
/// playing window, and any later time the loop is not already running, the
/// phase advances. Defer holds one tick. Apply commands the loop for a camera
/// that hears the fountain, marks the loop running, and returns to wait.
enum {
    ACROPOLIS_FOUNTAIN_WATER_LOOP_WAIT  = 0,
    ACROPOLIS_FOUNTAIN_WATER_LOOP_DEFER = 1,
    ACROPOLIS_FOUNTAIN_WATER_LOOP_APPLY = 2
};

/// First encoded movie frame that admits a fountain-loop request (frame 15).
#define ACROPOLIS_FOUNTAIN_WATER_LOOP_FRAME_FIRST 0xF

/// First encoded movie frame that fades a requested fountain loop (frame 240).
#define ACROPOLIS_FOUNTAIN_WATER_LOOP_FRAME_FADE 0xF0

/// Exclusive unsigned-halfword bound on the movie frame minus its first frame.
///
/// The 226-frame span includes frame 240, but the preceding fade test consumes
/// that frame: only frames 15..239 can advance the request state. Keep the u16
/// subtraction so frames before 15 wrap outside the admitted span.
#define ACROPOLIS_FOUNTAIN_WATER_LOOP_WINDOW (ACROPOLIS_FOUNTAIN_WATER_LOOP_FRAME_FADE - ACROPOLIS_FOUNTAIN_WATER_LOOP_FRAME_FIRST + 1)

/// Audio updates in the fountain loop's requested fade (20, independent of movie frames).
#define ACROPOLIS_FOUNTAIN_WATER_LOOP_FADE_TICKS 0x14

/// Task work for the fountain's waterfall loop.
///
/// Allocated into `Task::work` and cleared. `seenWindow` records that this
/// task has already observed the playing window, so it does not command the
/// loop again while that loop is still running. Whether the script is running
/// lives in a separate room global, which survives the task being replaced on
/// a camera cut. A new task has not seen the window, so while the script is
/// still running it re-pans that script instead of starting another.
typedef struct {
    u16 state;      // Phase (0 wait, 1 defer one tick, 2 start or re-pan)
    u16 seenWindow; // Nonzero once this task has observed the playing window
} _AcropolisFountainWaterLoop;
STATIC_ASSERT_SIZEOF(_AcropolisFountainWaterLoop, 4);

static void _acropolisFountainUpdateWaterLoop(Task* task, u16 logicalViewIndex);

/// Ordered phases of the player's single-step climb and subsequent walk.
enum {
    ACROPOLIS_FOUNTAIN_CLIMB_TURN = 0,
    ACROPOLIS_FOUNTAIN_CLIMB_WAIT_FOR_TURN,
    ACROPOLIS_FOUNTAIN_CLIMB_STEP,
    ACROPOLIS_FOUNTAIN_CLIMB_WAIT_FOR_STEP,
    ACROPOLIS_FOUNTAIN_CLIMB_MOVE,
    ACROPOLIS_FOUNTAIN_CLIMB_FINISH,
    ACROPOLIS_FOUNTAIN_CLIMB_STATE_COUNT
};

static void _acropolisFountainClimbTurnPlayer(Task* task);
static void _acropolisFountainClimbWaitForTurn(Task* task);
static void _acropolisFountainClimbStep(Task* task);
static void _acropolisFountainClimbWaitForStep(Task* task);
static void _acropolisFountainClimbMovePlayer(Task* task);
static void _acropolisFountainClimbFinish(Task* task);

static void _acropolisFountainWaterMovieTask(Task* task);
static void _acropolisFountainStopWaterMovieTask(Task* task);

#define ROOM_PART _gAcropolisFountainClimbTrigger
#include "assets/rooms/akropolis/fountain/action_triggers.inc"
#include "assets/rooms/akropolis/fountain/markers.inc"
/// Persistent water-loop request latch (0 no applied request, 1 request applied).
///
/// Applying a movie window sets this even for logical views with no sound mix
/// and when sound-queue admission fails. It selects mix versus start on the next
/// request and survives camera-cut task replacement. Frame 240 or later clears
/// it after requesting a fade; killing the movie task alone leaves it set.
/// Retains signed-halfword storage; this is not confirmed playback state.
static s16 _gAcropolisFountainWaterLoopRequested = 0;

enum { ACROPOLIS_FOUNTAIN_WATER_MOVIE_TASK_PRIORITY = 192 };

/// Bodyless water-movie recipes: slot 0 starts the updater, slot 1 stops it.
///
/// Both use priority 192 and ignore spawn payloads. This complete indexed table
/// has no end marker. The stop recipe requires the published updater to be live;
/// callbacks and movie/sound resources must remain loaded through task teardown.
static TaskDesc _gAcropolisFountainWaterMovieTaskDescriptors[2] = {
    { { { TASK_BODY_NONE, ACROPOLIS_FOUNTAIN_WATER_MOVIE_TASK_PRIORITY } }, _acropolisFountainWaterMovieTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACROPOLIS_FOUNTAIN_WATER_MOVIE_TASK_PRIORITY } }, _acropolisFountainStopWaterMovieTask, { .value = 0 } },
};

enum { ACROPOLIS_FOUNTAIN_LOGICAL_VIEW_COUNT = 22 };

/// Door-bank sound IDs and map-marker nibbles stored in fountain warp endpoints.
enum {
    ACROPOLIS_FOUNTAIN_DOOR_ARRIVAL_SOUND      = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_FOUNTAIN, 6),
    ACROPOLIS_FOUNTAIN_DOOR_DEPARTURE_SOUND    = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_FOUNTAIN, 5),
    ACROPOLIS_FOUNTAIN_HALLWAY_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_FOUNTAIN, 8),
    ACROPOLIS_FOUNTAIN_HALLWAY_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_FOUNTAIN, 7),
    ACROPOLIS_FOUNTAIN_PATIO_MAP_FLAG          = 496,
    ACROPOLIS_FOUNTAIN_FORKED_ROAD_MAP_FLAG    = 495,
    ACROPOLIS_FOUNTAIN_HALLWAY_MAP_FLAG        = 497,
};

/// Packed warp requests put the departure endpoint in the upper nibble.
/// CAP IDs select the room's command table; the lock command selects its message handler.
enum {
    ACROPOLIS_FOUNTAIN_WARP_ENDPOINT_SHIFT = 4,
    ACROPOLIS_FOUNTAIN_WARP_PATIO          = 1,
    ACROPOLIS_FOUNTAIN_WARP_FORKED_ROAD    = 2,
    ACROPOLIS_FOUNTAIN_WARP_HALLWAY        = 3,
    ACROPOLIS_FOUNTAIN_CAP_COMMAND_2       = 2,
    ACROPOLIS_FOUNTAIN_ROOM_COMMAND_LOCK   = 3,
    ACROPOLIS_FOUNTAIN_CAP_COMMAND_4       = 4,
    ACROPOLIS_FOUNTAIN_CAP_COMMAND_5       = 5,
};

/// Neutral RGB modulation in the GPU sprite's seven-fractional-bit scale.
enum { ACROPOLIS_FOUNTAIN_SPRITE_COLOR_ONE = 128 };

/// Type-1 footstep cue-2 requests in the currently loaded surface-sound bank.
///
/// Cue 1 adds one to each base; scripted stair climbing uses the walking pair.
enum {
    ACROPOLIS_FOUNTAIN_CLASS4_WALK_SOUND = 0x10000011,
    ACROPOLIS_FOUNTAIN_CLASS4_RUN_SOUND  = 0x10000013,
    ACROPOLIS_FOUNTAIN_CLASS5_WALK_SOUND = 0x10000039,
    ACROPOLIS_FOUNTAIN_CLASS5_RUN_SOUND  = 0x1000003B
};

#include "assets/rooms/akropolis/fountain/header.inc"
#include "assets/rooms/akropolis/fountain/geometry.inc"
#include "assets/rooms/akropolis/fountain/view_boundaries.inc"
#define ROOM_PART _gAcropolisFountainActionTriggers
#include "assets/rooms/akropolis/fountain/action_triggers.inc"
#include "assets/rooms/akropolis/fountain/placements.inc"
#include "assets/rooms/akropolis/fountain/lights.inc"
#include "assets/rooms/akropolis/fountain/sprites.inc"
#include "assets/rooms/akropolis/fountain/cameras.inc"
#include "assets/rooms/akropolis/fountain/surfaces.inc"

u8 gAcropolisFountainPatioArrivalWarp = 0;

u8 gAcropolisFountainPatioRequestedRoom = 0;

/// Unreferenced halfword retained as 0x2000; its role and units are unproven.
static u16 D_acropolis_fountain_80183BB2 = 8192;

/// Borrowed singleton task handle for the fountain movie and water-loop updater.
///
/// Published on every movie callback before the room-1 guard or work allocation.
/// Allocation failure and normal teardown leave the pointer set; the stop
/// descriptor requires a still-live published task and kills it without clearing
/// the handle. The task system owns its lifetime and releases its work on kill.
static Task* _gAcropolisFountainWaterMovieTask = NULL;

/// Sets the fountain spray quad's texture region and additive grayscale modulation.
///
/// `quad` borrows a writable `POLY_FT4` initialized by `setPolyFT4`. Its screen
/// corners and ordering-table link are filled by the caller; no pointer is retained.
/// The 40-by-40-texel region uses 4-bit indices and the palette at VRAM (32, 270).
/// `modulation` is an unsigned-byte RGB multiplier, with 128 neutral; the spray
/// drawer supplies 64 or 80 on alternating frames.
static inline void _acropolisFountainInitSprayQuad(POLY_FT4* quad, u8 modulation)
{
    enum {
        ACROPOLIS_FOUNTAIN_SPRAY_TPAGE   = getTPage(0, GPU_BLEND_ADD, 704, 0), // 4-bit page; VRAM X is in words, Y in scanlines
        ACROPOLIS_FOUNTAIN_SPRAY_CLUT    = getClut(32, 270),
        ACROPOLIS_FOUNTAIN_SPRAY_U_FIRST = 0x50,
        ACROPOLIS_FOUNTAIN_SPRAY_U_LAST  = 0x77,
        ACROPOLIS_FOUNTAIN_SPRAY_V_FIRST = 0,
        ACROPOLIS_FOUNTAIN_SPRAY_V_LAST  = 0x27
    };

    quad->tpage = ACROPOLIS_FOUNTAIN_SPRAY_TPAGE;
    quad->clut  = ACROPOLIS_FOUNTAIN_SPRAY_CLUT;
    setUV4(quad, ACROPOLIS_FOUNTAIN_SPRAY_U_FIRST, ACROPOLIS_FOUNTAIN_SPRAY_V_FIRST,
           ACROPOLIS_FOUNTAIN_SPRAY_U_LAST, ACROPOLIS_FOUNTAIN_SPRAY_V_FIRST,
           ACROPOLIS_FOUNTAIN_SPRAY_U_FIRST, ACROPOLIS_FOUNTAIN_SPRAY_V_LAST,
           ACROPOLIS_FOUNTAIN_SPRAY_U_LAST, ACROPOLIS_FOUNTAIN_SPRAY_V_LAST);
    setRGB0(quad, modulation, modulation, modulation);
    setSemiTrans(quad, true);
}

void acropolisFountainEnableClimbTrigger(void)
{
    enum { ACROPOLIS_FOUNTAIN_INSPECTION_TRIGGER = 5 };

    worldCollisionUnlinkTrigger(WORLD_COLLISION_TRIGGER_LIST_ACTION, &_gAcropolisFountainActionTriggers[ACROPOLIS_FOUNTAIN_INSPECTION_TRIGGER]);
    _gAcropolisFountainClimbTrigger.coord = &gGfxViewCoord;
    worldCollisionLinkTrigger(WORLD_COLLISION_TRIGGER_LIST_ACTION, &_gAcropolisFountainClimbTrigger);
    _gAcropolisFountainClimbTrigger.flags |= WORLD_COLLISION_TRIGGER_ENABLED;
}

void acropolisFountainStartClimb(s32 unusedActionId, s32 unusedControl)
{
    enum { ACROPOLIS_FOUNTAIN_CLIMB_TASK_BANK = 2,
           ACROPOLIS_FOUNTAIN_CLIMB_TASK_SLOT = 14 };

    taskSpawn(ACROPOLIS_FOUNTAIN_CLIMB_TASK_BANK, ACROPOLIS_FOUNTAIN_CLIMB_TASK_SLOT, 0, 0);
}

/// Takes scripted control and turns the player to face the ascent at yaw 2048.
static void _acropolisFountainClimbTurnPlayer(Task* task)
{
    ActorTransform facing;
    Task*          player;

    player        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    facing.rot.vx = 0;
    facing.rot.vy = ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    facing.rot.vz = 0;
    TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &facing, 0);
    task->state = task->state + 1;
}

/// Advances to the climb once the player's scripted turn has finished.
static void _acropolisFountainClimbWaitForTurn(Task* task)
{
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        task->state = task->state + 1;
    }
}

/// Starts the player's one-step ascent along its current facing direction.
static void _acropolisFountainClimbStep(Task* task)
{
    enum { ACROPOLIS_FOUNTAIN_CLIMB_ASCEND = 0 };

    GameActorStairClimb climb;
    Task*               player;

    player          = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    climb.descend   = ACROPOLIS_FOUNTAIN_CLIMB_ASCEND;
    climb.stepCount = 1;
    TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_CLIMB_STAIRS, &climb, 0);
    task->state = task->state + 1;
}

/// Advances to the destination walk once the player's ascent has finished.
static void _acropolisFountainClimbWaitForStep(Task* task)
{
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        task->state = task->state + 1;
    }
}

/// Walks the player to world position (2599, -200, -6054) after the ascent.
static void _acropolisFountainClimbMovePlayer(Task* task)
{
    ActorTransform destination;
    Task*          player;

    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    player             = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    destination.pos.vx = 2599;
    destination.pos.vy = -200;
    destination.pos.vz = -6054;
    TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_MOVE_TO, &destination, 0);
    task->state = task->state + 1;
}

/// Waits for arrival, restores player control and releases the climb task.
static void _acropolisFountainClimbFinish(Task* task)
{
    Task* player;

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (taskMessageDispatch(player, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        taskKill(task);
    }
}

void acropolisFountainClimbTask(Task* task)
{
    TaskFunc stateHandlers[ACROPOLIS_FOUNTAIN_CLIMB_STATE_COUNT] = {
        [ACROPOLIS_FOUNTAIN_CLIMB_TURN]          = _acropolisFountainClimbTurnPlayer,
        [ACROPOLIS_FOUNTAIN_CLIMB_WAIT_FOR_TURN] = _acropolisFountainClimbWaitForTurn,
        [ACROPOLIS_FOUNTAIN_CLIMB_STEP]          = _acropolisFountainClimbStep,
        [ACROPOLIS_FOUNTAIN_CLIMB_WAIT_FOR_STEP] = _acropolisFountainClimbWaitForStep,
        [ACROPOLIS_FOUNTAIN_CLIMB_MOVE]          = _acropolisFountainClimbMovePlayer,
        [ACROPOLIS_FOUNTAIN_CLIMB_FINISH]        = _acropolisFountainClimbFinish,
    };

    stateHandlers[task->state](task);
}

void acropolisFountainSprayTask(Task* task)
{
    enum {
        ACROPOLIS_FOUNTAIN_SPRAY_VISIBLE_VIEWS    = (1 << 6) | (1 << 7) | (1 << 14) | (1 << 20),
        ACROPOLIS_FOUNTAIN_SPRAY_MIN_DEPTH        = 0x11,   // GTE SZ3 / 4
        ACROPOLIS_FOUNTAIN_SPRAY_SIZE_DEPTH       = 0x4E00, // Pixel half-extent multiplied by depth
        ACROPOLIS_FOUNTAIN_SPRAY_DIM              = 0x40,   // Texture modulation on even frames
        ACROPOLIS_FOUNTAIN_SPRAY_BRIGHT_INCREMENT = 0x10
    };

    RoomGlowSpriteScratch* projection;
    GfxCoord*              coord;
    POLY_FT4*              sprite;
    s16                    edgeX;
    s16                    edgeY;
    s32                    brightness;

    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN && ((ACROPOLIS_FOUNTAIN_SPRAY_VISIBLE_VIEWS >> (gGameSession->location.view - 1)) & 1)) {
        // Project the effect's world centre; the GTE input keeps its signed low halfwords.
        actorRenderComposeCoord(coord);
        projection              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
        projection->worldPos.vx = coord->workm.t[0];
        projection->worldPos.vy = coord->workm.t[1];
        projection->worldPos.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projection->worldPos);
        gte_rtps();
        // Even a depth-rejected centre consumes one packet from the frame arena.
        sprite         = gGpuPrimCursor;
        gGpuPrimCursor = sprite + 1;
        setPolyFT4(sprite);
        gte_stsxy(&projection->screenPos);
        gte_stszotz(&projection->otz);
        if (projection->otz >= ACROPOLIS_FOUNTAIN_SPRAY_MIN_DEPTH) {
            brightness = (((u8)gDisplayState.animFrame & 1) * ACROPOLIS_FOUNTAIN_SPRAY_BRIGHT_INCREMENT) + ACROPOLIS_FOUNTAIN_SPRAY_DIM;
            _acropolisFountainInitSprayQuad(sprite, brightness);
            projection->halfExtent = ACROPOLIS_FOUNTAIN_SPRAY_SIZE_DEPTH / projection->otz;
            edgeX                  = projection->screenPos.vx - projection->halfExtent;
            sprite->x2             = edgeX;
            sprite->x0             = edgeX;
            edgeX                  = projection->screenPos.vx + projection->halfExtent;
            sprite->x3             = edgeX;
            sprite->x1             = edgeX;
            edgeY                  = projection->screenPos.vy - projection->halfExtent;
            sprite->y1             = edgeY;
            sprite->y0             = edgeY;
            edgeY                  = projection->screenPos.vy + projection->halfExtent;
            sprite->y3             = edgeY;
            sprite->y2             = edgeY;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->otz
                                                               << gDisplayState.otDepthShift) >>
                                                              2) &
                                                             GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    sprite);
        }
        SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    }
}

void acropolisFountainViewEffectsTask(Task* task)
{
    enum {
        ACROPOLIS_FOUNTAIN_VIEW_EFFECTS_INIT        = 0,
        ACROPOLIS_FOUNTAIN_VIEW_EFFECTS_START_MOVIE = 1,
        ACROPOLIS_FOUNTAIN_VIEW_EFFECTS_WATCH_VIEW  = 2,
        ACROPOLIS_FOUNTAIN_WATER_MOVIE_START_ENTRY  = 0,
        ACROPOLIS_FOUNTAIN_WATER_MOVIE_STOP_ENTRY   = 1,
        ACROPOLIS_FOUNTAIN_WATER_MOVIE_MAPPED_VIEWS = (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) |
                                                      (1 << 5) | (1 << 6) | (1 << 7) | (1 << 16)
    };
    EffectWork* effectWork;
    GfxCoord*   effectCoord;
    s32         mappedViewIndex;
    s32         latchedViewBit;
    s16         latchedViewIndex;

    effectCoord     = task->extra.coordBody->coord;
    effectWork      = task->spawnArg2.pointer;
    mappedViewIndex = viewGetMappedIndex();
    switch (task->state) {
        case ACROPOLIS_FOUNTAIN_VIEW_EFFECTS_INIT:
            // The shared effect work's scale halfword retains the mapped view.
            effectSpawn(EFFECT_ACROPOLIS_FOUNTAIN_SPRAY, effectCoord, 0, &_gAcropolisFountainSprayOffset);
            effectWork->scale = mappedViewIndex & 0xFF;
            task->state       = ACROPOLIS_FOUNTAIN_VIEW_EFFECTS_START_MOVIE;
            /* fallthrough */
        case ACROPOLIS_FOUNTAIN_VIEW_EFFECTS_START_MOVIE:
            latchedViewBit = 1 << (effectWork->scale - 1);
            if (latchedViewBit & ACROPOLIS_FOUNTAIN_WATER_MOVIE_MAPPED_VIEWS) {
                taskSpawnFromTable(_gAcropolisFountainWaterMovieTaskDescriptors, ACROPOLIS_FOUNTAIN_WATER_MOVIE_START_ENTRY, 0, 0);
            }
            task->state = ACROPOLIS_FOUNTAIN_VIEW_EFFECTS_WATCH_VIEW;
            break;
        case ACROPOLIS_FOUNTAIN_VIEW_EFFECTS_WATCH_VIEW:
            latchedViewIndex = effectWork->scale;
            latchedViewBit   = 1 << (latchedViewIndex - 1);
            if (latchedViewIndex != (mappedViewIndex & 0xFF)) {
                // Retire the old view's movie before starting the new one next update.
                if (latchedViewBit & ACROPOLIS_FOUNTAIN_WATER_MOVIE_MAPPED_VIEWS) {
                    taskSpawnFromTable(_gAcropolisFountainWaterMovieTaskDescriptors, ACROPOLIS_FOUNTAIN_WATER_MOVIE_STOP_ENTRY, 0, 0);
                }
                effectWork->scale = (u8)mappedViewIndex;
                task->state       = ACROPOLIS_FOUNTAIN_VIEW_EFFECTS_START_MOVIE;
            }
            break;
    }
}

/// Starts the water loop or changes the mix of its previously requested instance.
///
/// The caller supplies a pan offset in three-SPU-step units and nonnegative
/// attenuation in 0..127 (zero keeps the script's base gain). Only their low
/// signed bytes reach the sound queue. The requested-running latch selects
/// start or mix and is left unchanged here; it does not prove playback began.
/// Requires the room sound bank to remain loaded through queued playback.
static inline void _acropolisFountainRequestWaterLoopMix(s32 panOffset, s32 attenuation)
{
    if (_gAcropolisFountainWaterLoopRequested != 0) {
        sndEvtRequestScriptMix(SOUND_ACROPOLIS_FOUNTAIN_WATER_LOOP, panOffset, attenuation);
    } else {
        sndEvtRequestScriptStart(SOUND_ACROPOLIS_FOUNTAIN_WATER_LOOP, panOffset, attenuation);
    }
}

/// Synchronizes the water sound's playback window and mix with the fountain movie.
///
/// Borrows initialized water-loop work. Movie frames 15..239 accept a command;
/// frame 240 onward fades a requested loop over 20 audio updates. After a new
/// window is accepted, one deferred tick precedes applying the current logical
/// view index's mix. Indices 2..8 select a mix; other indices still set the
/// requested-running latch without sending sound. Requests can be dropped by
/// the sound queue. The movie queue and room sound data must remain live.
static void _acropolisFountainUpdateWaterLoop(Task* task, u16 logicalViewIndex)
{
    _AcropolisFountainWaterLoop* waterLoop;
    CdCmdQueue*                  queue;
    u16                          frame;

    queue     = &gCdCmdQueue;
    waterLoop = task->work;
    switch (waterLoop->state) {
        case ACROPOLIS_FOUNTAIN_WATER_LOOP_WAIT:
            // Outside the window, fade a running loop out. Inside it, command
            // the loop the first time, and again only after that fade. A new
            // task has not seen the window, so a camera cut re-pans a loop
            // the previous task left running.
            frame = queue->movieFrame;
            if (frame >= ACROPOLIS_FOUNTAIN_WATER_LOOP_FRAME_FADE) {
                if (_gAcropolisFountainWaterLoopRequested != 0) {
                    sndEvtRequestScriptStop(SOUND_ACROPOLIS_FOUNTAIN_WATER_LOOP, ACROPOLIS_FOUNTAIN_WATER_LOOP_FADE_TICKS);
                    _gAcropolisFountainWaterLoopRequested = 0;
                }
            } else if ((u16)(frame - ACROPOLIS_FOUNTAIN_WATER_LOOP_FRAME_FIRST) < ACROPOLIS_FOUNTAIN_WATER_LOOP_WINDOW) {
                if (waterLoop->seenWindow == 0 || _gAcropolisFountainWaterLoopRequested == 0) {
                    waterLoop->seenWindow = 1;
                    waterLoop->state      = waterLoop->state + 1;
                }
            }
            break;

        case ACROPOLIS_FOUNTAIN_WATER_LOOP_DEFER:
            // The command waits one tick after the window accepts it.
            waterLoop->state = ACROPOLIS_FOUNTAIN_WATER_LOOP_APPLY;
            break;

        case ACROPOLIS_FOUNTAIN_WATER_LOOP_APPLY:
            // Mark every index requested, including indices without a mix.
            switch (logicalViewIndex) {
                case 2:
                case 3:
                    _acropolisFountainRequestWaterLoopMix(7, 2);
                    break;
                case 4:
                    _acropolisFountainRequestWaterLoopMix(0, 0);
                    break;
                case 5:
                    _acropolisFountainRequestWaterLoopMix(-7, 2);
                    break;
                case 6:
                    _acropolisFountainRequestWaterLoopMix(-7, 0);
                    break;
                case 7:
                    _acropolisFountainRequestWaterLoopMix(0, 2);
                    break;
                case 8:
                    _acropolisFountainRequestWaterLoopMix(7, 3);
                    break;
            }
            _gAcropolisFountainWaterLoopRequested = 1;
            waterLoop->state                      = ACROPOLIS_FOUNTAIN_WATER_LOOP_WAIT;
            break;
    }
}

/// Controls the fountain water sound and presents or advances its texture movie.
///
/// Publishes this task on every call and acts only in room 1. State 0 owns a
/// zeroed primary-heap water-loop block until task teardown; failure kills the
/// task without clearing the published pointer. State 1 updates the sound,
/// draws an opaque raw 15-bit sprite in logical indices 3/5, and advances the
/// shared encoded-frame clock on odd animation ticks in indices 2/8. That clock
/// wraps to frame 1 ten frames before the selected movie's stop limit.
/// Requires live view mappings, movie stream slots and room sound data, and
/// space for SPRT/DR_TPAGE packets until GPU completion. Other indices only
/// update sound; presentation suppression affects drawing rather than sound.
static void _acropolisFountainWaterMovieTask(Task* task)
{
    enum {
        ACROPOLIS_FOUNTAIN_MOVIE_INIT             = 0,
        ACROPOLIS_FOUNTAIN_MOVIE_UPDATE           = 1,
        ACROPOLIS_FOUNTAIN_MOVIE_TEXTURE_DEPTH    = 2,
        ACROPOLIS_FOUNTAIN_MOVIE_VIEW3_OT_SLOT    = 35,
        ACROPOLIS_FOUNTAIN_MOVIE_VIEW5_OT_SLOT    = 29,
        ACROPOLIS_FOUNTAIN_MOVIE_LOOP_TAIL_FRAMES = 10,
        ACROPOLIS_FOUNTAIN_MOVIE_FIRST_FRAME      = 1,
    };
    CdCmdQueue*     movieQueue;
    CdCmdQueue*     loopQueue;
    StreamSlot*     movieTexture;
    StreamSlot*     loopMovie;
    SPRT*           movieSprite;
    DR_TPAGE*       pageCommand;
    GameLocationKey textureKey;
    GameLocationKey loopKey;
    s16             logicalViewIndex;
    s32             orderingTableSlot;
    u16             nextFrame;

    _gAcropolisFountainWaterMovieTask = task;
    movieQueue                        = &gCdCmdQueue;
    if (gGameSession->location.roomState != 1) {
        return;
    }
    switch (task->state) {
        case ACROPOLIS_FOUNTAIN_MOVIE_INIT:
            task->work = memCalloc(sizeof(_AcropolisFountainWaterLoop), 0);
            if (task->work == NULL) {
                taskKill(task);
                return;
            }
            memFillBytes(task->work, 0, sizeof(_AcropolisFountainWaterLoop));
            task->state = task->state + 1;
            break;

        case ACROPOLIS_FOUNTAIN_MOVIE_UPDATE:
            logicalViewIndex = viewFindLogicalIndex(gGameSession->location.view);
            _acropolisFountainUpdateWaterLoop(task, logicalViewIndex);
            switch ((u16)logicalViewIndex) {
                case 3:
                case 5:
                    if (movieQueue->suppressMoviePresentation != 0) {
                        return;
                    }
                    gGameSession->field_4E = 1;
                    textureKey             = gGameSession->location;
                    textureKey.view        = logicalViewIndex;
                    movieTexture           = streamGetSlot(streamFindViewMovieSlot(&textureKey));

                    movieSprite    = gGpuPrimCursor;
                    gGpuPrimCursor = movieSprite + 1;
                    // Raw texture ignores modulation RGB and needs no CLUT at 15-bit depth.
                    setSprt(movieSprite);
                    setShadeTex(movieSprite, true);
                    movieSprite->u0 = 0;
                    movieSprite->v0 = 0;
                    if ((u16)logicalViewIndex == 3) {
                        orderingTableSlot = ACROPOLIS_FOUNTAIN_MOVIE_VIEW3_OT_SLOT;
                        movieSprite->x0   = 0x70;
                        movieSprite->y0   = 8;
                    } else if ((u16)logicalViewIndex == 5) {
                        orderingTableSlot = ACROPOLIS_FOUNTAIN_MOVIE_VIEW5_OT_SLOT;
                        movieSprite->x0   = -0xA0;
                        movieSprite->y0   = 0x2A;
                    }
                    pageCommand    = gGpuPrimCursor;
                    movieSprite->w = movieTexture->data.movie.width;
                    movieSprite->h = movieTexture->data.movie.height;
                    addPrim(&gGpuCurrentOt[orderingTableSlot], movieSprite);

                    gGpuPrimCursor = pageCommand + 1;
                    setDrawTPage(pageCommand, false, false,
                                 getTPage(ACROPOLIS_FOUNTAIN_MOVIE_TEXTURE_DEPTH, 0,
                                          (u32)movieTexture->data.movie.vramX,
                                          (u32)movieTexture->data.movie.vramY));
                    addPrim(&gGpuCurrentOt[orderingTableSlot], pageCommand);
                    break;

                case 2:
                case 8:
                    // Views without a drawn movie still keep its sound clock looping.
                    if (gDisplayState.animFrame & 1) {
                        loopQueue             = &gCdCmdQueue;
                        loopKey               = gGameSession->location;
                        loopKey.view          = viewFindLogicalIndex(4);
                        loopMovie             = streamGetSlot(streamFindMovieSlot(&loopKey, 0, true));
                        nextFrame             = loopQueue->movieFrame + 1;
                        loopQueue->movieFrame = nextFrame;
                        if (nextFrame >= loopMovie->data.movie.frameLimit - ACROPOLIS_FOUNTAIN_MOVIE_LOOP_TAIL_FRAMES) {
                            loopQueue->movieFrame = ACROPOLIS_FOUNTAIN_MOVIE_FIRST_FRAME;
                        }
                    }
                    break;
            }
            break;
    }
}

/// Retires the fountain's movie/water-loop task and this one-shot stop task.
///
/// The saved movie-task pointer must still be live; it is not cleared here.
/// Task teardown releases the movie task's work, but does not stop its sound.
static void _acropolisFountainStopWaterMovieTask(Task* task)
{
    taskKill(_gAcropolisFountainWaterMovieTask);
    taskKill(task);
}
