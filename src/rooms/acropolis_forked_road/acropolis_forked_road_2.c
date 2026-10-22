#include "rooms/acropolis_forked_road.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "acropolis_forked_road_private.h"

#include "actors/task_tables.h"

#include "gameplay/companion_load.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag_ids.h"
#include "main/gameflow.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/falling_leaves.h"

#include "assets/rooms/akropolis/forked_road/declarations.inc"

static void _acropolisForkedRoadOutboundMoviePathTask(Task* task);
static void _acropolisForkedRoadReturnMoviePathTask(Task* task);
static void _acropolisForkedRoadSkipFadeOutTask(Task* task);
static void _acropolisForkedRoadSkipFadeInTask(Task* task);

static void _acropolisForkedRoadReleaseMaggotCaterpillarEntrance(void);

enum { ACROPOLIS_FORKED_ROAD_MOVIE_TASK_PRIORITY = 192 };

TaskDesc gAcropolisForkedRoadMovieTaskDescriptors[ACROPOLIS_FORKED_ROAD_MOVIE_TASK_COUNT] = {
    [ACROPOLIS_FORKED_ROAD_TASK_OUTBOUND_MOVIE] = { { { TASK_BODY_NONE, ACROPOLIS_FORKED_ROAD_MOVIE_TASK_PRIORITY } }, _acropolisForkedRoadOutboundMoviePathTask, { .value = 0 } },
    [ACROPOLIS_FORKED_ROAD_TASK_UNUSED_MOVIE]   = { { { TASK_BODY_NONE, ACROPOLIS_FORKED_ROAD_MOVIE_TASK_PRIORITY } }, NULL, { .value = 0 } },
    [ACROPOLIS_FORKED_ROAD_TASK_RETURN_MOVIE]   = { { { TASK_BODY_NONE, ACROPOLIS_FORKED_ROAD_MOVIE_TASK_PRIORITY } }, _acropolisForkedRoadReturnMoviePathTask, { .value = 0 } },
    [ACROPOLIS_FORKED_ROAD_TASK_SKIP_FADE_OUT]  = { { { TASK_BODY_NONE, ACROPOLIS_FORKED_ROAD_MOVIE_TASK_PRIORITY } }, _acropolisForkedRoadSkipFadeOutTask, { .value = 0 } },
    [ACROPOLIS_FORKED_ROAD_TASK_SKIP_FADE_IN]   = { { { TASK_BODY_NONE, ACROPOLIS_FORKED_ROAD_MOVIE_TASK_PRIORITY } }, _acropolisForkedRoadSkipFadeInTask, { .value = 0 } },
};

/// Player-root translations sampled along the observatory route's movie path.
///
/// All 300 signed-halfword XYZ samples use whole units in the root's parent
/// frame, without a terminator. Outbound indexing is `movieFrame - 1`; the
/// movie descriptor caps frames at 300, and the controller requests departure
/// on index 120 or Start. Return indexing is `59 - movieFrame` and requires
/// frames 1..59 at every access; normal exit begins below index 11. Keep the
/// overlay loaded while either task reads the table. The fourth halfword is unused.
static SVECTOR _gAcropolisForkedRoadMoviePlayerPositions[300] = {
#include "assets/acropolis_forked_road_path_039C0.inc"
};

#include "assets/rooms/akropolis/forked_road/clips.inc"

/// Three set pointers copied to equipped-weapon bank entries 47..49 for the lock-release event.
///
/// Entry 47 is cleared; entries 48 and 49 borrow the two room clips. No
/// terminator: the copy request covers all three words. Keep the overlay and
/// clip pools loaded while the player bank retains these pointers.
#include "assets/rooms/akropolis/forked_road/acropolis_forked_road_lock_release_bank.inc"

enum {
    ACROPOLIS_FORKED_ROAD_LOCK_RELEASE_PLAYER_CLIP  = 48,
    ACROPOLIS_FORKED_ROAD_LOCK_RELEASE_BLEND_FRAMES = 20,
    ACROPOLIS_FORKED_ROAD_ENTRANCE_WAIT_TICKS       = 10,
    ACROPOLIS_FORKED_ROAD_BATTLE_WAIT_TICKS         = 3,
};

