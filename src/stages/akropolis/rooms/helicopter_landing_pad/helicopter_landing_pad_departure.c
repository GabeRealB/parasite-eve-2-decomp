#include "rooms/akropolis_helicopter_landing_pad.h"

#include "config.h"
#include "../../event_flags.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "helicopter_landing_pad_private.h"

#include "gameplay/actor_presentation.h"
#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/companion_load.h"
#include "gameplay/direction.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_state.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/random.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#define ROOM_PART declarations_helicopter_landing_pad_departure
#include "assets/rooms/akropolis/helicopter_landing_pad/declarations.inc"

static Task* _gAkropolisHelicopterLandingPadPlayerPitchTask;

static void _akropolisHelicopterLandingPadSpawnScreenShake(s32 halfDurationFrames, s32 amplitudePixels);
static void _akropolisHelicopterLandingPadInitializeRoomTask(Task* task);

/// Unused format for a label followed by three signed, minimum-five-column integers.
static const char _gAkropolisHelicopterLandingPadUnusedLabelTripleFormat[] = "%s (%5d,%5d,%5d)";

/// Room-task lifecycle: initialize, update the encounter phase, release.
static const TaskFuncTable3 _gAkropolisHelicopterLandingPadRoomTaskStates = {
    { _akropolisHelicopterLandingPadInitializeRoomTask, akropolisHelicopterLandingPadUpdateEncounterPhase, taskKill },
};

#include "assets/rooms/akropolis/helicopter_landing_pad/action_triggers.inc"
#include "assets/rooms/akropolis/helicopter_landing_pad/occluders.inc"
#include "assets/rooms/akropolis/helicopter_landing_pad/placements.inc"
#include "assets/rooms/akropolis/helicopter_landing_pad/lights.inc"
#include "assets/rooms/akropolis/helicopter_landing_pad/sprites.inc"
#include "assets/rooms/akropolis/helicopter_landing_pad/cameras.inc"
#define ROOM_PART gAkropolisHelicopterLandingPadLethalStrikeVibrationCommands
#include "assets/rooms/akropolis/helicopter_landing_pad/vibrations.inc"
#include "assets/rooms/akropolis/helicopter_landing_pad/surfaces.inc"
static TmdBone _gAkropolisHelicopterLandingPadModel0A8E8Skeleton[1] = {
#include "assets/akropolis_helicopter_landing_pad_model_0A8E8_skeleton.inc"
};

static u32 _gAkropolisHelicopterLandingPadModel0A8E8PartVerts[1] = {
#include "assets/akropolis_helicopter_landing_pad_model_0A8E8_partVerts.inc"
};

static SVECTOR _gAkropolisHelicopterLandingPadModel0A8E8Verts[13] = {
#include "assets/akropolis_helicopter_landing_pad_model_0A8E8_verts.inc"
};

static SVECTOR _gAkropolisHelicopterLandingPadModel0A8E8Normals[6] = {
#include "assets/akropolis_helicopter_landing_pad_model_0A8E8_normals.inc"
};

static u32 _gAkropolisHelicopterLandingPadModel0A8E8Stream[42] = {
#include "assets/akropolis_helicopter_landing_pad_model_0A8E8_stream.inc"
};

TmdSource gAkropolisHelicopterLandingPadModel0A8E8 = {
    0,
    312,
    0,
    1,
    _gAkropolisHelicopterLandingPadModel0A8E8PartVerts,
    _gAkropolisHelicopterLandingPadModel0A8E8Verts,
    _gAkropolisHelicopterLandingPadModel0A8E8Normals,
    _gAkropolisHelicopterLandingPadModel0A8E8Skeleton,
    _gAkropolisHelicopterLandingPadModel0A8E8Stream,
};

/// Current unwrapped yaw of the singleton player-turn task.
static s32 _gAkropolisHelicopterLandingPadTurnYaw = 0;

/// Signed yaw increment of the singleton player-turn task, in angle units per update.
static s32 _gAkropolisHelicopterLandingPadTurnStep = 0;

/// Q12 weight of the persistent player model-part pitch pulse.
static s16 _gAkropolisHelicopterLandingPadPitchWeight = 0;

/// Borrowed singleton task that pulses the player's model-part pitch.
static Task* _gAkropolisHelicopterLandingPadPlayerPitchTask = NULL;

s32 gAkropolisHelicopterLandingPadPostEncounterPlacementEnabled = 0;

SVECTOR ActorContact_ScratchPosition = { 0, 0, 0, 0 };

RoomEventMsg gAkropolisHelicopterLandingPadExitRequest = { 0, 0, 0, 0, 0, 0 };

