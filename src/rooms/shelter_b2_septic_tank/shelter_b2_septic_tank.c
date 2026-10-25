#include "rooms/shelter_b2_septic_tank.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/actor_00400.h"
#include "actors/actor_01100.h"
#include "actors/actor_02000.h"
#include "actors/actor_02100.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/sound.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/sound_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/effects/water_effects.h"
#include "../../shared/room/room_events.h"

/// World-space beam endpoint pairs and indicator positions for the septic tank.
///
/// Signed-halfword XYZ positions use integer game units. Each beam draw
/// borrows both endpoints of one pair; disc draws borrow one centre. View
/// selection changes the subset drawn, while the positions remain fixed.
typedef struct {
    SVECTOR beamEnds[35][2]; // Two consecutive world positions per beam
    SVECTOR discCenters[3];  // Indicator centres (index 0 green, indices 1 and 2 red)
} _ShelterB2SepticTankGlowPoints;
STATIC_ASSERT_SIZEOF(_ShelterB2SepticTankGlowPoints, 0x248);

static void _waterDrawSpin(const GfxCoord* coord, s16 textureColumn, s16 radiusScale, s16 spinAngle);
static void _waterDrawTile(const GfxCoord* coord, s16 frameIndex, s16 radiusScale);

static inline s32 _shelterB2SepticTankStartEvent(const RoomEventMsg* destination, const RoomLatchedEvent* event);
static void       _shelterB2SepticTankInitializeRoom(Task* task);
static void       _shelterB2SepticTankMonitorEncounterIntro(Task* task);

static void _shelterB2SepticTankInitializeWater(Task* task);
static void _shelterB2SepticTankDrawWater(Task* task);

extern RoomFadeStorage  gRoomEventFade;
static RoomEventMsg     _gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

/// Next byte for mixed water quad and draw-mode packets in a borrowed actor-load buffer.
///
/// Reset once per frame to the current word-aligned 0xC000-byte half, then
/// advanced by both surface lists (at most 0x1800 bytes together). The buffer
/// stays reserved until the GPU finishes the frame's ordering table.
u8* _gShelterB2SepticTankWaterPacketCursor;

static void _shelterB2SepticTankWaterTask(Task* task);

static void _shelterB2SepticTankSetEncounterPhase(s32 encounterPhase);
static void _shelterB2SepticTankPrepareEncounterFacing(void);

static s32 _shelterB2SepticTankRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32 _shelterB2SepticTankResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelterB2SepticTankIgnoreCommand(Task* task, s32 messageId, s32 command, s32 commandArg);
static s32 _shelterB2SepticTankHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused);

enum {
    SHELTER_B2_SEPTIC_TANK_ENCOUNTER_INTRO_COMPLETE = 1,
    SHELTER_B2_SEPTIC_TANK_ENCOUNTER_COMBAT_READY   = 2
};

enum {
    SHELTER_B2_SEPTIC_TANK_WATER_INITIALIZE,
    SHELTER_B2_SEPTIC_TANK_WATER_DRAW,
    SHELTER_B2_SEPTIC_TANK_WATER_STATE_COUNT
};

enum {
    SHELTER_B2_SEPTIC_TANK_ENCOUNTER_BLEND_FRAMES = 8,
    SHELTER_B2_SEPTIC_TANK_COMBAT_SETUP_ANIMATION = 1
};

// Commands in the Bog Diver's room-introduction namespace.
enum {
    SHELTER_B2_SEPTIC_TANK_COMMAND_INTRO_SWIM    = 3,
    SHELTER_B2_SEPTIC_TANK_COMMAND_INTRO_SURFACE = 4,
    SHELTER_B2_SEPTIC_TANK_COMMAND_FIGHT         = 5
};

static AnimationPackedPose _gShelterB2SepticTankAnimation05958Bank1[6] = {
#include "assets/shelter_b2_septic_tank_animation_05958_bank1.inc"
};

static AnimationPackedRotation _gShelterB2SepticTankAnimation05958Bank4[64] = {
#include "assets/shelter_b2_septic_tank_animation_05958_bank4.inc"
};

static AnimationRecord _gShelterB2SepticTankAnimation05958Records[141] = {
#include "assets/shelter_b2_septic_tank_animation_05958_records.inc"
};

static u16 _gShelterB2SepticTankAnimation05958Indices[20] = {
#include "assets/shelter_b2_septic_tank_animation_05958_indices.inc"
};

static AnimationSet _gShelterB2SepticTankAnimation05958 = {
    _gShelterB2SepticTankAnimation05958Records,
    _gShelterB2SepticTankAnimation05958Indices,
    { NULL, _gShelterB2SepticTankAnimation05958Bank1, NULL, NULL, _gShelterB2SepticTankAnimation05958Bank4, NULL, NULL, NULL },
};

/// Bodyless priority-32 task recipe for the latched departure event.
///
/// The gate copies the destination and event records before spawning this
/// task at state zero with unused zero payloads. The loaded room owns those
/// records until the task finishes CAP/sound playback and requests reload.
static TaskDesc _gShelterB2SepticTankStagedEventTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, roomEventStagedTask, { .value = 0 } };

/// Room-message routes: resolve departures, refuse key items, handle encounter actions, ignore commands.
///
/// 4 live entries followed by TASK_MESSAGE_TABLE_END/NULL. The room controller
/// borrows this table; message-specific payloads are consumed synchronously.
/// Keep the room overlay and callback code loaded while the receiver is live.
static TaskMessageEntry _gShelterB2SepticTankRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB2SepticTankResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB2SepticTankRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB2SepticTankHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB2SepticTankIgnoreCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// One room-owned player clip installed at the weapon bank's first extension slot.
///
/// The bank copy borrows this one-word table during dispatch; the set and its
/// packed animation arrays must remain loaded throughout introductory playback.
static AnimationSet* _gShelterB2SepticTankEncounterAnimationSets[1] = {
    &_gShelterB2SepticTankAnimation05958,
};

/// Installs the encounter clip in the player's loaded weapon bank without playing it.
///
/// Copies exactly one set pointer at `ANIMATION_BANK_BASE_SET_COUNT`, leaving
/// the base clips and later extension words intact. The request is borrowed
/// through dispatch; playback subsequently borrows the room-owned clip data.
static AnimationBankCopyRequest _gShelterB2SepticTankEncounterAnimationBankCopy = { _gShelterB2SepticTankEncounterAnimationSets, ARRAY_SIZE(_gShelterB2SepticTankEncounterAnimationSets) };

/// Plays the installed encounter clip with an eight-frame blend and grid collision.
///
/// Event dispatch replaces `source.index` with the equipped weapon bank before
/// consuming the request. Clip data remains borrowed from the loaded room.
static AnimationPlayRequest _gShelterB2SepticTankIntroAnimationRequest = { { .index = 1 }, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_INTERPOLATE, SHELTER_B2_SEPTIC_TANK_ENCOUNTER_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Three unreferenced command records tagged for this room; their intended use is unproven.
ActorCommand D_shelter_b2_septic_tank_80182F94[3] = {
    { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK } }, 0 },
    { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK } }, 1 },
    { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK } }, 2 },
};

/// Broadcast cue starting the introductory diver's 48-tick swim.
///
/// The scene forwards this borrowed room-context command synchronously; divers
/// latch its value, and the waiting partner ignores it until the fight cue.
static ActorCommand _gShelterB2SepticTankIntroSwimCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK } }, SHELTER_B2_SEPTIC_TANK_COMMAND_INTRO_SWIM };

/// Broadcast cue making the introductory diver surface and discharge.
///
/// The scene forwards this borrowed room-context command synchronously; the
/// diver retains the value until its scripted surface-cue step consumes it.
static ActorCommand _gShelterB2SepticTankIntroSurfaceCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK } }, SHELTER_B2_SEPTIC_TANK_COMMAND_INTRO_SURFACE };

/// Broadcast cue handing both introductory divers to ordinary combat.
///
/// The scene forwards this borrowed room-context command synchronously. Each
/// waiting diver becomes targetable, claims a surface spot and enters its dive.
static ActorCommand _gShelterB2SepticTankFightCommand = { { .loc = { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK } }, SHELTER_B2_SEPTIC_TANK_COMMAND_FIGHT };

/// Unreferenced indexed request for bank 1/clip 1, reset with collision enabled; its receiver is unproven.
AnimationPlayRequest D_shelter_b2_septic_tank_80182FAC = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Selects weapon-bank clip 1 before the player's encounter turn and combat setup.
///
/// Blends for eight frames with grid collision enabled. Event dispatch replaces
/// `source.index` with the equipped weapon bank; that bank owns the clip data.
static AnimationPlayRequest _gShelterB2SepticTankCombatSetupAnimationRequest = { { .index = 1 }, SHELTER_B2_SEPTIC_TANK_COMBAT_SETUP_ANIMATION, ANIMATION_BLEND_INTERPOLATE, SHELTER_B2_SEPTIC_TANK_ENCOUNTER_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE };

/// Player placement applied after the fight cue and the switch to view 3.
///
/// Position uses room-space game units and yaw uses 4096 units per turn.
/// The player's placement message consumes both complete vectors synchronously.
static ActorTransform _gShelterB2SepticTankCombatPlayerPlacement = { .pos = { 256, 0, -6802, 0 }, .rot = { 0, -1536, 0, 0 } };

/// Persistent target-yaw request for the player's turn toward placed actor zero.
///
/// Only `rot.vy` is updated, in 4096ths of a turn wrapped to 0..4095. A missing
/// target preserves the previous yaw. The turn message consumes that yaw during
/// dispatch; the full transform remains allocated for the overlay's lifetime.
static ActorTransform _gShelterB2SepticTankEncounterFacingRequest = { 0 };

/// Introductory CAP selection, its direct view-id policy and interpreter settling delay.
enum {
    SHELTER_B2_SEPTIC_TANK_INTRO_CAP_COMMAND         = 1,
    SHELTER_B2_SEPTIC_TANK_INTRO_CAP_DIRECT_VIEW_IDS = 1,
    SHELTER_B2_SEPTIC_TANK_INTRO_SETTLE_FRAMES       = 3,
};

/// Bog Diver introduction synchronized to CAP command 1 and its swim cue.
///
/// Eleven commands including END. Installs the player intro clip, uses direct
/// CAP view ids and broadcasts the swim command between CAP cues. Ends player
/// scripting with an animation reset, waits a three-update countdown, then
/// records encounter phase 1. The room monitor starts it once with HUD
/// hide/restore; room, player, scene, CAP and animation resources stay borrowed.
static EvsCommand _gShelterB2SepticTankEncounterIntroScript[11] = {
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, .operand3 = { .message = { .animationBankCopy = &_gShelterB2SepticTankEncounterAnimationBankCopy } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_CAP_CONTROL }, .operand2 = { .value = CAP_CONTROL_MESSAGE_START }, .operand3 = { .value = SHELTER_B2_SEPTIC_TANK_INTRO_CAP_COMMAND } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gShelterB2SepticTankIntroAnimationRequest } },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, .operand0 = { .value = SHELTER_B2_SEPTIC_TANK_INTRO_CAP_DIRECT_VIEW_IDS } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, .operand2 = { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, .operand3 = { .message = { .command = &_gShelterB2SepticTankIntroSwimCommand } }, .operand4 = { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, .operand3 = { .value = PLAYER_ACTOR_END_SCRIPTED_RESET_ANIMATION } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = SHELTER_B2_SEPTIC_TANK_INTRO_SETTLE_FRAMES } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _shelterB2SepticTankSetEncounterPhase }, .operand1 = { .value = SHELTER_B2_SEPTIC_TANK_ENCOUNTER_INTRO_COMPLETE } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

enum {
    SHELTER_B2_SEPTIC_TANK_COMBAT_CAP_COMMAND   = 2,
    SHELTER_B2_SEPTIC_TANK_COMBAT_VIEW          = 3,
    SHELTER_B2_SEPTIC_TANK_COMBAT_SETTLE_FRAMES = 3,
};

