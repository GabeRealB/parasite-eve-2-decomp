#include "stages/akropolis/rooms/fountain.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "fountain_private.h"

#include "gameplay/actor_render.h"
#include "gameplay/collision.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
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

#include "rooms/room_common.h"

#include "assets/rooms/akropolis/fountain/declarations.inc"

/// Phases of `_WaterLoop`.
enum {
    WATER_LOOP_WAIT  = 0,
    WATER_LOOP_DEFER = 1,
    WATER_LOOP_APPLY = 2
};

/// First encoded movie frame that admits a fountain-loop request (frame 15).
#define WATER_LOOP_FRAME_FIRST 0xF

/// First encoded movie frame that fades a requested fountain loop (frame 240).
#define WATER_LOOP_FRAME_FADE 0xF0

/// Exclusive unsigned-halfword bound on the movie frame minus its first frame.
#define WATER_LOOP_WINDOW (WATER_LOOP_FRAME_FADE - WATER_LOOP_FRAME_FIRST + 1)

/// Audio updates in the fountain loop's requested fade (20, independent of movie frames).
#define WATER_LOOP_FADE_TICKS 0x14

/// Task work for the fountain's waterfall loop.
typedef struct {
    u16 state;      // Phase (0 wait, 1 defer one tick, 2 start or re-pan)
    u16 seenWindow; // Nonzero once this task has observed the playing window
} _WaterLoop;
STATIC_ASSERT_SIZEOF(_WaterLoop, 4);

static void _akropolisFountainUpdateWaterLoop(Task* task, u16 logicalViewIndex);

/// Ordered phases of the player's single-step climb and subsequent walk.
enum {
    CLIMB_TURN = 0,
    AKROPOLIS_FOUNTAIN_CLIMB_WAIT_FOR_TURN,
    AKROPOLIS_FOUNTAIN_CLIMB_STEP,
    AKROPOLIS_FOUNTAIN_CLIMB_WAIT_FOR_STEP,
    AKROPOLIS_FOUNTAIN_CLIMB_MOVE,
    AKROPOLIS_FOUNTAIN_CLIMB_FINISH,
    AKROPOLIS_FOUNTAIN_CLIMB_STATE_COUNT
};

static void _akropolisFountainClimbTurnPlayer(Task* task);
static void _akropolisFountainClimbWaitForTurn(Task* task);
static void _akropolisFountainClimbStep(Task* task);
static void _akropolisFountainClimbWaitForStep(Task* task);
static void _akropolisFountainClimbMovePlayer(Task* task);
static void _akropolisFountainClimbFinish(Task* task);

static void _akropolisFountainWaterMovieTask(Task* task);
static void _akropolisFountainStopWaterMovieTask(Task* task);

#define ROOM_PART _gAkropolisFountainClimbTrigger
#include "assets/rooms/akropolis/fountain/action_triggers.inc"
#include "assets/rooms/akropolis/fountain/markers.inc"
/// Persistent water-loop request latch (0 no applied request, 1 request applied).
static s16 _gAkropolisFountainWaterLoopRequested = 0;

/// Bodyless water-movie recipes: slot 0 starts the updater, slot 1 stops it.
static TaskDesc _gAkropolisFountainWaterMovieTaskDescriptors[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisFountainWaterMovieTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisFountainStopWaterMovieTask, { .value = 0 } },
};

#include "assets/rooms/akropolis/fountain/header.inc"
#include "assets/rooms/akropolis/fountain/geometry.inc"
#include "assets/rooms/akropolis/fountain/view_boundaries.inc"
#define ROOM_PART _gAkropolisFountainActionTriggers
#include "assets/rooms/akropolis/fountain/action_triggers.inc"
#include "assets/rooms/akropolis/fountain/placements.inc"
#include "assets/rooms/akropolis/fountain/lights.inc"
#include "assets/rooms/akropolis/fountain/sprites.inc"
#include "assets/rooms/akropolis/fountain/cameras.inc"
#include "assets/rooms/akropolis/fountain/surfaces.inc"

u8 gAkropolisFountainPatioArrivalWarp = 0;

u8 gAkropolisFountainPatioRequestedRoom = 0;

/// Unreferenced halfword retained as 0x2000; its role and units are unproven.
static u16 _gAkropolisFountainUnusedHalfword = 8192;

/// Borrowed singleton task handle for the fountain movie and water-loop updater.
static Task* _gAkropolisFountainWaterMovieTask = NULL;

