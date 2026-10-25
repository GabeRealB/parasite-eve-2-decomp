#include "stages/akropolis/rooms/helicopter_landing_pad.h"

#include "../../room_ids.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/abs.h>

#include "common.h"
#include "gte.h"

#include "helicopter_landing_pad_private.h"

#include "stages/akropolis/actors/helicopter_departure.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/companion_load.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/random.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../../../shared/actor/actor_contacts.h"
#include "../../../../shared/actor/actor_messages.h"

#define ROOM_PART declarations_helicopter_landing_pad_effects
#include "assets/rooms/akropolis/helicopter_landing_pad/declarations.inc"

/// Returns this carrier's persistent last contact-push correction.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

/// Converts one local spark endpoint to signed 16-bit view coordinates in place.
#define TRANSFORM_SPARK_ENDPOINT(line, endpointIndex, coord)    \
    gte_SetRotMatrix(&(coord)->workm);                          \
    gte_ldv0(&(line)->endpoints[endpointIndex]);                \
    gte_rtv0();                                                 \
    gte_stsv(&(line)->endpoints[endpointIndex]);                \
    (line)->endpoints[endpointIndex].vx += (coord)->workm.t[0]; \
    (line)->endpoints[endpointIndex].vy += (coord)->workm.t[1]; \
    (line)->endpoints[endpointIndex].vz += (coord)->workm.t[2]

// The two sprite tasks use signed Q12 trig results and the same texture page.
enum {
    SPRITE_TEXTURE_PAGE       = 0x2B,
    SPRITE_TRIG_FRACTION_BITS = 12,
    SPRITE_RAW_TEXTURE_CODE   = 0x2D,
};

// Transient-light RGB uses twelve fractional bits; blue varies from 9/16 to 1.
enum {
    LIGHT_HALF_INTENSITY   = ONE / 2,
    LIGHT_BLUE_RANDOM_MASK = 7 * ONE / 16,
    LIGHT_BLUE_BASE        = 9 * ONE / 16,
};

static void _akropolisHelicopterLandingPadResolveExit(Task* task);
static void _akropolisHelicopterLandingPadTurnPlayerToExit(Task* task);
static void _akropolisHelicopterLandingPadDescendExitStairs(Task* task);
static void _akropolisHelicopterLandingPadWaitForPlayerTurn(Task* task);
static void _akropolisHelicopterLandingPadDrawPerimeterLight(const SVECTOR* worldPoint, s16 lightIndex, s32 brightness);
static void _akropolisHelicopterLandingPadDrawUpperSparkLine(GfxCoord* coord);

static void _akropolisHelicopterLandingPadMovieTask(Task* task);
static void _akropolisHelicopterLandingPadStartMovieTask(Task* task);

// Room-authored scene selectors, CAP slots and receiver-specific actor controls.
enum {
    DEPARTURE_CAP_SLOT = 3,
    ENCOUNTER_ACTOR    = 0,
    LIFT_CHILD         = 40,
    // The encounter receiver treats this authored value as nonzero/show.
};

TaskMessageEntry gAkropolisHelicopterLandingPadRoomMessages[5] = {
    { .messageId = ROOM_EVENT_MESSAGE_RESOLVE, .handler = akropolisHelicopterLandingPadResolveRoomTransition },
    { .messageId = ROOM_MESSAGE_USE_KEY_ITEM, .handler = akropolisHelicopterLandingPadRejectKeyItemUse },
    { .messageId = DIRECTION_MESSAGE_ROOM_ACTION, .handler = akropolisHelicopterLandingPadHandleRoomAction },
    { .messageId = ROOM_MESSAGE_COMMAND, .handler = akropolisHelicopterLandingPadHandleRoomCommand },
    { .messageId = TASK_MESSAGE_TABLE_END, .handler = NULL },
};

#define ROOM_PART _gAkropolisHelicopterLandingPadEncounterVibrationCommands
#include "assets/rooms/akropolis/helicopter_landing_pad/vibrations.inc"
#define ROOM_PART _gAkropolisHelicopterLandingPadSceneOriginPlacement
#include "assets/rooms/akropolis/helicopter_landing_pad/events.inc"

AkropolisHelicopterLandingPadSceneMarkStorage gAkropolisHelicopterLandingPadSceneMarkStorage = {
    .destination = { -3496, -2871, -1283 },
    .unknown_0C  = { 0, 0x3280000, 0, -360, -120, 480, 0, 0, 0, -3188, -2871, -694, 0, 0x9C40000, 0 },
};

#define ROOM_PART _gAkropolisHelicopterLandingPadEncounterEndingActorPlacement
#include "assets/rooms/akropolis/helicopter_landing_pad/events.inc"

/// Three words copied into the equipped-weapon bank's departure-scene extension.
static AnimationSet* _gAkropolisHelicopterLandingPadDeparturePlayerAnimationSets[3] = {
    NULL,
    &gAkropolisHelicopterLandingPadPlayerHelipadReportEscapeClip,
    &gAkropolisHelicopterLandingPadPlayerHelipadDetonatorTalkClip,
};

#define ROOM_PART _gAkropolisHelicopterLandingPadDeparturePlayerBankCopyRequest
#include "assets/rooms/akropolis/helicopter_landing_pad/events.inc"

s32 gAkropolisHelicopterLandingPadEncounterPhase = AKROPOLIS_HELICOPTER_LANDING_PAD_PHASE_WAITING;

enum {
    PITCH_TASK_PRIORITY = 97,
};

