#include "rooms/acropolis_helicopter_landing_pad.h"
#include "../../stages/akropolis/event_flags.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "common.h"

#include "acropolis_helicopter_landing_pad_private.h"

#include "actors/actor_511000.h"
#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/companion_load.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_state.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#define ROOM_PART declarations_acropolis_helicopter_landing_pad_2
#include "assets/rooms/akropolis/helicopter_landing_pad/declarations.inc"

static Task* _gAcropolisHelicopterLandingPadPlayerPitchTask;

static void _acropolisHelicopterLandingPadSpawnScreenShake(s32 halfDurationFrames, s32 amplitudePixels);
static void _acropolisHelicopterLandingPadInitRoom(Task* task);

/// Unused format for a label followed by three signed, minimum-five-column integers.
static const char _gAcropolisHelicopterLandingPadUnusedLabelTripleFormat[] = "%s (%5d,%5d,%5d)";

/// Room-task lifecycle: initialize, update the encounter phase, release.
///
/// Indexed by unchecked state 0..2; keep the room loaded until task teardown.
static const TaskFuncTable3 _gAcropolisHelicopterLandingPadRoomTaskStates = {
    { _acropolisHelicopterLandingPadInitRoom, acropolisHelicopterLandingPadUpdateEncounterPhase, taskKill },
};

/// Packed selectors carried by the landing pad's exit, stair and room-action regions.
enum {
    ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_CALLBACK_CONTROL    = WORLD_COLLISION_TRIGGER_ACTION_CALLBACK | (1 << 8),
    ACROPOLIS_HELICOPTER_LANDING_PAD_TRIGGER_START_ENCOUNTER  = 0,
    ACROPOLIS_HELICOPTER_LANDING_PAD_TRIGGER_ENABLE_PLACEMENT = 1,
    ACROPOLIS_HELICOPTER_LANDING_PAD_STAIR_SURFACE_AND_STEPS  = (3 << 4) | 3,
    ACROPOLIS_HELICOPTER_LANDING_PAD_STAIR_FACING             = ACTOR_TRANSFORM_ANGLE_HALF_TURN / 16,
};

/// Placement-entry keys used by this room's resource recipes.
enum {
    ACROPOLIS_HELICOPTER_LANDING_PAD_NO9_RESOURCE_ENTRY        = 27,
    ACROPOLIS_HELICOPTER_LANDING_PAD_RUPERT_RESOURCE_ENTRY     = 144,
    ACROPOLIS_HELICOPTER_LANDING_PAD_HELICOPTER_RESOURCE_ENTRY = 254,
};

enum { ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_SECOND_VIBRATION_LOOP_COUNT = 10 };

enum { ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_FIRST_VIBRATION_LOOP_COUNT = 10 };

/// Cue-2 bases in the loaded type-1 bank; cue 1 uses the following entry.
enum {
    ACROPOLIS_HELICOPTER_LANDING_PAD_FLOOR_WALK_SOUND          = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x15,
    ACROPOLIS_HELICOPTER_LANDING_PAD_FLOOR_RUN_SOUND           = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x17,
    ACROPOLIS_HELICOPTER_LANDING_PAD_FLOOR_SCRIPTED_JUMP_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x19,
    ACROPOLIS_HELICOPTER_LANDING_PAD_STAIR_WALK_SOUND          = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x0D,
    ACROPOLIS_HELICOPTER_LANDING_PAD_STAIR_RUN_SOUND           = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x0F,
};

#include "assets/rooms/akropolis/helicopter_landing_pad/action_triggers.inc"
#include "assets/rooms/akropolis/helicopter_landing_pad/occluders.inc"
#include "assets/rooms/akropolis/helicopter_landing_pad/placements.inc"
#include "assets/rooms/akropolis/helicopter_landing_pad/lights.inc"
#include "assets/rooms/akropolis/helicopter_landing_pad/sprites.inc"
#include "assets/rooms/akropolis/helicopter_landing_pad/cameras.inc"
#define ROOM_PART gAcropolisHelicopterLandingPadLethalStrikeVibrationCommands
#include "assets/rooms/akropolis/helicopter_landing_pad/vibrations.inc"
#include "assets/rooms/akropolis/helicopter_landing_pad/surfaces.inc"

static TmdBone _gAcropolisHelicopterLandingPadModel0A8E8Skeleton[1] = {
#include "assets/acropolis_helicopter_landing_pad_model_0A8E8_skeleton.inc"
};

static u32 _gAcropolisHelicopterLandingPadModel0A8E8PartVerts[1] = {
#include "assets/acropolis_helicopter_landing_pad_model_0A8E8_partVerts.inc"
};

