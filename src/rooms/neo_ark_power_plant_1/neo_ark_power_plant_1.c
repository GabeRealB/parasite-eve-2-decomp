#include "rooms/neo_ark_power_plant_1.h"

#include "types.h"

#include "neo_ark_power_plant_1_private.h"

#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/areas.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

static void _neoArkPowerPlant1UpdateRoom(Task* unusedTask);
static void _neoArkPowerPlant1InitializeRoom(Task* task);

/// Power-plant room states: 0 initialize, 1 update generator events/sound, 2 release.
///
/// The dispatcher copies all three callbacks before indexing by `Task::state`.
/// There is no terminator or bounds check; keep the room loaded through dispatch.
static const TaskFuncTable3 _gNeoArkPowerPlant1RoomStateHandlers = {
    { _neoArkPowerPlant1InitializeRoom, _neoArkPowerPlant1UpdateRoom, taskKill },
};

enum {
    NEO_ARK_POWER_PLANT_1_UNUSED_WALK_SOUND = 0x10000015,
    NEO_ARK_POWER_PLANT_1_UNUSED_RUN_SOUND  = 0x10000017,
    NEO_ARK_POWER_PLANT_1_UNUSED_JUMP_SOUND = 0x10000019,
};

/// Retained, unused walking, running and scripted-jump footstep sound bases.
///
/// Type-1 script IDs 0x15, 0x17 and 0x19 select the loaded sound bank;
/// paired animation cues add zero or one. No surface record points here,
/// so this recipe supplies no footsteps in the first plant.
static const WorldCollisionFootstepSounds _gNeoArkPowerPlant1UnusedFootstepSounds SECTION(".data") = {
    .walk         = NEO_ARK_POWER_PLANT_1_UNUSED_WALK_SOUND,
    .run          = NEO_ARK_POWER_PLANT_1_UNUSED_RUN_SOUND,
    .scriptedJump = NEO_ARK_POWER_PLANT_1_UNUSED_JUMP_SOUND,
};

/// Collision policy for surface classes 0..2 and 6..7: probes block, impacts are enabled and pushback applies.
///
/// No footstep cues; the loaded room overlay owns this record.
static WorldCollisionSurfaceProperties _gNeoArkPowerPlant1DefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Collision policy for surface class 3: probes pass and weapon impacts are ignored.
///
/// Grid contacts still apply pushback; no footstep cues.
static WorldCollisionSurfaceProperties _gNeoArkPowerPlant1Surface3Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_PASS_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Collision policy for surface class 4: probes block, impacts are enabled and pushback applies.
///
/// Borrows room-owned walking, running and scripted-jump cue bases while loaded.
static WorldCollisionSurfaceProperties _gNeoArkPowerPlant1Surface4Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &gNeoArkPowerPlant1Surface4FootstepSounds,
};

/// Collision policy for surface class 5: probes block, impacts are enabled and pushback applies.
///
/// Borrows room-owned walking, running and scripted-jump cue bases while loaded.
static WorldCollisionSurfaceProperties _gNeoArkPowerPlant1Surface5Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &gNeoArkPowerPlant1Surface5FootstepSounds,
};

WorldCollisionSurfaceProperties* gNeoArkPowerPlant1SurfacePropertiesByClass[8] = {
    &_gNeoArkPowerPlant1DefaultSurfaceProperties,
    &_gNeoArkPowerPlant1DefaultSurfaceProperties,
    &_gNeoArkPowerPlant1DefaultSurfaceProperties,
    &_gNeoArkPowerPlant1Surface3Properties,
    &_gNeoArkPowerPlant1Surface4Properties,
    &_gNeoArkPowerPlant1Surface5Properties,
    &_gNeoArkPowerPlant1DefaultSurfaceProperties,
    &_gNeoArkPowerPlant1DefaultSurfaceProperties,
};

enum {
    NEO_ARK_POWER_PLANT_1_CLEARED_SAVANNA_VARIANT = 2,
    NEO_ARK_POWER_PLANT_1_SAVANNA_MAP_MARK_SHOW   = 1,
};