/// Sets the fountain spray quad's texture region and additive grayscale modulation.
static inline void _akropolisFountainInitSprayQuad(POLY_FT4* quad, u8 modulation)
{
    enum {
        SPRAY_TPAGE   = getTPage(0, GPU_BLEND_ADD, 704, 0), // 4-bit page; VRAM X is in words, Y in scanlines
        SPRAY_CLUT    = getClut(32, 270),
        SPRAY_U_FIRST = 0x50,
        SPRAY_U_LAST  = 0x77,
        SPRAY_V_FIRST = 0,
        SPRAY_V_LAST  = 0x27
    };

    quad->tpage = SPRAY_TPAGE;
    quad->clut  = SPRAY_CLUT;
    setUV4(quad, SPRAY_U_FIRST, SPRAY_V_FIRST,
           SPRAY_U_LAST, SPRAY_V_FIRST,
           SPRAY_U_FIRST, SPRAY_V_LAST,
           SPRAY_U_LAST, SPRAY_V_LAST);
    setRGB0(quad, modulation, modulation, modulation);
    setSemiTrans(quad, true);
}

void akropolisFountainEnableClimbTrigger(void)
{
    enum { INSPECTION_TRIGGER = 5 };

    worldCollisionUnlinkTrigger(WORLD_COLLISION_TRIGGER_LIST_ACTION, &_gAkropolisFountainActionTriggers[INSPECTION_TRIGGER]);
    _gAkropolisFountainClimbTrigger.coord = &gGfxViewCoord;
    worldCollisionLinkTrigger(WORLD_COLLISION_TRIGGER_LIST_ACTION, &_gAkropolisFountainClimbTrigger);
    _gAkropolisFountainClimbTrigger.flags |= WORLD_COLLISION_TRIGGER_ENABLED;
}

void akropolisFountainStartClimb(s32 unusedActionId, s32 unusedControl)
{
    enum { BANK = 2,
           SLOT = 14 };

    taskSpawn(BANK, SLOT, 0, 0);
}

/// Takes scripted control and turns the player to face the ascent at yaw 2048.
static void _akropolisFountainClimbTurnPlayer(Task* task)
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
static void _akropolisFountainClimbWaitForTurn(Task* task)
{
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        task->state = task->state + 1;
    }
}

/// Starts the player's one-step ascent along its current facing direction.
static void _akropolisFountainClimbStep(Task* task)
{
    enum { CLIMB_ASCEND = 0 };

    GameActorStairClimb climb;
    Task*               player;

    player          = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    climb.descend   = CLIMB_ASCEND;
    climb.stepCount = 1;
    TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_CLIMB_STAIRS, &climb, 0);
    task->state = task->state + 1;
}

/// Advances to the destination walk once the player's ascent has finished.
static void _akropolisFountainClimbWaitForStep(Task* task)
{
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        task->state = task->state + 1;
    }
}

/// Walks the player to world position (2599, -200, -6054) after the ascent.
static void _akropolisFountainClimbMovePlayer(Task* task)
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
static void _akropolisFountainClimbFinish(Task* task)
{
    Task* player;

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (taskMessageDispatch(player, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        taskKill(task);
    }
}

void akropolisFountainClimbTask(Task* task)
{
    TaskFunc stateHandlers[AKROPOLIS_FOUNTAIN_CLIMB_STATE_COUNT] = {
        [CLIMB_TURN]                             = _akropolisFountainClimbTurnPlayer,
        [AKROPOLIS_FOUNTAIN_CLIMB_WAIT_FOR_TURN] = _akropolisFountainClimbWaitForTurn,
        [AKROPOLIS_FOUNTAIN_CLIMB_STEP]          = _akropolisFountainClimbStep,
        [AKROPOLIS_FOUNTAIN_CLIMB_WAIT_FOR_STEP] = _akropolisFountainClimbWaitForStep,
        [AKROPOLIS_FOUNTAIN_CLIMB_MOVE]          = _akropolisFountainClimbMovePlayer,
        [AKROPOLIS_FOUNTAIN_CLIMB_FINISH]        = _akropolisFountainClimbFinish,
    };

    stateHandlers[task->state](task);
}

void akropolisFountainSprayTask(Task* task)
{
    enum {
        SPRAY_VISIBLE_VIEWS    = (1 << 6) | (1 << 7) | (1 << 14) | (1 << 20),
        SPRAY_MIN_DEPTH        = 0x11,   // GTE SZ3 / 4
        SPRAY_SIZE_DEPTH       = 0x4E00, // Pixel half-extent multiplied by depth
        SPRAY_DIM              = 0x40,   // Texture modulation on even frames
        SPRAY_BRIGHT_INCREMENT = 0x10
    };

    RoomGlowSpriteScratch* projection;
    GfxCoord*              coord;
    POLY_FT4*              sprite;
    s16                    edgeX;
    s16                    edgeY;
    s32                    brightness;

    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN && ((SPRAY_VISIBLE_VIEWS >> (gGameSession->location.view - 1)) & 1)) {
        // Project the effect's world center; the GTE input keeps its signed low halfwords.
        actorRenderComposeCoord(coord);
        projection              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
        projection->worldPos.vx = coord->workm.t[0];
        projection->worldPos.vy = coord->workm.t[1];
        projection->worldPos.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projection->worldPos);
        gte_rtps();
        // Even a depth-rejected center consumes one packet from the frame arena.
        sprite         = gGpuPrimCursor;
        gGpuPrimCursor = sprite + 1;
        setPolyFT4(sprite);
        gte_stsxy(&projection->screenPos);
        gte_stszotz(&projection->otz);
        if (projection->otz >= SPRAY_MIN_DEPTH) {
            brightness = (((u8)gDisplayState.animFrame & 1) * SPRAY_BRIGHT_INCREMENT) + SPRAY_DIM;
            _akropolisFountainInitSprayQuad(sprite, brightness);
            projection->halfExtent = SPRAY_SIZE_DEPTH / projection->otz;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->otz),
                    sprite);
        }
        SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    }
}