/// Composes one weighted pitch pulse into the player's borrowed model coordinate.
static inline void _akropolisHelicopterLandingPadApplyPlayerPitchPulse(GfxCoord* playerPart, s32 weight)
{
    enum {
        AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_MAX_ANGLE  = 96,
        AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_ONE = 4096,
    };

    gfxRotMatrixX(&playerPart->coord, -(weight * AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_MAX_ANGLE) / AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_ONE, GRAPHICS_ROTATION_COMPOSE);
    playerPart->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(playerPart);
}

/// Samples one triangular shake envelope point, advancing the shared random sequence.
static inline s32 _akropolisHelicopterLandingPadSampleScreenShake(Task* task, s32 packedShake, s32 halfDurationFrames)
{
    enum {
        AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_AMPLITUDE_SHIFT      = 8,
        AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_RANDOM_FRACTION_BITS = 16,
    };
    s32 shakeSample;
    s32 envelopeAmplitude;

    shakeSample       = halfDurationFrames - ABS(task->spawnArg1.value);
    envelopeAmplitude = shakeSample * (packedShake >> AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_AMPLITUDE_SHIFT);
    RANDOM_LCG_ADVANCE();
    shakeSample = (envelopeAmplitude * (s32)(gRandomLcgState >> AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_RANDOM_FRACTION_BITS)) / halfDurationFrames >> AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_RANDOM_FRACTION_BITS;
    if (task->spawnArg1.value & 1) {
        shakeSample = ABS(shakeSample);
    } else {
        shakeSample = -ABS(shakeSample);
    }
    return shakeSample;
}

