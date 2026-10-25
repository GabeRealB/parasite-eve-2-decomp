#include "stages/akropolis/rooms/forked_road.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "../../room_ids.h"
#include "forked_road_private.h"

#include "gameplay/actor_render.h"
#include "gameplay/captions.h"
#include "gameplay/companion_load.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflow.h"
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
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../../../shared/room/room_visual_effects.h"
#include "../../../../shared/effects/falling_leaves.h"

#include "assets/rooms/akropolis/forked_road/declarations.inc"
#include "assets/engine/animation/player_schema.h"

static void _akropolisForkedRoadOutboundMoviePathTask(Task* task);
static void _akropolisForkedRoadReturnMoviePathTask(Task* task);
static void _akropolisForkedRoadSkipFadeOutTask(Task* task);
static void _akropolisForkedRoadSkipFadeInTask(Task* task);

static void _akropolisForkedRoadReleaseMaggotCaterpillarEntrance(void);

TaskDesc gAkropolisForkedRoadMovieTaskDescriptors[AKROPOLIS_FORKED_ROAD_MOVIE_TASK_COUNT] = {
    [AKROPOLIS_FORKED_ROAD_TASK_OUTBOUND_MOVIE] = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisForkedRoadOutboundMoviePathTask, { .value = 0 } },
    [AKROPOLIS_FORKED_ROAD_TASK_UNUSED_MOVIE]   = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, NULL, { .value = 0 } },
    [AKROPOLIS_FORKED_ROAD_TASK_RETURN_MOVIE]   = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisForkedRoadReturnMoviePathTask, { .value = 0 } },
    [AKROPOLIS_FORKED_ROAD_TASK_SKIP_FADE_OUT]  = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisForkedRoadSkipFadeOutTask, { .value = 0 } },
    [AKROPOLIS_FORKED_ROAD_TASK_SKIP_FADE_IN]   = { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisForkedRoadSkipFadeInTask, { .value = 0 } },
};

/// Player-root translations sampled along the observatory route's movie path.
static SVECTOR _gAkropolisForkedRoadMoviePlayerPositions[300] = {
#include "assets/rooms/akropolis/forked_road/movie_player_path.inc"
};

#include "assets/rooms/akropolis/forked_road/clips.inc"

/// Three set pointers copied to equipped-weapon bank entries 47..49 for the lock-release event.
#include "assets/rooms/akropolis/forked_road/ambush_player_bank.inc"

#include "assets/rooms/akropolis/forked_road/events.inc"
#include "assets/rooms/akropolis/forked_road/markers.inc"
#include "assets/rooms/akropolis/forked_road/view_masks.inc"

#include "../../../../shared/room/room_visual_effects_trail_data.inc.c"
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

// Action-1 control bit 8 selects descent; its parameter0 packs surface class and steps.

// Encoded 8-bit texture pages, palette rows and neutral RGB for the room sprites.

/// Copies a frame-indexed movie-path sample into the borrowed player root.
#define APPLY_MOVIE_PATH_POSITION(movieWork, pathIndex)                                           \
    {                                                                                             \
        (movieWork)->playerMtx->t[0] = _gAkropolisForkedRoadMoviePlayerPositions[(pathIndex)].vx; \
        (movieWork)->playerMtx->t[1] = _gAkropolisForkedRoadMoviePlayerPositions[(pathIndex)].vy; \
        (movieWork)->playerMtx->t[2] = _gAkropolisForkedRoadMoviePlayerPositions[(pathIndex)].vz; \
    }