void akropolisFountainViewEffectsTask(Task* task)
{
    enum {
        VIEW_EFFECTS_INIT        = 0,
        VIEW_EFFECTS_START_MOVIE = 1,
        VIEW_EFFECTS_WATCH_VIEW  = 2,
        WATER_MOVIE_START_ENTRY  = 0,
        WATER_MOVIE_STOP_ENTRY   = 1,
        WATER_MOVIE_MAPPED_VIEWS = (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) |
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
        case VIEW_EFFECTS_INIT:
            // The shared effect work's scale halfword retains the mapped view.
            effectSpawn(EFFECT_AKROPOLIS_FOUNTAIN_SPRAY, effectCoord, 0, &_gAkropolisFountainSprayOffset);
            effectWork->scale = mappedViewIndex & 0xFF;
            task->state       = VIEW_EFFECTS_START_MOVIE;
            /* fallthrough */
        case VIEW_EFFECTS_START_MOVIE:
            latchedViewBit = 1 << (effectWork->scale - 1);
            if (latchedViewBit & WATER_MOVIE_MAPPED_VIEWS) {
                taskSpawnFromTable(_gAkropolisFountainWaterMovieTaskDescriptors, WATER_MOVIE_START_ENTRY, 0, 0);
            }
            task->state = VIEW_EFFECTS_WATCH_VIEW;
            break;
        case VIEW_EFFECTS_WATCH_VIEW:
            latchedViewIndex = effectWork->scale;
            latchedViewBit   = 1 << (latchedViewIndex - 1);
            if (latchedViewIndex != (mappedViewIndex & 0xFF)) {
                // Retire the old view's movie before starting the new one next update.
                if (latchedViewBit & WATER_MOVIE_MAPPED_VIEWS) {
                    taskSpawnFromTable(_gAkropolisFountainWaterMovieTaskDescriptors, WATER_MOVIE_STOP_ENTRY, 0, 0);
                }
                effectWork->scale = (u8)mappedViewIndex;
                task->state       = VIEW_EFFECTS_START_MOVIE;
            }
            break;
    }
}

/// Starts the water loop or changes the mix of its previously requested instance.
static inline void _akropolisFountainRequestWaterLoopMix(s32 panOffset, s32 attenuation)
{
    if (_gAkropolisFountainWaterLoopRequested != 0) {
        sndEvtRequestScriptMix(SOUND_AKROPOLIS_FOUNTAIN_WATER_LOOP, panOffset, attenuation);
    } else {
        sndEvtRequestScriptStart(SOUND_AKROPOLIS_FOUNTAIN_WATER_LOOP, panOffset, attenuation);
    }
}