void akropolisHelicopterLandingPadPrepareDepartureTask(Task* task)
{
    enum {
        AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_SETUP            = 0,
        AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_WAIT_VIEW        = 1,
        AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_EXIT_ACTORS      = 2,
        AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_WAIT_ACTOR_EXIT  = 3,
        AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_RESTORE_VIEW     = 4,
        AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_START_LIFT       = 5,
        AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_LOAD_RESOURCES   = 6,
        AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_WAIT_LOAD        = 7,
        AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_SPAWN_PLACEMENTS = 8,
        AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_START_SCENE      = 9,
        AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VARIANT          = 2,
        AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VIEW             = 18,
        AKROPOLIS_HELICOPTER_LANDING_PAD_LIFT_CHILD                 = 40,
        AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_PLAYER_CLIP      = 9,
        AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_DELAY_FRAMES     = 120,
        AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_SOUND_REMAINING  = 90,
        AKROPOLIS_HELICOPTER_LANDING_PAD_REVISIT_HIDDEN_ACTOR       = 1,
        AKROPOLIS_HELICOPTER_LANDING_PAD_ACTOR_HIDE_MODEL           = 0,
        AKROPOLIS_HELICOPTER_LANDING_PAD_CD_FILE_KEY_BYTES          = 4,
    };
    u8        fileKey[AKROPOLIS_HELICOPTER_LANDING_PAD_CD_FILE_KEY_BYTES];
    u8        loadArgs[sizeof(gCdCmdQueue.entries[0].args.bytes)];
    SVECTOR   ambientColor;
    Task*     liftTask;
    GfxCoord* liftCoord;

    /// Enqueues the stage-zero departure bundle with its image-placement offsets.
#define AKROPOLIS_HELICOPTER_LANDING_PAD_QUEUE_DEPARTURE_RESOURCES(fileKey, loadArgs) \
    {                                                                                 \
        enum {                                                                        \
            AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_FILE_GROUP    = 51,            \
            AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_FILE_HUNDREDS = 10,            \
            AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_IMAGE_X_PAGES = 3,             \
            AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_IMAGE_Y_ROWS  = 5,             \
        };                                                                            \
        (fileKey)[2]  = AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_FILE_GROUP;        \
        (loadArgs)[0] = AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_FILE_HUNDREDS;     \
        (loadArgs)[2] = AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_IMAGE_X_PAGES;     \
        (fileKey)[3]  = 0;                                                            \
        (fileKey)[0]  = 0;                                                            \
        (loadArgs)[1] = CD_COMMAND_LOAD_DEFAULT;                                      \
        (loadArgs)[3] = AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_IMAGE_Y_ROWS;      \
        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, (fileKey), (loadArgs));                    \
        func_800ABFF8();                                                              \
        func_800AC000();                                                              \
    }

    switch (task->state) {
        case AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_SETUP:
            task->spawnArg1.value = 0;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            task->state += 1;
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.variant >= AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VARIANT) {
                taskMessageDispatch(sceneFindPlacedActor(AKROPOLIS_HELICOPTER_LANDING_PAD_REVISIT_HIDDEN_ACTOR), ACTOR_MESSAGE_SET_MODEL_DRAW, AKROPOLIS_HELICOPTER_LANDING_PAD_ACTOR_HIDE_MODEL, 0);
            }
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VIEW;
            ambientColor.vx                                        = 1200;
            ambientColor.vy                                        = 1200;
            ambientColor.vz                                        = 1552;
            worldCoordSetAmbientColorOverride(&ambientColor);
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_WAIT_VIEW:
            if (gGameSession->viewReady != 0) {
                task->state += 1;
            }
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_EXIT_ACTORS:
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.variant < AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VARIANT) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_EXIT_PLACED_ACTORS, 0, 0);
            }
            task->state += 1;
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_WAIT_ACTOR_EXIT:
            task->state += 1;
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_RESTORE_VIEW:
            loadingRequestViewGraphicsRestore();
            task->state += 1;
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_START_LIFT:
            // Place in the lift's frame before attaching the player to its root.
            TASK_MESSAGE_DISPATCH_SECOND_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_FIND_OTHER_CHILD, AKROPOLIS_HELICOPTER_LANDING_PAD_LIFT_CHILD, &liftTask);
            TASK_MESSAGE_DISPATCH_POINTER(liftTask, ACTOR_MESSAGE_PLAY_ANIMATION, &gAkropolisHelicopterLandingPadLiftRaiseRequest, 0);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &gAkropolisHelicopterLandingPadLiftPlayerPlacement, 0);
            gAkropolisHelicopterLandingPadLiftPlayerAnimationRequest.animationId  = AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_PLAYER_CLIP;
            gAkropolisHelicopterLandingPadLiftPlayerAnimationRequest.source.index = gPlayerStatus.weapon + 1;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &gAkropolisHelicopterLandingPadLiftPlayerAnimationRequest, 0);
            liftCoord = liftTask->extra.tmd->coords;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_ATTACH_TO_COORD, liftCoord, 0);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &gAkropolisHelicopterLandingPadLiftPlayerPlacement, 0);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            task->state += 1;
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_LOAD_RESOURCES:
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.variant < AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VARIANT) {
                AKROPOLIS_HELICOPTER_LANDING_PAD_QUEUE_DEPARTURE_RESOURCES(fileKey, loadArgs);
            }
            task->spawnArg1.value = AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_DELAY_FRAMES;
            task->state          += 1;
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_WAIT_LOAD:
            task->spawnArg1.value -= 1;
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.variant >= AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VARIANT) {
                task->state = AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_START_SCENE;
            } else if (cdCmdIsIdle()) {
                loadingRequestViewGraphicsRestore();
                task->state += 1;
            }
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_SPAWN_PLACEMENTS:
            task->spawnArg1.value -= 1;
            // The newly loaded actor variant must be spawned before the scene script.
            areaSetPlacementVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location, AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VARIANT, AREA_VARIANT_RESET_ALWAYS);
            areaSyncLocationVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location);
            areaSpawnPlacements(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location);
            gAkropolisHelicopterLandingPadEncounterPhase = AKROPOLIS_HELICOPTER_LANDING_PAD_PHASE_DEPARTURE_READY;
            task->state                                 += 1;
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_START_SCENE:
            task->spawnArg1.value -= 1;
            if (task->spawnArg1.value <= 0) {
                evsStartScriptWithSkip(gAkropolisHelicopterLandingPadDepartureScript, EVENT_SCRIPT_HUD_KEEP,
                                       gAkropolisHelicopterLandingPadDepartureSkipScript);
                taskKill(task);
            }
            break;
    }
    if (task->spawnArg1.value == AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_SOUND_REMAINING) {
        sndEvtRequestScriptStart(AKROPOLIS_HELICOPTER_LANDING_PAD_SOUND(AKROPOLIS_HELICOPTER_LANDING_PAD_SOUND_LIFT_MOVE), 0, 0);
    }
}
#undef AKROPOLIS_HELICOPTER_LANDING_PAD_QUEUE_DEPARTURE_RESOURCES