static SVECTOR _gAcropolisHelicopterLandingPadModel0A8E8Verts[13] = {
#include "assets/acropolis_helicopter_landing_pad_model_0A8E8_verts.inc"
};

static SVECTOR _gAcropolisHelicopterLandingPadModel0A8E8Normals[6] = {
#include "assets/acropolis_helicopter_landing_pad_model_0A8E8_normals.inc"
};

static u32 _gAcropolisHelicopterLandingPadModel0A8E8Stream[42] = {
#include "assets/acropolis_helicopter_landing_pad_model_0A8E8_stream.inc"
};

TmdSource gAcropolisHelicopterLandingPadModel0A8E8 = {
    0,
    312,
    0,
    1,
    _gAcropolisHelicopterLandingPadModel0A8E8PartVerts,
    _gAcropolisHelicopterLandingPadModel0A8E8Verts,
    _gAcropolisHelicopterLandingPadModel0A8E8Normals,
    _gAcropolisHelicopterLandingPadModel0A8E8Skeleton,
    _gAcropolisHelicopterLandingPadModel0A8E8Stream,
};

/// Current unwrapped yaw of the singleton player-turn task.
///
/// Angles use 4096 units per turn. Initialization selects the shorter path,
/// possibly outside 0..4095; updates clamp at the target before narrowing to the player's yaw.
static s32 _gAcropolisHelicopterLandingPadTurnYaw = 0;

/// Signed yaw increment of the singleton player-turn task, in angle units per update.
///
/// Initialization chooses +256 or -256, with 4096 units per turn.
/// The value persists through task completion and is replaced by the next turn.
static s32 _gAcropolisHelicopterLandingPadTurnStep = 0;

/// Q12 weight of the persistent player model-part pitch pulse.
///
/// Idle resets it to zero; rise and fall step by 400 and clamp to 0..4096.
/// The signed halfword permits the falling update to test a negative intermediate.
static s16 _gAcropolisHelicopterLandingPadPitchWeight = 0;

/// Borrowed singleton task that pulses the player's model-part pitch.
///
/// Published during room initialization, without a spawn-success check.
/// State requests require the live task; room teardown ends its lifetime
/// without clearing this handle.
static Task* _gAcropolisHelicopterLandingPadPlayerPitchTask = NULL;

s32 gAcropolisHelicopterLandingPadPostEncounterPlacementEnabled = 0;

SVECTOR ActorContact_ScratchPosition = { 0, 0, 0, 0 };

RoomEventMsg gAcropolisHelicopterLandingPadExitRequest = { 0, 0, 0, 0, 0, 0 };

/// Composes one weighted pitch pulse into the player's borrowed model coordinate.
///
/// Borrows a live, writable coordinate with an animation-supplied local matrix.
/// Weight is signed Q12 in 0..4096; pitch peaks at -96 angle units (4096 per
/// turn). Composes about local X, dirties the cache and recomposes the chain.
static inline void _acropolisHelicopterLandingPadApplyPlayerPitchPulse(GfxCoord* playerPart, s32 weight)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_MAX_ANGLE  = 96,
        ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_ONE = 4096,
    };

    gfxRotMatrixX(&playerPart->coord, -(weight * ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_MAX_ANGLE) / ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_ONE, GRAPHICS_ROTATION_COMPOSE);
    playerPart->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(playerPart);
}

/// Samples one triangular shake envelope point, advancing the shared random sequence.
///
/// Borrows the shake task and re-reads its cursor for the alternating sign.
/// The cursor must be in [-halfDurationFrames, halfDurationFrames], with a
/// half-duration of 1..255 updates. Arithmetic shift by 8 gives the packed
/// signed pixel amplitude. Both signed products must fit s32; division truncates
/// toward zero before the Q16 shift. Changes the LCG but does not move the cursor.
static inline s32 _acropolisHelicopterLandingPadSampleScreenShake(Task* task, s32 packedShake, s32 halfDurationFrames)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_AMPLITUDE_SHIFT      = 8,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_RANDOM_FRACTION_BITS = 16,
    };
    s32 shakeSample;
    s32 envelopeAmplitude;

    shakeSample       = halfDurationFrames - ABS(task->spawnArg1.value);
    envelopeAmplitude = shakeSample * (packedShake >> ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_AMPLITUDE_SHIFT);
    gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    shakeSample       = (envelopeAmplitude * (s32)(gRandomLcgState >> ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_RANDOM_FRACTION_BITS)) / halfDurationFrames >> ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_RANDOM_FRACTION_BITS;
    if (task->spawnArg1.value & 1) {
        shakeSample = ABS(shakeSample);
    } else {
        shakeSample = -ABS(shakeSample);
    }
    return shakeSample;
}