/// Synchronizes the water sound's playback window and mix with the fountain movie.
static void _akropolisFountainUpdateWaterLoop(Task* task, u16 logicalViewIndex)
{
    _WaterLoop* waterLoop;
    CdCmdQueue* queue;
    u16         frame;

    queue     = &gCdCmdQueue;
    waterLoop = task->work;
    switch (waterLoop->state) {
        case WATER_LOOP_WAIT:
            // Outside the window, fade a running loop out. Inside it, command
            // the loop the first time, and again only after that fade. A new
            // task has not seen the window, so a camera cut re-pans a loop
            // the previous task left running.
            frame = queue->movieFrame;
            if (frame >= WATER_LOOP_FRAME_FADE) {
                if (_gAkropolisFountainWaterLoopRequested != 0) {
                    sndEvtRequestScriptStop(SOUND_AKROPOLIS_FOUNTAIN_WATER_LOOP, WATER_LOOP_FADE_TICKS);
                    _gAkropolisFountainWaterLoopRequested = 0;
                }
            } else if ((u16)(frame - WATER_LOOP_FRAME_FIRST) < WATER_LOOP_WINDOW) {
                if (waterLoop->seenWindow == 0 || _gAkropolisFountainWaterLoopRequested == 0) {
                    waterLoop->seenWindow = 1;
                    waterLoop->state      = waterLoop->state + 1;
                }
            }
            break;

        case WATER_LOOP_DEFER:
            // The command waits one tick after the window accepts it.
            waterLoop->state = WATER_LOOP_APPLY;
            break;

        case WATER_LOOP_APPLY:
            // Mark every index requested, including indices without a mix.
            switch (logicalViewIndex) {
                case 2:
                case 3:
                    _akropolisFountainRequestWaterLoopMix(7, 2);
                    break;
                case 4:
                    _akropolisFountainRequestWaterLoopMix(0, 0);
                    break;
                case 5:
                    _akropolisFountainRequestWaterLoopMix(-7, 2);
                    break;
                case 6:
                    _akropolisFountainRequestWaterLoopMix(-7, 0);
                    break;
                case 7:
                    _akropolisFountainRequestWaterLoopMix(0, 2);
                    break;
                case 8:
                    _akropolisFountainRequestWaterLoopMix(7, 3);
                    break;
            }
            _gAkropolisFountainWaterLoopRequested = 1;
            waterLoop->state                      = WATER_LOOP_WAIT;
            break;
    }
}

/// Controls the fountain water sound and presents or advances its texture movie.
static void _akropolisFountainWaterMovieTask(Task* task)
{
    enum {
        INIT             = 0,
        UPDATE           = 1,
        TEXTURE_DEPTH    = 2,
        VIEW3_OT_SLOT    = 35,
        VIEW5_OT_SLOT    = 29,
        LOOP_TAIL_FRAMES = 10,
        FIRST_FRAME      = 1,
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

    _gAkropolisFountainWaterMovieTask = task;
    movieQueue                        = &gCdCmdQueue;
    if (gGameSession->location.roomState != 1) {
        return;
    }
    switch (task->state) {
        case INIT:
            task->work = memCalloc(sizeof(_WaterLoop), 0);
            if (task->work == NULL) {
                taskKill(task);
                return;
            }
            memFillBytes(task->work, 0, sizeof(_WaterLoop));
            task->state = task->state + 1;
            break;

        case UPDATE:
            logicalViewIndex = viewFindLogicalIndex(gGameSession->location.view);
            _akropolisFountainUpdateWaterLoop(task, logicalViewIndex);
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
                        orderingTableSlot = VIEW3_OT_SLOT;
                        movieSprite->x0   = 0x70;
                        movieSprite->y0   = 8;
                    } else if ((u16)logicalViewIndex == 5) {
                        orderingTableSlot = VIEW5_OT_SLOT;
                        movieSprite->x0   = -0xA0;
                        movieSprite->y0   = 0x2A;
                    }
                    pageCommand    = gGpuPrimCursor;
                    movieSprite->w = movieTexture->data.movie.width;
                    movieSprite->h = movieTexture->data.movie.height;
                    addPrim(&gGpuCurrentOt[orderingTableSlot], movieSprite);

                    gGpuPrimCursor = pageCommand + 1;
                    setDrawTPage(pageCommand, false, false,
                                 getTPage(TEXTURE_DEPTH, 0,
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
                        if (nextFrame >= loopMovie->data.movie.frameLimit - LOOP_TAIL_FRAMES) {
                            loopQueue->movieFrame = FIRST_FRAME;
                        }
                    }
                    break;
            }
            break;
    }
}

/// Retires the fountain's movie/water-loop task and this one-shot stop task.
static void _akropolisFountainStopWaterMovieTask(Task* task)
{
    taskKill(_gAkropolisFountainWaterMovieTask);
    taskKill(task);
}