void akropolisHelicopterLandingPadDepartureShakeTimelineTask(Task* task)
{
    enum {
        AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_FIRST_FRAME        = 0,
        AKROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_FIRST_FRAME    = 7,
        AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_REPEAT_FRAME       = 200,
        AKROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_REPEAT_FRAME   = 207,
        AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_SECOND_FRAME       = 410,
        AKROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_SECOND_FRAME   = 421,
        AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_THIRD_FRAME        = 520,
        AKROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_THIRD_FRAME    = 535,
        AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_TIMELINE_END_FRAME = 640,
    };

    // Screen impulses precede the corresponding controller vibration cues.
    switch (task->state) {
        case AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_REPEAT_FRAME:
        case AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_FIRST_FRAME:
            _akropolisHelicopterLandingPadSpawnScreenShake(15, 2);
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_FIRST_FRAME:
        case AKROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_REPEAT_FRAME:
            padScriptSpawn(_gAkropolisHelicopterLandingPadDepartureFirstVibrationCommands, _gAkropolisHelicopterLandingPadDepartureFirstVibrationSegments);
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_SECOND_FRAME:
            _akropolisHelicopterLandingPadSpawnScreenShake(22, 3);
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_SECOND_FRAME:
            padScriptSpawn(_gAkropolisHelicopterLandingPadDepartureSecondVibrationCommands, _gAkropolisHelicopterLandingPadDepartureSecondVibrationSegments);
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_THIRD_FRAME:
            _akropolisHelicopterLandingPadSpawnScreenShake(30, 4);
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_THIRD_FRAME:
            padScriptSpawn(_gAkropolisHelicopterLandingPadDepartureThirdVibrationCommands, _gAkropolisHelicopterLandingPadDepartureThirdVibrationSegments);
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_TIMELINE_END_FRAME:
            taskKill(task);
            break;
    }
    task->state += 1;
}

void akropolisHelicopterLandingPadReturnToMistTask(Task* task)
{
    enum {
        AKROPOLIS_HELICOPTER_LANDING_PAD_RETURN_PREPARE        = 0,
        AKROPOLIS_HELICOPTER_LANDING_PAD_RETURN_WAIT_MOVIE     = 1,
        AKROPOLIS_HELICOPTER_LANDING_PAD_RETURN_RELOAD         = 2,
        AKROPOLIS_HELICOPTER_LANDING_PAD_ITEM_MICRO_DEVICE     = 0x102,
        AKROPOLIS_HELICOPTER_LANDING_PAD_RETURN_SCENE_EVENT    = 1,
        AKROPOLIS_HELICOPTER_LANDING_PAD_RETURN_WARP           = 1,
        AKROPOLIS_HELICOPTER_LANDING_PAD_RETURN_ROOM           = 1,
        AKROPOLIS_HELICOPTER_LANDING_PAD_RETURN_SPRITE_VARIANT = 1,
        AKROPOLIS_HELICOPTER_LANDING_PAD_RETURN_MOVIE_TASK     = 0,
    };

    switch (task->state) {
        case AKROPOLIS_HELICOPTER_LANDING_PAD_RETURN_PREPARE:
            // Reset Acropolis rewards and progress before the streamed interlude.
            playerStateRestoreFullHpMp();
            inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_PARTHENON_KEY);
            inventoryClearCollectedBit(AKROPOLIS_HELICOPTER_LANDING_PAD_ITEM_MICRO_DEVICE);
            itemSetIdentified(AKROPOLIS_HELICOPTER_LANDING_PAD_ITEM_MICRO_DEVICE, true);
            gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
            gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_STORY_DIALOGUE_STILL_HERE);
            taskSpawnFromTable(gAkropolisHelicopterLandingPadMovieTasks, AKROPOLIS_HELICOPTER_LANDING_PAD_RETURN_MOVIE_TASK, 0, 0);
            task->state += 1;
            return;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_RETURN_WAIT_MOVIE:
            task->state = AKROPOLIS_HELICOPTER_LANDING_PAD_RETURN_RELOAD;
            return;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_RETURN_RELOAD:
            // The movie task holds the task list while playback owns the display.
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent         = AKROPOLIS_HELICOPTER_LANDING_PAD_RETURN_SCENE_EVENT;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_AKROPOLIS;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = AKROPOLIS_ROOM_ID_MIST_R18;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = AKROPOLIS_HELICOPTER_LANDING_PAD_RETURN_WARP;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = AKROPOLIS_HELICOPTER_LANDING_PAD_RETURN_ROOM;
            gDisplayState.spriteVariant                                 = AKROPOLIS_HELICOPTER_LANDING_PAD_RETURN_SPRITE_VARIANT;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            displayReleaseMenuHold();
            taskKill(task);
            return;
    }
}