void acropolisHelicopterLandingPadPrepareDepartureTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_SETUP            = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_WAIT_VIEW        = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_EXIT_ACTORS      = 2,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_WAIT_ACTOR_EXIT  = 3,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_RESTORE_VIEW     = 4,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_START_LIFT       = 5,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_LOAD_RESOURCES   = 6,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_WAIT_LOAD        = 7,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_SPAWN_PLACEMENTS = 8,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_START_SCENE      = 9,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VARIANT          = 2,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VIEW             = 18,
        ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_CHILD                 = 40,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_PLAYER_CLIP      = 9,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_DELAY_FRAMES     = 120,
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_SOUND_REMAINING  = 90,
        ACROPOLIS_HELICOPTER_LANDING_PAD_REVISIT_HIDDEN_ACTOR       = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_ACTOR_HIDE_MODEL           = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_CD_FILE_KEY_BYTES          = 4,
    };
    u8        fileKey[ACROPOLIS_HELICOPTER_LANDING_PAD_CD_FILE_KEY_BYTES];
    u8        loadArgs[sizeof(gCdCmdQueue.entries[0].args.bytes)];
    SVECTOR   ambientColor;
    Task*     liftTask;
    GfxCoord* liftCoord;

    /// Enqueues the stage-zero departure bundle with its image-placement offsets.
    ///
    /// Expands to a compound statement and repeatedly evaluates both arguments.
    /// They must be side-effect-free, distinct writable byte arrays of at least
    /// four bytes. Enqueue copies the consumed bytes synchronously and ignores
    /// key byte 1. Captures no local identifiers; subsequent calls retain their
    /// original order. Kept as a macro so stores address the caller's arrays directly.
#define ACROPOLIS_HELICOPTER_LANDING_PAD_QUEUE_DEPARTURE_RESOURCES(fileKey, loadArgs) \
    {                                                                                 \
        enum {                                                                        \
            ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_FILE_GROUP    = 51,            \
            ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_FILE_HUNDREDS = 10,            \
            ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_IMAGE_X_PAGES = 3,             \
            ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_IMAGE_Y_ROWS  = 5,             \
        };                                                                            \
        (fileKey)[2]  = ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_FILE_GROUP;        \
        (loadArgs)[0] = ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_FILE_HUNDREDS;     \
        (loadArgs)[2] = ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_IMAGE_X_PAGES;     \
        (fileKey)[3]  = 0;                                                            \
        (fileKey)[0]  = 0;                                                            \
        (loadArgs)[1] = CD_COMMAND_LOAD_DEFAULT;                                      \
        (loadArgs)[3] = ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_IMAGE_Y_ROWS;      \
        cdCmdEnqueue(CD_COMMAND_LOAD_FILE, (fileKey), (loadArgs));                    \
        func_800ABFF8();                                                              \
        func_800AC000();                                                              \
    }

    switch (task->state) {
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_SETUP:
            task->spawnArg1.value = 0;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            task->state += 1;
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.variant >= ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VARIANT) {
                taskMessageDispatch(sceneFindPlacedActor(ACROPOLIS_HELICOPTER_LANDING_PAD_REVISIT_HIDDEN_ACTOR), ACTOR_MESSAGE_SET_MODEL_DRAW, ACROPOLIS_HELICOPTER_LANDING_PAD_ACTOR_HIDE_MODEL, 0);
            }
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VIEW;
            ambientColor.vx                                        = 1200;
            ambientColor.vy                                        = 1200;
            ambientColor.vz                                        = 1552;
            worldCoordSetAmbientColorOverride(&ambientColor);
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_WAIT_VIEW:
            if (gGameSession->viewReady != 0) {
                task->state += 1;
            }
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_EXIT_ACTORS:
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.variant < ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VARIANT) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_EXIT_PLACED_ACTORS, 0, 0);
            }
            task->state += 1;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_WAIT_ACTOR_EXIT:
            task->state += 1;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_RESTORE_VIEW:
            loadingRequestViewGraphicsRestore();
            task->state += 1;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_START_LIFT:
            // Place in the lift's frame before attaching the player to its root.
            TASK_MESSAGE_DISPATCH_SECOND_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_FIND_OTHER_CHILD, ACROPOLIS_HELICOPTER_LANDING_PAD_LIFT_CHILD, &liftTask);
            TASK_MESSAGE_DISPATCH_POINTER(liftTask, ACTOR_MESSAGE_PLAY_ANIMATION, &gAcropolisHelicopterLandingPadLiftRaiseRequest, 0);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &gAcropolisHelicopterLandingPadLiftPlayerPlacement, 0);
            gAcropolisHelicopterLandingPadLiftPlayerAnimationRequest.animationId  = ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_PLAYER_CLIP;
            gAcropolisHelicopterLandingPadLiftPlayerAnimationRequest.source.index = gPlayerStatus.weapon + 1;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &gAcropolisHelicopterLandingPadLiftPlayerAnimationRequest, 0);
            liftCoord = liftTask->extra.tmd->coords;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_ATTACH_TO_COORD, liftCoord, 0);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &gAcropolisHelicopterLandingPadLiftPlayerPlacement, 0);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            task->state += 1;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_LOAD_RESOURCES:
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.variant < ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VARIANT) {
                ACROPOLIS_HELICOPTER_LANDING_PAD_QUEUE_DEPARTURE_RESOURCES(fileKey, loadArgs);
            }
            task->spawnArg1.value = ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_DELAY_FRAMES;
            task->state          += 1;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_WAIT_LOAD:
            task->spawnArg1.value -= 1;
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.variant >= ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VARIANT) {
                task->state = ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_START_SCENE;
            } else if (cdCmdIsIdle()) {
                loadingRequestViewGraphicsRestore();
                task->state += 1;
            }
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_SPAWN_PLACEMENTS:
            task->spawnArg1.value -= 1;
            // The newly loaded actor variant must be spawned before the scene script.
            areaSetPlacementVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location, ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_VARIANT, AREA_VARIANT_RESET_ALWAYS);
            areaSyncLocationVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location);
            areaSpawnPlacements(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location);
            gAcropolisHelicopterLandingPadEncounterPhase = ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_DEPARTURE_READY;
            task->state                                 += 1;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_START_SCENE:
            task->spawnArg1.value -= 1;
            if (task->spawnArg1.value <= 0) {
                evsStartScriptWithSkip(gAcropolisHelicopterLandingPadDepartureScript, EVENT_SCRIPT_HUD_KEEP,
                                       gAcropolisHelicopterLandingPadDepartureSkipScript);
                taskKill(task);
            }
            break;
    }
    if (task->spawnArg1.value == ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_SOUND_REMAINING) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_HELICOPTER_LANDING_PAD, 3), 0, 0);
    }
}
#undef ACROPOLIS_HELICOPTER_LANDING_PAD_QUEUE_DEPARTURE_RESOURCES