/// Turns the player toward the diver, synchronizes CAP 2 and hands the encounter to combat.
///
/// Eighteen instructions including END; room action 2 starts this after phase 1.
/// Broadcasts surface/fight cues, engages battle, places the player in view 3,
/// ends scripted control and commits saved phase 2. Zero-count waits still yield
/// between updates. The loaded room, actors, CAP and animation data remain borrowed.
static EvsCommand _gShelterB2SepticTankCombatSetupScript[18] = {
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gShelterB2SepticTankCombatSetupAnimationRequest } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_CAP_CONTROL }, .operand2 = { .value = CAP_CONTROL_MESSAGE_START }, .operand3 = { .value = SHELTER_B2_SEPTIC_TANK_COMBAT_CAP_COMMAND } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackNoArg = _shelterB2SepticTankPrepareEncounterFacing } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, .operand3 = { .message = { .transform = &_gShelterB2SepticTankEncounterFacingRequest } } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, .operand2 = { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, .operand3 = { .message = { .command = &_gShelterB2SepticTankIntroSurfaceCommand } }, .operand4 = { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = sceneEngageBattle }, .operand1 = { .value = 1 } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_VIEW, .operand0 = { .value = SHELTER_B2_SEPTIC_TANK_COMBAT_VIEW } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_SCENE }, .operand1 = { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, .operand2 = { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, .operand3 = { .message = { .command = &_gShelterB2SepticTankFightCommand } }, .operand4 = { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &_gShelterB2SepticTankCombatPlayerPlacement } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, .operand3 = { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = SHELTER_B2_SEPTIC_TANK_COMBAT_SETTLE_FRAMES } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _shelterB2SepticTankSetEncounterPhase }, .operand1 = { .value = SHELTER_B2_SEPTIC_TANK_ENCOUNTER_COMBAT_READY } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

s16 gShelterB2SepticTankWaterY = 150;

/// Bodyless priority-96 water-surface task recipe, spawned at index 0 with unused zero payloads.
///
/// Spawning reads the recipe synchronously; water drawing borrows this room's
/// geometry and reserved actor-load buffer until task teardown and GPU completion.
static TaskDesc _gShelterB2SepticTankWaterTaskDesc = { { { TASK_BODY_NONE, TASK_PRIORITY_ACTOR } }, _shelterB2SepticTankWaterTask, { .value = 0 } };

/// Two water rectangles starting at Z = -13500, drawn first, followed by the list end.
static RoomWaterSurface _gShelterB2SepticTankFirstWaterWaveSurfaces[] = {
    { -3500, -0x34BC, 2500, 7000, 0 },
    { 1100, -0x34BC, 2400, 7000, 0 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

/// Two water rectangles starting at Z = -6500, appended second, followed by the list end.
static RoomWaterSurface _gShelterB2SepticTankSecondWaterWaveSurfaces[] = {
    { -3500, -6500, 2500, 7000, 0 },
    { 1100, -6500, 2400, 7000, 0 },
    { 0, 0, 0, 0, WATER_SURFACE_LIST_END },
};

/// Authored world-space beam endpoint pairs and indicator-disc centres.
///
/// Coordinates are signed game units, borrowed only during glow drawing.
/// Each mapped view selects a subset of the pairs and coloured discs.
static _ShelterB2SepticTankGlowPoints _gShelterB2SepticTankGlowPoints = {
    {
        { { -813, -33, -0x31FC, 0 }, { -813, -33, -0x3022, 0 } },
        { { -813, -33, -0x29AC, 0 }, { -813, -33, -0x27DA, 0 } },
        { { -813, -33, -8572, 0 }, { -813, -33, -8114, 0 } },
        { { -813, -33, -6503, 0 }, { -813, -33, -6051, 0 } },
        { { -813, -33, -4335, 0 }, { -813, -33, -3968, 0 } },
        { { -813, -33, -2372, 0 }, { -813, -33, -1929, 0 } },
        { { -1941, -33, -404, 0 }, { -1941, -33, 38, 0 } },
        { { 813, -33, -0x31FC, 0 }, { 813, -33, -0x3022, 0 } },
        { { 813, -33, -0x29AC, 0 }, { 813, -33, -0x27DA, 0 } },
        { { 813, -33, -8572, 0 }, { 813, -33, -8114, 0 } },
        { { 813, -33, -6503, 0 }, { 813, -33, -6051, 0 } },
        { { 813, -33, -4335, 0 }, { 813, -33, -3968, 0 } },
        { { 813, -33, -2372, 0 }, { 813, -33, -1929, 0 } },
        { { 1962, -33, -404, 0 }, { 1962, -33, 38, 0 } },
        { { -3238, -3345, -0x32B1, 0 }, { -3238, -3345, -0x2F8D, 0 } },
        { { -3238, -3345, -0x2CF9, 0 }, { -3238, -3345, -0x29D5, 0 } },
        { { -3238, -3345, -9590, 0 }, { -3238, -3345, -8678, 0 } },
        { { -3238, -3345, -7933, 0 }, { -3238, -3345, -7018, 0 } },
        { { -3238, -3345, -5910, 0 }, { -3238, -3345, -5104, 0 } },
        { { -3238, -3345, -4443, 0 }, { -3238, -3345, -3639, 0 } },
        { { -3238, -3345, -2527, 0 }, { -3238, -3345, -1613, 0 } },
        { { -3238, -3345, -868, 0 }, { -3238, -3345, 42, 0 } },
        { { 3238, -3345, -0x32B1, 0 }, { 3238, -3345, -0x2F8D, 0 } },
        { { 3238, -3345, -0x2CF9, 0 }, { 3238, -3345, -0x29D5, 0 } },
        { { 3238, -3345, -9590, 0 }, { 3238, -3345, -8678, 0 } },
        { { 3238, -3345, -7933, 0 }, { 3238, -3345, -7018, 0 } },
        { { 3238, -3345, -5910, 0 }, { 3238, -3345, -5104, 0 } },
        { { 3238, -3345, -4443, 0 }, { 3238, -3345, -3639, 0 } },
        { { 3238, -3345, -2527, 0 }, { 3238, -3345, -1613, 0 } },
        { { 3238, -3345, -868, 0 }, { 3238, -3345, 42, 0 } },
        { { -2971, -2126, -0x352D, 0 }, { -2223, -2126, -0x352D, 0 } },
        { { 2153, -2126, -0x352D, 0 }, { 2891, -2126, -0x352D, 0 } },
        { { -2739, -2387, 577, 0 }, { -1999, -2387, 577, 0 } },
        { { 1527, -2387, 577, 0 }, { 2265, -2387, 577, 0 } },
        { { -368, -2420, -105, 0 }, { 370, -2420, -105, 0 } },
    },
    {
        { 29, -3157, -0x34E8, 0 },
        { 407, -4428, -0x34E8, 0 },
        { -2557, -2725, -0x34E8, 0 },
    },
};

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

u8* gShelterB2SepticTankViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gShelterB2SepticTankRoomViewCounts[1] = { 6 };

DirectionWarpEntry gShelterB2SepticTankWarpEntries[2] = {
    { { { .word = 0 }, -89, 0, -0x3395 }, { 0, 0, 0, 0 }, { { .word = 3840 }, 400, 0, -0x3106 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK, 2), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK, 1), DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2048 }, -89, 0, -112 }, { 0, 0, 0, 0 }, { { .word = 2560 }, 800, 0, -300 }, { 0, 0, 0, 0 }, SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK, 4), SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK, 3), DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

/// Eight-waypoint Bog Diver patrol ring selected by spawn bits 4..5 equal to zero.
///
/// X/Z use room units and Y is depth 1500, added to the sampled water height.
/// Wraps at index 7 without a terminator; the fourth halfwords are unused.
/// The actor borrows the ring for its lifetime in the loaded room.
static SVECTOR _gShelterB2SepticTankDiverPatrolRing0[8] = {
    { -1700, 1500, -6500, 0 },
    { 1700, 1500, -9000, 0 },
    { -1700, 1500, -9000, 0 },
    { -1700, 1500, -3900, 0 },
    { 1700, 1500, -6500, 0 },
    { -1700, 1500, -9000, 0 },
    { 1700, 1500, -9000, 0 },
    { 1700, 1500, -3900, 0 },
};

/// Eight-waypoint Bog Diver patrol ring shared by selections 1 and 3.
///
/// X/Z use room units and Y is depth 3000, added to the sampled water height.
/// Wraps at index 7 without a terminator; the fourth halfwords are unused.
/// Both directory slots borrow this same storage for each diver's lifetime.
static SVECTOR _gShelterB2SepticTankDiverPatrolRing1[8] = {
    { 1700, 3000, -8000, 0 },
    { -1700, 3000, -5500, 0 },
    { -1700, 3000, -0x2AF8, 0 },
    { 1700, 3000, -5500, 0 },
    { -1700, 3000, -5500, 0 },
    { 1700, 3000, -0x2AF8, 0 },
    { -1700, 3000, -8000, 0 },
    { 1700, 3000, -5500, 0 },
};

/// Eight-waypoint Bog Diver patrol ring selected by spawn bits 4..5 equal to two.
///
/// X/Z use room units and Y is depth 2200, added to the sampled water height.
/// Wraps at index 7 without a terminator; the fourth halfwords are unused.
/// The actor borrows the ring for its lifetime in the loaded room.
static SVECTOR _gShelterB2SepticTankDiverPatrolRing2[8] = {
    { -1700, 2200, -7200, 0 },
    { 1700, 2200, -7200, 0 },
    { 1700, 2200, -3000, 0 },
    { -1700, 2200, -3000, 0 },
    { -1700, 2200, -7200, 0 },
    { 1700, 2200, -7200, 0 },
    { 1700, 2200, -0x2710, 0 },
    { -1700, 2200, -0x2710, 0 },
};

SVECTOR* gShelterB2SepticTankDiverPatrolRings[4] = {
    _gShelterB2SepticTankDiverPatrolRing0,
    _gShelterB2SepticTankDiverPatrolRing1,
    _gShelterB2SepticTankDiverPatrolRing2,
    _gShelterB2SepticTankDiverPatrolRing1,
};

enum {
    SHELTER_B2_SEPTIC_TANK_SURFACE_SPOT_FREE = 0,
    SHELTER_B2_SEPTIC_TANK_SURFACE_SPOT_END  = -1
};

SVECTOR gShelterB2SepticTankDiverSurfaceSpots[12] = {
    { 0, 0, 0, SHELTER_B2_SEPTIC_TANK_SURFACE_SPOT_FREE },
    { -1700, 0, -3900, SHELTER_B2_SEPTIC_TANK_SURFACE_SPOT_FREE },
    { 1700, 0, -3900, SHELTER_B2_SEPTIC_TANK_SURFACE_SPOT_FREE },
    { -1700, 0, -6000, SHELTER_B2_SEPTIC_TANK_SURFACE_SPOT_FREE },
    { 1700, 0, -6000, SHELTER_B2_SEPTIC_TANK_SURFACE_SPOT_FREE },
    { -1700, 0, -8000, SHELTER_B2_SEPTIC_TANK_SURFACE_SPOT_FREE },
    { 1700, 0, -8000, SHELTER_B2_SEPTIC_TANK_SURFACE_SPOT_FREE },
    { -1700, 0, -0x2904, SHELTER_B2_SEPTIC_TANK_SURFACE_SPOT_FREE },
    { 1700, 0, -0x2904, SHELTER_B2_SEPTIC_TANK_SURFACE_SPOT_FREE },
    { -1700, 0, -0x2FA8, SHELTER_B2_SEPTIC_TANK_SURFACE_SPOT_FREE },
    { 1700, 0, -0x2FA8, SHELTER_B2_SEPTIC_TANK_SURFACE_SPOT_FREE },
    { 0, 0, 0, SHELTER_B2_SEPTIC_TANK_SURFACE_SPOT_END },
};

/// Room-space face normals, with 4096 representing unit length.
static SVECTOR _gShelterB2SepticTankCollisionGridNormals[18] = {
#include "assets/shelter_b2_septic_tank_collision_0684C_normals.inc"
};

/// Collision-mesh positions in signed room-space game units.
static SVECTOR _gShelterB2SepticTankCollisionGridVerts[82] = {
#include "assets/shelter_b2_septic_tank_collision_0684C_verts.inc"
};

/// Indexed collision triangles and quads with room-local surface classes.
static WorldCollisionGridFace _gShelterB2SepticTankCollisionGridFaces[31] = {
#include "assets/shelter_b2_septic_tank_collision_0684C_faces.inc"
};

/// Signed face-index lists, each ending with WORLD_COLLISION_GRID_CELL_END.
static s16 _gShelterB2SepticTankCollisionGridCells[270] = {
#include "assets/shelter_b2_septic_tank_collision_0684C_cells.inc"
};

/// Supplies a cell face-list address for the generated collision table.
///
/// Captures `_gShelterB2SepticTankCollisionGridCells`.
/// `halfwordOffset` is evaluated once and must be a constant in 0..269
/// selecting a list start in `s16` elements. Returns a borrowed `s16*`
/// valid while this room is loaded; lists end at `WORLD_COLLISION_GRID_CELL_END`.
/// The producer selects this binding through `cell_face_list_macro`.
#define SHELTER_B2_SEPTIC_TANK_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB2SepticTankCollisionGridCells[(halfwordOffset)])
/// X-major cell directory: three rows of six terminated face-index lists.
static s16* _gShelterB2SepticTankCollisionGridTable[18] = {
#include "assets/shelter_b2_septic_tank_collision_0684C_table.inc"
};
#undef SHELTER_B2_SEPTIC_TANK_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB2SepticTankCollisionGrid = { NULL, _gShelterB2SepticTankCollisionGridNormals, _gShelterB2SepticTankCollisionGridVerts, _gShelterB2SepticTankCollisionGridFaces, _gShelterB2SepticTankCollisionGridTable, 5374, 0x47A9, 3, 6, 4000, ARRAY_SIZE(_gShelterB2SepticTankCollisionGridFaces) };