TaskDesc gAkropolisHelicopterLandingPadScriptTasks[AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_COUNT + 1] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisHelicopterLandingPadPrepareDepartureTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisHelicopterLandingPadMovePlayerToSceneMarkTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisHelicopterLandingPadScreenShakeTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisHelicopterLandingPadDepartureShakeTimelineTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisHelicopterLandingPadConfirmDepartureTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisHelicopterLandingPadReturnToMistTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, akropolisHelicopterLandingPadTurnPlayerYawTask, { .value = 0 } },
    { { { TASK_BODY_NONE, PITCH_TASK_PRIORITY } }, akropolisHelicopterLandingPadPlayerPitchPulseTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gAkropolisHelicopterLandingPadEncounterReady = 0;

/// Unreferenced; their role is unproven.
static s32 _gAkropolisHelicopterLandingPadUnusedWords[6] = {
    0,
    1,
    0,
    0,
    0,
    0,
};

#define ROOM_PART gAkropolisHelicopterLandingPadLiftRaiseRequest
#include "assets/rooms/akropolis/helicopter_landing_pad/events.inc"

TaskDesc gAkropolisHelicopterLandingPadMovieTasks[2] = {
    { .header = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_CONTROL } }, .callback = _akropolisHelicopterLandingPadStartMovieTask, .data = { .value = 0 } },
    { .header = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_CONTROL } }, .callback = _akropolisHelicopterLandingPadMovieTask, .data = { .value = 0 } },
};
#include "assets/rooms/akropolis/helicopter_landing_pad/markers.inc"
#include "assets/rooms/akropolis/helicopter_landing_pad/view_masks.inc"
#include "assets/rooms/akropolis/helicopter_landing_pad/header.inc"
#include "assets/rooms/akropolis/helicopter_landing_pad/geometry.inc"
#include "assets/rooms/akropolis/helicopter_landing_pad/view_boundaries.inc"

/// Plays the room's view-100 movie, permits Start cancellation and restores gameplay.
static void _akropolisHelicopterLandingPadMovieTask(Task* task)
{
    enum {
        PREPARE    = 0,
        QUEUE      = 1,
        WAIT_READY = 2,
        PLAY       = 3,
        WAIT_IDLE  = 4,
        RESTORE    = 5,
        VIEW       = 100,
    };
    u8              movieArgs[sizeof(gCdCmdQueue.entries[0].args.bytes)];
    GameLocationKey movieLocation;
    s16             movieSlot;
    CdCmdQueue*     cdQueue;

    cdQueue = &gCdCmdQueue;
    switch (task->state) {
        case PREPARE:
            SetDispMask(0);
            streamPrepareMovieWorkspace(1);
            task->state++;
            break;
        case QUEUE:
            movieLocation      = gGameSession->location;
            movieLocation.view = VIEW;
            movieSlot          = streamFindMovieSlot(&movieLocation, 0, 0);
            // The queue copies four bytes; playback uses only the slot byte.
            movieArgs[0] = movieSlot;
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, NULL, movieArgs);
            task->state++;
            break;
        case WAIT_READY:
            if (cdQueue->movieReady != 0) {
                SetDispMask(1);
                task->state++;
                break;
            }
            break;
        case PLAY:
            if (cdCmdIsIdle()) {
                SetDispMask(0);
                task->state++;
                break;
            }
            if (padIsStartPressed() != 0) {
                SetDispMask(0);
                cdCmdRequestCancel();
                task->state++;
                break;
            }
            break;
        case WAIT_IDLE:
            if (cdCmdIsIdle()) {
                streamResetGameRestore();
                task->state++;
                break;
            }
            break;
        case RESTORE:
            if (streamPollGameRestore(0, 0)) {
                memFillBytes(gFsImageWorkspacePtr, 0, sizeof(*gFsImageWorkspacePtr));
                SetDispMask(1);
                taskKill(task);
                displayResumeGameLoop();
            }
            break;
    }
}

/// Hands display presentation to the movie task and retires the requesting room task.
static void _akropolisHelicopterLandingPadStartMovieTask(Task* task)
{
    enum { MOVIE_TASK_ENTRY = 1 };

    displaySpawnTaskFromTable(gAkropolisHelicopterLandingPadMovieTasks, MOVIE_TASK_ENTRY, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
    taskKill(task);
}

/// Resolves the fire-escape exit before the player turns and descends the stairs.
static void _akropolisHelicopterLandingPadResolveExit(Task* task)
{
    enum {
        ROOM = 1,
        WARP = 3,
    };
    Task* roomTask = gameGetTaskSlot(GAME_TASK_SLOT_ROOM);

    gAkropolisHelicopterLandingPadExitRequest.field_4   = 1;
    gAkropolisHelicopterLandingPadExitRequest.roomState = ROOM;
    gAkropolisHelicopterLandingPadExitRequest.areaId    = AKROPOLIS_ROOM_ID_FIRE_ESCAPE;
    gAkropolisHelicopterLandingPadExitRequest.warp      = WARP;
    gAkropolisHelicopterLandingPadExitRequest.queryOnly = ROOM_EVENT_EXECUTE;
    if (TASK_MESSAGE_DISPATCH_POINTERS(roomTask, ROOM_EVENT_MESSAGE_RESOLVE, &gAkropolisHelicopterLandingPadExitRequest,
                                       &gAkropolisHelicopterLandingPadExitRequest) != 0) {
        task->state += 1;
    } else {
        taskKill(task);
    }
}

/// Starts the player's scripted turn toward the exit stairs, then advances the task.
static void _akropolisHelicopterLandingPadTurnPlayerToExit(Task* task)
{
    enum { EXIT_YAW = 0 };

    ActorTransform targetTransform;
    Task*          playerTask;

    playerTask             = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    targetTransform.rot.vx = 0;
    targetTransform.rot.vy = EXIT_YAW;
    targetTransform.rot.vz = 0;
    TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &targetTransform, 0);
    task->state = task->state + 1;
}

/// Holds the exit sequence until the player's scripted turn has finished.
static void _akropolisHelicopterLandingPadWaitForPlayerTurn(Task* task)
{
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        task->state = task->state + 1;
    }
}

/// Starts the player's three-step descent from the landing pad, then advances.
static void _akropolisHelicopterLandingPadDescendExitStairs(Task* task)
{
    enum { EXIT_STAIR_STEPS = 3 };

    GameActorStairClimb climb;
    Task*               playerTask;

    playerTask      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    climb.descend   = true;
    climb.stepCount = EXIT_STAIR_STEPS;
    TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_CLIMB_STAIRS, &climb, 0);
    task->state = task->state + 1;
}

/// Commits the resolved fire-escape destination once the player's descent finishes.
static void _akropolisHelicopterLandingPadCommitExitTask(Task* task)
{
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = (u8)gAkropolisHelicopterLandingPadExitRequest.areaId;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = gAkropolisHelicopterLandingPadExitRequest.warp;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = gAkropolisHelicopterLandingPadExitRequest.roomState;
        taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
        taskKill(task);
    }
}