void acropolisHelicopterLandingPadDepartureShakeTimelineTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_FIRST_FRAME        = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_FIRST_FRAME    = 7,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_REPEAT_FRAME       = 200,
        ACROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_REPEAT_FRAME   = 207,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_SECOND_FRAME       = 410,
        ACROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_SECOND_FRAME   = 421,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_THIRD_FRAME        = 520,
        ACROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_THIRD_FRAME    = 535,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_TIMELINE_END_FRAME = 640,
    };

    // Screen impulses precede the corresponding controller vibration cues.
    switch (task->state) {
        case ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_REPEAT_FRAME:
        case ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_FIRST_FRAME:
            _acropolisHelicopterLandingPadSpawnScreenShake(15, 2);
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_FIRST_FRAME:
        case ACROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_REPEAT_FRAME:
            padScriptSpawn(_gAcropolisHelicopterLandingPadDepartureFirstVibrationCommands, _gAcropolisHelicopterLandingPadDepartureFirstVibrationSegments);
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_SECOND_FRAME:
            _acropolisHelicopterLandingPadSpawnScreenShake(22, 3);
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_SECOND_FRAME:
            padScriptSpawn(_gAcropolisHelicopterLandingPadDepartureSecondVibrationCommands, _gAcropolisHelicopterLandingPadDepartureSecondVibrationSegments);
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_THIRD_FRAME:
            _acropolisHelicopterLandingPadSpawnScreenShake(30, 4);
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_VIBRATION_THIRD_FRAME:
            padScriptSpawn(_gAcropolisHelicopterLandingPadDepartureThirdVibrationCommands, _gAcropolisHelicopterLandingPadDepartureThirdVibrationSegments);
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_TIMELINE_END_FRAME:
            taskKill(task);
            break;
    }
    task->state += 1;
}