ViewCamera gShelterB2SepticTankViewCameras[6] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { 34, 0x510F, 6334 } }, 230 },
    { { { { -4042, 0, -661 }, { -25, 4093, 153 }, { 661, 155, -4039 } }, { 821, 1121, 8316 } }, 275 },
    { { { { -4032, 0, -719 }, { 3, 4095, -19 }, { 719, -19, -4032 } }, { 792, 1135, 3708 } }, 269 },
    { { { { 4049, 0, -613 }, { -63, 4074, -418 }, { 609, 422, 4028 } }, { 750, 1507, 9189 } }, 269 },
    { { { { 3860, 0, -1369 }, { -607, 3671, -1712 }, { 1227, 1816, 3459 } }, { 1731, 2887, 3765 } }, 257 },
    { { { { -4024, 0, 763 }, { -152, 4013, -804 }, { -748, -819, -3942 } }, { 1196, 493, 5408 } }, 297 },
};

// Packed GPU addresses and neutral modulation for the view's 8-bit source rectangles.
enum {
    SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896 = getTPage(1, GPU_BLEND_AVERAGE, 896, 0),
    SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960 = getTPage(1, GPU_BLEND_AVERAGE, 960, 0),
    SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT     = getClut(0, 255),
    SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL  = 128
};