void akropolisHelicopterLandingPadStartExit(s32 unusedArgument, s32 unusedActionId)
{
    enum { BANK = 2,
           SLOT = 15 };

    taskSpawn(BANK, SLOT, 0, 0);
}

void akropolisHelicopterLandingPadExitTask(Task* task)
{
    enum { EXIT_STAIR_SURFACE = 2 };

    GameActor* playerActor     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    TaskFunc   stateHandlers[] = {
        _akropolisHelicopterLandingPadResolveExit,
        _akropolisHelicopterLandingPadTurnPlayerToExit,
        _akropolisHelicopterLandingPadWaitForPlayerTurn,
        _akropolisHelicopterLandingPadDescendExitStairs,
        _akropolisHelicopterLandingPadCommitExitTask,
    };

    // Scripted stairs bypass collision-based surface selection.
    playerActor->surfaceClass = EXIT_STAIR_SURFACE;
    stateHandlers[task->state](task);
}

/// Sets a Gouraud glow wedge's packet header and red-to-black vertex colors.
static inline void _akropolisHelicopterLandingPadInitGlowWedge(POLY_G4* wedge, u8 centerRed)
{
    setPolyG4(wedge);
    setRGB0(wedge, 0, 0, 0);
    setRGB1(wedge, 0, 0, 0);
    setRGB2(wedge, centerRed, 0, 0);
    setRGB3(wedge, 0, 0, 0);
}

/// Draws a pulsing red perimeter glow and refreshes its shared transient light.
static void _akropolisHelicopterLandingPadDrawPerimeterLight(const SVECTOR* worldPoint, s16 lightIndex, s32 brightness)
{
    enum {
        FIRST_LIGHT_SLOT     = 6,
        LIGHT_FRAMES         = 2,
        LIGHT_INNER_DISTANCE = 1600,
        LIGHT_OUTER_DISTANCE = 12800,
        LIGHT_COLOR_SCALE    = ONE / 256,
        OUTER_RADIUS_SCALE   = 0xC000,
        INNER_RADIUS_SCALE   = 0x1800,
        TRIG_FRACTION_BITS   = 12,
        FULL_TURN            = 0x1000,
        WEDGE_ANGLE          = 0x200,
        QUARTER_TURN         = 0x400,
        HALF_TURN            = 0x800,
    };

    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          pointLight;
    GlowCentreRadiiScratch*        block;
    POLY_G4*                       wedge;
    s32                            angle;
    s32                            halfStepAngle;
    s32                            nextAngle;
    s32                            armEdgeAngle;
    s16                            wedgeBrightness;
    s32                            halfBrightness;
    s32                            visibleViewBit;

    wedgeBrightness = brightness;
    lightSlot       = &gWorldCoordTransientPointLights[FIRST_LIGHT_SLOT + (lightIndex & 1)];
    pointLight      = &lightSlot->light;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            lightSlot->framesLeft = WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE;
        }
    } else {
        visibleViewBit = _gAkropolisHelicopterLandingPadPerimeterLightViewMasks[lightIndex] & (1 << ((viewGetMappedIndex() & 0xFF) - 1));
        if (visibleViewBit == 0) {
            return;
        }
        // Project the authored world position with the current view.
        block = SCRATCH_STACK_RESERVE_BLOCK(GlowCentreRadiiScratch);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(worldPoint);
        gte_rtps();
        gte_stsxy(&block->sx);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            lightSlot->framesLeft                              = LIGHT_FRAMES;
            pointLight->inner                                  = LIGHT_INNER_DISTANCE;
            pointLight->outer                                  = LIGHT_OUTER_DISTANCE;
            pointLight->head.color.r                           = brightness * LIGHT_COLOR_SCALE;
            pointLight->head.color.g                           = 0;
            pointLight->head.color.b                           = 0;
            pointLight->head.transform.lighting.local.t[0]     = worldPoint->vx;
            pointLight->head.transform.lighting.local.t[1]     = worldPoint->vy;
            pointLight->head.transform.lighting.local.t[2]     = worldPoint->vz;
            lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            block->outerRadius                                 = OUTER_RADIUS_SCALE / block->otz;
            block->innerRadius                                 = INNER_RADIUS_SCALE / block->otz;

            // Layer eight dim outer wedges with eight brighter half-radius wedges.
            for (angle = 0; angle < FULL_TURN; angle += WEDGE_ANGLE) {
                wedge          = gGpuPrimCursor;
                gGpuPrimCursor = wedge + 1;
                setPolyG4(wedge);
                setRGB0(wedge, 0, 0, 0);
                setRGB1(wedge, 0, 0, 0);
                halfBrightness = wedgeBrightness >> 1;
                setRGB2(wedge, halfBrightness, 0, 0);
                setRGB3(wedge, 0, 0, 0);
                wedge->x0     = block->sx + ((block->outerRadius * rsin(angle)) >> TRIG_FRACTION_BITS);
                wedge->y0     = block->sy + ((block->outerRadius * rcos(angle)) >> TRIG_FRACTION_BITS);
                halfStepAngle = angle + (WEDGE_ANGLE / 2);
                wedge->x1     = block->sx + ((block->outerRadius * rsin(halfStepAngle)) >> TRIG_FRACTION_BITS);
                wedge->y1     = block->sy + ((block->outerRadius * rcos(halfStepAngle)) >> TRIG_FRACTION_BITS);
                wedge->x2     = block->sx;
                wedge->y2     = block->sy;
                nextAngle     = angle + WEDGE_ANGLE;
                wedge->x3     = block->sx + ((block->outerRadius * rsin(nextAngle)) >> TRIG_FRACTION_BITS);
                wedge->y3     = block->sy + ((block->outerRadius * rcos(nextAngle)) >> TRIG_FRACTION_BITS);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->otz),
                        wedge);
                gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, block->otz);

                wedge          = gGpuPrimCursor;
                gGpuPrimCursor = wedge + 1;
                _akropolisHelicopterLandingPadInitGlowWedge(wedge, wedgeBrightness);
                wedge->x0 = block->sx + ((block->outerRadius * rsin(angle)) >> (TRIG_FRACTION_BITS + 1));
                wedge->y0 = block->sy + ((block->outerRadius * rcos(angle)) >> (TRIG_FRACTION_BITS + 1));
                wedge->x1 = block->sx + ((block->outerRadius * rsin(halfStepAngle)) >> (TRIG_FRACTION_BITS + 1));
                wedge->y1 = block->sy + ((block->outerRadius * rcos(halfStepAngle)) >> (TRIG_FRACTION_BITS + 1));
                wedge->x2 = block->sx;
                wedge->y2 = block->sy;
                wedge->x3 = block->sx + ((block->outerRadius * rsin(nextAngle)) >> (TRIG_FRACTION_BITS + 1));
                wedge->y3 = block->sy + ((block->outerRadius * rcos(nextAngle)) >> (TRIG_FRACTION_BITS + 1));
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->otz),
                        wedge);
                gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, block->otz);
            }

            // Overlay four red blades; alternate tips reach twice the disc radius.
            wedgeBrightness = halfBrightness;
            for (angle = WEDGE_ANGLE; angle < FULL_TURN; angle += HALF_TURN) {
                armEdgeAngle   = angle - QUARTER_TURN;
                wedge          = gGpuPrimCursor;
                gGpuPrimCursor = wedge + 1;
                _akropolisHelicopterLandingPadInitGlowWedge(wedge, wedgeBrightness);
                wedge->x0    = block->sx + ((block->innerRadius * rsin(armEdgeAngle)) >> (TRIG_FRACTION_BITS + 1));
                wedge->y0    = block->sy + ((block->innerRadius * rcos(armEdgeAngle)) >> (TRIG_FRACTION_BITS + 1));
                wedge->x1    = block->sx + ((block->outerRadius * rsin(angle)) >> TRIG_FRACTION_BITS);
                wedge->y1    = block->sy + ((block->outerRadius * rcos(angle)) >> TRIG_FRACTION_BITS);
                wedge->x2    = block->sx;
                wedge->y2    = block->sy;
                armEdgeAngle = angle + QUARTER_TURN;
                wedge->x3    = block->sx + ((block->innerRadius * rsin(armEdgeAngle)) >> (TRIG_FRACTION_BITS + 1));
                wedge->y3    = block->sy + ((block->innerRadius * rcos(armEdgeAngle)) >> (TRIG_FRACTION_BITS + 1));
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->otz),
                        wedge);
                gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, block->otz);

                wedge          = gGpuPrimCursor;
                gGpuPrimCursor = wedge + 1;
                _akropolisHelicopterLandingPadInitGlowWedge(wedge, wedgeBrightness);
                wedge->x0    = block->sx + ((block->innerRadius * rsin(angle)) >> TRIG_FRACTION_BITS);
                wedge->y0    = block->sy + ((block->innerRadius * rcos(angle)) >> TRIG_FRACTION_BITS);
                wedge->x1    = block->sx + ((block->outerRadius * rsin(armEdgeAngle)) >> (TRIG_FRACTION_BITS - 1));
                wedge->y1    = block->sy + ((block->outerRadius * rcos(armEdgeAngle)) >> (TRIG_FRACTION_BITS - 1));
                wedge->x2    = block->sx;
                wedge->y2    = block->sy;
                armEdgeAngle = angle + HALF_TURN;
                wedge->x3    = block->sx + ((block->innerRadius * rsin(armEdgeAngle)) >> TRIG_FRACTION_BITS);
                wedge->y3    = block->sy + ((block->innerRadius * rcos(armEdgeAngle)) >> TRIG_FRACTION_BITS);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->otz),
                        wedge);
                gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, block->otz);
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(GlowCentreRadiiScratch);
    }
}

