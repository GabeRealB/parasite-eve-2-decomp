#include "stages/akropolis/rooms/helicopter_landing_pad.h"

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
#include "assets/rooms/akropolis/helicopter_landing_pad/ammunition_pickup.inc"

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
        MAX_ANGLE  = 96,
        WEIGHT_ONE = 4096,
    };

    gfxRotMatrixX(&playerPart->coord, -(weight * MAX_ANGLE) / WEIGHT_ONE, GRAPHICS_ROTATION_COMPOSE);
    playerPart->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(playerPart);
}

/// Samples one triangular shake envelope point, advancing the shared random sequence.
static inline s32 _akropolisHelicopterLandingPadSampleScreenShake(Task* task, s32 packedShake, s32 halfDurationFrames)
{
    enum {
        SHAKE_AMPLITUDE_SHIFT = 8,
        RANDOM_FRACTION_BITS  = 16,
    };
    s32 shakeSample;
    s32 envelopeAmplitude;

    shakeSample       = halfDurationFrames - ABS(task->spawnArg1.value);
    envelopeAmplitude = shakeSample * (packedShake >> SHAKE_AMPLITUDE_SHIFT);
    RANDOM_LCG_ADVANCE();
    shakeSample = (envelopeAmplitude * (s32)(gRandomLcgState >> RANDOM_FRACTION_BITS)) / halfDurationFrames >> RANDOM_FRACTION_BITS;
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
        DEPARTURE_SETUP            = 0,
        DEPARTURE_WAIT_VIEW        = 1,
        DEPARTURE_EXIT_ACTORS      = 2,
        DEPARTURE_WAIT_ACTOR_EXIT  = 3,
        DEPARTURE_RESTORE_VIEW     = 4,
        DEPARTURE_START_LIFT       = 5,
        DEPARTURE_LOAD_RESOURCES   = 6,
        DEPARTURE_WAIT_LOAD        = 7,
        DEPARTURE_SPAWN_PLACEMENTS = 8,
        DEPARTURE_START_SCENE      = 9,
        DEPARTURE_VARIANT          = 2,
        DEPARTURE_VIEW             = 18,
        LIFT_CHILD                 = 40,
        DEPARTURE_PLAYER_CLIP      = 9,
        DEPARTURE_DELAY_FRAMES     = 120,
        DEPARTURE_SOUND_REMAINING  = 90,
        REVISIT_HIDDEN_ACTOR       = 1,
        CD_FILE_KEY_BYTES          = 4,
    };
    u8        fileKey[CD_FILE_KEY_BYTES];
    u8        loadArgs[sizeof(gCdCmdQueue.entries[0].args.bytes)];
    SVECTOR   ambientColor;
    Task*     liftTask;
    GfxCoord* liftCoord;

    /// Enqueues the stage-zero departure bundle with its image-placement offsets.