/// Empty sprite list for mapped view 1, preserving decoded background strips.
static SpriteBatch _gShelterB2SepticTankView1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground texture rectangles and sorting depths for mapped view 2.
///
/// Positions and sizes are pixels; UVs are texels within the encoded page.
/// The view's batches select all 83 elements without a source terminator.
static SpriteSource _gShelterB2SepticTankView2SpriteSources[83] = {
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 40, 112, 525, { .fields = { 96, 64 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 40, 104, 535, { .fields = { 96, 72 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 40, 96, 534, { .fields = { 96, 8 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 40, 88, 574, { .fields = { 96, 16 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 40, 80, 794, { .fields = { 96, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 40, 72, 890, { .fields = { 104, 160 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 40, 64, 937, { .fields = { 88, 120 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 40, 56, 1000, { .fields = { 104, 48 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 40, 48, 1048, { .fields = { 104, 56 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 40, 40, 1092, { .fields = { 104, 40 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -160, 112, 674, { .fields = { 88, 32 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -160, 104, 662, { .fields = { 112, 88 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -144, 104, 667, { .fields = { 104, 144 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -160, 96, 678, { .fields = { 112, 80 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -144, 96, 662, { .fields = { 96, 112 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -128, 88, 666, { .fields = { 96, 96 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -152, 88, 670, { .fields = { 104, 128 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -56, 32, 1335, { .fields = { 104, 24 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -56, 40, 1314, { .fields = { 112, 120 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -72, 40, 1247, { .fields = { 112, 152 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -56, 48, 1264, { .fields = { 112, 136 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -80, 48, 1217, { .fields = { 104, 176 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -96, 56, 1039, { .fields = { 104, 232 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -72, 56, 1108, { .fields = { 104, 168 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -136, 80, 734, { .fields = { 104, 200 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -112, 80, 858, { .fields = { 104, 184 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -120, 72, 799, { .fields = { 104, 208 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -96, 72, 947, { .fields = { 104, 216 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -112, 64, 879, { .fields = { 96, 240 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -80, 64, 1036, { .fields = { 112, 224 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 56, 72, 1000, { .fields = { 88, 80 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 56, 64, 1025, { .fields = { 80, 40 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 48, 56, 1037, { .fields = { 96, 248 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 48, 48, 1176, { .fields = { 80, 136 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 48, 40, 1460, { .fields = { 80, 88 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -96, 64, 1408, { .fields = { 96, 224 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -96, 56, 1050, { .fields = { 104, 192 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -96, 48, 1125, { .fields = { 72, 152 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -96, 40, 1370, { .fields = { 56, 48 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -112, 112, 895, { .fields = { 56, 24 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -64, 112, 895, { .fields = { 88, 104 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -24, 112, 911, { .fields = { 64, 144 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -104, 104, 936, { .fields = { 64, 208 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -64, 104, 936, { .fields = { 64, 128 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -24, 104, 936, { .fields = { 64, 176 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -96, 96, 983, { .fields = { 64, 160 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -56, 96, 983, { .fields = { 64, 192 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -16, 96, 983, { .fields = { 64, 248 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -88, 88, 1036, { .fields = { 64, 184 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -48, 88, 1036, { .fields = { 72, 216 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -16, 88, 1036, { .fields = { 64, 200 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -80, 80, 1099, { .fields = { 72, 168 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -48, 80, 1083, { .fields = { 64, 232 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -8, 80, 1099, { .fields = { 64, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -64, 72, 1173, { .fields = { 64, 8 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -32, 72, 1173, { .fields = { 64, 16 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 0, 72, 1157, { .fields = { 72, 64 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -56, 64, 1247, { .fields = { 56, 72 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -16, 64, 1247, { .fields = { 56, 96 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -48, 56, 1340, { .fields = { 56, 112 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -8, 56, 1340, { .fields = { 56, 224 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -40, 48, 1428, { .fields = { 56, 240 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 0, 48, 1444, { .fields = { 56, 80 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -40, 40, 1564, { .fields = { 32, 152 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 0, 40, 1386, { .fields = { 56, 32 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 8, 112, 788, { .fields = { 32, 40 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 8, 104, 818, { .fields = { 40, 104 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 8, 96, 864, { .fields = { 32, 136 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, 16, 88, 936, { .fields = { 64, 56 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, 16, 80, 996, { .fields = { 40, 88 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, 16, 72, 1057, { .fields = { 32, 168 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 16, 64, 1141, { .fields = { 40, 64 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 24, 56, 1242, { .fields = { 64, 120 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 24, 48, 1363, { .fields = { 48, 216 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 64, 8 } }, -152, 112, 783, { .fields = { 0, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 64, 8 } }, -144, 104, 816, { .fields = { 0, 8 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 64, 8 } }, -136, 96, 869, { .fields = { 0, 16 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 56, 8 } }, -120, 88, 931, { .fields = { 8, 56 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -104, 80, 992, { .fields = { 16, 120 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -96, 72, 1056, { .fields = { 16, 128 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -80, 64, 1130, { .fields = { 24, 144 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -72, 56, 1238, { .fields = { 16, 160 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -56, 48, 1235, { .fields = { 32, 176 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
};

/// Four foreground ranges for mapped view 2, after a zero-count background selector.
///
/// Ranges partition all 83 sources; SPRITE_BATCH_END terminates the list.
static SpriteBatch _gShelterB2SepticTankView2SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 30, 0, 0, { 3, 0 } },
    { 30, 9, 0, 0, { 0, 0 } },
    { 39, 26, 0, 0, { 2, 0 } },
    { 65, 18, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground texture rectangles and sorting depths for mapped view 3.
///
/// Positions and sizes are pixels; UVs are texels within the encoded page.
/// The view's batches select all 127 elements without a source terminator.
static SpriteSource _gShelterB2SepticTankView3SpriteSources[127] = {
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 24 } }, 64, 64, 1575, { .fields = { 120, 56 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 16 } }, 56, 64, 1575, { .fields = { 120, 168 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 24 } }, 72, 72, 1575, { .fields = { 120, 80 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 24 } }, 80, 80, 1575, { .fields = { 120, 104 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 16 } }, 88, 88, 1575, { .fields = { 120, 184 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 16 } }, 96, 96, 1575, { .fields = { 120, 152 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 24 } }, 104, 96, 1575, { .fields = { 120, 128 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 48, 112, 666, { .fields = { 88, 24 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 48, 104, 709, { .fields = { 96, 96 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 48, 96, 760, { .fields = { 96, 104 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 48, 88, 819, { .fields = { 96, 16 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 48, 80, 890, { .fields = { 96, 56 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 48, 72, 979, { .fields = { 96, 32 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 48, 64, 1098, { .fields = { 112, 248 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 48, 56, 1200, { .fields = { 104, 112 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 48, 48, 1392, { .fields = { 96, 232 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 48, 40, 1632, { .fields = { 80, 104 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 48, 32, 1964, { .fields = { 80, 120 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 48, 24, 2363, { .fields = { 104, 80 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -8, 24, 2528, { .fields = { 104, 88 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -8, 32, 2382, { .fields = { 104, 64 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -24, 32, 1928, { .fields = { 104, 72 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -16, 40, 1953, { .fields = { 104, 128 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -40, 40, 1632, { .fields = { 96, 136 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -32, 48, 1589, { .fields = { 104, 224 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -56, 48, 1367, { .fields = { 96, 120 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -48, 56, 1351, { .fields = { 96, 160 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -64, 56, 1216, { .fields = { 104, 168 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -56, 64, 1226, { .fields = { 96, 144 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -80, 64, 1053, { .fields = { 88, 200 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -72, 72, 1113, { .fields = { 96, 192 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -96, 72, 951, { .fields = { 96, 40 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -88, 80, 956, { .fields = { 96, 48 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -112, 80, 915, { .fields = { 96, 184 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -96, 88, 895, { .fields = { 88, 208 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -120, 88, 844, { .fields = { 96, 216 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -112, 96, 800, { .fields = { 88, 176 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -136, 96, 801, { .fields = { 88, 240 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -120, 104, 754, { .fields = { 80, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -152, 104, 759, { .fields = { 72, 224 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -136, 112, 717, { .fields = { 56, 16 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -160, 112, 700, { .fields = { 80, 8 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 16 } }, 48, 24, 2070, { .fields = { 104, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 16 } }, -16, 24, 2139, { .fields = { 112, 232 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -160, 80, 1278, { .fields = { 72, 248 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -160, 72, 1164, { .fields = { 56, 32 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -160, 64, 1165, { .fields = { 64, 128 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -160, 56, 1165, { .fields = { 64, 168 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -120, 56, 0, { .fields = { 88, 88 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 16 } }, -112, 40, 0, { .fields = { 120, 216 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -136, 48, 1352, { .fields = { 80, 112 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -160, 48, 1240, { .fields = { 72, 56 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -160, 40, 1224, { .fields = { 72, 216 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -136, 40, 1352, { .fields = { 72, 232 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -120, 32, 0, { .fields = { 80, 48 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -80, 24, 1988, { .fields = { 72, 136 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -80, 16, 4569, { .fields = { 72, 96 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -104, 32, 1605, { .fields = { 72, 40 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -104, 40, 1604, { .fields = { 64, 24 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -104, 48, 1604, { .fields = { 80, 64 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -104, 56, 1604, { .fields = { 80, 72 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -80, 32, 1988, { .fields = { 80, 80 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -80, 40, 1988, { .fields = { 72, 184 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -80, 48, 1987, { .fields = { 72, 192 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -80, 56, 0, { .fields = { 72, 144 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 64, 8 } }, -96, 112, 917, { .fields = { 32, 160 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 0, 24, 2631, { .fields = { 72, 152 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 0, 32, 2384, { .fields = { 32, 48 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 56, 8 } }, -32, 112, 917, { .fields = { 32, 240 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 56, 8 } }, -80, 104, 966, { .fields = { 32, 176 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -24, 104, 966, { .fields = { 32, 8 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 56, 8 } }, -72, 96, 1023, { .fields = { 16, 224 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -16, 96, 1039, { .fields = { 32, 232 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 56, 8 } }, -64, 88, 1090, { .fields = { 24, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -8, 88, 1090, { .fields = { 48, 200 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -56, 80, 1170, { .fields = { 32, 104 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -8, 80, 1170, { .fields = { 40, 64 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -48, 72, 1267, { .fields = { 32, 72 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 0, 72, 1267, { .fields = { 40, 40 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -40, 64, 1386, { .fields = { 48, 208 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 0, 64, 1386, { .fields = { 48, 112 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -24, 56, 1538, { .fields = { 48, 120 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 8, 56, 1538, { .fields = { 40, 192 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -16, 48, 1736, { .fields = { 40, 144 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 16, 48, 1736, { .fields = { 48, 56 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -8, 40, 1991, { .fields = { 24, 184 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 56, 8 } }, -128, 112, 890, { .fields = { 16, 152 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -112, 104, 927, { .fields = { 24, 96 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -8, 24, 2636, { .fields = { 40, 168 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -8, 32, 2269, { .fields = { 48, 248 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -24, 40, 1982, { .fields = { 32, 216 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -32, 48, 1710, { .fields = { 40, 136 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -48, 56, 1505, { .fields = { 24, 24 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -56, 64, 1352, { .fields = { 24, 128 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -72, 72, 1231, { .fields = { 32, 80 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -80, 80, 1138, { .fields = { 8, 32 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -96, 88, 1052, { .fields = { 8, 88 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -104, 96, 991, { .fields = { 0, 56 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 8 } }, 24, 64, 2472, { .fields = { 32, 136 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 32, 32, 2175, { .fields = { 24, 112 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 32, 40, 1987, { .fields = { 24, 120 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 32, 48, 1696, { .fields = { 24, 200 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 32, 56, 1489, { .fields = { 24, 208 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 32, 64, 1352, { .fields = { 16, 144 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 24, 72, 1231, { .fields = { 8, 40 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 24, 80, 1140, { .fields = { 8, 64 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 24, 88, 1036, { .fields = { 56, 88 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 24, 96, 991, { .fields = { 16, 248 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, 16, 104, 933, { .fields = { 0, 168 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, 16, 112, 896, { .fields = { 0, 192 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 128, 72, 1446, { .fields = { 0, 72 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 128, 64, 1500, { .fields = { 8, 8 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 128, 56, 1372, { .fields = { 8, 16 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 128, 48, 1372, { .fields = { 8, 136 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 128, 40, 1372, { .fields = { 32, 16 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 128, 32, 1372, { .fields = { 8, 80 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 128, 24, 1373, { .fields = { 8, 104 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 56 } }, 104, 24, 0, { .fields = { 120, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 112, 40, 1750, { .fields = { 16, 48 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 112, 48, 1750, { .fields = { 16, 160 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 112, 32, 1750, { .fields = { 16, 176 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 112, 24, 1766, { .fields = { 16, 216 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 104, 16, 1776, { .fields = { 8, 232 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 104, 8, 1767, { .fields = { 8, 240 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 104, 0, 4276, { .fields = { 0, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 16 } }, 112, 64, 0, { .fields = { 112, 200 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 112, 56, 1940, { .fields = { 8, 24 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
};

/// Eight foreground ranges for mapped view 3, after a zero-count background selector.
///
/// Ranges partition all 127 sources; SPRITE_BATCH_END terminates the list.
static SpriteBatch _gShelterB2SepticTankView3SpriteBatches[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 6, 0 } },
    { 7, 35, 0, 0, { 1, 0 } },
    { 42, 2, 0, 0, { 4, 0 } },
    { 44, 21, 0, 0, { 0, 0 } },
    { 65, 21, 0, 0, { 5, 0 } },
    { 86, 12, 0, 0, { 3, 0 } },
    { 98, 12, 0, 0, { 7, 0 } },
    { 110, 17, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground texture rectangles and sorting depths for mapped view 4.
///
/// Positions and sizes are pixels; UVs are texels within the encoded page.
/// The view's batches select all 144 elements without a source terminator.
static SpriteSource _gShelterB2SepticTankView4SpriteSources[144] = {
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 16 } }, -56, 48, 3599, { .fields = { 112, 112 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 56 } }, -56, 64, 0, { .fields = { 120, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 32 } }, -64, 88, 3202, { .fields = { 120, 144 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 24 } }, -64, 48, 1612, { .fields = { 120, 232 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 32 } }, -72, 48, 1691, { .fields = { 120, 176 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 24 } }, -72, 96, 1065, { .fields = { 120, 208 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 24 } }, -80, 96, 1102, { .fields = { 112, 24 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 16 } }, -88, 72, 1603, { .fields = { 112, 128 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 16 } }, -88, 104, 1110, { .fields = { 112, 48 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -104, 112, 1111, { .fields = { 64, 232 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 16 } }, -104, 80, 1640, { .fields = { 112, 80 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 24 } }, -96, 72, 1650, { .fields = { 112, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 16 } }, -80, 64, 1613, { .fields = { 112, 64 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -72, 112, 676, { .fields = { 64, 128 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -64, 104, 714, { .fields = { 80, 232 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -64, 96, 756, { .fields = { 72, 120 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -64, 88, 804, { .fields = { 88, 128 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -64, 80, 858, { .fields = { 88, 16 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -64, 72, 922, { .fields = { 88, 56 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -64, 64, 1000, { .fields = { 88, 112 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -56, 56, 1242, { .fields = { 96, 8 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -56, 48, 1233, { .fields = { 96, 48 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -56, 40, 1328, { .fields = { 96, 24 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -56, 32, 1325, { .fields = { 96, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -56, 24, 1687, { .fields = { 96, 120 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -56, 16, 1923, { .fields = { 96, 80 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -88, 16, 2027, { .fields = { 56, 112 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -88, 8, 2385, { .fields = { 48, 224 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -88, 24, 2014, { .fields = { 80, 72 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 136, 72, 1302, { .fields = { 88, 88 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 128, 64, 1285, { .fields = { 80, 64 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 128, 56, 1250, { .fields = { 80, 40 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 128, 48, 1246, { .fields = { 64, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 128, 40, 1243, { .fields = { 64, 8 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 16 } }, 128, 24, 3770, { .fields = { 88, 192 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 16 } }, 128, 8, 3764, { .fields = { 88, 208 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 16 } }, 96, 0, 4069, { .fields = { 96, 224 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 96, 16, 1574, { .fields = { 72, 224 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 16 } }, 64, 0, 1985, { .fields = { 88, 240 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 64, 16, 1994, { .fields = { 80, 136 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 64, 24, 2000, { .fields = { 64, 80 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 96, 24, 1594, { .fields = { 72, 24 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 96, 32, 1567, { .fields = { 64, 88 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 64, 32, 2118, { .fields = { 56, 16 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 88, 40, 1683, { .fields = { 56, 32 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 96, 48, 1710, { .fields = { 88, 32 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -80, 8, 2377, { .fields = { 48, 96 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -40, 16, 2086, { .fields = { 48, 104 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -40, 8, 2406, { .fields = { 48, 56 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -80, 16, 2008, { .fields = { 48, 192 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 0, 16, 2086, { .fields = { 40, 200 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 0, 8, 2481, { .fields = { 32, 40 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 56, 8 } }, -24, 112, 784, { .fields = { 32, 240 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 56, 8 } }, 32, 112, 784, { .fields = { 32, 248 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 56, 8 } }, 24, 104, 826, { .fields = { 32, 208 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -24, 104, 826, { .fields = { 32, 72 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 24, 96, 872, { .fields = { 32, 64 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -24, 96, 856, { .fields = { 32, 136 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -24, 88, 908, { .fields = { 32, 176 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 24, 88, 908, { .fields = { 32, 168 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 16, 80, 968, { .fields = { 32, 160 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -24, 80, 968, { .fields = { 40, 184 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -24, 72, 1037, { .fields = { 32, 24 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, 16, 72, 1037, { .fields = { 32, 48 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, 8, 64, 1117, { .fields = { 24, 80 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -32, 64, 1117, { .fields = { 24, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -32, 56, 1212, { .fields = { 24, 8 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 8, 56, 1212, { .fields = { 40, 120 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 0, 48, 1325, { .fields = { 32, 216 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -32, 48, 1325, { .fields = { 32, 128 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -8, 40, 1464, { .fields = { 32, 144 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -32, 40, 1464, { .fields = { 64, 216 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -8, 32, 1636, { .fields = { 32, 88 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -32, 32, 1636, { .fields = { 40, 232 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -32, 24, 1873, { .fields = { 8, 16 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 16 } }, 8, 8, 4590, { .fields = { 80, 176 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 16 } }, -32, 8, 4772, { .fields = { 80, 160 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 56, 16 } }, -88, 8, 4772, { .fields = { 64, 144 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 72, 112, 807, { .fields = { 16, 152 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 64, 104, 832, { .fields = { 0, 192 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 56, 96, 884, { .fields = { 0, 104 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 48, 88, 944, { .fields = { 0, 96 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, 48, 80, 993, { .fields = { 8, 224 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, 40, 72, 1063, { .fields = { 120, 208 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, 32, 64, 1149, { .fields = { 120, 248 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, 24, 56, 1241, { .fields = { 120, 240 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, 16, 48, 1367, { .fields = { 120, 216 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 8, 40, 1513, { .fields = { 24, 32 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 8, 32, 1666, { .fields = { 8, 64 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 0, 24, 1908, { .fields = { 8, 48 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 0, 16, 2069, { .fields = { 16, 184 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -152, 64, 1488, { .fields = { 0, 24 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -152, 56, 1354, { .fields = { 16, 56 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -152, 48, 1335, { .fields = { 24, 112 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -128, 40, 1881, { .fields = { 8, 40 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -128, 32, 1748, { .fields = { 16, 120 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -128, 24, 1743, { .fields = { 8, 144 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -128, 16, 1771, { .fields = { 8, 168 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -128, 8, 1782, { .fields = { 8, 136 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -152, 40, 1324, { .fields = { 8, 160 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -152, 32, 1368, { .fields = { 0, 8 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -152, 24, 1364, { .fields = { 0, 32 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 48 } }, -160, 48, 1085, { .fields = { 120, 56 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 40 } }, -160, 8, 1157, { .fields = { 120, 104 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -48, 16, 2022, { .fields = { 8, 176 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -48, 24, 1874, { .fields = { 8, 72 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -48, 32, 1658, { .fields = { 8, 88 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -48, 40, 1479, { .fields = { 8, 128 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -48, 48, 1337, { .fields = { 0, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -48, 56, 1222, { .fields = { 16, 200 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -48, 64, 1128, { .fields = { 120, 56 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -48, 72, 1058, { .fields = { 120, 112 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -48, 80, 976, { .fields = { 104, 16 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -56, 88, 937, { .fields = { 104, 120 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -56, 96, 877, { .fields = { 104, 200 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -56, 104, 817, { .fields = { 104, 184 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -56, 112, 787, { .fields = { 104, 152 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 96, 112, 811, { .fields = { 112, 232 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 96, 104, 833, { .fields = { 112, 48 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 128, 112, 724, { .fields = { 112, 40 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 120, 104, 762, { .fields = { 72, 48 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 88, 96, 865, { .fields = { 112, 64 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 112, 96, 804, { .fields = { 120, 88 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 96, 88, 852, { .fields = { 112, 80 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 80, 88, 937, { .fields = { 120, 72 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 72, 80, 1004, { .fields = { 120, 128 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 88, 80, 906, { .fields = { 104, 96 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 80, 72, 970, { .fields = { 120, 168 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 56, 72, 1087, { .fields = { 104, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 56, 64, 1153, { .fields = { 120, 144 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 72, 64, 1043, { .fields = { 120, 136 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 64, 56, 1130, { .fields = { 8, 80 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 40, 56, 1267, { .fields = { 16, 232 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 48, 48, 1297, { .fields = { 112, 224 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 24, 48, 1448, { .fields = { 112, 176 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 24, 40, 1566, { .fields = { 112, 104 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 40, 40, 1360, { .fields = { 104, 72 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 32, 32, 1474, { .fields = { 104, 32 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 8, 32, 1786, { .fields = { 104, 24 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 8, 24, 1991, { .fields = { 112, 8 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 24, 24, 1691, { .fields = { 120, 160 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 8, 16, 2024, { .fields = { 104, 56 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 16 } }, 24, 0, 2461, { .fields = { 88, 96 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_896, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 24, 16, 2172, { .fields = { 96, 192 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
};

/// Nine foreground ranges for mapped view 4, after a zero-count background selector.
///
/// Ranges partition all 144 sources; SPRITE_BATCH_END terminates the list.
static SpriteBatch _gShelterB2SepticTankView4SpriteBatches[11] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { 13, 16, 0, 0, { 5, 0 } },
    { 29, 17, 0, 0, { 4, 0 } },
    { 46, 6, 0, 0, { 6, 0 } },
    { 52, 26, 0, 0, { 1, 0 } },
    { 78, 13, 0, 0, { 7, 0 } },
    { 91, 13, 0, 0, { 3, 0 } },
    { 104, 13, 0, 0, { 8, 0 } },
    { 117, 27, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Foreground texture rectangles and sorting depths for mapped view 5.
///
/// Positions and sizes are pixels; UVs are texels within the encoded page.
/// The view's batches select all 143 elements without a source terminator.
static SpriteSource _gShelterB2SepticTankView5SpriteSources[143] = {
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 8, 8 } }, 152, 112, 810, { .fields = { 96, 224 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 136, 112, 887, { .fields = { 112, 24 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 144, 104, 885, { .fields = { 112, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 128, 104, 915, { .fields = { 112, 64 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 120, 96, 952, { .fields = { 112, 16 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 136, 96, 915, { .fields = { 112, 8 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -8, 112, 842, { .fields = { 104, 32 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -16, 104, 869, { .fields = { 104, 40 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -8, 96, 899, { .fields = { 112, 48 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -16, 88, 898, { .fields = { 104, 56 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -32, 80, 916, { .fields = { 96, 96 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -32, 96, 939, { .fields = { 104, 72 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 128, 88, 946, { .fields = { 96, 24 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 120, 80, 980, { .fields = { 112, 88 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 112, 88, 985, { .fields = { 112, 80 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 104, 80, 1026, { .fields = { 112, 104 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -80, 112, 903, { .fields = { 104, 112 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -104, 112, 876, { .fields = { 104, 120 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -104, 104, 837, { .fields = { 104, 128 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -80, 104, 849, { .fields = { 104, 168 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -56, 104, 914, { .fields = { 104, 144 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -56, 96, 870, { .fields = { 104, 208 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -80, 96, 851, { .fields = { 104, 160 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -104, 96, 931, { .fields = { 104, 152 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -104, 88, 1042, { .fields = { 104, 176 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -104, 80, 1057, { .fields = { 104, 184 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -104, 72, 1050, { .fields = { 104, 192 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -104, 64, 1040, { .fields = { 104, 200 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -104, 56, 1116, { .fields = { 104, 240 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -104, 48, 1161, { .fields = { 104, 216 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -104, 40, 1196, { .fields = { 104, 136 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -104, 32, 1204, { .fields = { 104, 232 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -104, 24, 1238, { .fields = { 104, 224 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -40, 88, 883, { .fields = { 104, 248 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -64, 88, 878, { .fields = { 88, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -80, 88, 0, { .fields = { 96, 8 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 88, 72, 1076, { .fields = { 88, 16 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 112, 72, 1016, { .fields = { 96, 88 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 104, 64, 1046, { .fields = { 88, 48 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 88, 64, 1105, { .fields = { 88, 72 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 88, 56, 1086, { .fields = { 96, 80 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 88, 48, 1117, { .fields = { 88, 64 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 104, 56, 1121, { .fields = { 96, 104 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 120, 56, 1162, { .fields = { 80, 32 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 144, 48, 1213, { .fields = { 88, 40 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 112, 48, 1129, { .fields = { 88, 56 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 128, 48, 1156, { .fields = { 88, 136 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 128, 40, 1260, { .fields = { 72, 112 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 112, 40, 1177, { .fields = { 88, 128 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 104, 32, 1308, { .fields = { 88, 120 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 120, 32, 1316, { .fields = { 72, 144 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 96, 24, 1406, { .fields = { 80, 152 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 72, 16, 0, { .fields = { 88, 160 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 88, 16, 1480, { .fields = { 88, 168 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 104, 16, 1316, { .fields = { 72, 176 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 72, 8, 1580, { .fields = { 80, 184 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 96, 8, 1490, { .fields = { 88, 192 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 72, 0, 1553, { .fields = { 72, 200 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 72, -8, 1525, { .fields = { 88, 208 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 120, 24, 1325, { .fields = { 88, 216 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 120, 112, 939, { .fields = { 72, 232 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 112, 104, 961, { .fields = { 72, 240 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 104, 96, 991, { .fields = { 72, 248 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 96, 88, 1486, { .fields = { 64, 8 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 88, 80, 1062, { .fields = { 64, 24 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 80, 72, 1088, { .fields = { 64, 80 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 80, 64, 1140, { .fields = { 72, 88 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 0, 112, 932, { .fields = { 64, 96 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -8, 104, 1561, { .fields = { 56, 104 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -8, 96, 1006, { .fields = { 64, 224 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -8, 88, 1025, { .fields = { 64, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 0, 80, 1060, { .fields = { 72, 64 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -88, 96, 999, { .fields = { 56, 40 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -88, 88, 1020, { .fields = { 56, 160 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -56, 88, 986, { .fields = { 64, 56 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -56, 80, 1056, { .fields = { 56, 48 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -24, 80, 1021, { .fields = { 64, 72 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 32, 112, 928, { .fields = { 40, 120 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, 80, 112, 928, { .fields = { 48, 128 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, 72, 104, 957, { .fields = { 48, 136 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, 32, 104, 957, { .fields = { 48, 216 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 24, 96, 987, { .fields = { 56, 168 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 56, 96, 987, { .fields = { 8, 48 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, 56, 88, 1020, { .fields = { 48, 208 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, 16, 88, 1020, { .fields = { 48, 192 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, 48, 80, 1056, { .fields = { 40, 32 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 16, 80, 1056, { .fields = { 48, 152 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 32, 72, 1062, { .fields = { 32, 184 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -16, 72, 1062, { .fields = { 24, 64 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -56, 72, 1062, { .fields = { 32, 200 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -16, 64, 1135, { .fields = { 24, 112 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -56, 64, 1135, { .fields = { 24, 8 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -40, 56, 1180, { .fields = { 24, 176 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 40, 8 } }, -96, 40, 1275, { .fields = { 32, 144 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -56, 40, 1228, { .fields = { 24, 232 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -8, 40, 1234, { .fields = { 24, 240 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -96, 32, 1302, { .fields = { 40, 248 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -64, 32, 1326, { .fields = { 16, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -16, 32, 1341, { .fields = { 16, 80 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, 80, 32, 1325, { .fields = { 32, 24 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 112, 32, 1293, { .fields = { 48, 88 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -72, 24, 1312, { .fields = { 16, 72 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -24, 24, 1397, { .fields = { 16, 56 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 24, 24, 1389, { .fields = { 16, 96 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 16, 16, 1469, { .fields = { 16, 224 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, -32, 16, 1428, { .fields = { 8, 16 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 48, 8 } }, 8, 8, 1479, { .fields = { 8, 40 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 112, 40, 1202, { .fields = { 24, 88 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 88, 40, 1234, { .fields = { 32, 104 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 96, 24, 1373, { .fields = { 24, 208 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 72, 24, 1389, { .fields = { 32, 168 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 88, 16, 1413, { .fields = { 32, 160 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 64, 16, 1429, { .fields = { 24, 128 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 80, 8, 1468, { .fields = { 32, 136 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 56, 8, 1516, { .fields = { 24, 152 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -96, 48, 1213, { .fields = { 24, 192 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -72, 48, 1229, { .fields = { 16, 248 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -96, 56, 1164, { .fields = { 24, 216 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 32, 8 } }, -72, 56, 1180, { .fields = { 56, 16 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 64, 40, 1282, { .fields = { 16, 120 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 40, 40, 1282, { .fields = { 16, 32 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 56, 32, 1325, { .fields = { 8, 24 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 32, 32, 1341, { .fields = { 8, 104 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 56, 56, 1180, { .fields = { 8, 136 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 80, 56, 1200, { .fields = { 8, 144 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 8, 56, 1180, { .fields = { 0, 8 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 32, 56, 1180, { .fields = { 8, 168 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 24, 48, 1229, { .fields = { 0, 152 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 0, 48, 1229, { .fields = { 8, 200 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 48, 48, 1229, { .fields = { 8, 184 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 72, 48, 1245, { .fields = { 0, 64 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -24, 48, 1229, { .fields = { 0, 88 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, -48, 48, 1229, { .fields = { 0, 112 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 56, 64, 1135, { .fields = { 0, 128 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 24, 8 } }, 32, 64, 1135, { .fields = { 0, 216 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -88, 64, 1119, { .fields = { 8, 176 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -72, 64, 1135, { .fields = { 16, 160 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -88, 72, 1078, { .fields = { 8, 208 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -72, 72, 1094, { .fields = { 8, 192 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -72, 80, 1056, { .fields = { 8, 232 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, -88, 80, 1040, { .fields = { 8, 240 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 96, 48, 1226, { .fields = { 0, 0 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
    { SHELTER_B2_SEPTIC_TANK_SPRITE_PAGE_960, SHELTER_B2_SEPTIC_TANK_SPRITE_CLUT, { .fields = { 16, 8 } }, 112, 48, 1180, { .fields = { 0, 32 } }, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, SHELTER_B2_SEPTIC_TANK_SPRITE_NEUTRAL, 0 },
};

/// Two foreground ranges for mapped view 5, after a zero-count background selector.
///
/// Ranges partition all 143 sources; SPRITE_BATCH_END terminates the list.
static SpriteBatch _gShelterB2SepticTankView5SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 60, 0, 0, { 1, 0 } },
    { 60, 83, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite list for mapped view 6, preserving decoded background strips.
static SpriteBatch _gShelterB2SepticTankView6SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gShelterB2SepticTankSpriteViews[6] = {
    { { .empty = _gShelterB2SepticTankView1SpriteBatches }, _gShelterB2SepticTankView1SpriteBatches, NULL },
    { { .elements = _gShelterB2SepticTankView2SpriteSources }, _gShelterB2SepticTankView2SpriteBatches, NULL },
    { { .elements = _gShelterB2SepticTankView3SpriteSources }, _gShelterB2SepticTankView3SpriteBatches, NULL },
    { { .elements = _gShelterB2SepticTankView4SpriteSources }, _gShelterB2SepticTankView4SpriteBatches, NULL },
    { { .elements = _gShelterB2SepticTankView5SpriteSources }, _gShelterB2SepticTankView5SpriteBatches, NULL },
    { { .empty = _gShelterB2SepticTankView6SpriteBatches }, _gShelterB2SepticTankView6SpriteBatches, NULL },
};

/// Eight authored room-space point lights, enabled in every view.
///
/// RGB uses 12 fractional bits; falloff distances use integer game units.
/// Composition and queries update the transform caches and attenuation.
static WorldCoordPointLight _gShelterB2SepticTankPointLights[8] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 20, -41, -4221 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 941, 2662 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 44, -41, -6238 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 961, 2502 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -21, -41, -8592 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 1121, 2522 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 31, -41, -0x28AE } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 921, 2502 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -18, -41, -0x3112 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 1061, 2542 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -499, -7288 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2846, 2876, 2873 }, { 0, 0 } }, 1501, 0x359B },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -1441, -388 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3038, 1650, 1695 }, { 0, 0 } }, 1961, 4841 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 78, -41, -2140 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 861, 4123 },
};

WorldCoordRoomLights gShelterB2SepticTankRoomLights = { 0, NULL, ARRAY_SIZE(_gShelterB2SepticTankPointLights), _gShelterB2SepticTankPointLights, 0, NULL };

WorldCollisionTrigger gShelterB2SepticTankViewBoundaryTriggers[6] = {
    { NULL, NULL, NULL, { -48, -1872, -1809, 0 }, { { 2576, -3904, -176, 0 }, { -2576, -3904, 176, 0 }, { 2576, 3904, -176, 0 }, { -2576, 3904, 176, 0 } }, { 279, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 80, -1985, -1712, 0 }, { { -2400, -3904, 192, 0 }, { 2400, -3904, -192, 0 }, { -2400, 3904, 192, 0 }, { 2400, 3904, -192, 0 } }, { -328, 0, -4092, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -32, -1889, -6671, 0 }, { { 2576, -3904, 0, 0 }, { -2576, -3904, 1, 0 }, { 2576, 3904, 0, 0 }, { -2576, 3904, 1, 0 } }, { 0, 0, 4118, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0, -1953, -6463, 0 }, { { -2400, -3904, 0, 0 }, { 2400, -3904, 0, 0 }, { -2400, 3904, 0, 0 }, { 2400, 3904, 0, 0 } }, { 0, 0, -4118, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0, -1920, -0x2BD1, 0 }, { { 2576, -3904, 336, 0 }, { -2576, -3904, -335, 0 }, { 2576, 3904, 336, 0 }, { -2576, 3904, -335, 0 } }, { -531, 0, 4072, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0, -1952, -0x2B40, 0 }, { { -2400, -3904, -320, 0 }, { 2400, -3904, 320, 0 }, { -2400, 3904, -320, 0 }, { 2400, 3904, 320, 0 } }, { 542, 0, -4066, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

enum {
    // High nibble is our departure slot; low nibble is the destination's arrival slot.
    SHELTER_B2_SEPTIC_TANK_CORRIDOR_WARP_SLOTS    = (1 << 4) | 6,
    SHELTER_B2_SEPTIC_TANK_TUNNEL_WARP_SLOTS      = (2 << 4) | 1,
    SHELTER_B2_SEPTIC_TANK_TRIGGER_IGNORED_ACTION = 1,
    SHELTER_B2_SEPTIC_TANK_TRIGGER_START_COMBAT   = 2
};

WorldCollisionTrigger gShelterB2SepticTankActionTriggers[4] = {
    { NULL, NULL, NULL, { 0, -48, -0x3290, 0 }, { { -1024, 0, -624, 0 }, { 1024, 0, -624, 0 }, { -1024, 0, 624, 0 }, { 1024, 0, 624, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1193, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B2_MAIN_CORRIDOR, SHELTER_B2_SEPTIC_TANK_CORRIDOR_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -48, 192, 0 }, { { -1024, 0, -624, 0 }, { 1024, 0, -624, 0 }, { -1024, 0, 624, 0 }, { 1024, 0, 624, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 1193, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL, SHELTER_B2_SEPTIC_TANK_TUNNEL_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -1793, 0 }, { { -1024, 0, -624, 0 }, { 1024, 0, -624, 0 }, { -1024, 0, 624, 0 }, { 1024, 0, 624, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1193, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, SHELTER_B2_SEPTIC_TANK_TRIGGER_IGNORED_ACTION, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0, -64, -6080, 0 }, { { -1024, 0, -624, 0 }, { 1024, 0, -624, 0 }, { -1024, 0, 624, 0 }, { 1024, 0, 624, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1193, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, SHELTER_B2_SEPTIC_TANK_TRIGGER_START_COMBAT, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

// Placement/resource join keys, independent of actor RAM-slot selection.
enum {
    SHELTER_B2_SEPTIC_TANK_RESOURCE_BOG_DIVER      = 4,
    SHELTER_B2_SEPTIC_TANK_RESOURCE_PAWN_GOLEM     = 20,
    SHELTER_B2_SEPTIC_TANK_RESOURCE_WATCHER        = 21,
    SHELTER_B2_SEPTIC_TANK_RESOURCE_LARGE_MOSSBACK = 49
};

// Bog Diver mode bits: low nibble selects behavior; bits 4..5 select a patrol ring.
enum {
    SHELTER_B2_SEPTIC_TANK_DIVER_SPAWN_SWIM          = 1,
    SHELTER_B2_SEPTIC_TANK_DIVER_SPAWN_INTRO         = 4,
    SHELTER_B2_SEPTIC_TANK_DIVER_SPAWN_INTRO_PARTNER = 5,
    SHELTER_B2_SEPTIC_TANK_DIVER_SPAWN_WOUNDED_FLOAT = 7,
    SHELTER_B2_SEPTIC_TANK_DIVER_PATROL_RING_1       = 1 << 4,
    SHELTER_B2_SEPTIC_TANK_DIVER_PATROL_RING_2       = 2 << 4
};

enum {
    SHELTER_B2_SEPTIC_TANK_WATCHER_STATIONARY_GUN = 4,
    SHELTER_B2_SEPTIC_TANK_PAWN_GOLEM_PATROLS     = 1
};

/// Slot-1 Bog Diver resource recipe for the paired-introduction layout, variant 1.
///
/// Entry 4 requests base file 100400 and body task index zero. Ends at
/// `AREA_PLACEMENT_END`; borrowed room/actor storage stays live through teardown.
static AreaResource _gShelterB2SepticTankVariant1Resources[2] = {
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_BOG_DIVER, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor100400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Slot-1 Bog Diver resource recipe for the two swimming patrols, variant 2.
///
/// Entry 4 requests base file 100400 and body task index zero. Ends at
/// `AREA_PLACEMENT_END`; borrowed room/actor storage stays live through teardown.
static AreaResource _gShelterB2SepticTankVariant2Resources[2] = {
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_BOG_DIVER, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor100400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Slot-1 Bog Diver and slot-2 large Mossback resources for variant 3.
///
/// Join keys 4/49 request files 100400/204900 and task descriptor zero.
/// Ends at `AREA_PLACEMENT_END`; room tables and actor recipes remain borrowed
/// through resource loading, spawning and actor teardown.
static AreaResource _gShelterB2SepticTankVariant3Resources[3] = {
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_BOG_DIVER, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor100400Tasks },
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_LARGE_MOSSBACK, 49, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor201100Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Slot-1 Bog Diver resource recipe for the single swimming patrol, variant 4.
///
/// Entry 4 requests base file 100400 and body task index zero. Ends at
/// `AREA_PLACEMENT_END`; borrowed room/actor storage stays live through teardown.
static AreaResource _gShelterB2SepticTankVariant4Resources[2] = {
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_BOG_DIVER, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor100400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Slot-1 Watcher and slot-2 Bog Diver resources for variant 11.
///
/// Join keys 21/4 request files 102100/200400 and task descriptor zero.
/// Ends at `AREA_PLACEMENT_END`; room tables and actor recipes remain borrowed
/// through resource loading, spawning and actor teardown.
static AreaResource _gShelterB2SepticTankVariant11Resources[3] = {
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_WATCHER, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_BOG_DIVER, 4, AREA_RESOURCE_FILE_GROUP_BASE_20, ACTOR_00400_BODY_TASK_INDEX, { 0, 0 }, gActor200400Tasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Slot-1 Watcher and slot-2 Pawn GOLEM resources for variant 21.
///
/// Join keys 21/20 request files 102100/202000 and task descriptor zero.
/// Ends at `AREA_PLACEMENT_END`; room tables and actor recipes remain borrowed
/// through resource loading, spawning and actor teardown.
static AreaResource _gShelterB2SepticTankVariant21Resources[3] = {
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_WATCHER, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor102100WatcherTaskDesc },
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_PAWN_GOLEM, 20, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, gActor202000PawnGolemTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Paired Bog Divers for variant 1: the scripted introduction and its waiting partner.
///
/// Both select patrol ring 0. Entry IDs join the resource list; coordinates use
/// room units and yaw uses 4096 per turn. Ends at `AREA_PLACEMENT_END`.
/// Actors borrow their placement records until teardown.
static AreaPlacement _gShelterB2SepticTankVariant1Placements[3] = {
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_BOG_DIVER, 0, SHELTER_B2_SEPTIC_TANK_DIVER_SPAWN_INTRO, -1952, 3000, -7000, 0, 0, 0, 2, 0 },
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_BOG_DIVER, 0, SHELTER_B2_SEPTIC_TANK_DIVER_SPAWN_INTRO_PARTNER, 1792, 3000, -0x2E60, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Two swimming Bog Diver patrols for variant 2, selecting rings 0 and 1.
///
/// Entry IDs join the resource list; coordinates use room units and yaw uses
/// 4096 per turn. Ends at `AREA_PLACEMENT_END`; actors borrow the records
/// until teardown.
static AreaPlacement _gShelterB2SepticTankVariant2Placements[3] = {
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_BOG_DIVER, 0, SHELTER_B2_SEPTIC_TANK_DIVER_SPAWN_SWIM, 1700, 1500, -3900, 2500, 0, 0, 2, 0 },
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_BOG_DIVER, 0, SHELTER_B2_SEPTIC_TANK_DIVER_SPAWN_SWIM | SHELTER_B2_SEPTIC_TANK_DIVER_PATROL_RING_1, -1700, 3000, -5500, 2048, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Ring-2 swimming Bog Diver and large Mossback placements for variant 3.
///
/// Entry 49 selects the Mossback's large model/parameter variant. Entry IDs join
/// the resource list; coordinates use room units and yaw uses 4096 per turn.
/// Ends at `AREA_PLACEMENT_END`; actors borrow the records until teardown.
static AreaPlacement _gShelterB2SepticTankVariant3Placements[3] = {
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_BOG_DIVER, 0, SHELTER_B2_SEPTIC_TANK_DIVER_SPAWN_SWIM | SHELTER_B2_SEPTIC_TANK_DIVER_PATROL_RING_2, -1700, 2200, -0x2EE0, 0, 0, 0, 2, 4 },
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_LARGE_MOSSBACK, 1, 0, 300, 0, -5500, 2200, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Single ring-2 swimming Bog Diver placement for variant 4.
///
/// Entry IDs join the resource list; coordinates use room units and yaw uses
/// 4096 per turn. Ends at `AREA_PLACEMENT_END`; the actor borrows its record
/// until teardown.
static AreaPlacement _gShelterB2SepticTankVariant4Placements[2] = {
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_BOG_DIVER, 0, SHELTER_B2_SEPTIC_TANK_DIVER_SPAWN_SWIM | SHELTER_B2_SEPTIC_TANK_DIVER_PATROL_RING_2, -1700, 2200, -0x2EE0, 0, 0, 0, 2, 4 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Two stationary gun Watchers and two wounded floating Bog Divers for variant 11.
///
/// Watcher row 5 sets sight range; divers select ring 0 and start at one eighth
/// maximum HP. Entry IDs join the resource list; coordinates use room units
/// and yaw uses 4096 per turn. Ends at `AREA_PLACEMENT_END`; actors borrow
/// their placement records until teardown.
static AreaPlacement _gShelterB2SepticTankVariant11Placements[5] = {
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_WATCHER, SHELTER_B2_SEPTIC_TANK_WATCHER_STATIONARY_GUN, 0, -700, -2400, 300, 2048, 0, 0, 2, 5 },
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_WATCHER, SHELTER_B2_SEPTIC_TANK_WATCHER_STATIONARY_GUN, 0, 700, -2400, 300, 2048, 0, 0, 2, 5 },
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_BOG_DIVER, 0, SHELTER_B2_SEPTIC_TANK_DIVER_SPAWN_WOUNDED_FLOAT, 1700, 0, -8300, 0, 0, 2, 4, 0 },
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_BOG_DIVER, 0, SHELTER_B2_SEPTIC_TANK_DIVER_SPAWN_WOUNDED_FLOAT, -1800, 0, -8000, 1600, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/// Two stationary gun Watchers and a patrolling Pawn GOLEM for variant 21.
///
/// Watcher row 5 sets sight range; the GOLEM's variant 9 gives a 9000-unit
/// patrol leg. Entry IDs join the resource list; coordinates use room units
/// and yaw uses 4096 per turn. Ends at `AREA_PLACEMENT_END`; actors borrow
/// their placement records until teardown.
static AreaPlacement _gShelterB2SepticTankVariant21Placements[4] = {
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_WATCHER, SHELTER_B2_SEPTIC_TANK_WATCHER_STATIONARY_GUN, 0, -700, -2400, -0x3200, 0, 0, 0, 2, 5 },
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_WATCHER, SHELTER_B2_SEPTIC_TANK_WATCHER_STATIONARY_GUN, 0, 700, -2400, -0x3200, 0, 0, 0, 2, 5 },
    { SHELTER_B2_SEPTIC_TANK_RESOURCE_PAWN_GOLEM, 9, SHELTER_B2_SEPTIC_TANK_PAWN_GOLEM_PATROLS, 0, 0, -0x2710, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant gShelterB2SepticTankAreaVariants[22] = {
    [1]  = { _gShelterB2SepticTankVariant1Placements, _gShelterB2SepticTankVariant1Resources },
    [2]  = { _gShelterB2SepticTankVariant2Placements, _gShelterB2SepticTankVariant2Resources },
    [3]  = { _gShelterB2SepticTankVariant3Placements, _gShelterB2SepticTankVariant3Resources },
    [4]  = { _gShelterB2SepticTankVariant4Placements, _gShelterB2SepticTankVariant4Resources },
    [11] = { _gShelterB2SepticTankVariant11Placements, _gShelterB2SepticTankVariant11Resources },
    [21] = { _gShelterB2SepticTankVariant21Placements, _gShelterB2SepticTankVariant21Resources },
};

// Cue-2 entries in the currently loaded type-1 bank; cue 1 uses the next entry.
enum {
    SHELTER_B2_SEPTIC_TANK_WALK_FOOTSTEP = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x59,
    SHELTER_B2_SEPTIC_TANK_RUN_FOOTSTEP  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5B
};

/// Class-2 paired footstep requests: walking and scripted jumps share cue bases.
///
/// Running uses its own pair; companion playback adds 100 to each entry.
static WorldCollisionFootstepSounds _gShelterB2SepticTankClass2FootstepSounds = {
    SHELTER_B2_SEPTIC_TANK_WALK_FOOTSTEP,
    SHELTER_B2_SEPTIC_TANK_RUN_FOOTSTEP,
    SHELTER_B2_SEPTIC_TANK_WALK_FOOTSTEP,
};

/// Silent blocking surface used by classes 0 and 3..7, with impacts and pushback.
static WorldCollisionSurfaceProperties _gShelterB2SepticTankDefaultSurface = { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL };

/// Silent class-1 surface passing probes and ignoring impacts, with grid pushback.
static WorldCollisionSurfaceProperties _gShelterB2SepticTankProbePassSurface = { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL };

/// Class-2 blocking surface with paired footstep cues, weapon impacts and pushback.
static WorldCollisionSurfaceProperties _gShelterB2SepticTankClass2Surface = { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &_gShelterB2SepticTankClass2FootstepSounds };

WorldCollisionSurfaceProperties* gShelterB2SepticTankSurfacePropertiesByClass[8] = {
    &_gShelterB2SepticTankDefaultSurface,
    &_gShelterB2SepticTankProbePassSurface,
    &_gShelterB2SepticTankClass2Surface,
    &_gShelterB2SepticTankDefaultSurface,
    &_gShelterB2SepticTankDefaultSurface,
    &_gShelterB2SepticTankDefaultSurface,
    &_gShelterB2SepticTankDefaultSurface,
    &_gShelterB2SepticTankDefaultSurface,
};

RoomFadeStorage gRoomEventFade = { 0 };

/// Destination snapshot retained from event acceptance through staged scene reload.
///
/// The start handler copies all eight bytes; playback commits area, warp and
/// room to the live save. Keep the snapshot unchanged until reload tears down
/// the room. A later accepted start replaces it; no separate queue is stored.
static RoomEventMsg _gRoomEventStagedMsg = { 0 };

/// Whether the latest staged-event gate call attempted an event spawn (0 no, 1 yes).
///
/// Every call clears this byte, including queries. Execution raises it after
/// the unchecked spawn; the task does not clear it on completion. No reader
/// is recovered, so this records the latest call rather than a running lifetime.
static u8 _gShelterB2SepticTankStagedEventStarted = 0;

/// Room-local encounter scene stage (0 intro pending, 1 intro considered, 2 combat setup started).
///
/// The view-4 monitor advances to 1 even if saved progress suppresses playback.
/// Arrival action 2 advances to 2 after requesting combat setup. This byte lasts
/// for the loaded room visit and is separate from the saved encounter phase.
static u8 _gShelterB2SepticTankEncounterSceneStage = 0;

/// Unreferenced initialized halfword; its role and access width are unproven.
u16 D_shelter_b2_septic_tank_80187046 = 0x5868;

RoomLatchedEvent gRoomEventLatched;

static void _glowDrawBeam(const SVECTOR worldPoints[2], s32 radiusScale, s32 startAngle, s32 packedColor);

/// Tests a staged room event and latches an eligible executing transition.
///
/// Borrows both complete records for this call; neither is modified. Returns
/// 2 when the event's flag is clear or absent, otherwise 1. Queries only reset
/// the room's event-started byte. Execution copies both records to room-owned
/// storage, sets a present flag to 1 and spawns the staged task. Flag zero
/// stays eligible; its nibble is still read before the absent-flag test.
static inline s32 _shelterB2SepticTankStartEvent(const RoomEventMsg* destination, const RoomLatchedEvent* event)
{
    enum {
        SHELTER_B2_SEPTIC_TANK_EVENT_ALREADY_LATCHED = 1,
        SHELTER_B2_SEPTIC_TANK_EVENT_ELIGIBLE        = 2,
        SHELTER_B2_SEPTIC_TANK_EVENT_FLAG_NONE       = 0,
        SHELTER_B2_SEPTIC_TANK_EVENT_FLAG_CLEAR      = 0,
        SHELTER_B2_SEPTIC_TANK_EVENT_FLAG_LATCHED    = 1
    };
    _gShelterB2SepticTankStagedEventStarted = false;
    if (gameFlagGetNibble(event->flagId) == SHELTER_B2_SEPTIC_TANK_EVENT_FLAG_CLEAR || event->flagId == SHELTER_B2_SEPTIC_TANK_EVENT_FLAG_NONE) {
        if (destination->queryOnly == ROOM_EVENT_EXECUTE) {
            // Own copies before starting the task that consumes them later.
            ROOM_EVENT_STAGED_DESTINATION = *destination;
            gRoomEventLatched             = *event;
            if (event->flagId != SHELTER_B2_SEPTIC_TANK_EVENT_FLAG_NONE) {
                gameFlagSetNibble(event->flagId, SHELTER_B2_SEPTIC_TANK_EVENT_FLAG_LATCHED);
            }
            taskSpawnFromTable(&_gShelterB2SepticTankStagedEventTaskDesc, 0, 0, 0);
            _gShelterB2SepticTankStagedEventStarted = true;
        }
        return SHELTER_B2_SEPTIC_TANK_EVENT_ELIGIBLE;
    }
    return SHELTER_B2_SEPTIC_TANK_EVENT_ALREADY_LATCHED;
}

#include "../../shared/room/room_event_staged_task.inc.c"

/// Refuses key-item use in this room with the inventory's zero reply.
///
/// The callback ABI supplies a live receiver, message 5105, an item ID and an
/// unused word. None are read and no room state is changed.
static s32 _shelterB2SepticTankRejectKeyItem(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return 0;
}

/// Resolves a destination and gates the one-time scene on departure to the B2 main corridor.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE` with borrowed complete eight-byte
/// records; request and reply may alias. Copies and resolves the reply first.
/// Other destinations return 1. The corridor returns 2 while its scene flag
/// is clear, otherwise 1; an executing request latches room-owned copies and
/// starts the scene. A query tests eligibility and clears the event-start marker
/// without latching or spawning the transition.
static s32 _shelterB2SepticTankResolveRoomEvent(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        SHELTER_B2_SEPTIC_TANK_TRANSITION_ALLOWED   = 1,
        SHELTER_B2_SEPTIC_TANK_CORRIDOR_CAP_COMMAND = 3,
        SHELTER_B2_SEPTIC_TANK_CORRIDOR_SOUND_ENTRY = 1,
        SHELTER_B2_SEPTIC_TANK_CORRIDOR_FADE_NONE   = 0
    };
    RoomLatchedEvent event;

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    if (request->areaId != GAME_AREA_SHELTER_B2_MAIN_CORRIDOR) {
        return SHELTER_B2_SEPTIC_TANK_TRANSITION_ALLOWED;
    }
    event.capCmd   = SHELTER_B2_SEPTIC_TANK_CORRIDOR_CAP_COMMAND;
    event.stageSnd = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_SEPTIC_TANK, SHELTER_B2_SEPTIC_TANK_CORRIDOR_SOUND_ENTRY);
    event.flagId   = GAME_FLAG_B2_SEPTIC_TANK_TO_CORRIDOR_SCENE;
    event.fade     = SHELTER_B2_SEPTIC_TANK_CORRIDOR_FADE_NONE;
    return _shelterB2SepticTankStartEvent(reply, &event);
}

/// Ignores room-command messages and returns zero without changing room state.
///
/// Retains the four-word message callback ABI; all arguments are unused.
static s32 _shelterB2SepticTankIgnoreCommand(Task* task, s32 messageId, s32 command, s32 commandArg)
{
    return 0;
}

/// Starts the encounter's combat setup when room action 2 follows the introduction.
///
/// Handles `DIRECTION_MESSAGE_ROOM_ACTION`; the four-byte request is borrowed
/// only for this call and the second payload word is unused. Requires both the
/// saved encounter phase and room-local stage to be 1, then starts the script
/// and advances the local stage to 2. Other actions do nothing; returns zero.
static s32 _shelterB2SepticTankHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unused)
{
    enum { SHELTER_B2_SEPTIC_TANK_ACTION_START_COMBAT = 2 };
    u8  actionId;
    s32 encounterPhase;

    actionId = request->actionId;
    if (actionId == SHELTER_B2_SEPTIC_TANK_ACTION_START_COMBAT) {
        encounterPhase = gameFlagGetNibble(GAME_FLAG_0EB);
        if (encounterPhase == SHELTER_B2_SEPTIC_TANK_ENCOUNTER_INTRO_COMPLETE && _gShelterB2SepticTankEncounterSceneStage == encounterPhase) {
            evsStartScript(_gShelterB2SepticTankCombatSetupScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            _gShelterB2SepticTankEncounterSceneStage = actionId;
        }
    }
    return 0;
}

/// Records the scripted diver encounter's progress in its game-flag nibble.
///
/// Scripts pass 1 after the introduction and 2 after the fight setup. Divers
/// use 1 to finish their introduction and 2 to enter normal combat.
/// `encounterPhase` is a nibble value (0..15); no pointer is retained.
static void _shelterB2SepticTankSetEncounterPhase(s32 encounterPhase)
{
    gameFlagSetNibble(GAME_FLAG_0EB, encounterPhase);
}

/// Prepares the player's scripted turn toward placed actor zero.
///
/// Requires a live player model and scene manager. Writes the room-owned turn
/// request's yaw in 4096ths of a turn, wrapped to 0..4095, from the roots' X/Z
/// translations in their common parent frame. A missing target leaves the
/// previous yaw intact. The encounter script sends the request after this call.
static void _shelterB2SepticTankPrepareEncounterFacing(void)
{
    enum { SHELTER_B2_SEPTIC_TANK_ENCOUNTER_ACTOR_INDEX = 0 };
    Task*           targetTask;
    const GfxCoord* playerRoot;
    const GfxCoord* targetRoot;

    targetTask = sceneFindPlacedActor(SHELTER_B2_SEPTIC_TANK_ENCOUNTER_ACTOR_INDEX);
    playerRoot = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    if (targetTask != NULL) {
        targetRoot = targetTask->extra.tmd->coords;
        _gShelterB2SepticTankEncounterFacingRequest.rot.vy =
            (ratan2(targetRoot->coord.t[0] - playerRoot->coord.t[0], targetRoot->coord.t[2] - playerRoot->coord.t[2]) + ACTOR_TRANSFORM_ANGLE_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
    }
}

/// Registers the room's message receiver and starts its water-surface task.
///
/// Requires the live room task in state 0; publishes it in the room slot and
/// advances to encounter monitoring. The room overlay must stay loaded while
/// either task is dispatched.
static void _shelterB2SepticTankInitializeRoom(Task* task)
{
    task->msgTable = _gShelterB2SepticTankRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(&_gShelterB2SepticTankWaterTaskDesc, 0, 0, 0);
    task->state = task->state + 1;
}

/// Handles the encounter introduction once the room's view and display are ready.
///
/// The room task's monitoring state waits for view 4, variant 1, a closed
/// attachment wheel and no display transition. At local stage zero it starts
/// the introduction if the saved encounter phase is zero, then marks the local
/// stage 1 even when that saved phase suppresses playback. `task` is unused.
static void _shelterB2SepticTankMonitorEncounterIntro(Task* task)
{
    enum {
        SHELTER_B2_SEPTIC_TANK_INTRO_VIEW              = 4,
        SHELTER_B2_SEPTIC_TANK_INTRO_VARIANT           = 1,
        SHELTER_B2_SEPTIC_TANK_INTRO_STAGE_PENDING     = 0,
        SHELTER_B2_SEPTIC_TANK_ENCOUNTER_PHASE_PENDING = 0
    };
    s32 variant;

    if (gGameSession->location.view == SHELTER_B2_SEPTIC_TANK_INTRO_VIEW) {
        variant = gGameSession->location.variant;
        if (variant == SHELTER_B2_SEPTIC_TANK_INTRO_VARIANT && gAttachmentState.mode != ATTACHMENT_MODE_WHEEL && gDisplayState.pendingMode == DISPLAY_MODE_NONE && _gShelterB2SepticTankEncounterSceneStage == SHELTER_B2_SEPTIC_TANK_INTRO_STAGE_PENDING) {
            if (gameFlagGetNibble(GAME_FLAG_0EB) == SHELTER_B2_SEPTIC_TANK_ENCOUNTER_PHASE_PENDING) {
                evsStartScript(_gShelterB2SepticTankEncounterIntroScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            }
            _gShelterB2SepticTankEncounterSceneStage = variant;
        }
    }
}

/// Room-controller states 0..2: initialize the receiver and water task, monitor the encounter intro, teardown.
///
/// The controller copies all three callbacks before unchecked state dispatch.
/// No terminator; keep the room overlay loaded while the controller is live.
static const TaskFuncTable3 _gShelterB2SepticTankStateHandlers = {
    {
        _shelterB2SepticTankInitializeRoom,
        _shelterB2SepticTankMonitorEncounterIntro,
        taskKill,
    },
};

void shelterB2SepticTankRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gShelterB2SepticTankStateHandlers;
    states.funcs[task->state](task);
}

/// Binds the readable first two-rectangle list, including its in-bounds terminator.
///
/// Supplies a `RoomWaterSurface*` without side effects or captured locals.
/// The shared strip include consumes and undefines this binding.
#define WATER_WAVE_STRIPS_SURFACES _gShelterB2SepticTankFirstWaterWaveSurfaces
/// Binds the undisplaced water Y in signed world units.
///
/// Read once per draw from the room's `s16`; no side effects or captured locals.
/// The shared strip include consumes and undefines this binding.
#define WATER_WAVE_STRIPS_HEIGHT gShelterB2SepticTankWaterY
/// Binds the writable byte cursor for mixed water quad and draw-mode packets.
///
/// A stable `u8*` lvalue, read and advanced repeatedly; requires word alignment
/// and a borrowed arena reserved until GPU consumption. The shared strip
/// include consumes and undefines this binding.
/// The caller resets it once before both drawers, which append in draw order.
#define WATER_WAVE_STRIPS_PACKET_CURSOR _gShelterB2SepticTankWaterPacketCursor
/// Scales the seam's sine displacement to -128..128 world-coordinate Y units.
///
/// Integer shift count for `water_wave_strips.inc.c`; see its configuration
/// contract. The include consumes and undefines this binding.
#define WATER_WAVE_STRIPS_AMPLITUDE_SHIFT 5
/// Sets the first X strip's blue vertex colours for subtractive blending.
///
/// Writes all RGB bytes: (0, 64, 128) on the flat edge (vertices 0/1),
/// (0, 16, 32) on the waving seam (2/3). `quad` is writable; other fields
/// are preserved and the pointer is not retained.
static inline void _shelterB2SepticTankSetFirstWaterStripColours(POLY_G4* quad)
{
    setRGB0(quad, 0, 0x40, 0x80);
    setRGB1(quad, 0, 0x40, 0x80);
    setRGB2(quad, 0, 0x10, 0x20);
    setRGB3(quad, 0, 0x10, 0x20);
}

/// Binds the first X strip's blue vertex colours for subtractive blending.
///
/// A writable `POLY_G4*` is evaluated once. The flat outer edge (vertices
/// 0/1) is (0, 0x40, 0x80); the waving seam (vertices 2/3) is (0, 0x10, 0x20),
/// in unsigned RGB bytes. Other packet fields are preserved; no caller locals
/// are captured and no packet pointer is retained. Psy-Q's `POLY_G4` and
/// `setRGB0` through `setRGB3` must be visible. `water_wave_strips.inc.c`
/// consumes and undefines this override.
#define WATER_WAVE_STRIPS_SET_FIRST_STRIP_COLOURS(quad) _shelterB2SepticTankSetFirstWaterStripColours(quad)
/// Sets the second X strip's blue vertex colours for subtractive blending.
///
/// Writes all RGB bytes: (0, 64, 128) on the waving seam (vertices 0/1),
/// (0, 16, 32) on the flat edge (2/3). This retains the first strip's
/// vertex-index palette, so the two sides of the seam have different colours.
/// `quad` is writable; other fields are preserved and the pointer is not retained.
static inline void _shelterB2SepticTankSetSecondWaterStripColours(POLY_G4* quad)
{
    setRGB0(quad, 0, 0x40, 0x80);
    setRGB1(quad, 0, 0x40, 0x80);
    setRGB2(quad, 0, 0x10, 0x20);
    setRGB3(quad, 0, 0x10, 0x20);
}

/// Binds the second X strip's blue vertex colours for subtractive blending.
///
/// A writable `POLY_G4*` is evaluated once. The seam (vertices 0/1) is
/// (0, 0x40, 0x80); the outer edge (vertices 2/3) is (0, 0x10, 0x20), in
/// unsigned RGB bytes. Other packet fields are preserved; no caller locals
/// are captured. Psy-Q's `POLY_G4` and `setRGB0` through `setRGB3` must be
/// visible. `water_wave_strips.inc.c` consumes and undefines this override.
#define WATER_WAVE_STRIPS_SET_SECOND_STRIP_COLOURS(quad) _shelterB2SepticTankSetSecondWaterStripColours(quad)
#include "../../shared/effects/water_wave_strips.inc.c"

/// Emits the second private water drawer with the task-compatible `void (Task*)` signature.
///
/// A single identifier; the argument is unused. No evaluation or token
/// construction occurs. The shared include consumes and undefines it.
#define WATER_WAVE_STRIPS_DRAW_FUNCTION _waterDrawSecondWaveStrips
/// Rebinds the second readable two-rectangle list with its own in-bounds terminator.
///
/// Same pointer contract as the first list; the preceding include undefined it.
#define WATER_WAVE_STRIPS_SURFACES _gShelterB2SepticTankSecondWaterWaveSurfaces
/// Rebinds the same signed world Y after the first include consumed it.
#define WATER_WAVE_STRIPS_HEIGHT gShelterB2SepticTankWaterY
/// Rebinds the same writable `u8*` cursor, appending after the first list.
#define WATER_WAVE_STRIPS_PACKET_CURSOR _gShelterB2SepticTankWaterPacketCursor
/// Rebinds the second surface list's seam displacement to -128..128 Y units.
#define WATER_WAVE_STRIPS_AMPLITUDE_SHIFT 5
/// Rebinds the second surface list to the same outer-edge-to-seam blue palette.
///
/// Has the same `POLY_G4*`, single-evaluation and packet-preservation contract
/// as the first instance; the preceding include has undefined that binding.
#define WATER_WAVE_STRIPS_SET_FIRST_STRIP_COLOURS(quad) _shelterB2SepticTankSetFirstWaterStripColours(quad)
/// Rebinds the second surface list to the same seam-to-edge blue palette.
///
/// Has the same `POLY_G4*`, single-evaluation and packet-preservation contract
/// as the first instance; the preceding include has undefined that binding.
#define WATER_WAVE_STRIPS_SET_SECOND_STRIP_COLOURS(quad) _shelterB2SepticTankSetSecondWaterStripColours(quad)
#include "../../shared/effects/water_wave_strips.inc.c"

/// Initializes and draws the room's water, publishing its height each tick.
///
/// Requires a live bodyless task in state 0 (initialize) or 1 (draw), with no
/// work allocation. Initialization clears the selected actor-load buffer
/// marker; drawing requires that buffer reserved until GPU consumption.
/// Water Y is a signed world-coordinate height, including the initial tick.
static void _shelterB2SepticTankWaterTask(Task* task)
{
    TaskFunc states[SHELTER_B2_SEPTIC_TANK_WATER_STATE_COUNT] = { _shelterB2SepticTankInitializeWater, _shelterB2SepticTankDrawWater };

    states[task->state](task);
    gGameSession->waterY = gShelterB2SepticTankWaterY;
}

/// Prepares the actor-load buffer used for water packets and advances to drawing.
///
/// Requires the water task in state 0 and previous uses of the buffer to have
/// ended. No saved companion selects buffer 2, otherwise buffer 1. Clears the
/// corresponding session marker (`field_80` or `field_7E`); its nonzero meaning
/// is unproven. No allocation or buffer clearing occurs here.
static void _shelterB2SepticTankInitializeWater(Task* task)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    task->state = task->state + 1;
}

/// Resets the water-packet byte cursor to the half being built for the current frame.
///
/// No saved companion selects actor-load buffer 2, otherwise buffer 1.
/// `otBuffer` must be 0 or 1, selecting a 0xC000-byte half of that buffer.
/// Previous actor-load uses must have ended; reserve word-aligned storage
/// until the GPU consumes its queued packets.
/// Call once before both surface lists; a later reset would overwrite packets
/// already linked into the ordering table. Only the cursor is written.
static inline void _shelterB2SepticTankResetWaterPacketCursor(void)
{
    enum {
        SHELTER_B2_SEPTIC_TANK_COMPANION_NONE          = 0,
        SHELTER_B2_SEPTIC_TANK_WATER_PACKET_HALF_BYTES = 0xC000
    };
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == SHELTER_B2_SEPTIC_TANK_COMPANION_NONE) {
        _gShelterB2SepticTankWaterPacketCursor = gFsActorBuffer2Base + gDisplayState.otBuffer * SHELTER_B2_SEPTIC_TANK_WATER_PACKET_HALF_BYTES;
    } else {
        _gShelterB2SepticTankWaterPacketCursor = gFsActorBuffer1Base + gDisplayState.otBuffer * SHELTER_B2_SEPTIC_TANK_WATER_PACKET_HALF_BYTES;
    }
}

/// Draws both water-surface lists into one display half of the borrowed arena.
///
/// Requires initialized water state and actor storage reserved through GPU
/// consumption. Four rectangles emit at most 0x1800 bytes in a 0xC000-byte
/// half. `task` is forwarded only for the drawers' task-compatible signatures.
static void _shelterB2SepticTankDrawWater(Task* task)
{
    // Reset once: the second list appends after the first list's packets.
    _shelterB2SepticTankResetWaterPacketCursor();
    _waterDrawWaveStrips(task);
    _waterDrawSecondWaveStrips(task);
}

/// Selects this room's flash, twin-trail, spark-burst, ripple and spray effect IDs.
///
/// Subsequent counted effect spawns use these resident selectors to dispatch
/// this overlay's callbacks in effect task bank 6. Register after the effect
/// controller initializes its selectors and keep the room overlay loaded
/// through the selected tasks' lifetimes. The assignments replace prior IDs.
static inline void _shelterB2SepticTankRegisterEffects(void)
{
    gRoomEffectFlashId       = EFFECT_SHELTER_B2_SEPTIC_TANK_FLASH;
    gRoomEffectTwinTrailId   = EFFECT_SHELTER_B2_SEPTIC_TANK_TWIN_TRAIL;
    gRoomEffectSparkBurstId  = EFFECT_SHELTER_B2_SEPTIC_TANK_SPARK_BURST;
    gRoomEffectWaterRippleId = EFFECT_SHELTER_B2_SEPTIC_TANK_WATER_RIPPLE;
    gRoomEffectWaterSprayId  = EFFECT_SHELTER_B2_SEPTIC_TANK_WATER_SPRAY;
}

void shelterB2SepticTankGlowTask(Task* task)
{
    enum {
        SHELTER_B2_SEPTIC_TANK_GLOW_INITIALIZE,
        SHELTER_B2_SEPTIC_TANK_GLOW_DRAW,
        SHELTER_B2_SEPTIC_TANK_MAPPED_VIEW_MASK  = 0xFF,
        SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE = 0x200,
        SHELTER_B2_SEPTIC_TANK_DISC_RADIUS_SCALE = 0x300,
        SHELTER_B2_SEPTIC_TANK_GLOW_WHITE        = 0x111,
        SHELTER_B2_SEPTIC_TANK_GLOW_GREEN        = 0x10,
        SHELTER_B2_SEPTIC_TANK_GLOW_RED          = 0x100
    };
    switch (task->state) {
        case SHELTER_B2_SEPTIC_TANK_GLOW_INITIALIZE:
            _shelterB2SepticTankRegisterEffects();
            task->state = SHELTER_B2_SEPTIC_TANK_GLOW_DRAW;
            // Registration and the first view's glow drawing share this tick.
            /* fallthrough */
        case SHELTER_B2_SEPTIC_TANK_GLOW_DRAW:
            switch (viewGetMappedIndex() & SHELTER_B2_SEPTIC_TANK_MAPPED_VIEW_MASK) {
                case 2: {
                    const SVECTOR(*beamEnds)[2] = _gShelterB2SepticTankGlowPoints.beamEnds;
                    _glowDrawBeam(beamEnds[0], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[1], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[7], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[8], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[30], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[31], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBitDisc(&_gShelterB2SepticTankGlowPoints.discCenters[0], SHELTER_B2_SEPTIC_TANK_DISC_RADIUS_SCALE, SHELTER_B2_SEPTIC_TANK_GLOW_GREEN);
                    _glowDrawBitDisc(&_gShelterB2SepticTankGlowPoints.discCenters[2], SHELTER_B2_SEPTIC_TANK_DISC_RADIUS_SCALE, SHELTER_B2_SEPTIC_TANK_GLOW_RED);
                    break;
                }
                case 3: {
                    const SVECTOR(*beamEnds)[2] = _gShelterB2SepticTankGlowPoints.beamEnds;
                    _glowDrawBeam(beamEnds[0], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[1], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[2], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[3], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[7], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[8], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[9], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[10], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[14], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[15], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[22], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[23], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[24], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[30], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[31], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBitDisc(&_gShelterB2SepticTankGlowPoints.discCenters[0], SHELTER_B2_SEPTIC_TANK_DISC_RADIUS_SCALE, SHELTER_B2_SEPTIC_TANK_GLOW_GREEN);
                    _glowDrawBitDisc(&_gShelterB2SepticTankGlowPoints.discCenters[1], SHELTER_B2_SEPTIC_TANK_DISC_RADIUS_SCALE, SHELTER_B2_SEPTIC_TANK_GLOW_RED);
                    _glowDrawBitDisc(&_gShelterB2SepticTankGlowPoints.discCenters[2], SHELTER_B2_SEPTIC_TANK_DISC_RADIUS_SCALE, SHELTER_B2_SEPTIC_TANK_GLOW_RED);
                    break;
                }
                case 4: {
                    const SVECTOR(*beamEnds)[2] = &_gShelterB2SepticTankGlowPoints.beamEnds[3];
                    _glowDrawBeam(beamEnds[0], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[1], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[2], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[3], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[7], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[8], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[9], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[10], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[17], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[18], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[24], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[25], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[26], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(_gShelterB2SepticTankGlowPoints.beamEnds[32], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(_gShelterB2SepticTankGlowPoints.beamEnds[33], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(_gShelterB2SepticTankGlowPoints.beamEnds[34], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_RED);
                    break;
                }
                case 5: {
                    const SVECTOR(*beamEnds)[2] = &_gShelterB2SepticTankGlowPoints.beamEnds[6];
                    _glowDrawBeam(beamEnds[0], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, -GLOW_QUARTER_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[7], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(_gShelterB2SepticTankGlowPoints.beamEnds[32], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(_gShelterB2SepticTankGlowPoints.beamEnds[33], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(_gShelterB2SepticTankGlowPoints.beamEnds[34], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_RED);
                    break;
                }
                case 6: {
                    const SVECTOR(*beamEnds)[2] = _gShelterB2SepticTankGlowPoints.beamEnds;
                    _glowDrawBeam(beamEnds[0], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[1], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[2], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_QUARTER_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[7], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[14], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[15], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, GLOW_HALF_TURN, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[30], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBeam(beamEnds[31], SHELTER_B2_SEPTIC_TANK_BEAM_RADIUS_SCALE, 0, SHELTER_B2_SEPTIC_TANK_GLOW_WHITE);
                    _glowDrawBitDisc(&_gShelterB2SepticTankGlowPoints.discCenters[0], SHELTER_B2_SEPTIC_TANK_DISC_RADIUS_SCALE, SHELTER_B2_SEPTIC_TANK_GLOW_GREEN);
                    _glowDrawBitDisc(&_gShelterB2SepticTankGlowPoints.discCenters[1], SHELTER_B2_SEPTIC_TANK_DISC_RADIUS_SCALE, SHELTER_B2_SEPTIC_TANK_GLOW_RED);
                    _glowDrawBitDisc(&_gShelterB2SepticTankGlowPoints.discCenters[2], SHELTER_B2_SEPTIC_TANK_DISC_RADIUS_SCALE, SHELTER_B2_SEPTIC_TANK_GLOW_RED);
                    break;
                }
            }
            break;
    }
}

#include "../../shared/effects/water_ripple_task.inc.c"

void shelterB2SepticTankWaterRippleTask(Task* task)
{
    _waterRippleTask(task);
}

#include "../../shared/effects/water_splash.inc.c"

#include "../../shared/effects/water_drift_task.inc.c"

void shelterB2SepticTankWaterDriftTask(Task* task)
{
    _waterDriftTask(task);
}

#include "../../shared/effects/water_spin.inc.c"

#include "../../shared/effects/glow_draw_beam.inc.c"

#include "../../shared/effects/glow_draw_bit_disc.inc.c"

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void shelterB2SepticTankRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void shelterB2SepticTankRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void shelterB2SepticTankRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