/// Savanna layout and map-mark update applied when the first generator is defeated.
///
/// Applies in every save mode, resetting saved enemy poses and selecting
/// layout 2 with the map mark set. One update followed by `AREA_APPLY_END`.
static const AreaApplyRec _gNeoArkPowerPlant1GeneratorClearAreaUpdates[2] SECTION(".data") = {
    { .stage = GAME_STAGE_SHELTER_NEO_ARK, .area = GAME_AREA_NEO_ARK_SAVANNA_ZONE, .variant = NEO_ARK_POWER_PLANT_1_CLEARED_SAVANNA_VARIANT, .policy = AREA_APPLY_MODE_ALWAYS | NEO_ARK_POWER_PLANT_1_SAVANNA_MAP_MARK_SHOW },
    { .stage = AREA_APPLY_END },
};

/// Advances generator-clear events and view-dependent sound in the first power plant.
///
/// Runs in room state 1 with live save, session, scene, display and sound state.
/// Generator defeat starts its clear scene only after ability-wheel and display
/// transitions finish. Entry into saved view 3 starts a separate one-shot scene.
/// After the first plant clears, view changes rearm a four-tick sound delay while
/// the second plant remains uncleared; view 7 starts the script and other views stop it.
/// The task argument is unused; the room overlay owns the retained countdown.
static void _neoArkPowerPlant1UpdateRoom(Task* unusedTask)
{
    enum {
        NEO_ARK_POWER_PLANT_1_GENERATOR_PLACEMENT     = 0,
        NEO_ARK_POWER_PLANT_1_CLEAR_SCENE_EVENT       = 0x16,
        NEO_ARK_POWER_PLANT_1_ENTRY_SCENE_VIEW        = 3,
        NEO_ARK_POWER_PLANT_1_GENERATOR_SOUND_VIEW    = 7,
        NEO_ARK_POWER_PLANT_1_VIEW_SOUND_DELAY_FRAMES = 4,
        NEO_ARK_POWER_PLANT_1_GENERATOR_SOUND_SCRIPT  = 0x0A,
    };
    Task* generatorTask;

    // Wait until presentation can accept the generator-clear scene.
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_1_CLEARED) == 0) {
        generatorTask = sceneFindPlacedActor(NEO_ARK_POWER_PLANT_1_GENERATOR_PLACEMENT);
        if (generatorTask != NULL) {
            if (taskMessageDispatch(generatorTask, ACTOR_MESSAGE_IS_PRESENT, 0, 0) == 0) {
                if (gAttachmentState.mode != ATTACHMENT_MODE_WHEEL) {
                    if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                        gameFlagSetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_1_CLEARED, 1);
                        gameFlagSetNibble(GAME_FLAG_NEO_ARK_FOREST_ZONE_UNLOCKED, 1);
                        gameFlagSetNibble(GAME_FLAG_MAP_MARK_POWER_PLANT_1, 0);
                        areaApplySavedUpdates(_gNeoArkPowerPlant1GeneratorClearAreaUpdates);
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = NEO_ARK_POWER_PLANT_1_CLEAR_SCENE_EVENT;
                        evsStartScriptWithSkip(gNeoArkPowerPlant1GeneratorClearScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gNeoArkPowerPlant1GeneratorClearSkipScript);
                    }
                }
            }
        }
    }
    if ((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view == NEO_ARK_POWER_PLANT_1_ENTRY_SCENE_VIEW) && (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_1_0FB) == 0)) {
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_1_0FB, 1);
        gGameSession->battleResetPending            = 0;
        gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_IDLE;
        evsStartScript(gNeoArkPowerPlant1BattleEntryScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
    // Delay sound changes until the saved and active views settle.
    if ((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view != gGameSession->location.view) && (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_1_CLEARED) != 0) && (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) == 0)) {
        gNeoArkPowerPlant1ViewSoundDelayFrames = NEO_ARK_POWER_PLANT_1_VIEW_SOUND_DELAY_FRAMES;
        return;
    }
    if (gNeoArkPowerPlant1ViewSoundDelayFrames != 0) {
        if (--gNeoArkPowerPlant1ViewSoundDelayFrames == 0) {
            if (gGameSession->location.view == NEO_ARK_POWER_PLANT_1_GENERATOR_SOUND_VIEW) {
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_1, NEO_ARK_POWER_PLANT_1_GENERATOR_SOUND_SCRIPT), 0, 0);
                return;
            }
            sndEvtRequestScriptStop(SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_POWER_PLANT_1, NEO_ARK_POWER_PLANT_1_GENERATOR_SOUND_SCRIPT), SOUND_SCRIPT_STOP_KEEP_RELEASE);
        }
    }
}