void akropolisHelicopterLandingPadTurnPlayerYawTask(Task* task)
{
    GameActor* player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    s32        wrappedYaw;
    s32        turnValue; // Wrapped distance/comparison scratch, then the stepped yaw
    s32        currentDistance;
    s32        targetYaw;
    s32        currentYaw;

    enum {
        AKROPOLIS_HELICOPTER_LANDING_PAD_TURN_INITIALIZE = 0,
        AKROPOLIS_HELICOPTER_LANDING_PAD_TURN_UPDATE     = 1,
        AKROPOLIS_HELICOPTER_LANDING_PAD_TURN_STEP       = 256,
    };

    /// Tests whether the wrapped yaw is strictly closer to the target than the current yaw.
#define AKROPOLIS_HELICOPTER_LANDING_PAD_COMPARE_YAW_DISTANCES(wrapped, current, target, comparison, currentDistance) \
    do {                                                                                                              \
        (comparison) = (wrapped) - (target);                                                                          \
        if ((comparison) < 0) {                                                                                       \
            (comparison) = -(comparison);                                                                             \
        }                                                                                                             \
        (currentDistance) = (current) - (target);                                                                     \
        if ((currentDistance) < 0) {                                                                                  \
            (currentDistance) = -(currentDistance);                                                                   \
        }                                                                                                             \
        (comparison) = (comparison) < (currentDistance);                                                              \
    } while (0)

    switch (task->state) {
        case AKROPOLIS_HELICOPTER_LANDING_PAD_TURN_INITIALIZE:
            _gAkropolisHelicopterLandingPadTurnYaw = player->rotation.vy & ACTOR_TRANSFORM_ANGLE_MASK;
            if (task->spawnArg1.value < _gAkropolisHelicopterLandingPadTurnYaw) {
                wrappedYaw = _gAkropolisHelicopterLandingPadTurnYaw - ACTOR_TRANSFORM_ANGLE_TURN;
            } else {
                wrappedYaw = _gAkropolisHelicopterLandingPadTurnYaw + ACTOR_TRANSFORM_ANGLE_TURN;
            }
            targetYaw  = task->spawnArg1.value;
            currentYaw = _gAkropolisHelicopterLandingPadTurnYaw;
            // Compare both unwrapped paths before choosing the turn direction.
            AKROPOLIS_HELICOPTER_LANDING_PAD_COMPARE_YAW_DISTANCES(wrappedYaw, currentYaw, targetYaw, turnValue, currentDistance);
            if (turnValue) {
                _gAkropolisHelicopterLandingPadTurnYaw = wrappedYaw;
            }
            if (task->spawnArg1.value > _gAkropolisHelicopterLandingPadTurnYaw) {
                _gAkropolisHelicopterLandingPadTurnStep = AKROPOLIS_HELICOPTER_LANDING_PAD_TURN_STEP;
            } else {
                _gAkropolisHelicopterLandingPadTurnStep = -AKROPOLIS_HELICOPTER_LANDING_PAD_TURN_STEP;
            }
            task->state++;
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_TURN_UPDATE:
            turnValue                              = _gAkropolisHelicopterLandingPadTurnYaw + _gAkropolisHelicopterLandingPadTurnStep;
            _gAkropolisHelicopterLandingPadTurnYaw = turnValue;
            if (_gAkropolisHelicopterLandingPadTurnStep > 0) {
                targetYaw = task->spawnArg1.value;
                if (targetYaw < turnValue) {
                    _gAkropolisHelicopterLandingPadTurnYaw = targetYaw;
                    taskKill(task);
                }
            }
            if (_gAkropolisHelicopterLandingPadTurnStep < 0) {
                if (_gAkropolisHelicopterLandingPadTurnYaw < task->spawnArg1.value) {
                    _gAkropolisHelicopterLandingPadTurnYaw = task->spawnArg1.value;
                    taskKill(task);
                }
            }
            // Keep the endpoint test strict: an exact arrival runs one more update.
            player->rotation.vy = _gAkropolisHelicopterLandingPadTurnYaw;
            break;
    }
}
#undef AKROPOLIS_HELICOPTER_LANDING_PAD_COMPARE_YAW_DISTANCES