/// Projects a sprite's cached origin through `GsWSMATRIX`.
static inline void _akropolisHelicopterLandingPadProjectSpriteCentre(EffectShapeScratch* projection, const GfxCoord* coord)
{
    projection->worldPoint.vx = coord->workm.t[0];
    projection->worldPoint.vy = coord->workm.t[1];
    projection->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    gte_stsxy(&projection->screenX);
    gte_stflg(&projection->projectionFlags);
}

void akropolisHelicopterLandingPadEmberTask(Task* task)
{
    enum {
        INITIALIZE       = 0,
        FLICKER_VARIANT  = 1,
        CLUT             = 0x4383,
        CELL_PIXELS      = 32,
        TEXTURE_ROW      = 0x28,
        FRAMES_PER_CELL  = 6,
        BRIGHT_RISE_STEP = 24,
    };

    EffectWork*         work;
    GfxCoord*           coord;
    EffectShapeScratch* projection;
    POLY_FT4*           sprite;
    u32                 colorSample;
    s16                 nextAge;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }
    // Render the current pose before advancing its local drift and atlas frame.
    actorRenderComposeCoord(coord);
    if (task->state == INITIALIZE) {
        if (task->spawnArg1.value != 0) {
            work->move.vx = 0;
            work->move.vy = -BRIGHT_RISE_STEP;
            work->move.vz = 0;
            RANDOM_LCG_ADVANCE();
            work->scale = (RANDOM_LCG_DRAW() & 0xFF) + 0x300;
            RANDOM_LCG_ADVANCE();
            work->angle = RANDOM_LCG_DRAW() & ACTOR_TRANSFORM_ANGLE_MASK;
            RANDOM_LCG_ADVANCE();
            work->period = RANDOM_LCG_DRAW() & 0x3F;
            RANDOM_LCG_ADVANCE();
            work->step = (RANDOM_LCG_DRAW() & 3) + 2;
        } else {
            RANDOM_LCG_ADVANCE();
            work->scale = (RANDOM_LCG_DRAW() & 0xFF) + 0x100;
            RANDOM_LCG_ADVANCE();
            work->angle = RANDOM_LCG_DRAW() & ACTOR_TRANSFORM_ANGLE_MASK;
            RANDOM_LCG_ADVANCE();
            work->period = (RANDOM_LCG_DRAW() & 3) + 1;
            RANDOM_LCG_ADVANCE();
            work->step = (RANDOM_LCG_DRAW() & 3) + 1;
            RANDOM_LCG_ADVANCE();
            work->move.vx = (RANDOM_LCG_DRAW() & 7) - 4;
            RANDOM_LCG_ADVANCE();
            work->move.vy = ~(RANDOM_LCG_DRAW() & 0xF);
            RANDOM_LCG_ADVANCE();
            work->move.vz = (RANDOM_LCG_DRAW() & 7) - 4;
        }
        task->state++;
    }
    projection = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    _akropolisHelicopterLandingPadProjectSpriteCentre(projection, coord);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        sprite         = gGpuPrimCursor;
        gGpuPrimCursor = sprite + 1;
        setPolyFT4(sprite);
        if (task->spawnArg1.value == FLICKER_VARIANT) {
            RANDOM_LCG_ADVANCE();
            if ((RANDOM_LCG_DRAW() & 3) == 0) {
                RANDOM_LCG_ADVANCE();
                colorSample = RANDOM_LCG_DRAW() & 0xFF;
                setRGB0(sprite, colorSample >> 1, colorSample, 0xFF);
            } else {
                setShadeTex(sprite, true);
            }
            if (work->age < work->step * FRAMES_PER_CELL - 2) {
                RANDOM_LCG_ADVANCE();
                if ((RANDOM_LCG_DRAW() & 0xF) == 0 && gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    effectSpawn(EFFECT_FLASH_BURST, coord, 0x100, NULL);
                }
            }
        } else {
            sprite->code = SPRITE_RAW_TEXTURE_CODE;
        }
        sprite->tpage = SPRITE_TEXTURE_PAGE;
        sprite->clut  = CLUT;
        setSemiTrans(sprite, true);
        sprite->u0                  = (work->age / work->step + 1) * CELL_PIXELS;
        sprite->v0                  = TEXTURE_ROW;
        sprite->u1                  = (work->age / work->step + 1) * CELL_PIXELS + (CELL_PIXELS - 1);
        sprite->v1                  = TEXTURE_ROW;
        sprite->u2                  = (work->age / work->step + 1) * CELL_PIXELS;
        sprite->v2                  = TEXTURE_ROW + CELL_PIXELS - 1;
        sprite->u3                  = (work->age / work->step + 1) * CELL_PIXELS + (CELL_PIXELS - 1);
        sprite->v3                  = TEXTURE_ROW + CELL_PIXELS - 1;
        projection->extent.corner.x = ((work->scale * (CELL_PIXELS - 1) / projection->depth) * rsin(work->angle)) >> SPRITE_TRIG_FRACTION_BITS;
        projection->extent.corner.y = ((work->scale * (CELL_PIXELS - 1) / projection->depth) * rcos(work->angle)) >> SPRITE_TRIG_FRACTION_BITS;
        sprite->x0                  = projection->screenX + (u16)projection->extent.corner.x;
        sprite->x3                  = projection->screenX - (u16)projection->extent.corner.x;
        sprite->y0                  = projection->screenY - (u16)projection->extent.corner.y;
        sprite->y3                  = projection->screenY + (u16)projection->extent.corner.y;
        projection->extent.corner.x = ((work->scale * (CELL_PIXELS - 1) / projection->depth) * rsin(work->angle + ACTOR_TRANSFORM_ANGLE_TURN / 4)) >> SPRITE_TRIG_FRACTION_BITS;
        projection->extent.corner.y = ((work->scale * (CELL_PIXELS - 1) / projection->depth) * rcos(work->angle + ACTOR_TRANSFORM_ANGLE_TURN / 4)) >> SPRITE_TRIG_FRACTION_BITS;
        sprite->x1                  = projection->screenX + (u16)projection->extent.corner.x;
        sprite->x2                  = projection->screenX - (u16)projection->extent.corner.x;
        sprite->y1                  = projection->screenY - (u16)projection->extent.corner.y;
        sprite->y2                  = projection->screenY + (u16)projection->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->depth), sprite);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->scale        += work->period;
        nextAge             = work->age + 1;
        work->age           = nextAge;
        if (nextAge > work->step * FRAMES_PER_CELL - 1) {
            effectKillTask(work, task);
        }
    }
}