#include "assets/rooms/akropolis/forked_road/events.inc"
#include "assets/rooms/akropolis/forked_road/markers.inc"
#include "assets/rooms/akropolis/forked_road/view_masks.inc"

#include "../../shared/room/room_visual_effects_trail_data.inc.c"
#include "assets/rooms/akropolis/forked_road/header.inc"
#include "assets/rooms/akropolis/forked_road/geometry.inc"
#include "assets/rooms/akropolis/forked_road/view_boundaries.inc"
#include "assets/rooms/akropolis/forked_road/action_triggers.inc"
#include "assets/rooms/akropolis/forked_road/placements.inc"
#include "assets/rooms/akropolis/forked_road/sprites.inc"
#include "assets/rooms/akropolis/forked_road/lights.inc"
#include "assets/rooms/akropolis/forked_road/cameras.inc"
#include "assets/rooms/akropolis/forked_road/vibrations.inc"
#include "assets/rooms/akropolis/forked_road/surfaces.inc"

enum {
    ACROPOLIS_FORKED_ROAD_FOUNTAIN_ARRIVAL_SOUND   = 0x51090003,
    ACROPOLIS_FORKED_ROAD_FOUNTAIN_DEPARTURE_SOUND = 0x51090002,
    ACROPOLIS_FORKED_ROAD_FOUNTAIN_BLOCKED_SOUND   = 0x51090006,
    ACROPOLIS_FORKED_ROAD_FOUNTAIN_WARP_MAP_FLAG   = 0x1EF,
};

enum {
    ACROPOLIS_FORKED_ROAD_GRID_CELLS_X = 6,
    ACROPOLIS_FORKED_ROAD_GRID_CELLS_Z = 4,
};

// Action-1 control bit 8 selects descent; its parameter0 packs surface class and steps.
enum {
    ACROPOLIS_FORKED_ROAD_STAIR_DESCEND       = 0x100,
    ACROPOLIS_FORKED_ROAD_STAIR_SURFACE_CLASS = 4,
    ACROPOLIS_FORKED_ROAD_ASCENT_STEPS        = 4,
    ACROPOLIS_FORKED_ROAD_DESCENT_STEPS       = 2,
    ACROPOLIS_FORKED_ROAD_LOCK_RELEASE_ACTION = 1,
};

// Encoded 8-bit texture pages, palette rows and neutral RGB for the room sprites.
enum {
    ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_896              = getTPage(1, GPU_BLEND_AVERAGE, 896, 0),
    ACROPOLIS_FORKED_ROAD_SPRITE_PAGE_960              = getTPage(1, GPU_BLEND_AVERAGE, 960, 0),
    ACROPOLIS_FORKED_ROAD_SPRITE_OPAQUE_CLUT           = getClut(0, 255),
    ACROPOLIS_FORKED_ROAD_SPRITE_SEMI_TRANSPARENT_CLUT = getClut(0, 256),
    ACROPOLIS_FORKED_ROAD_SPRITE_NEUTRAL_COLOR         = 128,
};

enum {
    ACROPOLIS_FORKED_ROAD_RETURN_VIBRATION_WAIT_FRAMES = 2,
    ACROPOLIS_FORKED_ROAD_RETURN_VIBRATION_LOOP_COUNT  = 16,
};

enum {
    ACROPOLIS_FORKED_ROAD_OUTBOUND_VIBRATION_WAIT_FRAMES = 2,
    ACROPOLIS_FORKED_ROAD_OUTBOUND_VIBRATION_LOOP_COUNT  = 38,
};

enum {
    ACROPOLIS_FORKED_ROAD_SURFACE_CLASS4_WALK_CUE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x11,
    ACROPOLIS_FORKED_ROAD_SURFACE_CLASS4_RUN_CUE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x13,
};