void akropolisHelicopterLandingPadPlayerPitchPulseTask(Task* task)
{
    enum {
        AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_PART        = 4,
        AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_ONE  = 4096,
        AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_STEP = 400,
    };
    GfxCoord* playerPart;
    // These unused aggregate roles are unproven; their stack slots and stores remain in the image.
    SVECTOR unusedShortVectorBefore;
    VECTOR  unusedVectorBefore;
    VECTOR  unusedVector;
    MATRIX  unusedMatrix;
    SVECTOR unusedShortVectorAfter;

    playerPart      = &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_PART];
    unusedVector.vx = -0x249;
    unusedVector.vy = 0;
    unusedVector.vz = 0xB8;

    switch (task->state) {
        case AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_IDLE:
            _gAkropolisHelicopterLandingPadPitchWeight = 0;
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_RISE:
            _akropolisHelicopterLandingPadApplyPlayerPitchPulse(playerPart, _gAkropolisHelicopterLandingPadPitchWeight);
            _gAkropolisHelicopterLandingPadPitchWeight += AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_STEP;
            if (_gAkropolisHelicopterLandingPadPitchWeight > AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_ONE) {
                _gAkropolisHelicopterLandingPadPitchWeight = AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_ONE;
                task->state                                = AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_FALL;
            }
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_FALL:
            _akropolisHelicopterLandingPadApplyPlayerPitchPulse(playerPart, _gAkropolisHelicopterLandingPadPitchWeight);
            _gAkropolisHelicopterLandingPadPitchWeight -= AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_STEP;
            if (_gAkropolisHelicopterLandingPadPitchWeight < 0) {
                _gAkropolisHelicopterLandingPadPitchWeight = 0;
                task->state                                = AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_IDLE;
            }
            break;
    }
}

s32 akropolisHelicopterLandingPadResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_CAP_SLOT = 9,
        AKROPOLIS_HELICOPTER_LANDING_PAD_EXIT_STOP_SELECTOR = -1,
        AKROPOLIS_HELICOPTER_LANDING_PAD_EXIT_STOP_CONTROL  = 30,
    };

    *reply = *request;
    if (request->areaId == AKROPOLIS_ROOM_ID_FIRE_ESCAPE) {
        if (gAkropolisHelicopterLandingPadEncounterPhase == AKROPOLIS_HELICOPTER_LANDING_PAD_PHASE_WAITING) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                // This exact-entry selector matches idle sound slots; its purpose is unproven.
                sndEvtRequestScriptStop(AKROPOLIS_HELICOPTER_LANDING_PAD_EXIT_STOP_SELECTOR, AKROPOLIS_HELICOPTER_LANDING_PAD_EXIT_STOP_CONTROL);
            }
            return ROOM_EVENT_DEPART;
        }
        if (gAkropolisHelicopterLandingPadEncounterPhase == AKROPOLIS_HELICOPTER_LANDING_PAD_PHASE_ENCOUNTER_FINISHED) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capStartSequenceSlot(AKROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
            }
            return ROOM_EVENT_STAY;
        }
        return ROOM_EVENT_STAY;
    }
    return ROOM_EVENT_DEPART;
}

s32 akropolisHelicopterLandingPadRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 akropolisHelicopterLandingPadHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum {
        AKROPOLIS_HELICOPTER_LANDING_PAD_ACTION_START_ENCOUNTER  = 0,
        AKROPOLIS_HELICOPTER_LANDING_PAD_ACTION_LATCH_PLACEMENT  = 1,
        AKROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_WAITING       = 0,
        AKROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_STARTED       = 1,
        AKROPOLIS_HELICOPTER_LANDING_PAD_PLACEMENT_TRIGGER       = 4,
        AKROPOLIS_HELICOPTER_LANDING_PAD_CAP_INTERACTION_TRIGGER = 8,
        AKROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_MUSIC_ENTRY   = 1,
    };
    u8                     actionId;
    WorldCollisionTrigger* placementTrigger;
    WorldCollisionTrigger* capInteractionTrigger;

    if ((request->actionId == AKROPOLIS_HELICOPTER_LANDING_PAD_ACTION_START_ENCOUNTER) && (gAkropolisHelicopterLandingPadEncounterPhase == AKROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_WAITING) && (gAkropolisHelicopterLandingPadEncounterReady != 0)) {
        // Swap the encounter triggers before normal play resumes.
        gGameSession->flowFlags   = (GAME_SESSION_FLOW_SKIP_AREA_MUSIC | GAME_SESSION_FLOW_REEQUIP_WEAPON);
        gStageCountdownMusicEntry = AKROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_MUSIC_ENTRY;
        evsStartScriptWithSkip(gAkropolisHelicopterLandingPadEncounterIntroScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gAkropolisHelicopterLandingPadEncounterIntroSkipScript);
        gAkropolisHelicopterLandingPadEncounterPhase = AKROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_STARTED;
        placementTrigger                             = &gAkropolisHelicopterLandingPadActionTriggers[AKROPOLIS_HELICOPTER_LANDING_PAD_PLACEMENT_TRIGGER];
        capInteractionTrigger                        = &gAkropolisHelicopterLandingPadActionTriggers[AKROPOLIS_HELICOPTER_LANDING_PAD_CAP_INTERACTION_TRIGGER];
        placementTrigger->flags                     |= WORLD_COLLISION_TRIGGER_ENABLED;
        capInteractionTrigger->flags                &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    }
    actionId = request->actionId;
    if (actionId == AKROPOLIS_HELICOPTER_LANDING_PAD_ACTION_LATCH_PLACEMENT) {
        gAkropolisHelicopterLandingPadPostEncounterPlacementEnabled = actionId;
    }
    return 0;
}

s32 akropolisHelicopterLandingPadHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum { AKROPOLIS_HELICOPTER_LANDING_PAD_COMMAND_CONFIRM_DEPARTURE = 4 };

    if ((commandId == AKROPOLIS_HELICOPTER_LANDING_PAD_COMMAND_CONFIRM_DEPARTURE) && (gAkropolisHelicopterLandingPadEncounterPhase == AKROPOLIS_HELICOPTER_LANDING_PAD_PHASE_ENCOUNTER_FINISHED)) {
        taskSpawnFromTable(gAkropolisHelicopterLandingPadScriptTasks, AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_CONFIRM_DEPARTURE, 0, 0);
    }
    return 0;
}

void akropolisHelicopterLandingPadStartDepartureShakeTimeline(void)
{
    taskSpawnFromTable(gAkropolisHelicopterLandingPadScriptTasks, AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_SHAKE_TIMELINE, 0, 0);
}

void akropolisHelicopterLandingPadStartPlayerMoveToSceneMark(void)
{
    taskSpawnFromTable(gAkropolisHelicopterLandingPadScriptTasks, AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_MOVE_PLAYER, 0, 0);
}

/// Spawns one triangular vertical shake from a duration and signed pixel amplitude.
static void _akropolisHelicopterLandingPadSpawnScreenShake(s32 halfDurationFrames, s32 amplitudePixels)
{
    enum { AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_AMPLITUDE_SHIFT = 8 };

    taskSpawnFromTable(gAkropolisHelicopterLandingPadScriptTasks, AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_SCREEN_SHAKE, 0, halfDurationFrames | (amplitudePixels << AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_AMPLITUDE_SHIFT));
}

void akropolisHelicopterLandingPadStartReturnToMist(void)
{
    taskSpawnFromTable(gAkropolisHelicopterLandingPadScriptTasks, AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_RETURN_TO_MIST, 0, 0);
}

void akropolisHelicopterLandingPadPlacePlayerAfterEncounter(void)
{
    if (gAkropolisHelicopterLandingPadPostEncounterPlacementEnabled != 0) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &gAkropolisHelicopterLandingPadPostEncounterPlayerPlacement, 0);
    }
}

void akropolisHelicopterLandingPadStartPlayerYawTurn(s32 targetYaw)
{
    taskSpawnFromTable(gAkropolisHelicopterLandingPadScriptTasks, AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_TURN_PLAYER, targetYaw, 0);
}

void akropolisHelicopterLandingPadReleaseEncounterBattle(void)
{
    enum {
        AKROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_ACTOR         = 0,
        AKROPOLIS_HELICOPTER_LANDING_PAD_BATTLE_END_DELAY_FRAMES = 3,
    };

    // The resident API ignores its second argument; rewards come from the actor.
    sceneReleaseBattleRefWithRewards(sceneFindPlacedActor(AKROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_ACTOR), 0x1B);
    gSceneCombatState.signals.bytes.endDelayFrames = AKROPOLIS_HELICOPTER_LANDING_PAD_BATTLE_END_DELAY_FRAMES;
}