void akropolisHelicopterLandingPadDamagedLightSparksTask(Task* task)
{
    enum {
        LINES_AND_BURSTS     = 0,
        BURSTS_ONLY          = 1,
        RELEASE              = 2,
        LINE_LIGHT_SLOT      = 4,
        BURST_LIGHT_SLOT     = 5,
        LIGHT_FRAMES         = 4,
        CHILD_COUNT          = 6,
        LINE_INNER_DISTANCE  = 5600,
        LINE_OUTER_DISTANCE  = 6400,
        BURST_INNER_DISTANCE = 4000,
        BURST_OUTER_DISTANCE = 4800,
    };

    EffectWork*                    work;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          light;
    EffectWork*                    childWork;
    s32                            sparkIndex;
    s32                            upperSparkCount;
    s32                            audioPan;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (task->state == RELEASE) {
            effectKillTask(work, task);
        }
        return;
    }
    // Follow the damaged model part in its local frame for the effect lifetime.
    if (work->index == 0) {
        coord->parent       = work->parent;
        coord->coord.t[0]   = work->pos.vx;
        coord->coord.t[1]   = work->pos.vy;
        coord->coord.t[2]   = work->pos.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        work->scale = 1;
        work->index++;
    }
    work->age++;
    switch (task->state) {
        case LINES_AND_BURSTS:
            RANDOM_LCG_ADVANCE();
            upperSparkCount = RANDOM_LCG_DRAW() & 3;
            for (sparkIndex = 0; sparkIndex < upperSparkCount; sparkIndex++) {
                _akropolisHelicopterLandingPadDrawUpperSparkLine(coord);
            }
            RANDOM_LCG_ADVANCE();
            if ((RANDOM_LCG_DRAW() & 3) == 0) {
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    akropolisHelicopterLandingPadDrawLowerSparkLine(coord);
                }
            }
            lightSlot             = &gWorldCoordTransientPointLights[LINE_LIGHT_SLOT];
            light                 = &lightSlot->light;
            lightSlot->framesLeft = LIGHT_FRAMES;
            light->inner          = LINE_INNER_DISTANCE;
            light->outer          = LINE_OUTER_DISTANCE;
            light->head.color.r   = LIGHT_HALF_INTENSITY;
            light->head.color.g   = LIGHT_HALF_INTENSITY;
            RANDOM_LCG_ADVANCE();
            light->head.color.b = (RANDOM_LCG_DRAW() & LIGHT_BLUE_RANDOM_MASK) + LIGHT_BLUE_BASE;
            gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &lightSlot->light.head.transform.coord.coord);
            lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            /* fallthrough */
        case BURSTS_ONLY:
            if ((gDisplayState.animFrame & 7) == 0) {
                RANDOM_LCG_ADVANCE();
                if ((RANDOM_LCG_DRAW() & 3) == 0) {
                    work->scale = 1;
                }
            }
            // Link emitted tasks under this controller so parent teardown reaches them.
            if (work->scale != 0) {
                audioPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(SOUND_HELICOPTER_LANDING_PAD_LIGHT_SPARK, audioPan, (s8)worldCoordGetOriginAudioDepth(coord));
                work->scale = 0;
                childWork   = effectSpawn(EFFECT_IMPACT_SPARK, coord, 0x200, NULL);
                if (childWork != NULL) {
                    taskReparent(task, childWork->task);
                }
                for (sparkIndex = 0; sparkIndex < CHILD_COUNT; sparkIndex++) {
                    childWork = effectSpawn(EFFECT_PIXEL_SPARK, coord, 1, NULL);
                    if (childWork != NULL) {
                        taskReparent(task, childWork->task);
                    }
                }
                lightSlot             = &gWorldCoordTransientPointLights[BURST_LIGHT_SLOT];
                light                 = &lightSlot->light;
                lightSlot->framesLeft = LIGHT_FRAMES;
                light->inner          = BURST_INNER_DISTANCE;
                light->outer          = BURST_OUTER_DISTANCE;
                light->head.color.r   = ONE * 3 / 4;
                light->head.color.g   = ONE * 3 / 4;
                light->head.color.b   = ONE * 3 / 8;
                gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &lightSlot->light.head.transform.coord.coord);
                lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            }
            break;
        case RELEASE:
            effectKillTask(work, task);
            break;
    }
}