#define QUEUE_DEPARTURE_RESOURCES(fileKey, loadArgs)               \
    {                                                              \
        enum {                                                     \
            FILE_GROUP    = 51,                                    \
            FILE_HUNDREDS = 10,                                    \
            IMAGE_X_PAGES = 3,                                     \
            IMAGE_Y_ROWS  = 5,                                     \
        };                                                         \
        (fileKey)[2]  = FILE_GROUP;                                \
        (loadArgs)[0] = FILE_HUNDREDS;                             \
        (loadArgs)[2] = IMAGE_X_PAGES;                             \
        (fileKey)[3]  = 0;                                         \
        (fileKey)[0]  = 0;                                         \
        (loadArgs)[1] = CD_COMMAND_LOAD_DEFAULT;                   \
        (loadArgs)[3] = IMAGE_Y_ROWS;                              \
        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, (fileKey), (loadArgs)); \
        func_800ABFF8();                                           \
        func_800AC000();                                           \
    }

    switch (task->state) {
        case DEPARTURE_SETUP:
            task->spawnArg1.value = 0;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            task->state += 1;
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.variant >= DEPARTURE_VARIANT) {
                taskMessageDispatch(sceneFindPlacedActor(REVISIT_HIDDEN_ACTOR), ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE, 0);
            }
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = DEPARTURE_VIEW;
            ambientColor.vx                                        = 1200;
            ambientColor.vy                                        = 1200;
            ambientColor.vz                                        = 1552;
            worldCoordSetAmbientColorOverride(&ambientColor);
            break;
        case DEPARTURE_WAIT_VIEW:
            if (gGameSession->viewReady != 0) {
                task->state += 1;
            }
            break;
        case DEPARTURE_EXIT_ACTORS:
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.variant < DEPARTURE_VARIANT) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_EXIT_PLACED_ACTORS, 0, 0);
            }
            task->state += 1;
            break;
        case DEPARTURE_WAIT_ACTOR_EXIT:
            task->state += 1;
            break;
        case DEPARTURE_RESTORE_VIEW:
            loadingRequestViewGraphicsRestore();
            task->state += 1;
            break;
        case DEPARTURE_START_LIFT:
            // Place in the lift's frame before attaching the player to its root.
            TASK_MESSAGE_DISPATCH_SECOND_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_FIND_OTHER_CHILD, LIFT_CHILD, &liftTask);
            TASK_MESSAGE_DISPATCH_POINTER(liftTask, ACTOR_MESSAGE_PLAY_ANIMATION, &gAkropolisHelicopterLandingPadLiftRaiseRequest, 0);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &gAkropolisHelicopterLandingPadLiftPlayerPlacement, 0);
            gAkropolisHelicopterLandingPadLiftPlayerAnimationRequest.animationId  = DEPARTURE_PLAYER_CLIP;
            gAkropolisHelicopterLandingPadLiftPlayerAnimationRequest.source.index = gPlayerStatus.weapon + 1;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &gAkropolisHelicopterLandingPadLiftPlayerAnimationRequest, 0);
            liftCoord = liftTask->extra.tmd->coords;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_ATTACH_TO_COORD, liftCoord, 0);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &gAkropolisHelicopterLandingPadLiftPlayerPlacement, 0);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            task->state += 1;
            break;
        case DEPARTURE_LOAD_RESOURCES:
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.variant < DEPARTURE_VARIANT) {
                QUEUE_DEPARTURE_RESOURCES(fileKey, loadArgs);
            }
            task->spawnArg1.value = DEPARTURE_DELAY_FRAMES;
            task->state          += 1;
            break;
        case DEPARTURE_WAIT_LOAD:
            task->spawnArg1.value -= 1;
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.variant >= DEPARTURE_VARIANT) {
                task->state = DEPARTURE_START_SCENE;
            } else if (cdCmdIsIdle()) {
                loadingRequestViewGraphicsRestore();
                task->state += 1;
            }
            break;
        case DEPARTURE_SPAWN_PLACEMENTS:
            task->spawnArg1.value -= 1;
            // The newly loaded actor variant must be spawned before the scene script.
            areaSetPlacementVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location, DEPARTURE_VARIANT, AREA_VARIANT_RESET_ALWAYS);
            areaSyncLocationVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location);
            areaSpawnPlacements(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location);
            gAkropolisHelicopterLandingPadEncounterPhase = AKROPOLIS_HELICOPTER_LANDING_PAD_PHASE_DEPARTURE_READY;
            task->state                                 += 1;
            break;
        case DEPARTURE_START_SCENE:
            task->spawnArg1.value -= 1;
            if (task->spawnArg1.value <= 0) {
                evsStartScriptWithSkip(gAkropolisHelicopterLandingPadDepartureScript, EVENT_SCRIPT_HUD_KEEP,
                                       gAkropolisHelicopterLandingPadDepartureSkipScript);
                taskKill(task);
            }
            break;
    }
    if (task->spawnArg1.value == DEPARTURE_SOUND_REMAINING) {
        sndEvtRequestScriptStart(AKROPOLIS_HELICOPTER_LANDING_PAD_SOUND(AKROPOLIS_HELICOPTER_LANDING_PAD_SOUND_LIFT_MOVE), 0, 0);
    }
}
#undef QUEUE_DEPARTURE_RESOURCES