void akropolisHelicopterLandingPadLockAttachmentsForEncounter(void)
{
    roomEffectRequestCancelAll();
    gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

void akropolisHelicopterLandingPadSetPlayerPitchPulseState(s32 pulseState)
{
    _gAkropolisHelicopterLandingPadPlayerPitchTask->state = pulseState;
}

void akropolisHelicopterLandingPadMovePlayerToSceneMarkTask(Task* task)
{
    enum {
        AKROPOLIS_HELICOPTER_LANDING_PAD_MOVE_START         = 0,
        AKROPOLIS_HELICOPTER_LANDING_PAD_MOVE_WAIT          = 1,
        AKROPOLIS_HELICOPTER_LANDING_PAD_MOVE_APPROACH_CLIP = 12,
        AKROPOLIS_HELICOPTER_LANDING_PAD_MOVE_ARRIVAL_CLIP  = 9,
    };
    GameActorMoveAnim moveAnim;

    switch (task->state) {
        case AKROPOLIS_HELICOPTER_LANDING_PAD_MOVE_START:
            moveAnim.approachAnimId = AKROPOLIS_HELICOPTER_LANDING_PAD_MOVE_APPROACH_CLIP;
            moveAnim.arrivalAnimId  = AKROPOLIS_HELICOPTER_LANDING_PAD_MOVE_ARRIVAL_CLIP;
            TASK_MESSAGE_DISPATCH_POINTERS(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_MOVE_TO, &gAkropolisHelicopterLandingPadSceneMarkStorage.destination, &moveAnim);
            task->state++;
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_MOVE_WAIT:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                taskKill(task);
            }
            break;
    }
}

void akropolisHelicopterLandingPadScreenShakeTask(Task* task)
{
    enum {
        AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_INITIALIZE    = 0,
        AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_UPDATE        = 1,
        AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_DURATION_MASK = 0xFF,
    };
    s32 packedShake;
    s32 halfDurationFrames;
    s32 shakeSample;

    packedShake        = task->spawnArg2.value;
    halfDurationFrames = packedShake & AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_DURATION_MASK;

    switch (task->state) {
        case AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_INITIALIZE:
            task->spawnArg1.value = -halfDurationFrames;
            task->state++;
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_UPDATE:
            if (halfDurationFrames < task->spawnArg1.value) {
                displaySetShakeY(0);
                taskKill(task);
            } else {
                // Grow and decay the envelope around the middle frame.
                shakeSample = _akropolisHelicopterLandingPadSampleScreenShake(task, packedShake, halfDurationFrames);
                displaySetShakeY(shakeSample);
                task->spawnArg1.value++;
            }
            break;
    }
}

void akropolisHelicopterLandingPadConfirmDepartureTask(Task* task)
{
    enum {
        AKROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_START         = 0,
        AKROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_CHECK_CHOICE  = 1,
        AKROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_DELAY         = 2,
        AKROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_WAIT_HOLD     = 3,
        AKROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_DEPART        = 4,
        AKROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_CAP_SLOT      = 4,
        AKROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_CANCEL_CHOICE = 1,
    };

    switch (task->state) {
        case AKROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_START:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capStartSequenceSlot(AKROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
            // Fall through: starting the CAP also advances to the choice check.
        case AKROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_DELAY:
        case AKROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_WAIT_HOLD:
            task->state++;
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_CHECK_CHOICE:
            if (capIsBusy() == 0) {
                if (gCapConfirmedChoiceIndex == AKROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_CANCEL_CHOICE) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                    taskKill(task);
                    break;
                }
                displayAcquireMenuHold();
                task->state++;
            }
            // Advance even while CAP is busy; acceptance advances twice in this update.
            task->state++;
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_DEPART:
            gAkropolisHelicopterLandingPadEncounterPhase = AKROPOLIS_HELICOPTER_LANDING_PAD_PHASE_DEPARTURE_ACCEPTED;
            taskSpawnFromTable(gAkropolisHelicopterLandingPadScriptTasks, AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_PREPARE_DEPARTURE, 0, 0);
            taskKill(task);
            break;
    }
}

/// Initializes the room task's message routing, encounter gates and persistent pitch task.
static void _akropolisHelicopterLandingPadInitializeRoomTask(Task* task)
{
    enum { AKROPOLIS_HELICOPTER_LANDING_PAD_PLACEMENT_TRIGGER = 4 };
    WorldCollisionTrigger* placementTrigger;

    task->msgTable = gAkropolisHelicopterLandingPadRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    gAkropolisHelicopterLandingPadPostEncounterPlacementEnabled = 0;
    gAkropolisHelicopterLandingPadEncounterReady                = 0;
    task->state++;
    evsStartScript(gAkropolisHelicopterLandingPadAmbientSoundScript, EVENT_SCRIPT_HUD_KEEP);
    _gAkropolisHelicopterLandingPadPlayerPitchTask = taskSpawnFromTable(gAkropolisHelicopterLandingPadScriptTasks, AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_PLAYER_PITCH, 0, 0);
    placementTrigger                               = &gAkropolisHelicopterLandingPadActionTriggers[AKROPOLIS_HELICOPTER_LANDING_PAD_PLACEMENT_TRIGGER];
    placementTrigger->flags                       &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
}

void akropolisHelicopterLandingPadRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gAkropolisHelicopterLandingPadRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}