/// Draws one blue spark from above the coordinate toward its origin.
static void _akropolisHelicopterLandingPadDrawUpperSparkLine(GfxCoord* coord)
{
    EffectLineScratch* line;
    LINE_F2*           spark;
    u32                colorSample;
    u16                greenIntensity;

    actorRenderComposeCoord(coord);
    line = SCRATCH_STACK_RESERVE_BLOCK(EffectLineScratch);
    RANDOM_LCG_ADVANCE();
    line->endpoints[0].vx = (RANDOM_LCG_DRAW() & 0x3F) - 0x20;
    RANDOM_LCG_ADVANCE();
    line->endpoints[0].vy = (RANDOM_LCG_DRAW() & 0x7F) - 0xC0;
    RANDOM_LCG_ADVANCE();
    line->endpoints[0].vz = (RANDOM_LCG_DRAW() & 0x3F) - 0x20;
    // Convert the local endpoint to view space, narrowing each component to s16.
    TRANSFORM_SPARK_ENDPOINT(line, 0, coord);
    RANDOM_LCG_ADVANCE();
    line->endpoints[1].vx = (RANDOM_LCG_DRAW() & 0x7F) - 0x40;
    RANDOM_LCG_ADVANCE();
    line->endpoints[1].vy = (RANDOM_LCG_DRAW() % 0xFF) - 0x80;
    RANDOM_LCG_ADVANCE();
    line->endpoints[1].vz = (RANDOM_LCG_DRAW() & 0x7F) - 0x40;
    TRANSFORM_SPARK_ENDPOINT(line, 1, coord);
    // Project both ends; only the second endpoint supplies rejection flags and depth.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&line->endpoints[0]);
    gte_rtps();
    gte_stsxy(&line->screenEndpoints[0]);
    gte_ldv0(&line->endpoints[1]);
    gte_rtps();
    RANDOM_LCG_ADVANCE();
    colorSample    = RANDOM_LCG_DRAW() & 0xFF;
    greenIntensity = colorSample;
    gte_stsxy(&line->screenEndpoints[1]);
    gte_stflg(&line->projectionFlags);
    if (line->projectionFlags >= 0) {
        gte_stszotz(&line->depth);
        spark          = gGpuPrimCursor;
        gGpuPrimCursor = spark + 1;
        setLineF2(spark);
        setRGB0(spark, colorSample >> 1, greenIntensity, 0xFF);
        spark->x0 = line->screenEndpoints[0].vx;
        spark->y0 = line->screenEndpoints[0].vy;
        spark->x1 = line->screenEndpoints[1].vx;
        spark->y1 = line->screenEndpoints[1].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(line->depth), spark);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectLineScratch);
}

void akropolisHelicopterLandingPadDrawLowerSparkLine(GfxCoord* coord)
{
    EffectLineScratch* line;
    LINE_F2*           spark;
    u32                colorSample;
    u16                greenIntensity;

    actorRenderComposeCoord(coord);
    line = SCRATCH_STACK_RESERVE_BLOCK(EffectLineScratch);
    RANDOM_LCG_ADVANCE();
    line->endpoints[0].vx = (RANDOM_LCG_DRAW() & 0x3F) - 0x20;
    RANDOM_LCG_ADVANCE();
    line->endpoints[0].vy = RANDOM_LCG_DRAW() & 0x7F;
    RANDOM_LCG_ADVANCE();
    line->endpoints[0].vz = (RANDOM_LCG_DRAW() & 0x3F) - 0x20;
    // Convert the local endpoint to view space, narrowing each component to s16.
    TRANSFORM_SPARK_ENDPOINT(line, 0, coord);
    RANDOM_LCG_ADVANCE();
    line->endpoints[1].vx = (RANDOM_LCG_DRAW() & 0x3F) - 0x20;
    RANDOM_LCG_ADVANCE();
    line->endpoints[1].vy = RANDOM_LCG_DRAW() & 0xFF;
    RANDOM_LCG_ADVANCE();
    line->endpoints[1].vz = (RANDOM_LCG_DRAW() & 0x3F) - 0x20;
    TRANSFORM_SPARK_ENDPOINT(line, 1, coord);
    // Project both ends; only the second endpoint supplies rejection flags and depth.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&line->endpoints[0]);
    gte_rtps();
    gte_stsxy(&line->screenEndpoints[0]);
    gte_ldv0(&line->endpoints[1]);
    gte_rtps();
    RANDOM_LCG_ADVANCE();
    colorSample    = RANDOM_LCG_DRAW() & 0xFF;
    greenIntensity = colorSample;
    gte_stsxy(&line->screenEndpoints[1]);
    gte_stflg(&line->projectionFlags);
    if (line->projectionFlags >= 0) {
        gte_stszotz(&line->depth);
        spark          = gGpuPrimCursor;
        gGpuPrimCursor = spark + 1;
        setLineF2(spark);
        setRGB0(spark, colorSample >> 1, greenIntensity, 0xFF);
        spark->x0 = line->screenEndpoints[0].vx;
        spark->y0 = line->screenEndpoints[0].vy;
        spark->x1 = line->screenEndpoints[1].vx;
        spark->y1 = line->screenEndpoints[1].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(line->depth), spark);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectLineScratch);
}