void akropolisHelicopterLandingPadDepartureShakeTimelineTask(Task* task)
{
    enum {
        SHAKE_FIRST_FRAME        = 0,
        VIBRATION_FIRST_FRAME    = 7,
        SHAKE_REPEAT_FRAME       = 200,
        VIBRATION_REPEAT_FRAME   = 207,
        SHAKE_SECOND_FRAME       = 410,
        VIBRATION_SECOND_FRAME   = 421,
        SHAKE_THIRD_FRAME        = 520,
        VIBRATION_THIRD_FRAME    = 535,
        SHAKE_TIMELINE_END_FRAME = 640,
    };

    // Screen impulses precede the corresponding controller vibration cues.
    switch (task->state) {
        case SHAKE_REPEAT_FRAME:
        case SHAKE_FIRST_FRAME:
            _akropolisHelicopterLandingPadSpawnScreenShake(15, 2);
            break;
        case VIBRATION_FIRST_FRAME:
        case VIBRATION_REPEAT_FRAME:
            padScriptSpawn(_gAkropolisHelicopterLandingPadDepartureFirstVibrationCommands, _gAkropolisHelicopterLandingPadDepartureFirstVibrationSegments);
            break;
        case SHAKE_SECOND_FRAME:
            _akropolisHelicopterLandingPadSpawnScreenShake(22, 3);
            break;
        case VIBRATION_SECOND_FRAME:
            padScriptSpawn(_gAkropolisHelicopterLandingPadDepartureSecondVibrationCommands, _gAkropolisHelicopterLandingPadDepartureSecondVibrationSegments);
            break;
        case SHAKE_THIRD_FRAME:
            _akropolisHelicopterLandingPadSpawnScreenShake(30, 4);
            break;
        case VIBRATION_THIRD_FRAME:
            padScriptSpawn(_gAkropolisHelicopterLandingPadDepartureThirdVibrationCommands, _gAkropolisHelicopterLandingPadDepartureThirdVibrationSegments);
            break;
        case SHAKE_TIMELINE_END_FRAME:
            taskKill(task);
            break;
    }
    task->state += 1;
}

void akropolisHelicopterLandingPadReturnToMistTask(Task* task)
{
    enum {
        RETURN_PREPARE        = 0,
        RETURN_WAIT_MOVIE     = 1,
        RETURN_RELOAD         = 2,
        ITEM_MICRO_DEVICE     = 0x102,
        RETURN_SCENE_EVENT    = 1,
        RETURN_WARP           = 1,
        RETURN_ROOM           = 1,
        RETURN_SPRITE_VARIANT = 1,
        RETURN_MOVIE_TASK     = 0,
    };

    switch (task->state) {
        case RETURN_PREPARE:
            // Reset Acropolis rewards and progress before the streamed interlude.
            playerStateRestoreFullHpMp();
            inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_PARTHENON_KEY);
            inventoryClearCollectedBit(ITEM_MICRO_DEVICE);
            itemSetIdentified(ITEM_MICRO_DEVICE, true);
            gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
            gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_STORY_DIALOGUE_STILL_HERE);
            taskSpawnFromTable(gAkropolisHelicopterLandingPadMovieTasks, RETURN_MOVIE_TASK, 0, 0);
            task->state += 1;
            return;
        case RETURN_WAIT_MOVIE:
            task->state = RETURN_RELOAD;
            return;
        case RETURN_RELOAD:
            // The movie task holds the task list while playback owns the display.
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent         = RETURN_SCENE_EVENT;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_AKROPOLIS;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = AKROPOLIS_ROOM_ID_MIST_R18;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = RETURN_WARP;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = RETURN_ROOM;
            gDisplayState.spriteVariant                                 = RETURN_SPRITE_VARIANT;
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
        INITIALIZE = 0,
        UPDATE     = 1,
        STEP       = 256,
    };

    /// Tests whether the wrapped yaw is strictly closer to the target than the current yaw.