void acropolisHelicopterLandingPadReturnToMistTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_PREPARE        = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_WAIT_MOVIE     = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_RELOAD         = 2,
        ACROPOLIS_HELICOPTER_LANDING_PAD_ITEM_MICRO_DEVICE     = 0x102,
        ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_SCENE_EVENT    = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_WARP           = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_ROOM           = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_SPRITE_VARIANT = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_MOVIE_TASK     = 0,
    };

    switch (task->state) {
        case ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_PREPARE:
            // Reset Acropolis rewards and progress before the streamed interlude.
            playerStateRestoreFullHpMp();
            inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_PARTHENON_KEY);
            inventoryClearCollectedBit(ACROPOLIS_HELICOPTER_LANDING_PAD_ITEM_MICRO_DEVICE);
            itemSetIdentified(ACROPOLIS_HELICOPTER_LANDING_PAD_ITEM_MICRO_DEVICE, true);
            gameFlagSetNibble(EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER, EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD);
            gameFlagSetNibble(EVENT_FLAG_STORY_DIALOGUE_INDEX, AKROPOLIS_STORY_DIALOGUE_STILL_HERE);
            taskSpawnFromTable(gAcropolisHelicopterLandingPadMovieTasks, ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_MOVIE_TASK, 0, 0);
            task->state += 1;
            return;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_WAIT_MOVIE:
            task->state = ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_RELOAD;
            return;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_RELOAD:
            // The movie task holds the task list while playback owns the display.
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent         = ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_SCENE_EVENT;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_AKROPOLIS;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_MIST_R18;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_WARP;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_ROOM;
            gDisplayState.spriteVariant                                 = ACROPOLIS_HELICOPTER_LANDING_PAD_RETURN_SPRITE_VARIANT;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            displayReleaseMenuHold();
            taskKill(task);
            return;
    }
}

void acropolisHelicopterLandingPadTurnPlayerYawTask(Task* task)
{
    GameActor* player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    s32        wrappedYaw;
    s32        turnValue; // Wrapped distance/comparison scratch, then the stepped yaw
    s32        currentDistance;
    s32        targetYaw;
    s32        currentYaw;

    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_TURN_INITIALIZE = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_TURN_UPDATE     = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_TURN_STEP       = 256,
    };

    /// Tests whether the wrapped yaw is strictly closer to the target than the current yaw.
    ///
    /// All arguments must be side-effect-free s32 values or local lvalues.
    /// The two scratch arguments must be distinct and must not alias the inputs.
    /// Yaws use 4096 units per turn; the caller supplies current/target in
    /// 0..4095 and a wrapped yaw one turn away, so signed differences fit s32.
    /// Ties produce zero. Leaves currentDistance at the absolute current-path
    /// distance and comparison at 0 or 1. Captures no caller identifiers.
#define ACROPOLIS_HELICOPTER_LANDING_PAD_COMPARE_YAW_DISTANCES(wrapped, current, target, comparison, currentDistance) \
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
        case ACROPOLIS_HELICOPTER_LANDING_PAD_TURN_INITIALIZE:
            _gAcropolisHelicopterLandingPadTurnYaw = player->rotation.vy & ACTOR_TRANSFORM_ANGLE_MASK;
            if (task->spawnArg1.value < _gAcropolisHelicopterLandingPadTurnYaw) {
                wrappedYaw = _gAcropolisHelicopterLandingPadTurnYaw - ACTOR_TRANSFORM_ANGLE_TURN;
            } else {
                wrappedYaw = _gAcropolisHelicopterLandingPadTurnYaw + ACTOR_TRANSFORM_ANGLE_TURN;
            }
            targetYaw  = task->spawnArg1.value;
            currentYaw = _gAcropolisHelicopterLandingPadTurnYaw;
            // Compare both unwrapped paths before choosing the turn direction.
            ACROPOLIS_HELICOPTER_LANDING_PAD_COMPARE_YAW_DISTANCES(wrappedYaw, currentYaw, targetYaw, turnValue, currentDistance);
            if (turnValue) {
                _gAcropolisHelicopterLandingPadTurnYaw = wrappedYaw;
            }
            if (task->spawnArg1.value > _gAcropolisHelicopterLandingPadTurnYaw) {
                _gAcropolisHelicopterLandingPadTurnStep = ACROPOLIS_HELICOPTER_LANDING_PAD_TURN_STEP;
            } else {
                _gAcropolisHelicopterLandingPadTurnStep = -ACROPOLIS_HELICOPTER_LANDING_PAD_TURN_STEP;
            }
            task->state++;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_TURN_UPDATE:
            turnValue                              = _gAcropolisHelicopterLandingPadTurnYaw + _gAcropolisHelicopterLandingPadTurnStep;
            _gAcropolisHelicopterLandingPadTurnYaw = turnValue;
            if (_gAcropolisHelicopterLandingPadTurnStep > 0) {
                targetYaw = task->spawnArg1.value;
                if (targetYaw < turnValue) {
                    _gAcropolisHelicopterLandingPadTurnYaw = targetYaw;
                    taskKill(task);
                }
            }
            if (_gAcropolisHelicopterLandingPadTurnStep < 0) {
                if (_gAcropolisHelicopterLandingPadTurnYaw < task->spawnArg1.value) {
                    _gAcropolisHelicopterLandingPadTurnYaw = task->spawnArg1.value;
                    taskKill(task);
                }
            }
            // Keep the endpoint test strict: an exact arrival runs one more update.
            player->rotation.vy = _gAcropolisHelicopterLandingPadTurnYaw;
            break;
    }
}
#undef ACROPOLIS_HELICOPTER_LANDING_PAD_COMPARE_YAW_DISTANCES