/// Copies a frame-indexed movie-path sample into the borrowed player root.
///
/// Requires a live `RoomMoviePathWork` and index 0..299 in the room's
/// `_gAcropolisForkedRoadMoviePlayerPositions` path. Both arguments are evaluated three
/// times, once per XYZ store, and must be stable and side-effect-free. Positions
/// use whole room units; rotation and coordinate cache stamps are untouched.
/// Expands to a braced statement block; use inside a braced scope.
#define ACROPOLIS_FORKED_ROAD_APPLY_MOVIE_PATH_POSITION(movieWork, pathIndex)                     \
    {                                                                                             \
        (movieWork)->playerMtx->t[0] = _gAcropolisForkedRoadMoviePlayerPositions[(pathIndex)].vx; \
        (movieWork)->playerMtx->t[1] = _gAcropolisForkedRoadMoviePlayerPositions[(pathIndex)].vy; \
        (movieWork)->playerMtx->t[2] = _gAcropolisForkedRoadMoviePlayerPositions[(pathIndex)].vz; \
    }

/// Moves the player along the outbound movie path, then reloads the observatory.
///
/// Starts bodyless in state 0 with a live player and loaded room/stream resources.
/// Owns a zeroed `RoomMoviePathWork`; allocation failure kills the controller.
/// Places the player before walking to the path head, hides the HUD and actors,
/// then queues the current view's movie. Once ready, adopts the vibration task
/// and writes XYZ from path index `movieFrame - 1` in whole room units. The
/// frame must be 1..300 on every path access; reaching index 120 or pressing
/// Start schedules departure on the next tick. Reload hands off to observatory
/// room 1, warp 4. Task teardown releases the work and adopted script task.
static void _acropolisForkedRoadOutboundMoviePathTask(Task* task)
{
    enum {
        ACROPOLIS_FORKED_ROAD_OUTBOUND_INITIALIZE          = 0,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_APPROACH            = 1,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_WAIT_APPROACH       = 2,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_WAIT_MOVIE          = 3,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_FOLLOW_PATH         = 4,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_DEPART              = 5,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_PATH_EXIT_INDEX     = 120,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_YAW                 = ACTOR_TRANSFORM_ANGLE_TURN / 4,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_APPROACH_X_DISTANCE = 1620,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_DESTINATION_WARP    = 4,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_DESTINATION_ROOM    = 1
    };
    ActorTransform     entryPose;
    ActorTransform     approachTarget;
    u8                 streamArgs[sizeof(gCdCmdQueue.entries[0].args.bytes)];
    RoomMoviePathWork* movieWork;
    CdCmdQueue*        cdQueue;

    cdQueue   = &gCdCmdQueue;
    movieWork = task->work;
    switch (task->state) {
        case ACROPOLIS_FORKED_ROAD_OUTBOUND_INITIALIZE:
            task->work = memCalloc(sizeof(RoomMoviePathWork), 0);
            if (task->work == NULL) {
                taskKill(task);
                break;
            }
            cdQueue->movieFrame = 1;
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_AND_HOLD, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            gSceneCombatState.actorControl               = SCENE_COMBAT_ACTORS_HIDDEN;
            ((RoomMoviePathWork*)task->work)->playerMtx  = gPlayerStatus.coordMtx;
            ((RoomMoviePathWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            entryPose.rot.vy = ACROPOLIS_FORKED_ROAD_OUTBOUND_YAW;
            entryPose.rot.vx = 0;
            entryPose.rot.vz = 0;
            entryPose.pos.vx = _gAcropolisForkedRoadMoviePlayerPositions[0].vx - ACROPOLIS_FORKED_ROAD_OUTBOUND_APPROACH_X_DISTANCE;
            entryPose.pos.vy = _gAcropolisForkedRoadMoviePlayerPositions[0].vy;
            entryPose.pos.vz = _gAcropolisForkedRoadMoviePlayerPositions[0].vz;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, &entryPose, 0);
            task->state = task->state + 1;
            break;

        case ACROPOLIS_FORKED_ROAD_OUTBOUND_APPROACH:
            approachTarget.rot.vy = ACROPOLIS_FORKED_ROAD_OUTBOUND_YAW;
            approachTarget.pos.vx = _gAcropolisForkedRoadMoviePlayerPositions[0].vx;
            approachTarget.pos.vy = _gAcropolisForkedRoadMoviePlayerPositions[0].vy;
            approachTarget.pos.vz = _gAcropolisForkedRoadMoviePlayerPositions[0].vz;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &approachTarget, 0);
            task->state = task->state + 1;
            break;

        case ACROPOLIS_FORKED_ROAD_OUTBOUND_WAIT_APPROACH:
            if (taskMessageDispatch(movieWork->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                // Only byte zero is interpreted by this opcode; preserve the unwritten tail and low-RAM key reads.
                streamArgs[0] = streamFindMovieSlot(&gGameSession->location, 0, 0);
                cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, streamArgs);
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_FORKED_ROAD_OUTBOUND_WAIT_MOVIE:
            if (cdQueue->movieReady != 0) {
                movieWork->padScriptTask      = padScriptSpawn(_gAcropolisForkedRoadOutboundMovieVibrationCommands,
                                                               _gAcropolisForkedRoadOutboundMovieVibrationSegments);
                gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE;
                taskReparent(task, movieWork->padScriptTask);
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_FORKED_ROAD_OUTBOUND_FOLLOW_PATH:
            // Follow encoded frames in the player root's parent-coordinate space.
            ACROPOLIS_FORKED_ROAD_APPLY_MOVIE_PATH_POSITION(movieWork, cdQueue->movieFrame - 1);
            if ((padIsStartPressed() != 0) || ((cdQueue->movieFrame - 1) >= ACROPOLIS_FORKED_ROAD_OUTBOUND_PATH_EXIT_INDEX)) {
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_FORKED_ROAD_OUTBOUND_DEPART:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR_ALIAS, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_AKROPOLIS;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_AKROPOLIS_OBSERVATORY;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = ACROPOLIS_FORKED_ROAD_OUTBOUND_DESTINATION_WARP;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = ACROPOLIS_FORKED_ROAD_OUTBOUND_DESTINATION_ROOM;
            gDisplayState.spriteVariant                                 = 1;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            gGameSession->padScriptFlags &= (0xFF ^ GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE);
            taskKill(task);
            break;
    }
}

/// Moves the player backwards along the arrival movie path and restores local play.
///
/// Starts bodyless in state 0 after arrival warp 2 has selected the view movie;
/// this controller waits for readiness and does not queue playback itself.
/// Owns a zeroed `RoomMoviePathWork`, borrows the live player/root matrix and
/// adopts its vibration task. Allocation failure kills the controller.
/// Path accesses require `movieFrame` in 1..59 and use index `59 - movieFrame`
/// in whole room units. The first two active path ticks blank the display.
/// Start requests a fade task polled to suspension before placing the exit pose;
/// otherwise indices below 11 start a walk to the exit. After movement ends,
/// restores the HUD, input, actors and logical view 5, then tears down its work
/// and adopted children. Fade and vibration spawns retain unchecked failures.
static void _acropolisForkedRoadReturnMoviePathTask(Task* task)
{
    enum {
        ACROPOLIS_FORKED_ROAD_RETURN_INITIALIZE          = 0,
        ACROPOLIS_FORKED_ROAD_RETURN_WAIT_MOVIE          = 1,
        ACROPOLIS_FORKED_ROAD_RETURN_FOLLOW_PATH         = 2,
        ACROPOLIS_FORKED_ROAD_RETURN_WAIT_EXIT           = 3,
        ACROPOLIS_FORKED_ROAD_RETURN_FRAME_ORIGIN        = 59,
        ACROPOLIS_FORKED_ROAD_RETURN_WALK_EXIT_INDEX     = 11,
        ACROPOLIS_FORKED_ROAD_RETURN_REVEAL_TICK         = 3,
        ACROPOLIS_FORKED_ROAD_RETURN_YAW                 = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4,
        ACROPOLIS_FORKED_ROAD_RETURN_LOGICAL_VIEW        = 5,
        ACROPOLIS_FORKED_ROAD_PRIMARY_CHARACTER_ID       = 1,
        ACROPOLIS_FORKED_ROAD_ALTERNATE_WEAPON_BANK_BASE = 0x22,
        ACROPOLIS_FORKED_ROAD_RETURN_IDLE_ANIMATION      = 1
    };
    AnimationPlayRequest animationRequest;
    ActorTransform       exitPose;
    s32                  fadeResult;
    RoomMoviePathWork*   movieWork;
    CdCmdQueue*          cdQueue;
    s32                  weaponId;

    cdQueue   = &gCdCmdQueue;
    movieWork = task->work;
    switch (task->state) {
        case ACROPOLIS_FORKED_ROAD_RETURN_INITIALIZE:
            task->work = memCalloc(sizeof(RoomMoviePathWork), 0);
            if (task->work == NULL) {
                taskKill(task);
                break;
            }
            ((RoomMoviePathWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            ((RoomMoviePathWork*)task->work)->playerMtx  = gPlayerStatus.coordMtx;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            weaponId                              = gPlayerStatus.weapon;
            animationRequest.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACROPOLIS_FORKED_ROAD_PRIMARY_CHARACTER_ID) ? weaponId + 1 : weaponId + ACROPOLIS_FORKED_ROAD_ALTERNATE_WEAPON_BANK_BASE;
            animationRequest.animationId          = ACROPOLIS_FORKED_ROAD_RETURN_IDLE_ANIMATION;
            animationRequest.blend                = ANIMATION_BLEND_RESET;
            animationRequest.blendFrames          = 0;
            animationRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, ANIMATION_MESSAGE_PLAY, &animationRequest, 0);
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_AND_HOLD, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_HIDDEN;
            task->state                    = task->state + 1;
            break;

        case ACROPOLIS_FORKED_ROAD_RETURN_WAIT_MOVIE:
            if (cdQueue->movieReady != 0) {
                ACROPOLIS_FORKED_ROAD_APPLY_MOVIE_PATH_POSITION(movieWork, ACROPOLIS_FORKED_ROAD_RETURN_FRAME_ORIGIN - cdQueue->movieFrame);
                movieWork->padScriptTask      = padScriptSpawn(_gAcropolisForkedRoadReturnMovieVibrationCommands,
                                                               _gAcropolisForkedRoadReturnMovieVibrationSegments);
                gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE;
                taskReparent(task, movieWork->padScriptTask);
                SetDispMask(0);
                task->killCountdown = 0;
                task->state         = task->state + 1;
            }
            break;

        case ACROPOLIS_FORKED_ROAD_RETURN_FOLLOW_PATH:
            // Hide the initial discontinuity for two ticks before revealing the reverse path.
            task->killCountdown = task->killCountdown + 1;
            if (task->killCountdown >= ACROPOLIS_FORKED_ROAD_RETURN_REVEAL_TICK) {
                SetDispMask(1);
            }
            ACROPOLIS_FORKED_ROAD_APPLY_MOVIE_PATH_POSITION(movieWork, ACROPOLIS_FORKED_ROAD_RETURN_FRAME_ORIGIN - cdQueue->movieFrame);
            // A completed skip fade places the final pose; normal completion walks there.
            if (movieWork->skipFadeStarted != 0) {
                if (taskPollKill(movieWork->skipFadeTask, &fadeResult) != 0) {
                    exitPose.pos.vx = -0x190;
                    exitPose.pos.vy = 1;
                    exitPose.pos.vz = _gAcropolisForkedRoadMoviePlayerPositions[ACROPOLIS_FORKED_ROAD_RETURN_FRAME_ORIGIN - cdQueue->movieFrame].vz;
                    exitPose.rot.vz = 0;
                    exitPose.rot.vx = 0;
                    exitPose.rot.vy = ACROPOLIS_FORKED_ROAD_RETURN_YAW;
                    TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, &exitPose, 0);
                    taskSpawnFromTable(gAcropolisForkedRoadMovieTaskDescriptors, ACROPOLIS_FORKED_ROAD_TASK_SKIP_FADE_IN, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (padIsStartPressed() != 0) {
                movieWork->skipFadeTask    = taskSpawnFromTable(gAcropolisForkedRoadMovieTaskDescriptors, ACROPOLIS_FORKED_ROAD_TASK_SKIP_FADE_OUT, 0, 0);
                movieWork->skipFadeStarted = 1;
            }
            if ((ACROPOLIS_FORKED_ROAD_RETURN_FRAME_ORIGIN - cdQueue->movieFrame) < ACROPOLIS_FORKED_ROAD_RETURN_WALK_EXIT_INDEX) {
                exitPose.pos.vx = -0x190;
                exitPose.pos.vy = 1;
                exitPose.pos.vz = _gAcropolisForkedRoadMoviePlayerPositions[ACROPOLIS_FORKED_ROAD_RETURN_FRAME_ORIGIN - cdQueue->movieFrame].vz;
                TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &exitPose, 0);
                task->state = task->state + 1;
            }
            break;

        case ACROPOLIS_FORKED_ROAD_RETURN_WAIT_EXIT:
            if (taskMessageDispatch(movieWork->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                taskMessageDispatch(movieWork->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = viewFindLogicalIndex(ACROPOLIS_FORKED_ROAD_RETURN_LOGICAL_VIEW);
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD, 0, 0);
                padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR_ALIAS, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                gGameSession->padScriptFlags  &= (0xFF ^ GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE);
                taskKill(task);
            }
            break;
    }
}

#undef ACROPOLIS_FORKED_ROAD_APPLY_MOVIE_PATH_POSITION

/// Darkens the return ride before its skip caller replaces the player's pose.
///
/// Requires a bodyless task with `killCountdown` initially zero. Queues eight
/// equal-channel subtractive overlays, intensities 0..224 in steps of 32,
/// before suspending with result zero at progress 256. The caller must collect
/// it with `taskPollKill`; suspension retains the task and its resources.
static void _acropolisForkedRoadSkipFadeOutTask(Task* task)
{
    enum { ACROPOLIS_FORKED_ROAD_SKIP_FADE_STEP = 32,
           ACROPOLIS_FORKED_ROAD_SKIP_FADE_SPAN = 256 };
    u8  fadeLevel;
    s16 nextProgress;

    fadeLevel = (u8)task->killCountdown;
    fadeDrawOverlay(fadeLevel, fadeLevel, fadeLevel, GPU_BLEND_SUBTRACT);
    nextProgress        = (u16)task->killCountdown + ACROPOLIS_FORKED_ROAD_SKIP_FADE_STEP;
    task->killCountdown = nextProgress;
    if (nextProgress >= ACROPOLIS_FORKED_ROAD_SKIP_FADE_SPAN) {
        taskRequestKill(task, 0);
    }
}

/// Reveals the return ride's final pose after its skip fade has hidden the scene.
///
/// Requires a bodyless task with `killCountdown` initially zero. Draws eight
/// subtractive overlays, from brightness 255 down to 31 in steps of 32, then
/// kills the task. The signed 16-bit counter is callback-owned fade progress.
static void _acropolisForkedRoadSkipFadeInTask(Task* task)
{
    enum { SKIP_FADE_LEVEL_STEP = 32,
           SKIP_FADE_LEVEL_SPAN = 256 };
    u8  fadeLevel;
    s16 nextProgress;

    fadeLevel = ~(u8)task->killCountdown;
    fadeDrawOverlay(fadeLevel, fadeLevel, fadeLevel, GPU_BLEND_SUBTRACT);
    nextProgress        = (u16)task->killCountdown + SKIP_FADE_LEVEL_STEP;
    task->killCountdown = nextProgress;
    if (nextProgress >= SKIP_FADE_LEVEL_SPAN) {
        taskKill(task);
    }
}

/// Releases the room's scripted Maggot/Caterpillar entrances.
static void _acropolisForkedRoadReleaseMaggotCaterpillarEntrance(void)
{
    gSceneCombatState.maggotCaterpillarEntranceReady = true;
}

void acropolisForkedRoadInitializeRoomEffectsTask(Task* task)
{
    enum {
        ACROPOLIS_FORKED_ROAD_ROOM_EFFECTS_INITIALIZE = 0,
        ACROPOLIS_FORKED_ROAD_CELL_0_SMALL_END        = 2,
        ACROPOLIS_FORKED_ROAD_CELL_0_LARGE_END        = 4,
        ACROPOLIS_FORKED_ROAD_CELL_1_END              = 12,
        ACROPOLIS_FORKED_ROAD_LAMP_SCALE_512          = 512 << 16,
        ACROPOLIS_FORKED_ROAD_LAMP_SCALE_768          = 768 << 16,
        ACROPOLIS_FORKED_ROAD_LAMP_CELL_1             = 1 << 8,
        ACROPOLIS_FORKED_ROAD_LAMP_CELL_2             = 2 << 8
    };
    GfxCoord* effectCoord;
    s32       lampIndex;

    effectCoord = task->extra.coordBody->coord;
    if (task->state == ACROPOLIS_FORKED_ROAD_ROOM_EFFECTS_INITIALIZE) {
        for (lampIndex = 0; lampIndex < ACROPOLIS_FORKED_ROAD_CELL_0_SMALL_END; lampIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_FORKED_ROAD_WALL_LAMP, effectCoord, lampIndex + ACROPOLIS_FORKED_ROAD_LAMP_SCALE_512, &_gAcropolisForkedRoadWallLampPositions[lampIndex]);
        }
        for (lampIndex = ACROPOLIS_FORKED_ROAD_CELL_0_SMALL_END; lampIndex < ACROPOLIS_FORKED_ROAD_CELL_0_LARGE_END; lampIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_FORKED_ROAD_WALL_LAMP, effectCoord, lampIndex + ACROPOLIS_FORKED_ROAD_LAMP_SCALE_768, &_gAcropolisForkedRoadWallLampPositions[lampIndex]);
        }
        for (lampIndex = ACROPOLIS_FORKED_ROAD_CELL_0_LARGE_END; lampIndex < ACROPOLIS_FORKED_ROAD_CELL_1_END; lampIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_FORKED_ROAD_WALL_LAMP, effectCoord, lampIndex + (ACROPOLIS_FORKED_ROAD_LAMP_SCALE_512 | ACROPOLIS_FORKED_ROAD_LAMP_CELL_1), &_gAcropolisForkedRoadWallLampPositions[lampIndex]);
        }
        for (lampIndex = ACROPOLIS_FORKED_ROAD_CELL_1_END; lampIndex < (s32)ARRAY_SIZE(_gAcropolisForkedRoadWallLampPositions); lampIndex++) {
            effectSpawn(EFFECT_ACROPOLIS_FORKED_ROAD_WALL_LAMP, effectCoord, lampIndex + ACROPOLIS_FORKED_ROAD_LAMP_CELL_2, &_gAcropolisForkedRoadWallLampPositions[lampIndex]);
        }
        gRoomEffectFlashId      = EFFECT_ACROPOLIS_FORKED_ROAD_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_ACROPOLIS_FORKED_ROAD_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_ACROPOLIS_FORKED_ROAD_SPARK_BURST;
        task->state             = task->state + 1;
    }
}

#include "../../shared/effects/acropolis_glow_sprite_bounds.inc.c"

void acropolisForkedRoadWallLampTask(Task* task)
{
    enum {
        WALL_LAMP_INITIALIZE     = 0,
        WALL_LAMP_INDEX_MASK     = 0xF,
        WALL_LAMP_CELL_SHIFT     = 8,
        WALL_LAMP_CELL_MASK      = 3,
        WALL_LAMP_CELL_COUNT     = 3,
        WALL_LAMP_SCALE_SHIFT    = 16,
        WALL_LAMP_SCALE_MASK     = 0xFFF,
        WALL_LAMP_DEFAULT_SCALE  = 640,
        WALL_LAMP_MIN_DRAW_DEPTH = 17,
        WALL_LAMP_FLICKER_STEP   = 16,
        WALL_LAMP_TEXTURE_PAGE   = getTPage(0, GPU_BLEND_ADD, 704, 0),
        WALL_LAMP_CLUT_X_STRIDE  = 16,
        WALL_LAMP_CLUT_Y         = 270,
        WALL_LAMP_CELL_WIDTH     = 40,
        WALL_LAMP_LAST_TEXEL     = WALL_LAMP_CELL_WIDTH - 1,
    };
    RoomGlowSpriteScratch* projection;
    EffectWork*            work;
    GfxCoord*              coord;
    POLY_FT4*              quad;
    s32                    greyLevel;
    s32                    flickerLevel;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN &&
        ((_gAcropolisForkedRoadWallLampViewMasks[task->spawnArg1.value & WALL_LAMP_INDEX_MASK] >> (gGameSession->location.view - 1)) & 1)) {
        actorRenderComposeCoord(coord);
        projection = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
        if (task->state == WALL_LAMP_INITIALIZE) {
            u8 restingLevels[WALL_LAMP_CELL_COUNT] = { 0x50, 0x30, 0x10 };

            // Decode once, retaining only the lamp's view-mask index in the spawn argument.
            if (task->spawnArg1.value & (WALL_LAMP_SCALE_MASK << WALL_LAMP_SCALE_SHIFT)) {
                work->scale = (task->spawnArg1.value >> WALL_LAMP_SCALE_SHIFT) & WALL_LAMP_SCALE_MASK;
            } else {
                work->scale = WALL_LAMP_DEFAULT_SCALE;
            }
            work->angle           = (task->spawnArg1.value >> WALL_LAMP_CELL_SHIFT) & WALL_LAMP_CELL_MASK;
            task->spawnArg1.value = task->spawnArg1.value & WALL_LAMP_INDEX_MASK;
            work->period          = restingLevels[work->angle];
            task->state++;
        }
        // Rejected projections still consume one frame-arena packet.
        projection->worldPos.vx = coord->workm.t[0];
        projection->worldPos.vy = coord->workm.t[1];
        projection->worldPos.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projection->worldPos);
        gte_rtps();
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        gte_stsxy(&projection->screenPos);
        gte_stszotz(&projection->otz);
        if (projection->otz >= WALL_LAMP_MIN_DRAW_DEPTH) {
            flickerLevel = ((u8)gDisplayState.animFrame & 1) * WALL_LAMP_FLICKER_STEP;
            greyLevel    = (u8)work->period + flickerLevel;
            quad->tpage  = WALL_LAMP_TEXTURE_PAGE;
            quad->r0     = greyLevel;
            quad->g0     = greyLevel;
            quad->b0     = greyLevel;
            setSemiTrans(quad, true);
            setClut(quad, work->angle * WALL_LAMP_CLUT_X_STRIDE, WALL_LAMP_CLUT_Y);
            quad->u0 = work->angle * WALL_LAMP_CELL_WIDTH;
            quad->v0 = 0;
            quad->u1 = work->angle * WALL_LAMP_CELL_WIDTH + WALL_LAMP_LAST_TEXEL;
            quad->v1 = 0;
            quad->u2 = work->angle * WALL_LAMP_CELL_WIDTH;
            quad->v2 = WALL_LAMP_LAST_TEXEL;
            quad->u3 = work->angle * WALL_LAMP_CELL_WIDTH + WALL_LAMP_LAST_TEXEL;
            quad->v3 = WALL_LAMP_LAST_TEXEL;

            projection->halfExtent = (work->scale * WALL_LAMP_LAST_TEXEL) / projection->otz;
            _acropolisGlowSetSpriteBounds(quad, projection);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), quad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    }
}

#include "../../shared/effects/falling_leaves_task.inc.c"

void acropolisForkedRoadLeafFallTask(Task* task)
{
    _leafFallTask(task);
}

#include "../../shared/effects/falling_leaves_draw.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void acropolisForkedRoadRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void acropolisForkedRoadRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void acropolisForkedRoadRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