#undef TRANSFORM_SPARK_ENDPOINT

void akropolisHelicopterLandingPadFlareEmitterTask(Task* task)
{
    enum {
        FLASH               = 0,
        RAW                 = 1,
        EMBERS              = 2,
        RELEASE             = 3,
        LIGHT_SLOT          = 4,
        LIGHT_FRAMES        = 4,
        INNER_DISTANCE      = 6400,
        OUTER_DISTANCE      = 7200,
        FRAME_HALF_BIT      = 0x40,
        LARGE_EMBER_VARIANT = 2,
    };

    EffectWork*                    work;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          light;

    lightSlot = &gWorldCoordTransientPointLights[LIGHT_SLOT];
    light     = &lightSlot->light;
    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    if (task->state == RELEASE) {
        effectKillTask(work, task);
        return;
    }
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING && task->state < RELEASE) {
        return;
    }
    actorRenderComposeCoord(coord);
    switch (task->state) {
        case FLASH:
            effectSpawn(EFFECT_ACROPOLIS_HELIPAD_LENS_FLARE, coord, 1, NULL);
            effectSpawn(EFFECT_ACROPOLIS_HELIPAD_LENS_FLARE, coord, 1, NULL);
            lightSlot->framesLeft = LIGHT_FRAMES;
            light->inner          = INNER_DISTANCE;
            light->outer          = OUTER_DISTANCE;
            light->head.color.r   = LIGHT_HALF_INTENSITY;
            light->head.color.g   = LIGHT_HALF_INTENSITY;
            RANDOM_LCG_ADVANCE();
            light->head.color.b = (RANDOM_LCG_DRAW() & LIGHT_BLUE_RANDOM_MASK) + LIGHT_BLUE_BASE;
            // Spawn places the emitter under the view; copy that parent-frame translation.
            light->head.transform.coord.coord.t[0]             = coord->coord.t[0];
            light->head.transform.coord.coord.t[1]             = coord->coord.t[1];
            light->head.transform.coord.coord.t[2]             = coord->coord.t[2];
            lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            break;
        case RAW:
            effectSpawn(EFFECT_ACROPOLIS_HELIPAD_LENS_FLARE, coord, 0, NULL);
            effectSpawn(EFFECT_ACROPOLIS_HELIPAD_LENS_FLARE, coord, 0, NULL);
            break;
        case EMBERS:
            if (gDisplayState.animFrame & FRAME_HALF_BIT) {
                RANDOM_LCG_ADVANCE();
                if ((RANDOM_LCG_DRAW() & 0xF) == 0) {
                    effectSpawn(EFFECT_ACROPOLIS_HELIPAD_EMBER, coord, LARGE_EMBER_VARIANT, NULL);
                }
            }
            break;
        case RELEASE:
            effectKillTask(work, task);
            break;
    }
}