void acropolisHelicopterLandingPadPlayerPitchPulseTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_PART        = 4,
        ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_ONE  = 4096,
        ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_STEP = 400,
    };
    GfxCoord* playerPart;
    // These unused aggregate roles are unproven; their stack slots and stores remain in the image.
    SVECTOR unusedShortVectorBefore;
    VECTOR  unusedVectorBefore;
    VECTOR  unusedVector;
    MATRIX  unusedMatrix;
    SVECTOR unusedShortVectorAfter;

    playerPart      = &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_PART];
    unusedVector.vx = -0x249;
    unusedVector.vy = 0;
    unusedVector.vz = 0xB8;

    switch (task->state) {
        case ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_IDLE:
            _gAcropolisHelicopterLandingPadPitchWeight = 0;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_RISE:
            _acropolisHelicopterLandingPadApplyPlayerPitchPulse(playerPart, _gAcropolisHelicopterLandingPadPitchWeight);
            _gAcropolisHelicopterLandingPadPitchWeight += ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_STEP;
            if (_gAcropolisHelicopterLandingPadPitchWeight > ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_ONE) {
                _gAcropolisHelicopterLandingPadPitchWeight = ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_ONE;
                task->state                                = ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_FALL;
            }
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_FALL:
            _acropolisHelicopterLandingPadApplyPlayerPitchPulse(playerPart, _gAcropolisHelicopterLandingPadPitchWeight);
            _gAcropolisHelicopterLandingPadPitchWeight -= ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_WEIGHT_STEP;
            if (_gAcropolisHelicopterLandingPadPitchWeight < 0) {
                _gAcropolisHelicopterLandingPadPitchWeight = 0;
                task->state                                = ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_IDLE;
            }
            break;
    }
}

s32 acropolisHelicopterLandingPadResolveDeparture(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_CAP_SLOT = 9,
        ACROPOLIS_HELICOPTER_LANDING_PAD_WARP_ORDINARY      = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_WARP_ROOM_HANDLED  = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_STOP_SELECTOR = -1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_STOP_CONTROL  = 30,
    };

    *reply = *request;
    if (request->areaId == GAME_AREA_AKROPOLIS_FIRE_ESCAPE) {
        if (gAcropolisHelicopterLandingPadEncounterPhase == ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_WAITING) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                // This exact-entry selector matches idle sound slots; its purpose is unproven.
                sndEvtRequestScriptStop(ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_STOP_SELECTOR, ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_STOP_CONTROL);
            }
            return ACROPOLIS_HELICOPTER_LANDING_PAD_WARP_ORDINARY;
        }
        if (gAcropolisHelicopterLandingPadEncounterPhase == ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_ENCOUNTER_FINISHED) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                capStartSequenceSlot(ACROPOLIS_HELICOPTER_LANDING_PAD_DEPARTURE_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
            }
            return ACROPOLIS_HELICOPTER_LANDING_PAD_WARP_ROOM_HANDLED;
        }
        return ACROPOLIS_HELICOPTER_LANDING_PAD_WARP_ROOM_HANDLED;
    }
    return ACROPOLIS_HELICOPTER_LANDING_PAD_WARP_ORDINARY;
}

s32 acropolisHelicopterLandingPadRefuseKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 acropolisHelicopterLandingPadHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_ACTION_START_ENCOUNTER  = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_ACTION_LATCH_PLACEMENT  = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_WAITING       = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_STARTED       = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_PLACEMENT_TRIGGER       = 4,
        ACROPOLIS_HELICOPTER_LANDING_PAD_CAP_INTERACTION_TRIGGER = 8,
        ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_MUSIC_ENTRY   = 1,
    };
    u8                     actionId;
    WorldCollisionTrigger* placementTrigger;
    WorldCollisionTrigger* capInteractionTrigger;

    if ((request->actionId == ACROPOLIS_HELICOPTER_LANDING_PAD_ACTION_START_ENCOUNTER) && (gAcropolisHelicopterLandingPadEncounterPhase == ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_WAITING) && (gAcropolisHelicopterLandingPadEncounterReady != 0)) {
        // Swap the encounter triggers before normal play resumes.
        gGameSession->flowFlags   = (GAME_SESSION_FLOW_SKIP_AREA_MUSIC | GAME_SESSION_FLOW_REEQUIP_WEAPON);
        gStageCountdownMusicEntry = ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_MUSIC_ENTRY;
        evsStartScriptWithSkip(gAcropolisHelicopterLandingPadEncounterIntroScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gAcropolisHelicopterLandingPadEncounterIntroSkipScript);
        gAcropolisHelicopterLandingPadEncounterPhase = ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_STARTED;
        placementTrigger                             = &gAcropolisHelicopterLandingPadActionTriggers[ACROPOLIS_HELICOPTER_LANDING_PAD_PLACEMENT_TRIGGER];
        capInteractionTrigger                        = &gAcropolisHelicopterLandingPadActionTriggers[ACROPOLIS_HELICOPTER_LANDING_PAD_CAP_INTERACTION_TRIGGER];
        placementTrigger->flags                     |= WORLD_COLLISION_TRIGGER_ENABLED;
        capInteractionTrigger->flags                &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    }
    actionId = request->actionId;
    if (actionId == ACROPOLIS_HELICOPTER_LANDING_PAD_ACTION_LATCH_PLACEMENT) {
        gAcropolisHelicopterLandingPadPostEncounterPlacementEnabled = actionId;
    }
    return 0;
}

s32 acropolisHelicopterLandingPadHandleCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum { ACROPOLIS_HELICOPTER_LANDING_PAD_COMMAND_CONFIRM_DEPARTURE = 4 };

    if ((commandId == ACROPOLIS_HELICOPTER_LANDING_PAD_COMMAND_CONFIRM_DEPARTURE) && (gAcropolisHelicopterLandingPadEncounterPhase == ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_ENCOUNTER_FINISHED)) {
        taskSpawnFromTable(gAcropolisHelicopterLandingPadScriptTasks, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_CONFIRM_DEPARTURE, 0, 0);
    }
    return 0;
}

void acropolisHelicopterLandingPadStartDepartureShakeTimeline(void)
{
    taskSpawnFromTable(gAcropolisHelicopterLandingPadScriptTasks, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_SHAKE_TIMELINE, 0, 0);
}

void acropolisHelicopterLandingPadStartPlayerMoveToSceneMark(void)
{
    taskSpawnFromTable(gAcropolisHelicopterLandingPadScriptTasks, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_MOVE_PLAYER, 0, 0);
}

/// Spawns one triangular vertical shake from a duration and signed pixel amplitude.
///
/// Half-duration is 1..255 updates; the amplitude's left shift and the shake
/// task's signed products must fit s32. Authored amplitudes are 2, 3 and 4.
/// Packs the duration into bits 0..7 and the amplitude above it; retains no pointer.
static void _acropolisHelicopterLandingPadSpawnScreenShake(s32 halfDurationFrames, s32 amplitudePixels)
{
    enum { ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_AMPLITUDE_SHIFT = 8 };

    taskSpawnFromTable(gAcropolisHelicopterLandingPadScriptTasks, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_SCREEN_SHAKE, 0, halfDurationFrames | (amplitudePixels << ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_AMPLITUDE_SHIFT));
}

void acropolisHelicopterLandingPadStartReturnToMist(void)
{
    taskSpawnFromTable(gAcropolisHelicopterLandingPadScriptTasks, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_RETURN_TO_MIST, 0, 0);
}

void acropolisHelicopterLandingPadPlacePlayerAfterEncounter(void)
{
    if (gAcropolisHelicopterLandingPadPostEncounterPlacementEnabled != 0) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &gAcropolisHelicopterLandingPadPostEncounterPlayerPlacement, 0);
    }
}

void acropolisHelicopterLandingPadStartPlayerYawTurn(s32 targetYaw)
{
    taskSpawnFromTable(gAcropolisHelicopterLandingPadScriptTasks, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_TURN_PLAYER, targetYaw, 0);
}

void acropolisHelicopterLandingPadReleaseEncounterBattle(void)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_ACTOR         = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_BATTLE_END_DELAY_FRAMES = 3,
    };

    // The resident API ignores its second argument; rewards come from the actor.
    sceneReleaseBattleRefWithRewards(sceneFindPlacedActor(ACROPOLIS_HELICOPTER_LANDING_PAD_ENCOUNTER_ACTOR), 0x1B);
    gSceneCombatState.signals.bytes.endDelayFrames = ACROPOLIS_HELICOPTER_LANDING_PAD_BATTLE_END_DELAY_FRAMES;
}