#define COMPARE_YAW_DISTANCES(wrapped, current, target, comparison, currentDistance) \
    do {                                                                             \
        (comparison) = (wrapped) - (target);                                         \
        if ((comparison) < 0) {                                                      \
            (comparison) = -(comparison);                                            \
        }                                                                            \
        (currentDistance) = (current) - (target);                                    \
        if ((currentDistance) < 0) {                                                 \
            (currentDistance) = -(currentDistance);                                  \
        }                                                                            \
        (comparison) = (comparison) < (currentDistance);                             \
    } while (0)

    switch (task->state) {
        case INITIALIZE:
            _gAkropolisHelicopterLandingPadTurnYaw = player->rotation.vy & ACTOR_TRANSFORM_ANGLE_MASK;
            if (task->spawnArg1.value < _gAkropolisHelicopterLandingPadTurnYaw) {
                wrappedYaw = _gAkropolisHelicopterLandingPadTurnYaw - ACTOR_TRANSFORM_ANGLE_TURN;
            } else {
                wrappedYaw = _gAkropolisHelicopterLandingPadTurnYaw + ACTOR_TRANSFORM_ANGLE_TURN;
            }
            targetYaw  = task->spawnArg1.value;
            currentYaw = _gAkropolisHelicopterLandingPadTurnYaw;
            // Compare both unwrapped paths before choosing the turn direction.
            COMPARE_YAW_DISTANCES(wrappedYaw, currentYaw, targetYaw, turnValue, currentDistance);
            if (turnValue) {
                _gAkropolisHelicopterLandingPadTurnYaw = wrappedYaw;
            }
            if (task->spawnArg1.value > _gAkropolisHelicopterLandingPadTurnYaw) {
                _gAkropolisHelicopterLandingPadTurnStep = STEP;
            } else {
                _gAkropolisHelicopterLandingPadTurnStep = -STEP;
            }
            task->state++;
            break;
        case UPDATE:
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
#undef COMPARE_YAW_DISTANCES

void akropolisHelicopterLandingPadPlayerPitchPulseTask(Task* task)
{
    enum {
        PART        = 4,
        WEIGHT_ONE  = 4096,
        WEIGHT_STEP = 400,
    };
    GfxCoord* playerPart;
    // These unused aggregate roles are unproven; their stack slots and stores remain in the image.
    SVECTOR unusedShortVectorBefore;
    VECTOR  unusedVectorBefore;
    VECTOR  unusedVector;
    MATRIX  unusedMatrix;
    SVECTOR unusedShortVectorAfter;

    playerPart      = &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[PART];
    unusedVector.vx = -0x249;
    unusedVector.vy = 0;
    unusedVector.vz = 0xB8;

    switch (task->state) {
        case AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_IDLE:
            _gAkropolisHelicopterLandingPadPitchWeight = 0;
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_RISE:
            _akropolisHelicopterLandingPadApplyPlayerPitchPulse(playerPart, _gAkropolisHelicopterLandingPadPitchWeight);
            _gAkropolisHelicopterLandingPadPitchWeight += WEIGHT_STEP;
            if (_gAkropolisHelicopterLandingPadPitchWeight > WEIGHT_ONE) {
                _gAkropolisHelicopterLandingPadPitchWeight = WEIGHT_ONE;
                task->state                                = AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_FALL;
            }
            break;
        case AKROPOLIS_HELICOPTER_LANDING_PAD_PITCH_FALL:
            _akropolisHelicopterLandingPadApplyPlayerPitchPulse(playerPart, _gAkropolisHelicopterLandingPadPitchWeight);
            _gAkropolisHelicopterLandingPadPitchWeight -= WEIGHT_STEP;
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
        DEPARTURE_CAP_SLOT = 9,
        EXIT_STOP_SELECTOR = -1,
        EXIT_STOP_CONTROL  = 30,
    };

    *reply = *request;
    if (request->areaId == AKROPOLIS_ROOM_ID_FIRE_ESCAPE) {
        if (gAkropolisHelicopterLandingPadEncounterPhase == AKROPOLIS_HELICOPTER_LANDING_PAD_PHASE_WAITING) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                // This exact-entry selector matches idle sound slots; its purpose is unproven.
                sndEvtRequestScriptStop(EXIT_STOP_SELECTOR, EXIT_STOP_CONTROL);
            }
            return ROOM_EVENT_DEPART;
        }
        if (gAkropolisHelicopterLandingPadEncounterPhase == AKROPOLIS_HELICOPTER_LANDING_PAD_PHASE_ENCOUNTER_FINISHED) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capStartSequenceSlot(DEPARTURE_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
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
        ACTION_START_ENCOUNTER  = 0,
        ACTION_LATCH_PLACEMENT  = 1,
        ENCOUNTER_WAITING       = 0,
        ENCOUNTER_STARTED       = 1,
        PLACEMENT_TRIGGER       = 4,
        CAP_INTERACTION_TRIGGER = 8,
        ENCOUNTER_MUSIC_ENTRY   = 1,
    };
    u8                     actionId;
    WorldCollisionTrigger* placementTrigger;
    WorldCollisionTrigger* capInteractionTrigger;

    if ((request->actionId == ACTION_START_ENCOUNTER) && (gAkropolisHelicopterLandingPadEncounterPhase == ENCOUNTER_WAITING) && (gAkropolisHelicopterLandingPadEncounterReady != 0)) {
        // Swap the encounter triggers before normal play resumes.
        gGameSession->flowFlags   = (GAME_SESSION_FLOW_SKIP_AREA_MUSIC | GAME_SESSION_FLOW_REEQUIP_WEAPON);
        gStageCountdownMusicEntry = ENCOUNTER_MUSIC_ENTRY;
        evsStartScriptWithSkip(gAkropolisHelicopterLandingPadEncounterIntroScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gAkropolisHelicopterLandingPadEncounterIntroSkipScript);
        gAkropolisHelicopterLandingPadEncounterPhase = ENCOUNTER_STARTED;
        placementTrigger                             = &gAkropolisHelicopterLandingPadActionTriggers[PLACEMENT_TRIGGER];
        capInteractionTrigger                        = &gAkropolisHelicopterLandingPadActionTriggers[CAP_INTERACTION_TRIGGER];
        placementTrigger->flags                     |= WORLD_COLLISION_TRIGGER_ENABLED;
        capInteractionTrigger->flags                &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    }
    actionId = request->actionId;
    if (actionId == ACTION_LATCH_PLACEMENT) {
        gAkropolisHelicopterLandingPadPostEncounterPlacementEnabled = actionId;
    }
    return 0;
}

s32 akropolisHelicopterLandingPadHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum { COMMAND_CONFIRM_DEPARTURE = 4 };

    if ((commandId == COMMAND_CONFIRM_DEPARTURE) && (gAkropolisHelicopterLandingPadEncounterPhase == AKROPOLIS_HELICOPTER_LANDING_PAD_PHASE_ENCOUNTER_FINISHED)) {
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
    enum { SHAKE_AMPLITUDE_SHIFT = 8 };

    taskSpawnFromTable(gAkropolisHelicopterLandingPadScriptTasks, AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_SCREEN_SHAKE, 0, halfDurationFrames | (amplitudePixels << SHAKE_AMPLITUDE_SHIFT));
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
        ENCOUNTER_ACTOR         = 0,
        BATTLE_END_DELAY_FRAMES = 3,
    };

    // The resident API ignores its second argument; rewards come from the actor.
    sceneReleaseBattleRefWithRewards(sceneFindPlacedActor(ENCOUNTER_ACTOR), 0x1B);
    gSceneCombatState.signals.bytes.endDelayFrames = BATTLE_END_DELAY_FRAMES;
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
        START         = 0,
        WAIT          = 1,
        APPROACH_CLIP = 12,
        ARRIVAL_CLIP  = 9,
    };
    GameActorMoveAnim moveAnim;

    switch (task->state) {
        case START:
            moveAnim.approachAnimId = APPROACH_CLIP;
            moveAnim.arrivalAnimId  = ARRIVAL_CLIP;
            TASK_MESSAGE_DISPATCH_POINTERS(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_MOVE_TO, &gAkropolisHelicopterLandingPadSceneMarkStorage.destination, &moveAnim);
            task->state++;
            break;
        case WAIT:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                taskKill(task);
            }
            break;
    }
}