/// Moves the player along the outbound movie path, then reloads the observatory.
static void _akropolisForkedRoadOutboundMoviePathTask(Task* task)
{
    enum {
        INITIALIZE          = 0,
        APPROACH            = 1,
        WAIT_APPROACH       = 2,
        WAIT_MOVIE          = 3,
        FOLLOW_PATH         = 4,
        DEPART              = 5,
        PATH_EXIT_INDEX     = 120,
        YAW                 = ACTOR_TRANSFORM_ANGLE_TURN / 4,
        APPROACH_X_DISTANCE = 1620,
        DESTINATION_WARP    = 4,
        DESTINATION_ROOM    = 1
    };
    ActorTransform     entryPose;
    ActorTransform     approachTarget;
    u8                 streamArgs[sizeof(gCdCmdQueue.entries[0].args.bytes)];
    RoomMoviePathWork* movieWork;
    CdCmdQueue*        cdQueue;

    cdQueue   = &gCdCmdQueue;
    movieWork = task->work;
    switch (task->state) {
        case INITIALIZE:
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
            entryPose.rot.vy = YAW;
            entryPose.rot.vx = 0;
            entryPose.rot.vz = 0;
            entryPose.pos.vx = _gAkropolisForkedRoadMoviePlayerPositions[0].vx - APPROACH_X_DISTANCE;
            entryPose.pos.vy = _gAkropolisForkedRoadMoviePlayerPositions[0].vy;
            entryPose.pos.vz = _gAkropolisForkedRoadMoviePlayerPositions[0].vz;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, &entryPose, 0);
            task->state = task->state + 1;
            break;

        case APPROACH:
            approachTarget.rot.vy = YAW;
            approachTarget.pos.vx = _gAkropolisForkedRoadMoviePlayerPositions[0].vx;
            approachTarget.pos.vy = _gAkropolisForkedRoadMoviePlayerPositions[0].vy;
            approachTarget.pos.vz = _gAkropolisForkedRoadMoviePlayerPositions[0].vz;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &approachTarget, 0);
            task->state = task->state + 1;
            break;

        case WAIT_APPROACH:
            if (taskMessageDispatch(movieWork->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                // Only byte zero is interpreted by this opcode; preserve the unwritten tail and low-RAM key reads.
                streamArgs[0] = streamFindMovieSlot(&gGameSession->location, 0, 0);
                cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, streamArgs);
                task->state = task->state + 1;
            }
            break;

        case WAIT_MOVIE:
            if (cdQueue->movieReady != 0) {
                movieWork->padScriptTask      = padScriptSpawn(_gAkropolisForkedRoadOutboundMovieVibrationCommands,
                                                               _gAkropolisForkedRoadOutboundMovieVibrationSegments);
                gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE;
                taskReparent(task, movieWork->padScriptTask);
                task->state = task->state + 1;
            }
            break;

        case FOLLOW_PATH:
            // Follow encoded frames in the player root's parent-coordinate space.
            APPLY_MOVIE_PATH_POSITION(movieWork, cdQueue->movieFrame - 1);
            if ((padIsStartPressed() != 0) || ((cdQueue->movieFrame - 1) >= PATH_EXIT_INDEX)) {
                task->state = task->state + 1;
            }
            break;

        case DEPART:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR_ALIAS, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_AKROPOLIS;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = AKROPOLIS_ROOM_ID_OBSERVATORY;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = DESTINATION_WARP;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = DESTINATION_ROOM;
            gDisplayState.spriteVariant                                 = 1;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            gGameSession->padScriptFlags &= (0xFF ^ GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE);
            taskKill(task);
            break;
    }
}