s32 neoArkPowerPlant1RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 neoArkPowerPlant1ResolveRoomVariant(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { NEO_ARK_POWER_PLANT_1_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    return NEO_ARK_POWER_PLANT_1_TRANSITION_ALLOWED;
}

s32 neoArkPowerPlant1HandleCapCommand(Task* unusedTask, s32 unusedMessageId, s32 commandIndex, s32 unusedSecondArg)
{
    enum { COMMAND_CHECK_PLANT                  = 2,
           COMMAND_CHECK_GENERATOR              = 3,
           COMMAND_CHECK_SECOND_PLANT           = 9,
           COMMAND_CHECK_SECOND_PLANT_ALTERNATE = 12,
           CAP_PLANT_UNCLEARED                  = 2,
           CAP_PLANT_CLEARED                    = 5,
           CAP_GENERATOR_ACTIVE                 = 3,
           CAP_GENERATOR_PART_DOWN              = 6,
           CAP_BATTLE_FINISHED                  = 7,
           CAP_SECOND_PLANT_UNCLEARED           = 9,
           CAP_SECOND_PLANT_CLEARED             = 11,
           CAP_SECOND_PLANT_UNCLEARED_ALTERNATE = 12,
           CAP_SECOND_PLANT_CLEARED_ALTERNATE   = 10 };
    s32 capCommand;

    switch (commandIndex) {
        case COMMAND_CHECK_PLANT:
            if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_1_CLEARED) == 0) {
                capCommand = CAP_PLANT_UNCLEARED;
            } else {
                capCommand = CAP_PLANT_CLEARED;
            }
            capRunCommandWithTransition(capCommand);
            break;
        case COMMAND_CHECK_GENERATOR:
            if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_FINISHED) {
                capCommand = CAP_BATTLE_FINISHED;
            } else if (gameFlagGetNibble(GAME_FLAG_POWER_PLANT_1_GENERATOR_PART_DOWN) != 0) {
                capCommand = CAP_GENERATOR_PART_DOWN;
            } else {
                capCommand = CAP_GENERATOR_ACTIVE;
            }
            capRunCommandWithTransition(capCommand);
            break;
        case COMMAND_CHECK_SECOND_PLANT:
            if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) == 0) {
                capCommand = CAP_SECOND_PLANT_UNCLEARED;
            } else {
                capCommand = CAP_SECOND_PLANT_CLEARED;
            }
            capRunCommandWithTransition(capCommand);
            break;
        case COMMAND_CHECK_SECOND_PLANT_ALTERNATE:
            if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) != 0) {
                capCommand = CAP_SECOND_PLANT_CLEARED_ALTERNATE;
            } else {
                capCommand = CAP_SECOND_PLANT_UNCLEARED_ALTERNATE;
            }
            capRunCommandWithTransition(capCommand);
            break;
    }
    return 0;
}

s32 neoArkPowerPlant1IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg)
{
    return 0;
}

void neoArkPowerPlant1PrepareGeneratorClearScene(void)
{
    roomEffectRequestCancelAll();
    gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

void neoArkPowerPlant1StopSkippedSceneVibration(void)
{
    padScriptHalt();
}

/// Installs the power plant's room-message receiver and prepares its battle state.
///
/// Variant 1 selects ending-music suppression. Before the view-3 event flag
/// is set, requests a battle/result reset and marks the battle finished.
/// Advances to the plant's update state; the room slot borrows the live task.
static void _neoArkPowerPlant1InitializeRoom(Task* task)
{
    enum { NEO_ARK_POWER_PLANT_1_SKIP_ENDING_MUSIC_VARIANT = 1,
           NEO_ARK_POWER_PLANT_1_BATTLE_RESET_REQUESTED    = 1 };

    task->msgTable = gNeoArkPowerPlant1RoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gGameSession->location.variant == NEO_ARK_POWER_PLANT_1_SKIP_ENDING_MUSIC_VARIANT) {
        gGameSession->flowFlags = GAME_SESSION_FLOW_SKIP_ENDING_MUSIC;
    }
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_1_0FB) == 0) {
        gGameSession->battleResetPending            = NEO_ARK_POWER_PLANT_1_BATTLE_RESET_REQUESTED;
        gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_FINISHED;
    }
    task->state = task->state + 1;
}

void neoArkPowerPlant1RoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gNeoArkPowerPlant1RoomStateHandlers;
    stateHandlers.funcs[task->state](task);
}