void akropolisHelicopterLandingPadScreenShakeTask(Task* task)
{
    enum {
        INITIALIZE    = 0,
        UPDATE        = 1,
        DURATION_MASK = 0xFF,
    };
    s32 packedShake;
    s32 halfDurationFrames;
    s32 shakeSample;

    packedShake        = task->spawnArg2.value;
    halfDurationFrames = packedShake & DURATION_MASK;

    switch (task->state) {
        case INITIALIZE:
            task->spawnArg1.value = -halfDurationFrames;
            task->state++;
            break;
        case UPDATE:
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
        START         = 0,
        CHECK_CHOICE  = 1,
        DELAY         = 2,
        WAIT_HOLD     = 3,
        DEPART        = 4,
        CAP_SLOT      = 4,
        CANCEL_CHOICE = 1,
    };

    switch (task->state) {
        case START:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capStartSequenceSlot(CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
            // Fall through: starting the CAP also advances to the choice check.
        case DELAY:
        case WAIT_HOLD:
            task->state++;
            break;
        case CHECK_CHOICE:
            if (capIsBusy() == 0) {
                if (gCapConfirmedChoiceIndex == CANCEL_CHOICE) {
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
        case DEPART:
            gAkropolisHelicopterLandingPadEncounterPhase = AKROPOLIS_HELICOPTER_LANDING_PAD_PHASE_DEPARTURE_ACCEPTED;
            taskSpawnFromTable(gAkropolisHelicopterLandingPadScriptTasks, AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_PREPARE_DEPARTURE, 0, 0);
            taskKill(task);
            break;
    }
}

/// Initializes the room task's message routing, encounter gates and persistent pitch task.
static void _akropolisHelicopterLandingPadInitializeRoomTask(Task* task)
{
    enum { PLACEMENT_TRIGGER = 4 };
    WorldCollisionTrigger* placementTrigger;

    task->msgTable = gAkropolisHelicopterLandingPadRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    gAkropolisHelicopterLandingPadPostEncounterPlacementEnabled = 0;
    gAkropolisHelicopterLandingPadEncounterReady                = 0;
    task->state++;
    evsStartScript(gAkropolisHelicopterLandingPadAmbientSoundScript, EVENT_SCRIPT_HUD_KEEP);
    _gAkropolisHelicopterLandingPadPlayerPitchTask = taskSpawnFromTable(gAkropolisHelicopterLandingPadScriptTasks, AKROPOLIS_HELICOPTER_LANDING_PAD_TASK_PLAYER_PITCH, 0, 0);
    placementTrigger                               = &gAkropolisHelicopterLandingPadActionTriggers[PLACEMENT_TRIGGER];
    placementTrigger->flags                       &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
}

void akropolisHelicopterLandingPadRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gAkropolisHelicopterLandingPadRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}