void akropolisHelicopterLandingPadLensFlareTask(Task* task)
{
    enum {
        INITIALIZE      = 0,
        CLUT            = 0x4384,
        CELL_PIXELS     = 40,
        TEXTURE_ROW     = 0x48,
        FRAMES_PER_CELL = 6,
        FADE_FRAMES     = 8,
    };

    EffectWork*         work;
    GfxCoord*           coord;
    EffectShapeScratch* projection;
    POLY_FT4*           sprite;
    s32                 lifetimeFrames;
    s32                 age;
    s32                 fadeIntensity;
    u8                  greenIntensity;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }
    actorRenderComposeCoord(coord);
    if (task->state == INITIALIZE) {
        RANDOM_LCG_ADVANCE();
        work->scale = (RANDOM_LCG_DRAW() & 0x1FF) + 0x200;
        RANDOM_LCG_ADVANCE();
        work->angle = RANDOM_LCG_DRAW() & ACTOR_TRANSFORM_ANGLE_MASK;
        RANDOM_LCG_ADVANCE();
        work->step = (RANDOM_LCG_DRAW() & 3) + 1;
        RANDOM_LCG_ADVANCE();
        work->move.vx = -(RANDOM_LCG_DRAW() & 0x1F) - 0x40;
        RANDOM_LCG_ADVANCE();
        work->move.vy = (RANDOM_LCG_DRAW() & 0xF) - 8;
        RANDOM_LCG_ADVANCE();
        work->move.vz = (RANDOM_LCG_DRAW() & 0xF) - 8;
        task->state++;
    }
    projection = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    _akropolisHelicopterLandingPadProjectSpriteCentre(projection, coord);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        sprite         = gGpuPrimCursor;
        gGpuPrimCursor = sprite + 1;
        setPolyFT4(sprite);
        // Retain the strict fade threshold: the final seven drawn ages fade.
        lifetimeFrames = work->step * FRAMES_PER_CELL;
        age            = work->age;
        if (lifetimeFrames - FADE_FRAMES < age) {
            fadeIntensity = (lifetimeFrames - age + 1) * 16;
            setRGB0(sprite, fadeIntensity, fadeIntensity, fadeIntensity);
        } else {
            if (task->spawnArg1.value != 0) {
                RANDOM_LCG_ADVANCE();
                if ((RANDOM_LCG_DRAW() & 3) == 0) {
                    RANDOM_LCG_ADVANCE();
                    greenIntensity = RANDOM_LCG_DRAW();
                    setRGB0(sprite, greenIntensity >> 1, greenIntensity, 0xFF);
                } else {
                    setShadeTex(sprite, true);
                }
                RANDOM_LCG_ADVANCE();
                if ((RANDOM_LCG_DRAW() & 0xF) == 0 && gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    effectSpawn(EFFECT_FLASH_BURST, coord, 0x100, NULL);
                }
            } else {
                sprite->code = SPRITE_RAW_TEXTURE_CODE;
            }
            RANDOM_LCG_ADVANCE();
            if ((RANDOM_LCG_DRAW() & 0xF) == 0 && gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                effectSpawn(EFFECT_ACROPOLIS_HELIPAD_EMBER, coord, 2 - task->spawnArg1.value, NULL);
            }
        }
        sprite->tpage = SPRITE_TEXTURE_PAGE;
        setSemiTrans(sprite, true);
        sprite->clut                = CLUT;
        sprite->u0                  = (work->age / work->step) * CELL_PIXELS;
        sprite->v0                  = TEXTURE_ROW;
        sprite->u1                  = (work->age / work->step) * CELL_PIXELS + (CELL_PIXELS - 1);
        sprite->v1                  = TEXTURE_ROW;
        sprite->u2                  = (work->age / work->step) * CELL_PIXELS;
        sprite->v2                  = TEXTURE_ROW + CELL_PIXELS - 1;
        sprite->u3                  = (work->age / work->step) * CELL_PIXELS + (CELL_PIXELS - 1);
        sprite->v3                  = TEXTURE_ROW + CELL_PIXELS - 1;
        projection->extent.corner.x = ((work->scale * (CELL_PIXELS - 1) / projection->depth) * rsin(work->angle)) >> SPRITE_TRIG_FRACTION_BITS;
        projection->extent.corner.y = ((work->scale * (CELL_PIXELS - 1) / projection->depth) * rcos(work->angle)) >> SPRITE_TRIG_FRACTION_BITS;
        sprite->x0                  = projection->screenX + (u16)projection->extent.corner.x;
        sprite->x3                  = projection->screenX - (u16)projection->extent.corner.x;
        sprite->y0                  = projection->screenY - (u16)projection->extent.corner.y;
        sprite->y3                  = projection->screenY + (u16)projection->extent.corner.y;
        projection->extent.corner.x = ((work->scale * (CELL_PIXELS - 1) / projection->depth) * rsin(work->angle + ACTOR_TRANSFORM_ANGLE_TURN / 4)) >> SPRITE_TRIG_FRACTION_BITS;
        projection->extent.corner.y = ((work->scale * (CELL_PIXELS - 1) / projection->depth) * rcos(work->angle + ACTOR_TRANSFORM_ANGLE_TURN / 4)) >> SPRITE_TRIG_FRACTION_BITS;
        sprite->x1                  = projection->screenX + (u16)projection->extent.corner.x;
        sprite->x2                  = projection->screenX - (u16)projection->extent.corner.x;
        sprite->y1                  = projection->screenY - (u16)projection->extent.corner.y;
        sprite->y2                  = projection->screenY + (u16)projection->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->depth), sprite);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->age++;
        if (work->age > work->step * FRAMES_PER_CELL - 1) {
            effectKillTask(work, task);
        }
    }
}

void akropolisHelicopterLandingPadPerimeterLightsTask(Task* task)
{
    enum {
        GROUND_SHADOW_DISABLED_VIEW = 18,
        PULSE_FRAME_SHIFT           = 2,
        PULSE_FALLING_BIT           = 0x80,
        PULSE_HALF_MASK             = 0x7F,
        PULSE_RISING_MASK           = 0x7C,
    };

    EffectWork*    work = task->spawnArg2.pointer;
    const SVECTOR* worldPoint;
    s32            lightIndex;
    s32            phase;
    s32            halfBrightness;

    if ((viewGetMappedIndex() & 0xFF) == GROUND_SHADOW_DISABLED_VIEW) {
        gRoomEffectState->groundShadowShade = ROOM_EFFECT_GROUND_SHADOW_DISABLED;
    } else {
        gRoomEffectState->groundShadowShade = ROOM_EFFECT_GROUND_SHADOW_UNMODULATED;
    }

    // Keep the full word for the wave; the work field deliberately narrows to s16.
    phase       = gDisplayState.animFrame << PULSE_FRAME_SHIFT;
    work->scale = phase;
    if (phase & PULSE_FALLING_BIT) {
        halfBrightness = PULSE_HALF_MASK - (phase & PULSE_HALF_MASK);
    } else {
        halfBrightness = phase & PULSE_RISING_MASK;
    }
    work->scale = halfBrightness * 2;

    lightIndex = 0;
    worldPoint = _gAkropolisHelicopterLandingPadPerimeterLightPositions;
    for (; lightIndex < ARRAY_SIZE(_gAkropolisHelicopterLandingPadPerimeterLightPositions); lightIndex++) {
        _akropolisHelicopterLandingPadDrawPerimeterLight(worldPoint++, lightIndex, work->scale);
    }
}

#include "../../../../shared/actor/actor_contacts_push_contact.inc.c"

#include "../../../../shared/actor/actor_contacts_push.inc.c"

void akropolisHelicopterLandingPadPickupModelTask(Task* task)
{
    enum {
        PICKUP_COLLECTED = 2,
    };

    Enemy*     placement;
    TmdObject* model;
    s32        placementState;

    placement      = task->spawnArg2.pointer;
    model          = task->extra.tmd;
    placementState = areaGetCurrentObjectState((u8)placement->placeKey);
    viewGetMappedIndex();
    if (placementState == PICKUP_COLLECTED) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    } else {
        model->flags    = TMD_OBJECT_FLAGGED_PASS;
        model->otOffset = 0;
        tmdAllocPrimitiveBuffer(model);
    }
}