/// Moves the player backwards along the arrival movie path and restores local play.
static void _akropolisForkedRoadReturnMoviePathTask(Task* task)
{
    enum {
        INITIALIZE      = 0,
        WAIT_MOVIE      = 1,
        FOLLOW_PATH     = 2,
        WAIT_EXIT       = 3,
        FRAME_ORIGIN    = 59,
        WALK_EXIT_INDEX = 11,
        REVEAL_TICK     = 3,
        YAW             = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4,
        LOGICAL_VIEW    = 5,
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
        case INITIALIZE:
            task->work = memCalloc(sizeof(RoomMoviePathWork), 0);
            if (task->work == NULL) {
                taskKill(task);
                break;
            }
            ((RoomMoviePathWork*)task->work)->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            ((RoomMoviePathWork*)task->work)->playerMtx  = gPlayerStatus.coordMtx;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            weaponId                              = gPlayerStatus.weapon;
            animationRequest.source.index         = PLAYER_ACTOR_SCENE_WEAPON_BANK(weaponId);
            animationRequest.animationId          = PLAYER_ANIMATION_IDLE;
            animationRequest.blend                = ANIMATION_BLEND_RESET;
            animationRequest.blendFrames          = 0;
            animationRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, ANIMATION_MESSAGE_PLAY, &animationRequest, 0);
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_AND_HOLD, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_HIDDEN;
            task->state                    = task->state + 1;
            break;

        case WAIT_MOVIE:
            if (cdQueue->movieReady != 0) {
                APPLY_MOVIE_PATH_POSITION(movieWork, FRAME_ORIGIN - cdQueue->movieFrame);
                movieWork->padScriptTask      = padScriptSpawn(_gAkropolisForkedRoadReturnMovieVibrationCommands,
                                                               _gAkropolisForkedRoadReturnMovieVibrationSegments);
                gGameSession->padScriptFlags |= GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE;
                taskReparent(task, movieWork->padScriptTask);
                SetDispMask(0);
                task->killCountdown = 0;
                task->state         = task->state + 1;
            }
            break;

        case FOLLOW_PATH:
            // Hide the initial discontinuity for two ticks before revealing the reverse path.
            task->killCountdown = task->killCountdown + 1;
            if (task->killCountdown >= REVEAL_TICK) {
                SetDispMask(1);
            }
            APPLY_MOVIE_PATH_POSITION(movieWork, FRAME_ORIGIN - cdQueue->movieFrame);
            // A completed skip fade places the final pose; normal completion walks there.
            if (movieWork->skipFadeStarted != 0) {
                if (taskPollKill(movieWork->skipFadeTask, &fadeResult) != 0) {
                    exitPose.pos.vx = -0x190;
                    exitPose.pos.vy = 1;
                    exitPose.pos.vz = _gAkropolisForkedRoadMoviePlayerPositions[FRAME_ORIGIN - cdQueue->movieFrame].vz;
                    exitPose.rot.vz = 0;
                    exitPose.rot.vx = 0;
                    exitPose.rot.vy = YAW;
                    TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, &exitPose, 0);
                    taskSpawnFromTable(gAkropolisForkedRoadMovieTaskDescriptors, AKROPOLIS_FORKED_ROAD_TASK_SKIP_FADE_IN, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (padIsStartPressed() != 0) {
                movieWork->skipFadeTask    = taskSpawnFromTable(gAkropolisForkedRoadMovieTaskDescriptors, AKROPOLIS_FORKED_ROAD_TASK_SKIP_FADE_OUT, 0, 0);
                movieWork->skipFadeStarted = 1;
            }
            if ((FRAME_ORIGIN - cdQueue->movieFrame) < WALK_EXIT_INDEX) {
                exitPose.pos.vx = -0x190;
                exitPose.pos.vy = 1;
                exitPose.pos.vz = _gAkropolisForkedRoadMoviePlayerPositions[FRAME_ORIGIN - cdQueue->movieFrame].vz;
                TASK_MESSAGE_DISPATCH_POINTER(((RoomMoviePathWork*)task->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &exitPose, 0);
                task->state = task->state + 1;
            }
            break;

        case WAIT_EXIT:
            if (taskMessageDispatch(movieWork->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                taskMessageDispatch(movieWork->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = viewFindLogicalIndex(LOGICAL_VIEW);
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_SHOW_HUD, 0, 0);
                padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR_ALIAS, PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                gGameSession->padScriptFlags  &= (0xFF ^ GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE);
                taskKill(task);
            }
            break;
    }
}

#undef APPLY_MOVIE_PATH_POSITION

/// Darkens the return ride before its skip caller replaces the player's pose.
static void _akropolisForkedRoadSkipFadeOutTask(Task* task)
{
    enum { STEP = 32,
           SPAN = 256 };
    u8  fadeLevel;
    s16 nextProgress;

    fadeLevel = (u8)task->killCountdown;
    fadeDrawOverlay(fadeLevel, fadeLevel, fadeLevel, GPU_BLEND_SUBTRACT);
    nextProgress        = (u16)task->killCountdown + STEP;
    task->killCountdown = nextProgress;
    if (nextProgress >= SPAN) {
        taskRequestKill(task, 0);
    }
}

/// Reveals the return ride's final pose after its skip fade has hidden the scene.
static void _akropolisForkedRoadSkipFadeInTask(Task* task)
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
static void _akropolisForkedRoadReleaseMaggotCaterpillarEntrance(void)
{
    gSceneCombatState.maggotCaterpillarEntranceReady = true;
}

void akropolisForkedRoadInitializeRoomEffectsTask(Task* task)
{
    enum {
        ROOM_EFFECTS_INITIALIZE = 0,
        CELL_0_SMALL_END        = 2,
        CELL_0_LARGE_END        = 4,
        CELL_1_END              = 12,
        LAMP_SCALE_512          = 512 << 16,
        LAMP_SCALE_768          = 768 << 16,
        LAMP_CELL_1             = 1 << 8,
        LAMP_CELL_2             = 2 << 8
    };
    GfxCoord* effectCoord;
    s32       lampIndex;

    effectCoord = task->extra.coordBody->coord;
    if (task->state == ROOM_EFFECTS_INITIALIZE) {
        for (lampIndex = 0; lampIndex < CELL_0_SMALL_END; lampIndex++) {
            effectSpawn(EFFECT_AKROPOLIS_FORKED_ROAD_WALL_LAMP, effectCoord, lampIndex + LAMP_SCALE_512, &_gAkropolisForkedRoadWallLampPositions[lampIndex]);
        }
        for (lampIndex = CELL_0_SMALL_END; lampIndex < CELL_0_LARGE_END; lampIndex++) {
            effectSpawn(EFFECT_AKROPOLIS_FORKED_ROAD_WALL_LAMP, effectCoord, lampIndex + LAMP_SCALE_768, &_gAkropolisForkedRoadWallLampPositions[lampIndex]);
        }
        for (lampIndex = CELL_0_LARGE_END; lampIndex < CELL_1_END; lampIndex++) {
            effectSpawn(EFFECT_AKROPOLIS_FORKED_ROAD_WALL_LAMP, effectCoord, lampIndex + (LAMP_SCALE_512 | LAMP_CELL_1), &_gAkropolisForkedRoadWallLampPositions[lampIndex]);
        }
        for (lampIndex = CELL_1_END; lampIndex < (s32)ARRAY_SIZE(_gAkropolisForkedRoadWallLampPositions); lampIndex++) {
            effectSpawn(EFFECT_AKROPOLIS_FORKED_ROAD_WALL_LAMP, effectCoord, lampIndex + LAMP_CELL_2, &_gAkropolisForkedRoadWallLampPositions[lampIndex]);
        }
        gRoomEffectFlashId      = EFFECT_AKROPOLIS_FORKED_ROAD_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_AKROPOLIS_FORKED_ROAD_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_AKROPOLIS_FORKED_ROAD_SPARK_BURST;
        task->state             = task->state + 1;
    }
}

#include "../../../../shared/effects/acropolis_glow_sprite_bounds.inc.c"

void akropolisForkedRoadWallLampTask(Task* task)
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
        ((_gAkropolisForkedRoadWallLampViewMasks[task->spawnArg1.value & WALL_LAMP_INDEX_MASK] >> (gGameSession->location.view - 1)) & 1)) {
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->otz), quad);
        }
        SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    }
}

#include "../../../../shared/effects/falling_leaves_task.inc.c"

void akropolisForkedRoadLeafFallTask(Task* task)
{
    _leafFallTask(task);
}

#include "../../../../shared/effects/falling_leaves_draw.inc.c"

#include "../../../../shared/room/room_visual_effects.inc.c"

#include "../../../../shared/room/room_visual_effects_flash_task.inc.c"

void akropolisForkedRoadRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../../../shared/room/room_visual_effects_trails.inc.c"

void akropolisForkedRoadRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../../../shared/room/room_visual_effects_trail_task.inc.c"

    void akropolisForkedRoadRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../../../shared/room/room_visual_effects_glow.inc.c"