void acropolisHelicopterLandingPadLockAttachmentsForEncounter(void)
{
    roomEffectRequestCancelAll();
    gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

void acropolisHelicopterLandingPadSetPlayerPitchPulseState(s32 pulseState)
{
    _gAcropolisHelicopterLandingPadPlayerPitchTask->state = pulseState;
}

void acropolisHelicopterLandingPadMovePlayerToSceneMarkTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_MOVE_START         = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_MOVE_WAIT          = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_MOVE_APPROACH_CLIP = 12,
        ACROPOLIS_HELICOPTER_LANDING_PAD_MOVE_ARRIVAL_CLIP  = 9,
    };
    GameActorMoveAnim moveAnim;

    switch (task->state) {
        case ACROPOLIS_HELICOPTER_LANDING_PAD_MOVE_START:
            moveAnim.approachAnimId = ACROPOLIS_HELICOPTER_LANDING_PAD_MOVE_APPROACH_CLIP;
            moveAnim.arrivalAnimId  = ACROPOLIS_HELICOPTER_LANDING_PAD_MOVE_ARRIVAL_CLIP;
            TASK_MESSAGE_DISPATCH_POINTERS(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_MOVE_TO, &gAcropolisHelicopterLandingPadSceneMarkStorage.destination, &moveAnim);
            task->state++;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_MOVE_WAIT:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                taskKill(task);
            }
            break;
    }
}

void acropolisHelicopterLandingPadScreenShakeTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_INITIALIZE    = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_UPDATE        = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_DURATION_MASK = 0xFF,
    };
    s32 packedShake;
    s32 halfDurationFrames;
    s32 shakeSample;

    packedShake        = task->spawnArg2.value;
    halfDurationFrames = packedShake & ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_DURATION_MASK;

    switch (task->state) {
        case ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_INITIALIZE:
            task->spawnArg1.value = -halfDurationFrames;
            task->state++;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_SHAKE_UPDATE:
            if (halfDurationFrames < task->spawnArg1.value) {
                displaySetShakeY(0);
                taskKill(task);
            } else {
                // Grow and decay the envelope around the middle frame.
                shakeSample = _acropolisHelicopterLandingPadSampleScreenShake(task, packedShake, halfDurationFrames);
                displaySetShakeY(shakeSample);
                task->spawnArg1.value++;
            }
            break;
    }
}

void acropolisHelicopterLandingPadConfirmDepartureTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_START         = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_CHECK_CHOICE  = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_DELAY         = 2,
        ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_WAIT_HOLD     = 3,
        ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_DEPART        = 4,
        ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_CAP_SLOT      = 4,
        ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_CANCEL_CHOICE = 1,
    };

    switch (task->state) {
        case ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_START:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capStartSequenceSlot(ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_CAP_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
            // Fall through: starting the CAP also advances to the choice check.
        case ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_DELAY:
        case ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_WAIT_HOLD:
            task->state++;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_CHECK_CHOICE:
            if (capIsBusy() == 0) {
                if (gCapConfirmedChoiceIndex == ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_CANCEL_CHOICE) {
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
        case ACROPOLIS_HELICOPTER_LANDING_PAD_CONFIRM_DEPART:
            gAcropolisHelicopterLandingPadEncounterPhase = ACROPOLIS_HELICOPTER_LANDING_PAD_PHASE_DEPARTURE_ACCEPTED;
            taskSpawnFromTable(gAcropolisHelicopterLandingPadScriptTasks, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_PREPARE_DEPARTURE, 0, 0);
            taskKill(task);
            break;
    }
}

/// Initializes the room task's message routing, encounter gates and persistent pitch task.
///
/// Requires the room resources and task bank to remain loaded. Registers the
/// task in the room slot, clears both encounter latches, starts the initial room sound script
/// and disables placement trigger 4. The retained pitch task lives until room
/// teardown; allocation failure is not checked. Advances to the room update state.
static void _acropolisHelicopterLandingPadInitRoom(Task* task)
{
    enum { ACROPOLIS_HELICOPTER_LANDING_PAD_PLACEMENT_TRIGGER = 4 };
    WorldCollisionTrigger* placementTrigger;

    task->msgTable = gAcropolisHelicopterLandingPadRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    gAcropolisHelicopterLandingPadPostEncounterPlacementEnabled = 0;
    gAcropolisHelicopterLandingPadEncounterReady                = 0;
    task->state++;
    evsStartScript(gAcropolisHelicopterLandingPadAmbientSoundScript, EVENT_SCRIPT_HUD_KEEP);
    _gAcropolisHelicopterLandingPadPlayerPitchTask = taskSpawnFromTable(gAcropolisHelicopterLandingPadScriptTasks, ACROPOLIS_HELICOPTER_LANDING_PAD_TASK_PLAYER_PITCH, 0, 0);
    placementTrigger                               = &gAcropolisHelicopterLandingPadActionTriggers[ACROPOLIS_HELICOPTER_LANDING_PAD_PLACEMENT_TRIGGER];
    placementTrigger->flags                       &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
}

void acropolisHelicopterLandingPadRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gAcropolisHelicopterLandingPadRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}
