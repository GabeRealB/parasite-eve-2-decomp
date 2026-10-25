#include "rooms/shelter_b1_sterilization_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "shelter_b1_sterilization_room_private.h"

#include "actors/actor_160600.h"

#include "gameplay/companion_load.h"
#include "gameplay/display.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"
#include "../../shared/room/room_cutscene.h"
/// Signed texture-frame index for the drifting effect's five-column sprite grid.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/effects/sprite_quad.h"

static void _shelterB1SterilizationRoomDrawPulsingGlow(const SVECTOR* worldPoint, s16 pulseRate, s16 radiusScale);

static WorldCoordRoomAmbientEntry _gShelterB1SterilizationRoomAmbientByView[25];
static WorldCoordRoomLights       _gShelterB1SterilizationRoomLights;

/// Room-effect positions, trap-puff drift directions and writable smoke samples.
///
/// Glow points use world coordinates; puff origins and smoke samples use the
/// controller's local frame. All positions are signed integer game units.
/// Consecutive groups of sixteen puff origins share one Q12 XY drift direction;
/// these diagonals are not normalized. Spawning copies placement and retains
/// the source pointer. Puff tasks use the copied placement; smoke samples are
/// overwritten before each spawn. Keep source storage loaded through the
/// effects' lifetimes. The four intervening vectors remain unproven.
typedef struct {
    SVECTOR puffOrigins[64];        // Source index 0..63; direction is index / 16
    SVECTOR field_200[4];           // Four retained vectors; purpose unproven
    SVECTOR capsuleEndpoints[4][2]; // Four world-space endpoint pairs for capsule glows
    SVECTOR singleDiscCenter;       // Large disc centre drawn in mapped view 11
    SVECTOR pairedDiscCenters[2];   // Large disc centres drawn together in mapped view 10
    SVECTOR bladeGlowCenter;        // Diamond or pulsing blade-glow centre in mapped views 8/19
    SVECTOR smallDiscCenter;        // Small disc centre drawn in mapped views 8/19
    SVECTOR smokePoints[2];         // Mutable local ring samples, copied by particle spawning
    SVECTOR puffDirections[4];      // Q12 XY drift directions; components are +/-ONE
} _ShelterB1SterilizationRoomEffectGeometry;
STATIC_ASSERT_SIZEOF(_ShelterB1SterilizationRoomEffectGeometry, 0x2B8);

static void _shelterB1SterilizationRoomReloadDumpingHole(void);
static void _shelterB1SterilizationRoomResetCapResource(s32 selectSceneFile);

/// CAP resource choices carried by the scene-entry and exit callbacks.
enum {
    SHELTER_B1_STERILIZATION_ROOM_CAP_DEFAULT = 0,
    SHELTER_B1_STERILIZATION_ROOM_CAP_SCENE   = 1,
};

static WorldCollisionOccluder _gShelterB1SterilizationRoomSightOccluders[2];
static WorldCollisionTrigger  _gShelterB1SterilizationRoomViewBoundaryTriggers[8];

static void _shelterB1SterilizationRoomSpawnTrapDamageOnce(void);
static void _shelterB1SterilizationRoomAttenuateTrapSounds(void);

DamageAttack gShelterB1SterilizationRoomTrapAttack = {
    .power    = 25,
    .reaction = GAME_ACTOR_REACTION_ORDINARY,
};

/// Trap-script view ids, CAP command and persistent sound-skip policy.
enum {
    SHELTER_B1_STERILIZATION_ROOM_TRAP_KEEP_SOUND_ON_SKIP = 1,
    SHELTER_B1_STERILIZATION_ROOM_TRAP_ENTRY_CAP_COMMAND  = 7,
    SHELTER_B1_STERILIZATION_ROOM_TRAP_CROSSFADE_VIEW     = 11,
    SHELTER_B1_STERILIZATION_ROOM_TRAP_PLACEMENT_VIEW     = 14,
    SHELTER_B1_STERILIZATION_ROOM_TRAP_WAIT_VIEW          = 15,
    SHELTER_B1_STERILIZATION_ROOM_TRAP_CAP_VIEW           = 16,
    SHELTER_B1_STERILIZATION_ROOM_TRAP_ESCAPE_BEGIN_VIEW  = 17,
    SHELTER_B1_STERILIZATION_ROOM_TRAP_ESCAPE_EXIT_VIEW   = 18,
    SHELTER_B1_STERILIZATION_ROOM_TRAP_AMBIENT_RED        = 100,
};

/// Interpreter-update delays and sound/music fade ticks used by the trap scripts.
enum {
    SHELTER_B1_STERILIZATION_ROOM_TRAP_CROSSFADE_WAIT_FRAMES       = 16,
    SHELTER_B1_STERILIZATION_ROOM_TRAP_ENTRY_ANIMATION_WAIT_FRAMES = 30,
    SHELTER_B1_STERILIZATION_ROOM_TRAP_ENTRY_VIEW_WAIT_FRAMES      = 90,
    SHELTER_B1_STERILIZATION_ROOM_TRAP_ROOM_SWITCH_WAIT_FRAMES     = 3,
    SHELTER_B1_STERILIZATION_ROOM_TRAP_ESCAPE_BEGIN_WAIT_FRAMES    = 90,
    SHELTER_B1_STERILIZATION_ROOM_TRAP_ESCAPE_EXIT_WAIT_FRAMES     = 100,
    SHELTER_B1_STERILIZATION_ROOM_TRAP_SKIP_WAIT_FRAMES            = 8,
    SHELTER_B1_STERILIZATION_ROOM_TRAP_SKIP_PLACEMENT_WAIT_FRAMES  = 2,
    SHELTER_B1_STERILIZATION_ROOM_TRAP_ENTRY_SOUND_FADE_TICKS      = 8,
    SHELTER_B1_STERILIZATION_ROOM_TRAP_MUSIC_FADE_TICKS            = 1,
};

EvsCommand gShelterB1SterilizationRoomTrapEntryScript[37] = {
    { .opcode = EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_KEEP_SOUND_ON_SKIP } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, .operand3 = { .message = { .animationBankCopy = &gShelterB1SterilizationRoomTrapAnimationBankCopyRequest } } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &gShelterB1SterilizationRoomTrapEntryBeginAnimationRequest } },
    { .opcode = EVENT_SCRIPT_OPCODE_START_SOUND, .operand0 = { .value = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 0x5) } },
    { .opcode = EVENT_SCRIPT_OPCODE_START_SOUND, .operand0 = { .value = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 0x7) } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = shelterB1SterilizationRoomSpawnTaskOnce }, .operand1 = { .value = SHELTER_B1_STERILIZATION_ROOM_TASK_CROSSFADE } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_VIEW, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_CROSSFADE_VIEW } },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_AMBIENT_RED } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_CROSSFADE_WAIT_FRAMES } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &gShelterB1SterilizationRoomTrapEntryView11AnimationRequest } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_ENTRY_ANIMATION_WAIT_FRAMES } },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_VIEW, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_PLACEMENT_VIEW } },
    { .opcode = EVENT_SCRIPT_OPCODE_HIDE_WEAPONS },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &gShelterB1SterilizationRoomTrapEntryPlayerPlacement } } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &gShelterB1SterilizationRoomTrapEntryPlacedAnimationRequest } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES },
    { .opcode = EVENT_SCRIPT_OPCODE_START_SOUND, .operand0 = { .value = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 0x6) } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_ENTRY_ANIMATION_WAIT_FRAMES } },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_VIEW, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_WAIT_VIEW } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_ENTRY_VIEW_WAIT_FRAMES } },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_VIEW, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_CAP_VIEW } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_CAP_CONTROL }, .operand2 = { .value = CAP_CONTROL_MESSAGE_START }, .operand3 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_ENTRY_CAP_COMMAND } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &gShelterB1SterilizationRoomTrapEntryCapCueAnimationRequest } },
    { .opcode = EVENT_SCRIPT_OPCODE_START_SOUND, .operand0 = { .value = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 0x10) } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = shelterB1SterilizationRoomSpawnTaskOnce }, .operand1 = { .value = SHELTER_B1_STERILIZATION_ROOM_TASK_SWITCH_ROOM } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_ROOM_SWITCH_WAIT_FRAMES } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackNoArg = _shelterB1SterilizationRoomSpawnTrapDamageOnce } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED } },
    { .opcode = EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS },
    { .opcode = EVENT_SCRIPT_OPCODE_RESTORE_VIEW },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackNoArg = _shelterB1SterilizationRoomAttenuateTrapSounds } },
    { .opcode = EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_MUSIC_FADE_TICKS } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gShelterB1SterilizationRoomTrapEntrySkipScript[20] = {
    { .opcode = EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, .operand0 = { .value = SCREEN_FADE_SUBTRACT } },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_KEEP_SOUND_ON_SKIP } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackNoArg = _shelterB1SterilizationRoomAttenuateTrapSounds } },
    { .opcode = EVENT_SCRIPT_OPCODE_STOP_SOUND, .operand0 = { .value = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 0x10) }, .operand1 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_ENTRY_SOUND_FADE_TICKS } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_SKIP_WAIT_FRAMES } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = shelterB1SterilizationRoomSpawnTaskOnce }, .operand1 = { .value = SHELTER_B1_STERILIZATION_ROOM_TASK_SWITCH_ROOM } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackNoArg = _shelterB1SterilizationRoomSpawnTrapDamageOnce } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, .operand3 = { .message = { .animationBankCopy = &gShelterB1SterilizationRoomTrapAnimationBankCopyRequest } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &gShelterB1SterilizationRoomTrapEntryPlayerPlacement } } },
    { .opcode = EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_AMBIENT_RED } },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_VIEW, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_PLACEMENT_VIEW } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_SKIP_PLACEMENT_WAIT_FRAMES } },
    { .opcode = EVENT_SCRIPT_OPCODE_RESTORE_VIEW },
    { .opcode = EVENT_SCRIPT_OPCODE_CLEANUP_SCENE },
    { .opcode = EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE },
    { .opcode = EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_MUSIC_FADE_TICKS } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_SKIP_WAIT_FRAMES } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Accepted trap-escape scene ending with a captured-frame Dumping Hole reload.
///
/// Sixteen commands including END. Places and animates the player in views
/// 17/18, clears the red ambient override and requests the destination reload.
/// The escape prompt starts this with HUD hide/restore and its paired skip
/// completion; keep the room, player, animation and sound resources loaded.
static EvsCommand _gShelterB1SterilizationRoomTrapEscapeScript[16] = {
    { .opcode = EVENT_SCRIPT_OPCODE_START_SOUND, .operand0 = { .value = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 0x8) } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, .operand3 = { .message = { .animationBankCopy = &gShelterB1SterilizationRoomTrapAnimationBankCopyRequest } } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &gShelterB1SterilizationRoomTrapEscapeBeginAnimationRequest } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &gShelterB1SterilizationRoomTrapEscapePlayerPlacement } } },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_AMBIENT_RED } },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_VIEW, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_ESCAPE_BEGIN_VIEW } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_ESCAPE_BEGIN_WAIT_FRAMES } },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_VIEW, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_ESCAPE_EXIT_VIEW } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &gShelterB1SterilizationRoomTrapEscapeView18AnimationRequest } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES },
    { .opcode = EVENT_SCRIPT_OPCODE_START_SOUND, .operand0 = { .value = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 0x9) } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_ESCAPE_EXIT_WAIT_FRAMES } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED } },
    { .opcode = EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackNoArg = _shelterB1SterilizationRoomReloadDumpingHole } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip completion for trap escape, preserving its placement and destination.
///
/// Eight commands including END. Fades before ending player scripting,
/// applying the escape placement and clearing ambient light, then requests
/// the same Dumping Hole reload and releases scene control. Storage is borrowed
/// until the interpreter ends; it owns no payload or callback resources.
static EvsCommand _gShelterB1SterilizationRoomTrapEscapeSkipScript[8] = {
    { .opcode = EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, .operand0 = { .value = SCREEN_FADE_SUBTRACT } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_SKIP_WAIT_FRAMES } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &gShelterB1SterilizationRoomTrapEscapePlayerPlacement } } },
    { .opcode = EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callbackNoArg = _shelterB1SterilizationRoomReloadDumpingHole } },
    { .opcode = EVENT_SCRIPT_OPCODE_CLEANUP_SCENE },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Command, skip policy and area-bank cues of the first trap exchange.
enum {
    SHELTER_B1_STERILIZATION_ROOM_TRAP_EXCHANGE_CAP_COMMAND        = 10,
    SHELTER_B1_STERILIZATION_ROOM_TRAP_EXCHANGE_KEEP_SOUND_ON_SKIP = 1,
    SHELTER_B1_STERILIZATION_ROOM_TRAP_EXCHANGE_FIRST_SOUND        = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 0x14),
    SHELTER_B1_STERILIZATION_ROOM_TRAP_EXCHANGE_SECOND_SOUND       = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 0x15),
};

EvsCommand gShelterB1SterilizationRoomTrapExchangeScript[11] = {
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _shelterB1SterilizationRoomResetCapResource }, .operand1 = { .value = SHELTER_B1_STERILIZATION_ROOM_CAP_SCENE } },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_EXCHANGE_KEEP_SOUND_ON_SKIP } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_CAP_CONTROL }, .operand2 = { .value = CAP_CONTROL_MESSAGE_START }, .operand3 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_EXCHANGE_CAP_COMMAND } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_START_SOUND, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_EXCHANGE_FIRST_SOUND } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_START_SOUND, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_EXCHANGE_SECOND_SOUND } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED } },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _shelterB1SterilizationRoomResetCapResource }, .operand1 = { .value = SHELTER_B1_STERILIZATION_ROOM_CAP_DEFAULT } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gShelterB1SterilizationRoomTrapExchangeSkipScript[8] = {
    { .opcode = EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, .operand0 = { .value = SCREEN_FADE_SUBTRACT } },
    { .opcode = EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_KEEP_SOUND_ON_SKIP } },
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = SHELTER_B1_STERILIZATION_ROOM_TRAP_SKIP_WAIT_FRAMES } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_PLAYER }, .operand2 = { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED } },
    { .opcode = EVENT_SCRIPT_OPCODE_CLEANUP_SCENE },
    { .opcode = EVENT_SCRIPT_OPCODE_CALLBACK, .operand0 = { .callback = _shelterB1SterilizationRoomResetCapResource }, .operand1 = { .value = SHELTER_B1_STERILIZATION_ROOM_CAP_DEFAULT } },
    { .opcode = EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Room-owned effect geometry and writable smoke samples.
static _ShelterB1SterilizationRoomEffectGeometry _gShelterB1SterilizationRoomEffectGeometry = {
    .puffOrigins = {
        { 4020, -2640, 6625, 0 },
        { 4020, -2640, 6775, 0 },
        { 4020, -2640, 6975, 0 },
        { 4020, -2640, 7125, 0 },
        { 4020, -2640, 7725, 0 },
        { 4020, -2640, 7875, 0 },
        { 4020, -2640, 8075, 0 },
        { 4020, -2640, 8225, 0 },
        { 4020, -2640, 8775, 0 },
        { 4020, -2640, 8925, 0 },
        { 4020, -2640, 9125, 0 },
        { 4020, -2640, 9275, 0 },
        { 4020, -2640, 9875, 0 },
        { 4020, -2640, 10025, 0 },
        { 4020, -2640, 10225, 0 },
        { 4020, -2640, 10375, 0 },
        { 6730, -2640, 6625, 0 },
        { 6730, -2640, 6775, 0 },
        { 6730, -2640, 6975, 0 },
        { 6730, -2640, 7125, 0 },
        { 6730, -2640, 7725, 0 },
        { 6730, -2640, 7875, 0 },
        { 6730, -2640, 8075, 0 },
        { 6730, -2640, 8225, 0 },
        { 6730, -2640, 8775, 0 },
        { 6730, -2640, 8925, 0 },
        { 6730, -2640, 9125, 0 },
        { 6730, -2640, 9275, 0 },
        { 6730, -2640, 9875, 0 },
        { 6730, -2640, 10025, 0 },
        { 6730, -2640, 10225, 0 },
        { 6730, -2640, 10375, 0 },
        { 4090, -150, 6625, 0 },
        { 4090, -150, 6775, 0 },
        { 4090, -150, 6975, 0 },
        { 4090, -150, 7125, 0 },
        { 4090, -150, 7725, 0 },
        { 4090, -150, 7875, 0 },
        { 4090, -150, 8075, 0 },
        { 4090, -150, 8225, 0 },
        { 4090, -150, 8775, 0 },
        { 4090, -150, 8925, 0 },
        { 4090, -150, 9125, 0 },
        { 4090, -150, 9275, 0 },
        { 4090, -150, 9875, 0 },
        { 4090, -150, 10025, 0 },
        { 4090, -150, 10225, 0 },
        { 4090, -150, 10375, 0 },
        { 6660, -150, 6625, 0 },
        { 6660, -150, 6775, 0 },
        { 6660, -150, 6975, 0 },
        { 6660, -150, 7125, 0 },
        { 6660, -150, 7725, 0 },
        { 6660, -150, 7875, 0 },
        { 6660, -150, 8075, 0 },
        { 6660, -150, 8225, 0 },
        { 6660, -150, 8775, 0 },
        { 6660, -150, 8925, 0 },
        { 6660, -150, 9125, 0 },
        { 6660, -150, 9275, 0 },
        { 6660, -150, 9875, 0 },
        { 6660, -150, 10025, 0 },
        { 6660, -150, 10225, 0 },
        { 6660, -150, 10375, 0 },
    },
    .field_200 = {
        { 4575, -2910, 10590, 0 },
        { 4575, -2910, 6390, 0 },
        { 6175, -2910, 10590, 0 },
        { 6175, -2910, 6390, 0 },
    },
    .capsuleEndpoints = {
        {
            { 910, -2150, 1270, 0 },
            { 910, -2150, 1720, 0 },
        },
        {
            { 910, -2150, 4270, 0 },
            { 910, -2150, 4720, 0 },
        },
        {
            { 910, -2150, 2270, 0 },
            { 910, -2150, 2730, 0 },
        },
        {
            { 910, -2150, 3270, 0 },
            { 910, -2150, 3730, 0 },
        },
    },
    .singleDiscCenter  = { 5000, -2310, 10650, 0 },
    .pairedDiscCenters = {
        { 5000, -2310, 6350, 0 },
        { 6250, -1660, 6350, 0 },
    },
    .bladeGlowCenter = { 1200, -1320, 12490, 0 },
    .smallDiscCenter = { 1200, -1570, 12470, 0 },
    .smokePoints     = {
        { 1500, 0, 2500, 0 },
        { 5250, 0, 2000, 0 },
    },
    .puffDirections = {
        { ONE, ONE, 0, 0 },
        { -ONE, ONE, 0, 0 },
        { ONE, -ONE, 0, 0 },
        { -ONE, -ONE, 0, 0 },
    },
};

WorldCoordRoomLighting gShelterB1SterilizationRoomLighting[3] = {
    { &_gShelterB1SterilizationRoomLights, _gShelterB1SterilizationRoomAmbientByView },
    { &_gShelterB1SterilizationRoomLights, _gShelterB1SterilizationRoomAmbientByView },
    { &_gShelterB1SterilizationRoomLights, _gShelterB1SterilizationRoomAmbientByView },
};

WorldCollisionRoomResources gShelterB1SterilizationRoomCollisionResources[3] = {
    { .grid = &gShelterB1SterilizationRoomCollisionGrid, .viewBoundaryTriggers = _gShelterB1SterilizationRoomViewBoundaryTriggers, .actionTriggers = gShelterB1SterilizationRoomActionTriggers, .occluders = _gShelterB1SterilizationRoomSightOccluders },
    { .grid = &gShelterB1SterilizationRoomCollisionGrid, .viewBoundaryTriggers = _gShelterB1SterilizationRoomViewBoundaryTriggers, .actionTriggers = gShelterB1SterilizationRoomActionTriggers, .occluders = _gShelterB1SterilizationRoomSightOccluders },
    { .grid = &gShelterB1SterilizationRoomCollisionGrid, .viewBoundaryTriggers = _gShelterB1SterilizationRoomViewBoundaryTriggers, .actionTriggers = gShelterB1SterilizationRoomActionTriggers, .occluders = _gShelterB1SterilizationRoomSightOccluders },
};

/// Room-2 map from 24 logical views to 1-based camera/image/sprite indices.
///
/// Indexed by logical view - 1, without a terminator. Views 4..7 use indices
/// 10..13, and views 10..13 use indices 4..7; all other slots are identity.
static u8 _gShelterB1SterilizationRoomViewMap2[24] = {
    1,
    2,
    3,
    10,
    11,
    12,
    13,
    8,
    9,
    4,
    5,
    6,
    7,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
};

/// Room-3 map from 24 logical views to 1-based camera/image/sprite indices.
///
/// Indexed by logical view - 1, without a terminator. Logical views 2/3 select
/// indices 23/24; those indices are also retained in logical slots 23/24.
static u8 _gShelterB1SterilizationRoomViewMap3[24] = {
    1,
    23,
    24,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
};

u8* gShelterB1SterilizationRoomViewMaps[3] = {
    gViewIdentityMap,
    _gShelterB1SterilizationRoomViewMap2,
    _gShelterB1SterilizationRoomViewMap3,
};

ViewCount gShelterB1SterilizationRoomViewCounts[3] = { 24, 24, 24 };

/// One-based local endpoint IDs used in the outgoing action triggers.
enum {
    SHELTER_B1_STERILIZATION_ROOM_CORRIDOR_WARP_ID   = 1,
    SHELTER_B1_STERILIZATION_ROOM_POD_TUNNEL_WARP_ID = 2,
};

/// Paired room-bank sounds of the Main Corridor and Pod Access Tunnel endpoints.
enum {
    SHELTER_B1_STERILIZATION_ROOM_CORRIDOR_ARRIVAL_SOUND     = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 2),
    SHELTER_B1_STERILIZATION_ROOM_CORRIDOR_DEPARTURE_SOUND   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 1),
    SHELTER_B1_STERILIZATION_ROOM_POD_TUNNEL_ARRIVAL_SOUND   = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 4),
    SHELTER_B1_STERILIZATION_ROOM_POD_TUNNEL_DEPARTURE_SOUND = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, 3),
};

DirectionWarpEntry gShelterB1SterilizationRoomWarpEntries[3] = {
    {
        .player         = { .yaw = { .word = 0 }, .x = 3456, .y = 0, .z = 960 },
        .companion      = { .yaw = { .word = 0 }, .x = 3456, .y = 0, .z = 960 },
        .arrivalSound   = SHELTER_B1_STERILIZATION_ROOM_CORRIDOR_ARRIVAL_SOUND,
        .departureSound = SHELTER_B1_STERILIZATION_ROOM_CORRIDOR_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
    {
        .player         = { .yaw = { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, .x = 3614, .y = 0, .z = 13000 },
        .companion      = { .yaw = { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, .x = 3614, .y = 0, .z = 13000 },
        .arrivalSound   = SHELTER_B1_STERILIZATION_ROOM_POD_TUNNEL_ARRIVAL_SOUND,
        .departureSound = SHELTER_B1_STERILIZATION_ROOM_POD_TUNNEL_DEPARTURE_SOUND,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 8,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
    {
        .player         = { .yaw = { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, .x = 6184, .y = 0, .z = 10235 },
        .companion      = { .yaw = { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, .x = 6184, .y = 0, .z = 10235 },
        .arrivalSound   = DIRECTION_WARP_SOUND_NONE,
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 5,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

/// Q12 mesh normals, including four writable slots reserved for the actor obstacle.
static SVECTOR _gShelterB1SterilizationRoomCollisionGridNormals[14] = {
#include "assets/shelter_b1_sterilization_room_collision_0C884_normals.inc"
};

/// Mesh vertices in room game units, including eight writable actor-obstacle slots.
static SVECTOR _gShelterB1SterilizationRoomCollisionGridVerts[124] = {
#include "assets/shelter_b1_sterilization_room_collision_0C884_verts.inc"
};

/// Mesh faces; the leading four are disabled until the actor obstacle is rebuilt.
static WorldCollisionGridFace _gShelterB1SterilizationRoomCollisionGridFaces[68] = {
#include "assets/shelter_b1_sterilization_room_collision_0C884_faces.inc"
};

/// Eight signed face-index lists, each terminated by WORLD_COLLISION_GRID_CELL_END.
static s16 _gShelterB1SterilizationRoomCollisionGridCells[270] = {
#include "assets/shelter_b1_sterilization_room_collision_0C884_cells.inc"
};

// The generated table supplies constant element offsets within this cell-list pool.
/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gShelterB1SterilizationRoomCollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..269,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define SHELTER_B1_STERILIZATION_ROOM_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gShelterB1SterilizationRoomCollisionGridCells[(halfwordOffset)])
/// X-major 2-by-4 cell directory borrowing the embedded face-index lists.
static s16* _gShelterB1SterilizationRoomCollisionGridTable[8] = {
#include "assets/shelter_b1_sterilization_room_collision_0C884_table.inc"
};
#undef SHELTER_B1_STERILIZATION_ROOM_CELL_FACE_LIST_AT_OFFSET

WorldCollisionGrid gShelterB1SterilizationRoomCollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gShelterB1SterilizationRoomCollisionGridNormals,
    .vertices    = _gShelterB1SterilizationRoomCollisionGridVerts,
    .faces       = _gShelterB1SterilizationRoomCollisionGridFaces,
    .cellFaceIds = _gShelterB1SterilizationRoomCollisionGridTable,
    .xBias       = 0,
    .zBias       = -500,
    .cellCountX  = 2,
    .cellCountZ  = 4,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gShelterB1SterilizationRoomCollisionGridFaces),
};

ViewCamera gShelterB1SterilizationRoomViewCameras[24] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3500, 0x61A8, -7000 } }, 329 },
    { { { { -4014, 0, 815 }, { 651, 2458, 3210 }, { -489, 3276, -2409 } }, { -4078, 4407, -4494 } }, 230 },
    { { { { 4009, 0, 835 }, { 674, 2419, -3235 }, { -493, 3305, 2368 } }, { -4078, 4407, -1394 } }, 230 },
    { { { { -3830, 0, 1450 }, { -422, 3918, -1115 }, { -1387, -1192, -3664 } }, { -6453, 407, -0x29C6 } }, 230 },
    { { { { 3845, 0, 1410 }, { -375, 3948, 1023 }, { -1359, -1090, 3706 } }, { -6453, 457, -6319 } }, 230 },
    { { { { -4068, 0, 473 }, { 206, 3686, 1773 }, { -425, 1785, -3661 } }, { -2059, 2735, -0x294C } }, 230 },
    { { { { 4060, 0, 536 }, { 258, 3590, -1954 }, { -470, 1971, 3559 } }, { -1865, 2682, -6335 } }, 230 },
    { { { { 916, 0, 3992 }, { 953, 3977, -219 }, { -3876, 978, 890 } }, { -6259, 1516, -0x2E4D } }, 230 },
    { { { { 1007, 0, 3970 }, { 3716, 1439, -943 }, { -1394, 3834, 354 } }, { -5758, 3397, -0x2F43 } }, 230 },
    { { { { -3830, 0, 1450 }, { -422, 3918, -1115 }, { -1387, -1192, -3664 } }, { -6453, 407, -0x29C6 } }, 230 },
    { { { { 3845, 0, 1410 }, { -375, 3948, 1023 }, { -1359, -1090, 3706 } }, { -6453, 457, -6319 } }, 230 },
    { { { { -4068, 0, 473 }, { 206, 3686, 1773 }, { -425, 1785, -3661 } }, { -2059, 2735, -0x294C } }, 230 },
    { { { { 4060, 0, 536 }, { 258, 3590, -1954 }, { -470, 1971, 3559 } }, { -1865, 2682, -6335 } }, 230 },
    { { { { 3420, 0, 2253 }, { -604, 3945, 917 }, { -2170, -1098, 3295 } }, { -6170, 740, -7020 } }, 289 },
    { { { { 3451, 0, 2205 }, { -85, 4092, 134 }, { -2203, -159, 3449 } }, { -4630, 2480, -7460 } }, 289 },
    { { { { 3448, 0, 2209 }, { 1712, 2588, -2672 }, { -1396, 3174, 2179 } }, { -5940, 2880, -7540 } }, 257 },
    { { { { 612, 0, 4049 }, { 869, 4000, -131 }, { -3955, 879, 598 } }, { -8900, 1805, -9990 } }, 680 },
    { { { { 3423, 0, 2248 }, { 495, 3995, -753 }, { -2193, 901, 3339 } }, { -7680, 1760, -8010 } }, 447 },
    { { { { 1256, 0, 3898 }, { 1644, 3713, -529 }, { -3534, 1728, 1138 } }, { -2000, 1830, -0x2F26 } }, 329 },
    { { { { 180, 0, 4092 }, { 115, 4094, -5 }, { -4090, 115, 180 } }, { -7570, 830, -3330 } }, 380 },
    { { { { 488, 0, -4066 }, { -1621, 3756, -194 }, { 3729, 1632, 448 } }, { -1500, 2630, -3240 } }, 541 },
    { { { { 3577, 0, 1993 }, { 149, 4084, -267 }, { -1988, 306, 3567 } }, { -6270, 1130, -1200 } }, 289 },
    { { { { -4014, 0, 815 }, { 651, 2458, 3210 }, { -489, 3276, -2409 } }, { -4078, 4407, -4494 } }, 230 },
    { { { { 4009, 0, 835 }, { 674, 2419, -3235 }, { -493, 3305, 2368 } }, { -4078, 4407, -1394 } }, 230 },
};

/// Mapped view 1's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView1SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 2's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView2SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 3's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView3SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 4's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView4SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 5's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView5SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 6's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView6SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 7's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView7SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// View-8 sprite texture, palette and neutral RGB modulation.
///
/// Texture data is 8-bit indexed at VRAM X=960 words, Y=0; its palette starts
/// at X=0 words, Y=255. The blend field is averaging, with opaque source flags.
enum {
    SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE  = getTPage(1, GPU_BLEND_AVERAGE, 960, 0),
    SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT          = getClut(0, 255),
    SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR = 128,
};

/// View-8 textured rectangles and sorting depths for its 89 cached sprites.
///
/// Position/size use drawing-origin-relative pixels; UVs are texture-page
/// texels and depths are unscaled OT sorting values. Batches cover elements
/// 0..88. Packet allocation snapshots geometry into both frame buffers, so
/// this array and its owning room overlay must be live during allocation.
static SpriteSource _gShelterB1SterilizationRoomView8SpriteSources[89] = {
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 32 } }, -112, -120, 800, { .fields = { 96, 128 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 32 } }, -112, -88, 825, { .fields = { 88, 192 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 24 } }, -96, -64, 850, { .fields = { 48, 120 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 24 } }, -96, -40, 875, { .fields = { 48, 96 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 24 } }, -96, -16, 900, { .fields = { 48, 168 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 16 } }, -96, 8, 925, { .fields = { 32, 224 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 24 } }, -88, 24, 925, { .fields = { 48, 216 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 24 } }, -88, 48, 925, { .fields = { 48, 192 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 24 } }, -96, 24, 925, { .fields = { 48, 72 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 24 } }, -96, 48, 925, { .fields = { 56, 24 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, -112, -56, 825, { .fields = { 48, 0 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, -112, -32, 825, { .fields = { 48, 144 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, -112, -8, 825, { .fields = { 40, 48 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, -112, 16, 825, { .fields = { 40, 24 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 16 } }, -112, 40, 825, { .fields = { 16, 0 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 16 } }, -112, 56, 825, { .fields = { 16, 88 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 32 } }, -128, -120, 675, { .fields = { 112, 128 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 32 } }, -128, -88, 675, { .fields = { 112, 160 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 32 } }, -128, -56, 675, { .fields = { 112, 0 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 32 } }, -128, -24, 675, { .fields = { 112, 32 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 32 } }, -128, 8, 675, { .fields = { 112, 64 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 32 } }, -128, 40, 675, { .fields = { 96, 32 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 24, 32 } }, -152, -120, 625, { .fields = { 88, 64 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 24, 32 } }, -152, -88, 625, { .fields = { 88, 96 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 24, 32 } }, -152, -56, 625, { .fields = { 104, 192 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 32 } }, -144, -24, 625, { .fields = { 112, 224 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 24 } }, -136, 8, 625, { .fields = { 40, 72 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 32 } }, -136, 32, 625, { .fields = { 104, 0 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 8 } }, 64, -16, 994, { .fields = { 32, 88 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 16 } }, 24, -8, 1025, { .fields = { 32, 120 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 24 } }, 24, 8, 1042, { .fields = { 40, 120 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, 32, 8, 1046, { .fields = { 32, 0 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, 48, 8, 1043, { .fields = { 32, 168 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, 32, -16, 1050, { .fields = { 32, 144 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, 48, -16, 1015, { .fields = { 56, 224 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, 32, -40, 992, { .fields = { 72, 56 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 24, 24 } }, 48, -40, 992, { .fields = { 72, 224 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, 32, -64, 967, { .fields = { 72, 80 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 24, 24 } }, 48, -64, 962, { .fields = { 64, 152 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, 32, -88, 949, { .fields = { 72, 104 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 24, 24 } }, 48, -88, 932, { .fields = { 72, 128 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 16 } }, 88, 72, 587, { .fields = { 24, 192 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 16 } }, 88, 88, 568, { .fields = { 24, 208 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 24, 8 } }, 96, 104, 549, { .fields = { 8, 16 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 16 } }, 104, 88, 544, { .fields = { 16, 104 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 16 } }, 104, 72, 575, { .fields = { 16, 224 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, 96, 48, 575, { .fields = { 64, 24 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 32 } }, 120, 72, 522, { .fields = { 112, 96 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 24, 32 } }, 136, 72, 468, { .fields = { 80, 0 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, 112, 48, 523, { .fields = { 64, 0 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, 128, 48, 528, { .fields = { 56, 72 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, 144, 48, 486, { .fields = { 56, 176 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 16 } }, 104, 32, 552, { .fields = { 16, 240 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 16 } }, 104, 16, 547, { .fields = { 16, 168 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 32 } }, 120, 16, 510, { .fields = { 96, 224 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 24, 32 } }, 136, 16, 492, { .fields = { 88, 160 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 16 } }, 112, 0, 535, { .fields = { 16, 120 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 16 } }, 112, -16, 526, { .fields = { 16, 136 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 16 } }, 112, -32, 523, { .fields = { 16, 152 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, 128, -32, 482, { .fields = { 72, 176 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, 128, -8, 497, { .fields = { 56, 120 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, 144, -32, 476, { .fields = { 56, 96 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, 144, -8, 479, { .fields = { 56, 200 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 16 } }, 120, -48, 510, { .fields = { 24, 56 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 16 } }, 120, -64, 501, { .fields = { 24, 72 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 16 } }, 120, -80, 494, { .fields = { 24, 24 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 16 } }, 120, -96, 492, { .fields = { 32, 240 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, 128, -120, 476, { .fields = { 56, 48 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, 144, -120, 443, { .fields = { 72, 200 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, 136, -96, 453, { .fields = { 80, 32 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 24 } }, 136, -72, 464, { .fields = { 32, 96 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 16, 16 } }, 136, -48, 475, { .fields = { 24, 40 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 24 } }, 152, -96, 457, { .fields = { 40, 192 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 24 } }, 152, -72, 462, { .fields = { 40, 216 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 16 } }, 152, -48, 464, { .fields = { 48, 240 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 8 } }, -120, -40, 598, { .fields = { 56, 248 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 8 } }, -112, -40, 644, { .fields = { 32, 136 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 8 } }, -104, -40, 650, { .fields = { 16, 32 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 8 } }, -112, -32, 616, { .fields = { 24, 184 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 8 } }, -104, -32, 626, { .fields = { 16, 24 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 8 } }, -96, -32, 669, { .fields = { 88, 248 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 8 } }, -112, -24, 621, { .fields = { 80, 248 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 8 } }, -104, -24, 625, { .fields = { 88, 56 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 8 } }, -96, -24, 637, { .fields = { 88, 152 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 8 } }, -88, -24, 662, { .fields = { 72, 48 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 8 } }, -112, -16, 620, { .fields = { 64, 248 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 8 } }, -104, -16, 679, { .fields = { 56, 168 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 8 } }, -112, -8, 624, { .fields = { 64, 144 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_VIEW8_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_VIEW8_CLUT, { .fields = { 8, 8 } }, -104, -8, 657, { .fields = { 72, 248 } }, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, SHELTER_B1_STERILIZATION_ROOM_VIEW8_NEUTRAL_COLOR, 0 },
};

/// View-8 image background followed by all 89 source sprites in three ranges.
///
/// Ranges [0,28), [28,75), [75,89) partition the source array. The leading
/// zero-count record requests decoded image strips; SPRITE_BATCH_END ends
/// the list. The retained field_6 bytes 1/2 have no established meaning.
static SpriteBatch _gShelterB1SterilizationRoomView8SpriteBatches[5] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = 28, .field_6 = { 1, 0 } },
    { .firstSprite = 28, .spriteCount = 47, .field_6 = { 2, 0 } },
    { .firstSprite = 75, .spriteCount = ARRAY_SIZE(_gShelterB1SterilizationRoomView8SpriteSources) - 75 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 9's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView9SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 10's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView10SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 11's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView11SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 12's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView12SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 13's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView13SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 14's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView14SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 15's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView15SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 16's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView16SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 17's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView17SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Encoded texture location and neutral RGB modulation of views 18/20's sprites.
///
/// Texture page 143 selects 8-bit texels at VRAM (960,0), CLUT 0x3FC0 selects
/// (0,255), and 128 is neutral modulation. Source coordinates remain texels.
enum {
    SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE       = 143,
    SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT               = 0x3FC0,
    SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION = 128,
};

/// Mapped view 18's 42 writable sprite rectangles and unscaled sorting depths.
///
/// Screen positions are pixels relative to the view centre. Batch ranges index
/// these elements without a source sentinel; all use opaque, neutral RGB.
static SpriteSource _gShelterB1SterilizationRoomView18SpriteSources[42] = {
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 8, 8 } }, 96, -32, 687, { .fields = { 104, 248 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 8, 8 } }, 80, -16, 875, { .fields = { 104, 184 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 104, -56, 691, { .fields = { 80, 216 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 104, -40, 696, { .fields = { 80, 232 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 120, -56, 690, { .fields = { 72, 0 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 120, -40, 695, { .fields = { 80, 168 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 24, 24 } }, 136, -56, 675, { .fields = { 104, 192 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 24, 24 } }, 136, -32, 681, { .fields = { 88, 144 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 24, 24 } }, 136, -8, 688, { .fields = { 88, 120 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 24, 24 } }, 136, 16, 695, { .fields = { 88, 96 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 24, 24 } }, 136, 40, 702, { .fields = { 104, 48 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 24, 24 } }, 136, 64, 712, { .fields = { 104, 72 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 24, 24 } }, 136, 88, 732, { .fields = { 104, 0 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 120, -24, 700, { .fields = { 80, 16 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 104, -24, 703, { .fields = { 80, 32 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 88, -24, 669, { .fields = { 80, 48 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 24 } }, 120, -8, 706, { .fields = { 112, 144 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 24 } }, 120, 16, 713, { .fields = { 112, 168 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 24 } }, 120, 40, 720, { .fields = { 112, 96 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 24 } }, 120, 64, 728, { .fields = { 112, 120 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 120, 88, 734, { .fields = { 72, 128 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 104, -8, 712, { .fields = { 72, 144 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 104, 8, 721, { .fields = { 72, 184 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 104, 24, 729, { .fields = { 72, 112 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 104, 40, 734, { .fields = { 72, 64 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 104, 56, 739, { .fields = { 72, 80 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 24 } }, 104, 72, 746, { .fields = { 112, 216 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 88, -8, 677, { .fields = { 72, 96 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 88, 8, 685, { .fields = { 96, 168 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 88, 24, 694, { .fields = { 88, 0 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 88, 40, 750, { .fields = { 96, 216 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 88, 56, 755, { .fields = { 96, 232 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 24 } }, 88, 72, 756, { .fields = { 112, 24 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 72, 72, 771, { .fields = { 112, 240 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 72, 56, 767, { .fields = { 88, 80 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 72, 40, 766, { .fields = { 88, 184 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 72, 24, 675, { .fields = { 88, 200 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, 72, 8, 675, { .fields = { 88, 64 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, 64, 32, 675, { .fields = { 96, 16 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, 64, 48, 780, { .fields = { 96, 32 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 8, 24 } }, 64, 64, 781, { .fields = { 104, 24 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, 80, -8, 675, { .fields = { 96, 48 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// Mapped view 18's image selector followed by its complete 42-sprite range.
///
/// Keeps decoded image strips; all source elements participate in cached drawing.
static SpriteBatch _gShelterB1SterilizationRoomView18SpriteBatches[3] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gShelterB1SterilizationRoomView18SpriteSources) },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 19's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView19SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 20's 30 writable sprite rectangles and unscaled sorting depths.
///
/// Screen positions are pixels relative to the view centre. Batch ranges index
/// these elements without a source sentinel; all use opaque, neutral RGB.
static SpriteSource _gShelterB1SterilizationRoomView20SpriteSources[30] = {
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 8, 8 } }, -160, 32, 645, { .fields = { 104, 136 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 8, 8 } }, -136, 40, 658, { .fields = { 96, 208 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 24, 24 } }, -160, 96, 481, { .fields = { 104, 168 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 24, 24 } }, -136, 96, 482, { .fields = { 104, 144 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 24, 24 } }, -160, 72, 551, { .fields = { 104, 192 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 24, 24 } }, -160, 48, 603, { .fields = { 88, 0 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 24, 8 } }, -160, 40, 622, { .fields = { 80, 200 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 24, 16 } }, -136, 80, 621, { .fields = { 88, 120 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 24, 16 } }, -136, 64, 625, { .fields = { 88, 88 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, -136, 48, 630, { .fields = { 96, 56 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 24, 16 } }, -112, 104, 475, { .fields = { 88, 72 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, -88, 104, 475, { .fields = { 96, 216 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, -72, 104, 477, { .fields = { 88, 168 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 24, 16 } }, -112, 88, 652, { .fields = { 80, 184 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 24, 16 } }, -112, 72, 655, { .fields = { 80, 152 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 24, 8 } }, -112, 64, 676, { .fields = { 88, 248 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -88, 80, 669, { .fields = { 96, 136 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 24 } }, -40, -120, 431, { .fields = { 112, 72 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 24 } }, -24, -120, 378, { .fields = { 112, 96 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, -40, -96, 416, { .fields = { 96, 24 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 24 } }, -24, -96, 418, { .fields = { 112, 48 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 24 } }, -40, -72, 433, { .fields = { 112, 0 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 24 } }, -24, -72, 436, { .fields = { 112, 24 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 24 } }, -40, -48, 433, { .fields = { 112, 120 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 24 } }, -24, -48, 452, { .fields = { 112, 216 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, -24, -24, 427, { .fields = { 96, 40 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -32, -16, 428, { .fields = { 120, 240 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 8, 16 } }, -32, 0, 426, { .fields = { 112, 240 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, -32, 16, 424, { .fields = { 96, 232 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
    { SHELTER_B1_STERILIZATION_ROOM_SPRITE_TEXTURE_PAGE, SHELTER_B1_STERILIZATION_ROOM_SPRITE_CLUT, { .fields = { 16, 16 } }, -32, 32, 424, { .fields = { 96, 104 } }, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, SHELTER_B1_STERILIZATION_ROOM_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// Source-element boundary between mapped view 20's two live sprite ranges.
enum { SHELTER_B1_STERILIZATION_ROOM_VIEW20_FIRST_BATCH_COUNT = 17 };

/// Mapped view 20's image selector and two ranges partitioning its 30 sources.
///
/// Both ranges are visible and processed by cached drawing. The first range
/// retains serialized byte field_6[0] = 1, whose meaning is unproven.
static SpriteBatch _gShelterB1SterilizationRoomView20SpriteBatches[4] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = 0, .spriteCount = SHELTER_B1_STERILIZATION_ROOM_VIEW20_FIRST_BATCH_COUNT, .field_6 = { 1, 0 } },
    { .firstSprite = SHELTER_B1_STERILIZATION_ROOM_VIEW20_FIRST_BATCH_COUNT, .spriteCount = ARRAY_SIZE(_gShelterB1SterilizationRoomView20SpriteSources) - SHELTER_B1_STERILIZATION_ROOM_VIEW20_FIRST_BATCH_COUNT },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 21's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView21SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 22's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView22SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 23's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView23SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Mapped view 24's image-background selector and empty sprite-batch list.
///
/// The zero-count first record requests decoded image strips; the following
/// SPRITE_BATCH_END record terminates the list without selecting any sprites.
static SpriteBatch _gShelterB1SterilizationRoomView24SpriteBatches[2] = {
    { .firstSprite = 0, .spriteCount = 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

SpriteView gShelterB1SterilizationRoomSpriteViews[24] = {
    { { .empty = _gShelterB1SterilizationRoomView1SpriteBatches }, _gShelterB1SterilizationRoomView1SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView2SpriteBatches }, _gShelterB1SterilizationRoomView2SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView3SpriteBatches }, _gShelterB1SterilizationRoomView3SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView4SpriteBatches }, _gShelterB1SterilizationRoomView4SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView5SpriteBatches }, _gShelterB1SterilizationRoomView5SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView6SpriteBatches }, _gShelterB1SterilizationRoomView6SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView7SpriteBatches }, _gShelterB1SterilizationRoomView7SpriteBatches, NULL },
    { { .elements = _gShelterB1SterilizationRoomView8SpriteSources }, _gShelterB1SterilizationRoomView8SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView9SpriteBatches }, _gShelterB1SterilizationRoomView9SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView10SpriteBatches }, _gShelterB1SterilizationRoomView10SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView11SpriteBatches }, _gShelterB1SterilizationRoomView11SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView12SpriteBatches }, _gShelterB1SterilizationRoomView12SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView13SpriteBatches }, _gShelterB1SterilizationRoomView13SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView14SpriteBatches }, _gShelterB1SterilizationRoomView14SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView15SpriteBatches }, _gShelterB1SterilizationRoomView15SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView16SpriteBatches }, _gShelterB1SterilizationRoomView16SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView17SpriteBatches }, _gShelterB1SterilizationRoomView17SpriteBatches, NULL },
    { { .elements = _gShelterB1SterilizationRoomView18SpriteSources }, _gShelterB1SterilizationRoomView18SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView19SpriteBatches }, _gShelterB1SterilizationRoomView19SpriteBatches, NULL },
    { { .elements = _gShelterB1SterilizationRoomView20SpriteSources }, _gShelterB1SterilizationRoomView20SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView21SpriteBatches }, _gShelterB1SterilizationRoomView21SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView22SpriteBatches }, _gShelterB1SterilizationRoomView22SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView23SpriteBatches }, _gShelterB1SterilizationRoomView23SpriteBatches, NULL },
    { { .empty = _gShelterB1SterilizationRoomView24SpriteBatches }, _gShelterB1SterilizationRoomView24SpriteBatches, NULL },
};

/// One room-owned point light with mutable transform and attenuation caches.
///
/// Position and falloff radii use world game units; RGB uses Q12 intensity.
/// It contributes in every view and is shared by all three room descriptors.
static WorldCoordPointLight _gShelterB1SterilizationRoomPointLights[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3500, -2000, 0x30D4 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2129, 3686, 3276 }, { 0, 0 } }, 3500, 3750 },
};

/// Eleven room-owned cone lights shared by all three room descriptors.
///
/// Positions and falloff radii use world game units, axes and RGB use Q12,
/// and full cone openings use 4096 angle units per turn. All contribute in
/// every view; coordinate composition and attenuation overwrite their caches.
static WorldCoordSpotLight _gShelterB1SterilizationRoomConeLights[11] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 865, -2750, 1490 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3031 }, { 0, 0 } }, { 208, 4090, 14, 0 }, 1000, 3500, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1625, -4500, 8500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 3276, 3522 }, { 0, 0 } }, { 0, ONE, 0, 0 }, 4500, 5250, 887 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5375, -4500, 8500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 3276, 3522 }, { 0, 0 } }, { 0, ONE, 0, 0 }, 4500, 5250, 887 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 865, -2750, 2490 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3031 }, { 0, 0 } }, { 208, 4090, 14, 0 }, 1000, 3500, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 865, -2750, 3490 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3031 }, { 0, 0 } }, { 208, 4090, 14, 0 }, 1000, 3500, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 865, -2750, 4490 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3031 }, { 0, 0 } }, { 208, 4090, 14, 0 }, 1000, 3500, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6140, -2750, 1490 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3031 }, { 0, 0 } }, { -356, 4080, 14, 0 }, 1000, 3500, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6140, -2750, 2490 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3031 }, { 0, 0 } }, { -356, 4080, 14, 0 }, 1000, 3500, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6140, -2750, 3490 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3031 }, { 0, 0 } }, { -356, 4080, 14, 0 }, 1000, 3500, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6140, -2750, 4490 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3031 }, { 0, 0 } }, { -356, 4080, 14, 0 }, 1000, 3500, 341 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3500, -4750, 3000 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 3686, 3276 }, { 0, 0 } }, { 0, ONE, 0, 0 }, 5500, 6000, 853 },
};

/// Shared collection of one point light and eleven cone lights, with no directional lights.
static WorldCoordRoomLights _gShelterB1SterilizationRoomLights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gShelterB1SterilizationRoomPointLights),
    .pointLights           = _gShelterB1SterilizationRoomPointLights,
    .coneLightCount        = ARRAY_SIZE(_gShelterB1SterilizationRoomConeLights),
    .coneLights            = _gShelterB1SterilizationRoomConeLights,
};

/// Eight directed quads joining logical view pairs 2/3, 4/5, 6/7 and 8/9.
///
/// The final included quad has LAST set. Setup binds/enables all eight records,
/// whose linked-list state and hit latches remain writable until teardown.
/// Origins, corner offsets and radii use room game units; normals use Q12.
static WorldCollisionTrigger _gShelterB1SterilizationRoomViewBoundaryTriggers[8] = {
    { NULL, NULL, NULL, { 3633, -1968, 2960, 0 }, { { -3680, -2736, 32, 0 }, { 3680, -2736, -32, 0 }, { -3680, 2736, 32, 0 }, { 3680, 2736, -32, 0 } }, { -36, 0, -4101, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3505, -1984, 2752, 0 }, { { 3088, -2736, -32, 0 }, { -3088, -2736, 32, 0 }, { 3088, 2736, -32, 0 }, { -3088, 2736, 32, 0 } }, { 42, 0, 4125, 0 }, { 0, 0, 4096, 0 }, 4096, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5456, -1856, 8240, 0 }, { { 1904, -2736, -16, 0 }, { -1904, -2736, 16, 0 }, { 1904, 2736, -16, 0 }, { -1904, 2736, 16, 0 } }, { 33, 0, 4110, 0 }, { 0, 0, 4096, 0 }, 3328, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5455, -1792, 8384, 0 }, { { -1920, -2736, 0, 0 }, { 1920, -2736, 0, 0 }, { -1920, 2736, 0, 0 }, { 1920, 2736, 0, 0 } }, { 0, 0, -4103, 0 }, { 0, 0, 4096, 0 }, 3337, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1568, -1888, 8256, 0 }, { { 1904, -2736, -16, 0 }, { -1904, -2736, 16, 0 }, { 1904, 2736, -16, 0 }, { -1904, 2736, 16, 0 } }, { 33, 0, 4110, 0 }, { 0, 0, 4096, 0 }, 3328, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1536, -1760, 8448, 0 }, { { -1920, -2736, 0, 0 }, { 1920, -2736, 0, 0 }, { -1920, 2736, 0, 0 }, { 1920, 2736, 0, 0 } }, { 0, 0, -4103, 0 }, { 0, 0, 4096, 0 }, 3337, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4433, -1601, 0x3081, 0 }, { { -416, -2736, -1536, 0 }, { 416, -2736, 1536, 0 }, { -416, 2736, -1536, 0 }, { 416, 2736, 1536, 0 } }, { 3980, 0, -1079, 0 }, { 0, 0, 4096, 0 }, 3156, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4591, -1472, 0x30D0, 0 }, { { 416, -2736, 1488, 0 }, { -416, -2736, -1488, 0 }, { 416, 2736, 1488, 0 }, { -416, 2736, -1488, 0 } }, { -3945, 0, 1102, 0 }, { 0, 0, 4096, 0 }, 3135, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Room-action and room-command IDs carried by the interaction table.
///
/// Actions dispatch through DIRECTION_MESSAGE_ROOM_ACTION; command IDs dispatch
/// through ROOM_MESSAGE_COMMAND when the CAP-room-message sentinel is selected.
enum {
    SHELTER_B1_STERILIZATION_ROOM_ACTION_START_TRAP               = 1,
    SHELTER_B1_STERILIZATION_ROOM_ACTION_ESCAPE_PROMPT            = 2,
    SHELTER_B1_STERILIZATION_ROOM_ACTION_NOTICE_DOOR_3            = 3,
    SHELTER_B1_STERILIZATION_ROOM_ACTION_SCENE_DOOR_4             = 4,
    SHELTER_B1_STERILIZATION_ROOM_ACTION_NOTE_DOOR_5              = 5,
    SHELTER_B1_STERILIZATION_ROOM_ACTION_NOTICE_DOOR_6            = 6,
    SHELTER_B1_STERILIZATION_ROOM_ACTION_NOTICE_DOOR_7            = 7,
    SHELTER_B1_STERILIZATION_ROOM_ACTION_NOTE_DOOR_8              = 8,
    SHELTER_B1_STERILIZATION_ROOM_ACTION_SCENE_DOOR_9             = 9,
    SHELTER_B1_STERILIZATION_ROOM_ACTION_NOTICE_DOOR_10           = 10,
    SHELTER_B1_STERILIZATION_ROOM_COMMAND_TRAP_DIALOGUE           = 4,
    SHELTER_B1_STERILIZATION_ROOM_COMMAND_TRAP_EXCHANGE           = 14,
    SHELTER_B1_STERILIZATION_ROOM_COMMAND_CUTSCENE                = 17,
    SHELTER_B1_STERILIZATION_ROOM_COMMAND_ALTERNATE_TRAP_DIALOGUE = 19,
    SHELTER_B1_STERILIZATION_ROOM_COMMAND_PRESENTATION_12         = 12,
    SHELTER_B1_STERILIZATION_ROOM_COMMAND_PRESENTATION_13         = 13,
    SHELTER_B1_STERILIZATION_ROOM_COMMAND_CAP_21                  = 21,
    // Packed transition keys: current endpoint in the high nibble, arrival in the low.
    SHELTER_B1_STERILIZATION_ROOM_CORRIDOR_ARRIVAL_WARP_ID   = 6,
    SHELTER_B1_STERILIZATION_ROOM_POD_TUNNEL_ARRIVAL_WARP_ID = 1,
};

WorldCollisionTrigger gShelterB1SterilizationRoomActionTriggers[28] = {
    { .origin = { 5536, -64, 9200, 0 }, .vertices = { { -1792, 0, -496, 0 }, { 1792, 0, -496, 0 }, { -1792, 0, 496, 0 }, { 1792, 0, 496, 0 } }, .normal = { 0, 4106, 0, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 1859, .control = WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, .parameter0 = SHELTER_B1_STERILIZATION_ROOM_ACTION_START_TRAP, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_QUAD },
    { .origin = { 6256, -64, 0x2840, 0 }, .vertices = { { -592, 0, -544, 0 }, { 592, 0, -544, 0 }, { -592, 0, 544, 0 }, { 592, 0, 544, 0 } }, .normal = { 0, 4101, 0, 0 }, .facingNormal = { 0, 0, -4096, 0 }, .radius = 801, .control = WORLD_COLLISION_TRIGGER_ACTION_ROOM, .parameter0 = SHELTER_B1_STERILIZATION_ROOM_ACTION_ESCAPE_PROMPT, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 4992, -64, 0x2980, 0 }, .vertices = { { -768, 0, -432, 0 }, { 768, 0, -432, 0 }, { -768, 0, 432, 0 }, { 768, 0, 432, 0 } }, .normal = { 0, 4105, 0, 0 }, .facingNormal = { 0, 0, -4096, 0 }, .radius = 879, .control = WORLD_COLLISION_TRIGGER_ACTION_ROOM, .parameter0 = SHELTER_B1_STERILIZATION_ROOM_ACTION_NOTICE_DOOR_3, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 4992, -64, 0x2D60, 0 }, .vertices = { { -768, 0, -432, 0 }, { 768, 0, -432, 0 }, { -768, 0, 432, 0 }, { 768, 0, 432, 0 } }, .normal = { 0, 4105, 0, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 879, .control = WORLD_COLLISION_TRIGGER_ACTION_ROOM, .parameter0 = SHELTER_B1_STERILIZATION_ROOM_ACTION_SCENE_DOOR_4, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 2016, -64, 0x2900, 0 }, .vertices = { { -768, 0, -432, 0 }, { 768, 0, -432, 0 }, { -768, 0, 432, 0 }, { 768, 0, 432, 0 } }, .normal = { 0, 4105, 0, 0 }, .facingNormal = { 0, 0, -4096, 0 }, .radius = 879, .control = WORLD_COLLISION_TRIGGER_ACTION_ROOM, .parameter0 = SHELTER_B1_STERILIZATION_ROOM_ACTION_NOTE_DOOR_5, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 1984, -64, 0x2D00, 0 }, .vertices = { { -768, 0, -432, 0 }, { 768, 0, -432, 0 }, { -768, 0, 432, 0 }, { 768, 0, 432, 0 } }, .normal = { 0, 4105, 0, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 879, .control = WORLD_COLLISION_TRIGGER_ACTION_ROOM, .parameter0 = SHELTER_B1_STERILIZATION_ROOM_ACTION_NOTICE_DOOR_6, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 5056, -64, 5536, 0 }, .vertices = { { -768, 0, -432, 0 }, { 768, 0, -432, 0 }, { -768, 0, 432, 0 }, { 768, 0, 432, 0 } }, .normal = { 0, 4105, 0, 0 }, .facingNormal = { 0, 0, -4096, 0 }, .radius = 879, .control = WORLD_COLLISION_TRIGGER_ACTION_ROOM, .parameter0 = SHELTER_B1_STERILIZATION_ROOM_ACTION_NOTICE_DOOR_7, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 5024, -64, 6432, 0 }, .vertices = { { -768, 0, -432, 0 }, { 768, 0, -432, 0 }, { -768, 0, 432, 0 }, { 768, 0, 432, 0 } }, .normal = { 0, 4105, 0, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 879, .control = WORLD_COLLISION_TRIGGER_ACTION_ROOM, .parameter0 = SHELTER_B1_STERILIZATION_ROOM_ACTION_NOTE_DOOR_8, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 1984, -64, 5472, 0 }, .vertices = { { -768, 0, -432, 0 }, { 768, 0, -432, 0 }, { -768, 0, 432, 0 }, { 768, 0, 432, 0 } }, .normal = { 0, 4105, 0, 0 }, .facingNormal = { 0, 0, -4096, 0 }, .radius = 879, .control = WORLD_COLLISION_TRIGGER_ACTION_ROOM, .parameter0 = SHELTER_B1_STERILIZATION_ROOM_ACTION_SCENE_DOOR_9, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 1920, -64, 6336, 0 }, .vertices = { { -768, 0, -432, 0 }, { 768, 0, -432, 0 }, { -768, 0, 432, 0 }, { 768, 0, 432, 0 } }, .normal = { 0, 4105, 0, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 879, .control = WORLD_COLLISION_TRIGGER_ACTION_ROOM, .parameter0 = SHELTER_B1_STERILIZATION_ROOM_ACTION_NOTICE_DOOR_10, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 3440, -80, 976, 0 }, .vertices = { { -912, 0, -464, 0 }, { 912, 0, -464, 0 }, { -912, 0, 464, 0 }, { 912, 0, 464, 0 } }, .normal = { 0, 4102, 0, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 1021, .control = WORLD_COLLISION_TRIGGER_ACTION_WARP, .parameter0 = GAME_AREA_SHELTER_B1_MAIN_CORRIDOR, .parameter1 = (SHELTER_B1_STERILIZATION_ROOM_CORRIDOR_WARP_ID << 4) | SHELTER_B1_STERILIZATION_ROOM_CORRIDOR_ARRIVAL_WARP_ID, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 3344, -80, 0x3380, 0 }, .vertices = { { -912, 0, -320, 0 }, { 912, 0, -320, 0 }, { -912, 0, 320, 0 }, { 912, 0, 320, 0 } }, .normal = { 0, 4103, 0, 0 }, .facingNormal = { 0, 0, -4096, 0 }, .radius = 966, .control = WORLD_COLLISION_TRIGGER_ACTION_WARP, .parameter0 = GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL, .parameter1 = (SHELTER_B1_STERILIZATION_ROOM_POD_TUNNEL_WARP_ID << 4) | SHELTER_B1_STERILIZATION_ROOM_POD_TUNNEL_ARRIVAL_WARP_ID, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 1088, -64, 3120, 0 }, .vertices = { { -512, 0, -2624, 0 }, { 512, 0, -2624, 0 }, { -512, 0, 2624, 0 }, { 512, 0, 2624, 0 } }, .normal = { 0, 4096, 0, 0 }, .facingNormal = { 4076, 0, -401, 0 }, .radius = 2672, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 15, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 800, -64, 6624, 0 }, .vertices = { { -512, 0, -288, 0 }, { 512, 0, -288, 0 }, { -512, 0, 288, 0 }, { 512, 0, 288, 0 } }, .normal = { 0, 4095, 0, 0 }, .facingNormal = { -202, 0, 4090, 0 }, .radius = 586, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 14, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 736, -64, 0x2900, 0 }, .vertices = { { -512, 0, -288, 0 }, { 512, 0, -288, 0 }, { -512, 0, 288, 0 }, { 512, 0, 288, 0 } }, .normal = { 0, 4095, 0, 0 }, .facingNormal = { -201, 0, -4093, 0 }, .radius = 586, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 6, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 6336, -64, 6560, 0 }, .vertices = { { -512, 0, -288, 0 }, { 512, 0, -288, 0 }, { -512, 0, 288, 0 }, { 512, 0, 288, 0 } }, .normal = { 0, 4095, 0, 0 }, .facingNormal = { -402, 0, 4078, 0 }, .radius = 586, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = SHELTER_B1_STERILIZATION_ROOM_COMMAND_TRAP_EXCHANGE, .parameter1 = WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 3472, -64, 0x2D20, 0 }, .vertices = { { -688, 0, -288, 0 }, { 688, 0, -288, 0 }, { -688, 0, 288, 0 }, { 688, 0, 288, 0 } }, .normal = { 0, 4102, 0, 0 }, .facingNormal = { -1, 0, 4096, 0 }, .radius = 743, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = SHELTER_B1_STERILIZATION_ROOM_COMMAND_TRAP_DIALOGUE, .parameter1 = WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 1280, -64, 0x3100, 0 }, .vertices = { { -368, 0, -640, 0 }, { 368, 0, -640, 0 }, { -368, 0, 640, 0 }, { 368, 0, 640, 0 } }, .normal = { 0, 4102, 0, 0 }, .facingNormal = { 4091, 0, -201, 0 }, .radius = 738, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = SHELTER_B1_STERILIZATION_ROOM_COMMAND_CUTSCENE, .parameter1 = WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 5712, -64, 0x30E0, 0 }, .vertices = { { -1248, 0, -448, 0 }, { 1248, 0, -448, 0 }, { -1248, 0, 448, 0 }, { 1248, 0, 448, 0 } }, .normal = { 0, 4117, 0, 0 }, .facingNormal = { 0, 0, -4096, 0 }, .radius = 1324, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 16, .parameter1 = CAP_EVENT_PAUSE_ACTORS, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 3536, -64, 5424, 0 }, .vertices = { { -688, 0, -368, 0 }, { 688, 0, -368, 0 }, { -688, 0, 368, 0 }, { 688, 0, 368, 0 } }, .normal = { 0, 4098, 0, 0 }, .facingNormal = { 0, 0, -4098, 0 }, .radius = 778, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = SHELTER_B1_STERILIZATION_ROOM_COMMAND_ALTERNATE_TRAP_DIALOGUE, .parameter1 = WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 1728, -64, 0x3380, 0 }, .vertices = { { -688, 0, -368, 0 }, { 688, 0, -368, 0 }, { -688, 0, 368, 0 }, { 688, 0, 368, 0 } }, .normal = { 0, 4098, 0, 0 }, .facingNormal = { 0, 0, -4098, 0 }, .radius = 778, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 18, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 5184, -64, 832, 0 }, .vertices = { { -688, 0, -368, 0 }, { 688, 0, -368, 0 }, { -688, 0, 368, 0 }, { 688, 0, 368, 0 } }, .normal = { 0, 4098, 0, 0 }, .facingNormal = { 0, 0, 4098, 0 }, .radius = 778, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 18, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 6208, -64, 3936, 0 }, .vertices = { { -1472, 0, -864, 0 }, { 352, 0, -864, 0 }, { -1472, 0, 864, 0 }, { 352, 0, 864, 0 } }, .normal = { 0, 4100, 0, 0 }, .facingNormal = { -4052, 0, 600, 0 }, .radius = 1702, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, .parameter0 = SHELTER_B1_STERILIZATION_ROOM_COMMAND_CAP_21, .parameter1 = WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, .flags = WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD },
    { .origin = { 5984, -64, 3200, 0 }, .vertices = { { -512, 0, -2624, 0 }, { 512, 0, -2624, 0 }, { -512, 0, 2624, 0 }, { 512, 0, 2624, 0 } }, .normal = { 0, 4096, 0, 0 }, .facingNormal = { -4091, 0, -202, 0 }, .radius = 2672, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 15, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 4576, -64, 0x3300, 0 }, .vertices = { { -352, 0, -448, 0 }, { 352, 0, -448, 0 }, { -352, 0, 448, 0 }, { 352, 0, 448, 0 } }, .normal = { 0, 4109, 0, 0 }, .facingNormal = { -4096, 0, 0, 0 }, .radius = 568, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = 16, .parameter1 = CAP_EVENT_PAUSE_ACTORS, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD },
    { .origin = { 5248, -64, 1632, 0 }, .vertices = { { -896, 0, -1072, 0 }, { 896, 0, -1072, 0 }, { -896, 0, 1072, 0 }, { 896, 0, 1072, 0 } }, .normal = { 0, 4101, 0, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 1396, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = SHELTER_B1_STERILIZATION_ROOM_COMMAND_PRESENTATION_12, .parameter1 = WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, .flags = WORLD_COLLISION_TRIGGER_QUAD },
    { .origin = { 1952, -64, 2992, 0 }, .vertices = { { -896, 0, -1408, 0 }, { 896, 0, -1408, 0 }, { -896, 0, 1408, 0 }, { 896, 0, 1408, 0 } }, .normal = { 0, 4109, 0, 0 }, .facingNormal = { 0, 0, 4096, 0 }, .radius = 1668, .control = WORLD_COLLISION_TRIGGER_ACTION_CAP, .parameter0 = SHELTER_B1_STERILIZATION_ROOM_COMMAND_PRESENTATION_13, .parameter1 = WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, .flags = WORLD_COLLISION_TRIGGER_QUAD },
    { .origin = { 6304, -64, 0x2840, 0 }, .vertices = { { -592, 0, -544, 0 }, { 592, 0, -544, 0 }, { -592, 0, 544, 0 }, { 592, 0, 544, 0 } }, .normal = { 0, 4101, 0, 0 }, .facingNormal = { -2276, 0, -3406, 0 }, .radius = 801, .control = WORLD_COLLISION_TRIGGER_ACTION_ROOM, .parameter0 = SHELTER_B1_STERILIZATION_ROOM_ACTION_ESCAPE_PROMPT, .parameter1 = 0, .flags = WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST },
};

/// Variant 1's empty resource list, consisting only of its terminator.
static AreaResource _gShelterB1SterilizationRoomVariant1Resources[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

/// Resource key and base-file number shared by the variant-5 scene actor's tables.
enum {
    SHELTER_B1_STERILIZATION_ROOM_SCENE_ACTOR_RESOURCE_ID = 117,
    SHELTER_B1_STERILIZATION_ROOM_SCENE_ACTOR_FILE_NUMBER = 606,
};

/// Variant 5's scene-actor resource and terminating record.
///
/// Entry 117 loads global file 160600 and spawns its single placed-actor task.
/// The placement supplies zero spawn mode/variant and a two-row CLUT offset.
static AreaResource _gShelterB1SterilizationRoomVariant5Resources[2] = {
    { .entryId = SHELTER_B1_STERILIZATION_ROOM_SCENE_ACTOR_RESOURCE_ID, .fileNumber = SHELTER_B1_STERILIZATION_ROOM_SCENE_ACTOR_FILE_NUMBER, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor160600PlacedActorTask },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 1's empty placement list, consisting only of its terminator.
static AreaPlacement _gShelterB1SterilizationRoomVariant1Placements[1] = {
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 5's scene actor at world (6080,0,4000), followed by the terminator.
///
/// Yaw -967 uses 4096 units per turn. Zero fileIdLow selects the base resource;
/// texture-page relocation is zero and CLUT relocation is two rows. Actor mode,
/// variant and parameter row are zero. The live actor borrows this placement.
static AreaPlacement _gShelterB1SterilizationRoomVariant5Placements[2] = {
    { .entryId = SHELTER_B1_STERILIZATION_ROOM_SCENE_ACTOR_RESOURCE_ID, .variant = 0, .mode = 0, .x = 6080, .y = 0, .z = 4000, .yaw = -967, .fileIdLow = 0, .texturePageOffset = 0, .clutRowOffset = 2, .rowIndex = 0 },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gShelterB1SterilizationRoomAreaVariants[11] = {
    [1] = { .placements = _gShelterB1SterilizationRoomVariant1Placements, .resources = _gShelterB1SterilizationRoomVariant1Resources },
    [5] = { .placements = _gShelterB1SterilizationRoomVariant5Placements, .resources = _gShelterB1SterilizationRoomVariant5Resources },
};

/// Two room-local wall quads blocking sight, including the LAST-marked second quad.
///
/// Setup links/enables both. Positions use game units and normals Q12; runtime
/// links and flags remain writable until teardown. Low flag bit 0 is retained
/// with no established meaning; these are separate from the collision mesh.
static WorldCollisionOccluder _gShelterB1SterilizationRoomSightOccluders[2] = {
    { NULL, NULL, { 3536, -1424, 6048, 0 }, { { -3888, 2448, 0, 0 }, { 3888, 2448, 0, 0 }, { -3888, -2448, 0, 0 }, { 3888, -2448, 0, 0 } }, { 0, 0, 4104, 0 }, 4579, 1, 0 },
    { NULL, NULL, { 3488, -1360, 0x2B00, 0 }, { { -3888, 2384, 0, 0 }, { 3888, 2384, 0, 0 }, { -3888, -2384, 0, 0 }, { 3888, -2384, 0, 0 } }, { 0, 0, 4099, 0 }, 4550, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Minimum ambient colours for 24 logical views, preceded by the view count.
///
/// Entry zero stores 24; entries 1..24 store RGB in 16 units per colour level
/// and retained luma. All three room descriptors borrow this table.
static WorldCoordRoomAmbientEntry _gShelterB1SterilizationRoomAmbientByView[25] = {
    { .viewCount = ARRAY_SIZE(_gShelterB1SterilizationRoomAmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 1006, 1603, 1284, 1339 } },
    { .color = { 925, 1526, 1344, 1277 } },
    { .color = { 870, 965, 955, 928 } },
    { .color = { 866, 959, 962, 924 } },
    { .color = { 864, 966, 957, 926 } },
    { .color = { 862, 980, 978, 935 } },
    { .color = { 1300, 1806, 1517, 1580 } },
    { .color = { 1523, 2045, 1610, 1794 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

/// Type-1 cue-2 bases; cue 1 uses the following entry and companions add 100.
enum {
    SHELTER_B1_STERILIZATION_ROOM_SURFACE1_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x59,
    SHELTER_B1_STERILIZATION_ROOM_SURFACE1_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5B,
    SHELTER_B1_STERILIZATION_ROOM_SURFACE2_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5D,
    SHELTER_B1_STERILIZATION_ROOM_SURFACE2_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x5F,
};

/// Surface-class-1 walk/run cue bases, with scripted jumps using the walk base.
///
/// Borrows the loaded type-1 sound bank; paired cues and companion offsets
/// select further entries. The surface property record borrows this storage.
static WorldCollisionFootstepSounds _gShelterB1SterilizationRoomSurface1FootstepSounds = {
    .walk         = SHELTER_B1_STERILIZATION_ROOM_SURFACE1_WALK_SOUND,
    .run          = SHELTER_B1_STERILIZATION_ROOM_SURFACE1_RUN_SOUND,
    .scriptedJump = SHELTER_B1_STERILIZATION_ROOM_SURFACE1_WALK_SOUND,
};

/// Surface-class-2 walk/run cue bases, with scripted jumps using the walk base.
///
/// Borrows the loaded type-1 sound bank; paired cues and companion offsets
/// select further entries. The surface property record borrows this storage.
static WorldCollisionFootstepSounds _gShelterB1SterilizationRoomSurface2FootstepSounds = {
    .walk         = SHELTER_B1_STERILIZATION_ROOM_SURFACE2_WALK_SOUND,
    .run          = SHELTER_B1_STERILIZATION_ROOM_SURFACE2_RUN_SOUND,
    .scriptedJump = SHELTER_B1_STERILIZATION_ROOM_SURFACE2_WALK_SOUND,
};

/// Blocking surface policy without footstep cues, shared by classes 0 and 3..7.
static WorldCollisionSurfaceProperties _gShelterB1SterilizationRoomDefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Blocking surface-class-1 policy with its walk, run and scripted-jump cues.
static WorldCollisionSurfaceProperties _gShelterB1SterilizationRoomSurface1Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB1SterilizationRoomSurface1FootstepSounds,
};

/// Blocking surface-class-2 policy with its walk, run and scripted-jump cues.
static WorldCollisionSurfaceProperties _gShelterB1SterilizationRoomSurface2Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gShelterB1SterilizationRoomSurface2FootstepSounds,
};

WorldCollisionSurfaceProperties* gShelterB1SterilizationRoomSurfacePropertiesByClass[8] = {
    &_gShelterB1SterilizationRoomDefaultSurfaceProperties,
    &_gShelterB1SterilizationRoomSurface1Properties,
    &_gShelterB1SterilizationRoomSurface2Properties,
    &_gShelterB1SterilizationRoomDefaultSurfaceProperties,
    &_gShelterB1SterilizationRoomDefaultSurfaceProperties,
    &_gShelterB1SterilizationRoomDefaultSurfaceProperties,
    &_gShelterB1SterilizationRoomDefaultSurfaceProperties,
    &_gShelterB1SterilizationRoomDefaultSurfaceProperties,
};

/// Parking layout selected when the first variant-5 actor event begins.
enum { SHELTER_B1_STERILIZATION_ROOM_ACTOR_EVENT_PARKING_VARIANT = 21 };

AreaApplyRec gShelterB1SterilizationRoomActorEventAreaUpdates[2] = {
    { GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING, SHELTER_B1_STERILIZATION_ROOM_ACTOR_EVENT_PARKING_VARIANT, AREA_APPLY_MODE_ALWAYS },
    { AREA_APPLY_END, 0, 0, 0 },
};

Task* gRoomCutsceneSoundTask = NULL;

s32 gShelterB1SterilizationRoomOnceSpawnMask = 0;

RoomCutsceneRec gShelterB1SterilizationRoomRepeatCutscene = { 0 };

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);
static void _glowDrawDiamond(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);

void shelterB1SterilizationRoomTrapEscapePromptTask(Task* task)
{
    enum {
        ESCAPE_PROMPT_START   = 0,
        ESCAPE_PROMPT_DELAY   = 1,
        ESCAPE_PROMPT_RESOLVE = 2,
        ESCAPE_CAP_COMMAND    = 8,
        ESCAPE_CHOICE_ACCEPT  = 1,
        ESCAPE_EVENT_IDLE     = 0,
        ESCAPE_EVENT_HELD     = 1,
        TRAP_ESCAPE_STARTED   = 1,
    };
    s32 promptState;

    promptState = task->state;
    switch (promptState) {
        case ESCAPE_PROMPT_START:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommandWithTransition(ESCAPE_CAP_COMMAND);
            gGameSession->eventState = ESCAPE_EVENT_HELD;
            task->state              = task->state + 1;
            return;
        case ESCAPE_PROMPT_DELAY:
            task->state = ESCAPE_PROMPT_RESOLVE;
            return;
        // The queued CAP transition has completed before this choice is read.
        case ESCAPE_PROMPT_RESOLVE:
            if (capGetVariantKey() == ESCAPE_CHOICE_ACCEPT) {
                evsStartScriptWithSkip(_gShelterB1SterilizationRoomTrapEscapeScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gShelterB1SterilizationRoomTrapEscapeSkipScript);
                gameFlagSetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED, TRAP_ESCAPE_STARTED);
                gGameSession->restartMode = GAME_SESSION_RESTART_NORMAL;
            } else {
                gGameSession->eventState = ESCAPE_EVENT_IDLE;
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            }
            taskKill(task);
            return;
        default:
            gGameSession->eventState = ESCAPE_EVENT_IDLE;
            taskKill(task);
            return;
    }
}

/// Requests a captured-frame reload at Dumping Hole arrival 3, room 1.
///
/// Used by both the normal and skipped trap-escape scripts. Updates only the
/// live save's destination; the resident reload task applies it to the session.
/// Requires the live save and scheduler, and owns no room-local work.
static void _shelterB1SterilizationRoomReloadDumpingHole(void)
{
    enum {
        DUMPING_HOLE_ARRIVAL = 3,
        DUMPING_HOLE_ROOM    = 1,
    };
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_SHELTER_B3_DUMPING_HOLE;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = DUMPING_HOLE_ARRIVAL;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = DUMPING_HOLE_ROOM;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
}

void shelterB1SterilizationRoomSwitchRoomTask(Task* task)
{
    enum {
        SHELTER_B1_STERILIZATION_ROOM_SWITCH_REQUEST_VIEW = 0,
        SHELTER_B1_STERILIZATION_ROOM_SWITCH_SELECT_ROOM  = 1,
        SHELTER_B1_STERILIZATION_ROOM_SWITCH_DESTINATION  = 2,
    };
    s32 switchState = task->state;

    switch (switchState) {
        case SHELTER_B1_STERILIZATION_ROOM_SWITCH_REQUEST_VIEW:
            gGameSession->viewDirty = 1;
            task->state            += 1;
            break;
        case SHELTER_B1_STERILIZATION_ROOM_SWITCH_SELECT_ROOM:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = SHELTER_B1_STERILIZATION_ROOM_SWITCH_DESTINATION;
            gGameSession->location.roomState                            = SHELTER_B1_STERILIZATION_ROOM_SWITCH_DESTINATION;
            gGameSession->roomObjsDirty                                 = switchState;
            task->state                                                += 1;
            break;
        default:
            taskKill(task);
            break;
    }
}

void shelterB1SterilizationRoomDoorNoticeTask(Task* task)
{
    enum {
        DOOR_NOTICE_START       = 0,
        DOOR_NOTICE_CAP_COMMAND = 9,
    };
    if (task->state == DOOR_NOTICE_START) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        capRunCommandWithTransition(DOOR_NOTICE_CAP_COMMAND);
        task->state += 1;
        return;
    }
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    taskKill(task);
}

/// Latches and requests the trap's repeating damage task once per spawn-mask reset.
///
/// Normal and skipped trap-entry scripts share bit 5. Both payloads are zero;
/// the bit remains set even if allocation fails. Keep the room overlay loaded
/// while the requested damage task is live.
static void _shelterB1SterilizationRoomSpawnTrapDamageOnce(void)
{
    enum {
        TRAP_DAMAGE_SPAWN_BIT = 1 << SHELTER_B1_STERILIZATION_ROOM_TASK_TRAP_DAMAGE,
    };
    if (!(gShelterB1SterilizationRoomOnceSpawnMask & TRAP_DAMAGE_SPAWN_BIT)) {
        gShelterB1SterilizationRoomOnceSpawnMask |= TRAP_DAMAGE_SPAWN_BIT;
        taskSpawnFromTable(gShelterB1SterilizationRoomEventTaskDescs, SHELTER_B1_STERILIZATION_ROOM_TASK_TRAP_DAMAGE, 0, 0);
    }
}

void shelterB1SterilizationRoomResetSpawnMaskTask(Task* task)
{
    gShelterB1SterilizationRoomOnceSpawnMask = 0;
    taskKill(task);
}

/// Lowers the two trap-sequence sound instances after normal or skipped playback.
///
/// Queues neutral pan and attenuation 36 for area-bank entries 6 and 7. Their
/// gain-index scale is 91/127; absent instances or a full sound-event pool make
/// the corresponding request a no-op. Does not stop either sound script.
static void _shelterB1SterilizationRoomAttenuateTrapSounds(void)
{
    enum { TRAP_SOUND_ENTRY_6     = 6,
           TRAP_SOUND_ENTRY_7     = 7,
           TRAP_SOUND_ATTENUATION = 36 };

    sndEvtRequestScriptMix(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, TRAP_SOUND_ENTRY_6), 0, TRAP_SOUND_ATTENUATION);
    sndEvtRequestScriptMix(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_STERILIZATION_ROOM, TRAP_SOUND_ENTRY_7), 0, TRAP_SOUND_ATTENUATION);
}

/// Resets CAP selection, optionally choosing the trap scene's dialogue resource.
///
/// Event scripts pass 1 on scene entry and 0 on normal or skipped exit. Exactly
/// 1 selects data-resource ordinal 1 and texture VRAM origin (704,256); every
/// other value keeps the default selection from `capReset`. Requires stopped
/// CAP playback and fully loaded, writable CAP resources and texture data;
/// the selected file stays borrowed while its text or command tables are used.
static void _shelterB1SterilizationRoomResetCapResource(s32 selectSceneFile)
{
    enum { SCENE_FILE_ORDINAL   = 1,
           SCENE_TEXTURE_VRAM_X = 704,
           SCENE_TEXTURE_VRAM_Y = 256 };

    capReset();
    if (selectSceneFile == SHELTER_B1_STERILIZATION_ROOM_CAP_SCENE) {
        gCapFile = NULL;
        capSelectLoadedFile(SCENE_FILE_ORDINAL);
        capSetTexturePage(SCENE_TEXTURE_VRAM_X, SCENE_TEXTURE_VRAM_Y);
    }
}

/// Resets CAP playback and binds the loaded trap-scene dialogue resource and texture.
///
/// Requires stopped playback, loaded writable CAP data-resource ordinal 1
/// (zero-based among data slots) and its texture at VRAM (704,256) in 16-bit
/// pixels. First restores default selection, then clears the current-file
/// pointer before selecting/relocating the scene resource. No allocation or I/O
/// occurs; its storage remains borrowed while the selected tables are used.
static inline void _shelterB1SterilizationRoomSelectSceneCap(void)
{
    enum { SCENE_FILE_ORDINAL   = 1,
           SCENE_TEXTURE_VRAM_X = 704,
           SCENE_TEXTURE_VRAM_Y = 256 };

    capReset();
    gCapFile = NULL;
    capSelectLoadedFile(SCENE_FILE_ORDINAL);
    capSetTexturePage(SCENE_TEXTURE_VRAM_X, SCENE_TEXTURE_VRAM_Y);
}

void shelterB1SterilizationRoomTrapDialogueTask(Task* task)
{
    enum {
        TRAP_DIALOGUE_START         = 0,
        TRAP_DIALOGUE_WAIT          = 1,
        CAP_PRIMARY_BEFORE_ESCAPE   = 6,
        CAP_PRIMARY_AFTER_ESCAPE    = 9,
        CAP_ALTERNATE_BEFORE_ESCAPE = 7,
        CAP_ALTERNATE_AFTER_ESCAPE  = 8,
        TRAP_RUNNING                = 0,
        DIALOGUE_STARTED            = 1,
    };
    s32 dialogueCommand;
    s32 trapStopped;

    switch (task->state) {
        case TRAP_DIALOGUE_START:
            _shelterB1SterilizationRoomSelectSceneCap();
            if (task->spawnArg1.value != SHELTER_B1_STERILIZATION_ROOM_DIALOGUE_PRIMARY) {
                trapStopped     = gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED);
                dialogueCommand = CAP_ALTERNATE_AFTER_ESCAPE;
                if (trapStopped == TRAP_RUNNING) {
                    dialogueCommand = CAP_ALTERNATE_BEFORE_ESCAPE;
                }
                capRunCommandWithTransition(dialogueCommand);
                gameFlagSetNibble(GAME_FLAG_SHELTER_B1_STERILIZATION_ROOM_149, DIALOGUE_STARTED);
            } else {
                trapStopped     = gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_TRAP_STOPPED);
                dialogueCommand = CAP_PRIMARY_BEFORE_ESCAPE;
                if (trapStopped != TRAP_RUNNING) {
                    gameFlagSetNibble(GAME_FLAG_SHELTER_B1_STERILIZATION_ROOM_14A, DIALOGUE_STARTED);
                    gameFlagSetNibble(GAME_FLAG_STERILIZATION_ROOM_ACTION4_SCENE, DIALOGUE_STARTED);
                    dialogueCommand = CAP_PRIMARY_AFTER_ESCAPE;
                }
                capRunCommandWithTransition(dialogueCommand);
            }
            task->state++;
            return;
        case TRAP_DIALOGUE_WAIT:
            if (capIsBusy() == 0) {
                capReset();
                taskKill(task);
            }
            return;
    }
}

void shelterB1SterilizationRoomSceneDialogueTask(Task* task)
{
    enum {
        SCENE_DIALOGUE_START = 0,
        SCENE_DIALOGUE_WAIT  = 1,
    };
    switch (task->state) {
        case SCENE_DIALOGUE_START:
            _shelterB1SterilizationRoomSelectSceneCap();
            capRunCommandWithTransition(task->spawnArg1.value);
            task->state = task->state + 1;
            // A command that finishes immediately needs no extra wait tick.
            /* fallthrough */
        case SCENE_DIALOGUE_WAIT:
            if (capIsBusy() == 0) {
                capReset();
                taskKill(task);
            }
            break;
    }
}

void shelterB1SterilizationRoomEffectsTask(Task* task)
{
    enum {
        SHELTER_B1_STERILIZATION_ROOM_EFFECTS_GLOW_AND_SMOKE             = 0,
        SHELTER_B1_STERILIZATION_ROOM_EFFECTS_TRAP_PUFFS                 = 1,
        SHELTER_B1_STERILIZATION_ROOM_EFFECTS_CANCEL_UNARMED             = 0,
        SHELTER_B1_STERILIZATION_ROOM_EFFECTS_CANCEL_ARMED               = 1,
        SHELTER_B1_STERILIZATION_ROOM_EFFECTS_VIEW_MASK                  = 0xFF,
        SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCE_COUNT          = ARRAY_SIZE(_gShelterB1SterilizationRoomEffectGeometry.puffOrigins),
        SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCES_PER_DIRECTION = ARRAY_SIZE(_gShelterB1SterilizationRoomEffectGeometry.puffOrigins) / ARRAY_SIZE(_gShelterB1SterilizationRoomEffectGeometry.puffDirections),
        SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCES_PER_GROUP     = 4,
        SHELTER_B1_STERILIZATION_ROOM_EFFECTS_RING_RADIUS_MIN            = 256,
        SHELTER_B1_STERILIZATION_ROOM_EFFECTS_RING_RADIUS_MASK           = 511,
        SHELTER_B1_STERILIZATION_ROOM_EFFECTS_SMOKE_PARAMETERS           = 0x80023400, // Size 1024, three ticks/frame, growth 2, additive blend
        SHELTER_B1_STERILIZATION_ROOM_EFFECTS_SMOKE_BLEND_VARIANT_SHIFT  = 30,
        SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SMALL_SIZE_BIAS       = 128 << 16,
        SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_MEDIUM_SIZE_BIAS      = 96 << 16,
        SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_LARGE_SIZE_BIAS       = 384 << 16,
        SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SECOND_SIZE_BIAS      = 256 << 16
    };
    GfxCoord* coord;

    s32 sourceGroup;
    s32 directionGroup;
    s32 sourceIndex;

    /// Emits one smoke puff at each of two floor rings, sharing their sampled angle/radius.
    ///
    /// Uses mutable room-owned smokePoints and this controller's particle constants.
    /// Draws angle, radius, then two blend choices from the unsigned shared LCG.
    /// Angle is 0..4095 units/turn, radius 256..767 parent-coordinate units;
    /// centres are XZ (1500,3000) and (5500,2000), both at Y=0. Q12 trig results
    /// narrow to s16 XYZ. The paired points use separate additive/subtractive rolls.
    /// parentCoord is a live, side-effect-free GfxCoord*, evaluated twice. Spawning
    /// snapshots XYZ; retain the room geometry and parent for the puffs' lifetime.
    /// Expands to a compound statement with private temporaries.
#define SHELTER_B1_STERILIZATION_ROOM_EMIT_PAIRED_RING_SMOKE(parentCoord)                                                                     \
    {                                                                                                                                         \
        enum { SHELTER_B1_STERILIZATION_ROOM_SECOND_RING_CENTER_X = 5500,                                                                     \
               SHELTER_B1_STERILIZATION_ROOM_SECOND_RING_CENTER_Z = 2000,                                                                     \
               SHELTER_B1_STERILIZATION_ROOM_FIRST_RING_CENTER_X  = 1500,                                                                     \
               SHELTER_B1_STERILIZATION_ROOM_FIRST_RING_CENTER_Z  = 3000 };                                                                    \
        s32      ringAngle;                                                                                                                   \
        u32      randomState;                                                                                                                 \
        u32      randomHigh;                                                                                                                  \
        s16      ringRadius;                                                                                                                  \
        SVECTOR* firstSmokePoint;                                                                                                             \
        SVECTOR* secondSmokePoint;                                                                                                            \
        firstSmokePoint      = &_gShelterB1SterilizationRoomEffectGeometry.smokePoints[0];                                                    \
        secondSmokePoint     = &_gShelterB1SterilizationRoomEffectGeometry.smokePoints[1];                                                    \
        randomHigh           = (RANDOM_LCG_ADVANCE()) >> 16;                                                                                  \
        ringAngle            = (randomHigh << 16) >> 16;                                                                                      \
        ringAngle           &= (GLOW_FULL_TURN - 1);                                                                                          \
        randomState          = RANDOM_LCG_NEXT(gRandomLcgState);                                                                              \
        gRandomLcgState      = randomState;                                                                                                   \
        randomHigh           = (randomState >> 16) & SHELTER_B1_STERILIZATION_ROOM_EFFECTS_RING_RADIUS_MASK;                                  \
        ringRadius           = randomHigh + SHELTER_B1_STERILIZATION_ROOM_EFFECTS_RING_RADIUS_MIN;                                            \
        firstSmokePoint->vx  = ((ringRadius * rcos(ringAngle)) >> GLOW_TRIG_SHIFT) + SHELTER_B1_STERILIZATION_ROOM_FIRST_RING_CENTER_X;       \
        firstSmokePoint->vy  = 0;                                                                                                             \
        firstSmokePoint->vz  = ((ringRadius * rsin((s16)ringAngle)) >> GLOW_TRIG_SHIFT) + SHELTER_B1_STERILIZATION_ROOM_FIRST_RING_CENTER_Z;  \
        secondSmokePoint->vx = ((ringRadius * rcos(ringAngle)) >> GLOW_TRIG_SHIFT) + SHELTER_B1_STERILIZATION_ROOM_SECOND_RING_CENTER_X;      \
        secondSmokePoint->vy = 0;                                                                                                             \
        secondSmokePoint->vz = ((ringRadius * rsin((s16)ringAngle)) >> GLOW_TRIG_SHIFT) + SHELTER_B1_STERILIZATION_ROOM_SECOND_RING_CENTER_Z; \
        effectSpawn(EFFECT_SMOKE_PUFF, (parentCoord),                                                                                         \
                    ((((RANDOM_LCG_ADVANCE()) >> 16) & 1)                                                                                     \
                     << SHELTER_B1_STERILIZATION_ROOM_EFFECTS_SMOKE_BLEND_VARIANT_SHIFT) |                                                    \
                        SHELTER_B1_STERILIZATION_ROOM_EFFECTS_SMOKE_PARAMETERS,                                                               \
                    firstSmokePoint);                                                                                                         \
        effectSpawn(EFFECT_SMOKE_PUFF, (parentCoord),                                                                                         \
                    ((((RANDOM_LCG_ADVANCE()) >> 16) & 1)                                                                                     \
                     << SHELTER_B1_STERILIZATION_ROOM_EFFECTS_SMOKE_BLEND_VARIANT_SHIFT) |                                                    \
                        SHELTER_B1_STERILIZATION_ROOM_EFFECTS_SMOKE_PARAMETERS,                                                               \
                    secondSmokePoint);                                                                                                        \
    }

    /// Emits one smoke puff at the second floor ring, centred at XZ (5500,2000).
    ///
    /// Draws angle, radius and blend in that order from the shared unsigned LCG.
    /// Uses the room-owned smokePoints[1] and this controller's particle constants.
    /// Angle is 0..4095 units/turn, radius 256..767 parent-coordinate units, Y=0;
    /// Q12 trig results narrow to s16 XYZ. Blend is additive or subtractive.
    /// Evaluates the live GfxCoord* parentCoord once. Spawning snapshots XYZ;
    /// retain the room geometry and parent for the puff's lifetime. Expands to
    /// a compound statement with private temporaries.
#define SHELTER_B1_STERILIZATION_ROOM_EMIT_SECOND_RING_SMOKE(parentCoord)                                                                     \
    {                                                                                                                                         \
        enum { SHELTER_B1_STERILIZATION_ROOM_SECOND_RING_CENTER_X = 5500,                                                                     \
               SHELTER_B1_STERILIZATION_ROOM_SECOND_RING_CENTER_Z = 2000 };                                                                   \
        s32      ringAngle;                                                                                                                   \
        u32      randomState;                                                                                                                 \
        u32      randomHigh;                                                                                                                  \
        s16      ringRadius;                                                                                                                  \
        SVECTOR* secondSmokePoint;                                                                                                            \
        secondSmokePoint     = &_gShelterB1SterilizationRoomEffectGeometry.smokePoints[1];                                                    \
        randomHigh           = (RANDOM_LCG_ADVANCE()) >> 16;                                                                                  \
        ringAngle            = (randomHigh << 16) >> 16;                                                                                      \
        ringAngle           &= (GLOW_FULL_TURN - 1);                                                                                          \
        randomState          = RANDOM_LCG_NEXT(gRandomLcgState);                                                                              \
        gRandomLcgState      = randomState;                                                                                                   \
        randomHigh           = (randomState >> 16) & SHELTER_B1_STERILIZATION_ROOM_EFFECTS_RING_RADIUS_MASK;                                  \
        ringRadius           = randomHigh + SHELTER_B1_STERILIZATION_ROOM_EFFECTS_RING_RADIUS_MIN;                                            \
        secondSmokePoint->vx = ((ringRadius * rcos(ringAngle)) >> GLOW_TRIG_SHIFT) + SHELTER_B1_STERILIZATION_ROOM_SECOND_RING_CENTER_X;      \
        secondSmokePoint->vy = 0;                                                                                                             \
        secondSmokePoint->vz = ((ringRadius * rsin((s16)ringAngle)) >> GLOW_TRIG_SHIFT) + SHELTER_B1_STERILIZATION_ROOM_SECOND_RING_CENTER_Z; \
        effectSpawn(EFFECT_SMOKE_PUFF, (parentCoord),                                                                                         \
                    ((((RANDOM_LCG_ADVANCE()) >> 16) & 1)                                                                                     \
                     << SHELTER_B1_STERILIZATION_ROOM_EFFECTS_SMOKE_BLEND_VARIANT_SHIFT) |                                                    \
                        SHELTER_B1_STERILIZATION_ROOM_EFFECTS_SMOKE_PARAMETERS,                                                               \
                    secondSmokePoint);                                                                                                        \
    }

    coord = task->extra.coordBody->coord;

    // Draw persistent glows; smoke emission alone pauses with room effects.
    if (task->state == SHELTER_B1_STERILIZATION_ROOM_EFFECTS_GLOW_AND_SMOKE) {
        switch (viewGetMappedIndex() & SHELTER_B1_STERILIZATION_ROOM_EFFECTS_VIEW_MASK) {
            case 2:
                _glowDrawCapsule(&_gShelterB1SterilizationRoomEffectGeometry.capsuleEndpoints[0][0], 0x200, 0x222);
                break;
            case 3:
                _glowDrawCapsule(&_gShelterB1SterilizationRoomEffectGeometry.capsuleEndpoints[1][0], 0x200, 0x222);
                break;
                // These cases share a scope in the original switch layout.
                do {
                    case 6:
                        if (task->spawnArg1.value != SHELTER_B1_STERILIZATION_ROOM_EFFECTS_CANCEL_UNARMED) {
                            roomEffectRequestCancelAll();
                            task->spawnArg1.value = SHELTER_B1_STERILIZATION_ROOM_EFFECTS_CANCEL_UNARMED;
                        }
                        break;
                    case 8:
                        glowDrawDisc(&_gShelterB1SterilizationRoomEffectGeometry.smallDiscCenter, 0x100, 0x440);
                        _glowDrawDiamond(&_gShelterB1SterilizationRoomEffectGeometry.bladeGlowCenter, 0x60, 0x80);
                        break;
                    case 14:
                        task->state = SHELTER_B1_STERILIZATION_ROOM_EFFECTS_TRAP_PUFFS;
                        break;
                    case 19:
                        glowDrawDisc(&_gShelterB1SterilizationRoomEffectGeometry.smallDiscCenter, 0x100, 0x440);
                        _shelterB1SterilizationRoomDrawPulsingGlow(&_gShelterB1SterilizationRoomEffectGeometry.bladeGlowCenter, 0x60, 0x80);
                        break;
                    case 20:
                        _glowDrawCapsule(&_gShelterB1SterilizationRoomEffectGeometry.capsuleEndpoints[0][0], 0x200, 0x222);
                        _glowDrawCapsule(&_gShelterB1SterilizationRoomEffectGeometry.capsuleEndpoints[1][0], 0x200, 0x222);
                        _glowDrawCapsule(&_gShelterB1SterilizationRoomEffectGeometry.capsuleEndpoints[2][0], 0x200, 0x222);
                        _glowDrawCapsule(&_gShelterB1SterilizationRoomEffectGeometry.capsuleEndpoints[3][0], 0x200, 0x222);
                        task->spawnArg1.value = SHELTER_B1_STERILIZATION_ROOM_EFFECTS_CANCEL_ARMED;
                        if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                            SHELTER_B1_STERILIZATION_ROOM_EMIT_PAIRED_RING_SMOKE(coord);
                        }
                        break;
                    case 21:
                        task->spawnArg1.value = SHELTER_B1_STERILIZATION_ROOM_EFFECTS_CANCEL_ARMED;
                        if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                            SHELTER_B1_STERILIZATION_ROOM_EMIT_SECOND_RING_SMOKE(coord);
                        }
                        break;
                    case 22:
                        _glowDrawCapsule(&_gShelterB1SterilizationRoomEffectGeometry.capsuleEndpoints[1][0], 0x200, 0x222);
                        task->spawnArg1.value = SHELTER_B1_STERILIZATION_ROOM_EFFECTS_CANCEL_ARMED;
                        if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                            SHELTER_B1_STERILIZATION_ROOM_EMIT_SECOND_RING_SMOKE(coord);
                        }
                        break;
                    case 23:
                        _glowDrawCapsule(&_gShelterB1SterilizationRoomEffectGeometry.capsuleEndpoints[0][0], 0x200, 0x222);
                        task->spawnArg1.value = SHELTER_B1_STERILIZATION_ROOM_EFFECTS_CANCEL_ARMED;
                        if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                            SHELTER_B1_STERILIZATION_ROOM_EMIT_PAIRED_RING_SMOKE(coord);
                        }
                        break;
                    case 24:
                        _glowDrawCapsule(&_gShelterB1SterilizationRoomEffectGeometry.capsuleEndpoints[1][0], 0x200, 0x222);
                        task->spawnArg1.value = SHELTER_B1_STERILIZATION_ROOM_EFFECTS_CANCEL_ARMED;
                        if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                            SHELTER_B1_STERILIZATION_ROOM_EMIT_PAIRED_RING_SMOKE(coord);
                        }
                        break;
                } while (0);
        }
    } else {
        // The trap phase selects one puff source per four-point group.
        switch (viewGetMappedIndex() & SHELTER_B1_STERILIZATION_ROOM_EFFECTS_VIEW_MASK) {
            case 14:
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    for (sourceGroup = 8; sourceGroup < SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCES_PER_DIRECTION; sourceGroup += SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCES_PER_GROUP) {
                        sourceIndex = sourceGroup + (((RANDOM_LCG_ADVANCE()) >> 16) & 3);
                        effectSpawn(EFFECT_SHELTER_B1_STERILIZATION_PUFF, coord, sourceIndex, &_gShelterB1SterilizationRoomEffectGeometry.puffOrigins[sourceIndex]);
                    }
                }
                break;
            case 15:
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    for (sourceGroup = 4; sourceGroup < SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCES_PER_DIRECTION; sourceGroup += SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCES_PER_GROUP) {
                        if (((RANDOM_LCG_ADVANCE()) >> 16) & 1) {
                            sourceIndex = sourceGroup + (((RANDOM_LCG_ADVANCE()) >> 16) & 3);
                            effectSpawn(EFFECT_SHELTER_B1_STERILIZATION_PUFF, coord, sourceIndex - SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SMALL_SIZE_BIAS, &_gShelterB1SterilizationRoomEffectGeometry.puffOrigins[sourceIndex]);
                        }
                    }
                }
                break;
            case 16:
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    for (sourceGroup = 0; sourceGroup < SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCE_COUNT; sourceGroup += SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCES_PER_GROUP) {
                        if (!(((RANDOM_LCG_ADVANCE()) >> 16) & 3)) {
                            sourceIndex = sourceGroup + (((RANDOM_LCG_ADVANCE()) >> 16) & 3);
                            effectSpawn(EFFECT_SHELTER_B1_STERILIZATION_PUFF, coord, sourceIndex, &_gShelterB1SterilizationRoomEffectGeometry.puffOrigins[sourceIndex]);
                        }
                    }
                }
                break;
            case 11:
                glowDrawDisc(&_gShelterB1SterilizationRoomEffectGeometry.singleDiscCenter, 0x300, 0x800);
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    for (directionGroup = 0; directionGroup < SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCE_COUNT; directionGroup += SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCES_PER_DIRECTION) {
                        for (sourceGroup = 4; sourceGroup < SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCES_PER_DIRECTION; sourceGroup += SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCES_PER_GROUP) {
                            if (!(((RANDOM_LCG_ADVANCE()) >> 16) & 3)) {
                                sourceIndex = directionGroup + sourceGroup + (((RANDOM_LCG_ADVANCE()) >> 16) & 3);
                                effectSpawn(EFFECT_SHELTER_B1_STERILIZATION_PUFF, coord, sourceIndex + SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_MEDIUM_SIZE_BIAS, &_gShelterB1SterilizationRoomEffectGeometry.puffOrigins[sourceIndex]);
                            }
                        }
                    }
                }
                break;
            case 10:
                glowDrawDisc(&_gShelterB1SterilizationRoomEffectGeometry.pairedDiscCenters[0], 0x300, 0x800);
                glowDrawDisc(&_gShelterB1SterilizationRoomEffectGeometry.pairedDiscCenters[1], 0x300, 0x800);
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    for (directionGroup = 0; directionGroup < SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCE_COUNT; directionGroup += SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCES_PER_DIRECTION) {
                        for (sourceGroup = 0; sourceGroup < 0xC; sourceGroup += SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCES_PER_GROUP) {
                            if (!(((RANDOM_LCG_ADVANCE()) >> 16) & 3)) {
                                sourceIndex = directionGroup + sourceGroup + (((RANDOM_LCG_ADVANCE()) >> 16) & 3);
                                effectSpawn(EFFECT_SHELTER_B1_STERILIZATION_PUFF, coord, sourceIndex + SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_MEDIUM_SIZE_BIAS, &_gShelterB1SterilizationRoomEffectGeometry.puffOrigins[sourceIndex]);
                            }
                        }
                    }
                }
                break;
            case 17:
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    for (sourceGroup = 0xC; sourceGroup < SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCE_COUNT; sourceGroup += SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCES_PER_DIRECTION) {
                        sourceIndex = sourceGroup + (((RANDOM_LCG_ADVANCE()) >> 16) & 3);
                        effectSpawn(EFFECT_SHELTER_B1_STERILIZATION_PUFF, coord, sourceIndex + SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_LARGE_SIZE_BIAS, &_gShelterB1SterilizationRoomEffectGeometry.puffOrigins[sourceIndex]);
                    }
                }
                break;
            case 18:
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    for (sourceGroup = 0xC; sourceGroup < SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCE_COUNT; sourceGroup += SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SOURCES_PER_DIRECTION) {
                        sourceIndex = sourceGroup + (((RANDOM_LCG_ADVANCE()) >> 16) & 3);
                        effectSpawn(EFFECT_SHELTER_B1_STERILIZATION_PUFF, coord, sourceIndex + SHELTER_B1_STERILIZATION_ROOM_EFFECTS_PUFF_SECOND_SIZE_BIAS, &_gShelterB1SterilizationRoomEffectGeometry.puffOrigins[sourceIndex]);
                    }
                }
                break;
        }
    }
#undef SHELTER_B1_STERILIZATION_ROOM_EMIT_SECOND_RING_SMOKE
#undef SHELTER_B1_STERILIZATION_ROOM_EMIT_PAIRED_RING_SMOKE
}

void shelterB1SterilizationRoomPuffTask(Task* task)
{
    enum {
        SHELTER_B1_STERILIZATION_ROOM_PUFF_INITIALIZE            = 0,
        SHELTER_B1_STERILIZATION_ROOM_PUFF_ANIMATE               = 1,
        SHELTER_B1_STERILIZATION_ROOM_PUFF_SOURCE_INDEX_MASK     = 0xFFF,
        SHELTER_B1_STERILIZATION_ROOM_PUFF_SOURCES_PER_DIRECTION = ARRAY_SIZE(_gShelterB1SterilizationRoomEffectGeometry.puffOrigins) / ARRAY_SIZE(_gShelterB1SterilizationRoomEffectGeometry.puffDirections),
        SHELTER_B1_STERILIZATION_ROOM_PUFF_FRAME_COUNT           = 10,
        SHELTER_B1_STERILIZATION_ROOM_PUFF_BASE_SIZE             = 0x180,
    };
    EffectWork* work;
    GfxCoord*   coord;
    SVECTOR*    velocity;
    s32         sizeBias;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    work->age++;
    switch (task->state) {
        case SHELTER_B1_STERILIZATION_ROOM_PUFF_INITIALIZE:
            // Choose the size, fixed sprite rotation and animation cadence once.
            sizeBias               = task->spawnArg1.halves.high;
            work->scale            = ((((RANDOM_LCG_ADVANCE()) >> 16) & 0xFF) + SHELTER_B1_STERILIZATION_ROOM_PUFF_BASE_SIZE) + sizeBias;
            work->angle            = ((RANDOM_LCG_ADVANCE()) >> 16) & (GLOW_FULL_TURN - 1);
            task->spawnArg1.value &= SHELTER_B1_STERILIZATION_ROOM_PUFF_SOURCE_INDEX_MASK;
            work->index            = (((RANDOM_LCG_ADVANCE()) >> 16) & 3) + 1;
            work->period           = (work->scale >> 5) + (((RANDOM_LCG_ADVANCE()) >> 16) & 0xF);
            work->step             = ((RANDOM_LCG_ADVANCE()) >> 16) & 0xF;
            // Scale the source group's Q12 direction, then jitter each component.
            gte_lddp(work->period);
            gte_ldsv(&_gShelterB1SterilizationRoomEffectGeometry.puffDirections[task->spawnArg1.value / SHELTER_B1_STERILIZATION_ROOM_PUFF_SOURCES_PER_DIRECTION]);
            gte_gpf12();
            velocity = &work->move;
            gte_stsv(velocity);
            work->move.vx -= (((RANDOM_LCG_ADVANCE()) >> 16) & 0xF) - 8;
            work->move.vy -= (((RANDOM_LCG_ADVANCE()) >> 16) & 0xF) - 8;
            work->move.vz -= (((RANDOM_LCG_ADVANCE()) >> 16) & 0xF) - 8;
            task->state    = SHELTER_B1_STERILIZATION_ROOM_PUFF_ANIMATE;
            // The first animation tick also runs on the initialization tick.
        case SHELTER_B1_STERILIZATION_ROOM_PUFF_ANIMATE:
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                work->scale        += work->step;
            } else {
                work->age--;
            }
            spriteQuadDraw(coord, (work->age - 1) / work->index,
                           work->scale, work->angle);
            if (work->index * SHELTER_B1_STERILIZATION_ROOM_PUFF_FRAME_COUNT - 1 < work->age) {
                effectKillTask(work, task);
            }
            break;
    }
}

/// Packed additive drifting-sprite page: 4-bit indexed texels at VRAM X=704 words, Y=0 scanlines.
#define SPRITE_QUAD_TEXTURE_PAGE getTPage(0, GPU_BLEND_ADD, 704, 0)
/// Drifting-sprite palette: VRAM X=256 words, Y=271 scanlines.
#define SPRITE_QUAD_CLUT getClut(256, 271)
/// Texel width and horizontal stride of each cell in the drifting sprite's five-column grid.
#define SPRITE_QUAD_CELL_WIDTH 48
/// Number of columns in the drifting sprite's grid of ten frames in two rows.
#define SPRITE_QUAD_CELLS_PER_ROW 5
/// Texel height and vertical stride of each cell in the two-row sprite grid.
///
/// Positive signed integer constant for the next drawer inclusion, with
/// SPRITE_QUAD_CELLS_PER_ROW. Frames 0..4 use the first row; 5..9 add 48 texels
/// to both V endpoints. The fragment clears this binding; see `sprite_quad.h`.
#define SPRITE_QUAD_CELL_HEIGHT 0x30
/// Inclusive top texel row of the grid's first row, relative to its texture page.
///
/// Keep this signed: adding the frame's row stride precedes narrowing to the
/// GPU byte. Frames 0..4 start at V=0x80; frames 5..9 start at V=0xB0.
/// The next drawer inclusion undefines this binding; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V (-0x80)
/// Inclusive bottom texel row of the puff grid's first row, before adding the frame's row stride.
///
/// Keep the endpoint signed: -0x51 becomes V=0xAF for frames 0..4 and
/// V=0xDF after the 48-texel row stride for frames 5..9. See `sprite_quad.h`.
#define SPRITE_QUAD_BOTTOM_V (SPRITE_QUAD_TOP_V + SPRITE_QUAD_CELL_HEIGHT - 1)
/// Perspective-sizing multiplier for the sterilization room's drifting sprite.
///
/// Uses the cell's inclusive 47-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
/// Uses the drifting puff's unmodified projected depth for sizing and sorting.
///
/// Zero disables the unit increment in the next sprite-quad drawer inclusion;
/// the fragment consumes and undefines this preprocessor switch.
#define SPRITE_QUAD_INCREMENT_DEPTH 0
#include "../../shared/effects/sprite_quad_draw.inc.c"

#include "../../shared/effects/glow_draw_capsule.inc.c"

#include "../../shared/effects/glow_draw_disc.inc.c"

#include "../../shared/effects/glow_draw_diamond.inc.c"

/// Initializes a cyan-centred Gouraud wedge with a black rim.
///
/// Sets packet length and command, and narrows green/blue to the intensity's
/// low byte. The caller supplies coordinates, linkage and additive blending.
static inline void _shelterB1SterilizationRoomInitGlowWedge(POLY_G4* wedge, s32 intensity)
{
    setPolyG4(wedge);
    setRGB0(wedge, 0, 0, 0);
    setRGB1(wedge, 0, 0, 0);
    setRGB2(wedge, 0, intensity, intensity);
    setRGB3(wedge, 0, 0, 0);
}

/// Draws a pulsing cyan disc with four alternating-length glow blades.
///
/// Borrows `worldPoint` in world coordinates for this call. `pulseRate` is in
/// 4096 angle units per animation frame; `radiusScale` gives a disc radius of
/// `radiusScale * 64 / depth` pixels, where depth is camera Z / 4. The rim is
/// black; green/blue intensity pulses around 120. Eight outer wedges have
/// brighter half-radius copies; four half-bright blades reach the outer radius
/// and twice it. Rejects negative GTE flags and requires nonzero accepted depth.
/// Needs a composed view matrix, 20 scratch bytes, and space for twenty Gouraud
/// quads plus additive blend packets in the current frame's GPU arena/table.
static void _shelterB1SterilizationRoomDrawPulsingGlow(const SVECTOR* worldPoint, s16 pulseRate, s16 radiusScale)
{
    RoomDiscScratch* scratch;
    POLY_G4*         wedge;
    s32              pulseSine;
    s32              intensity;
    s32              halfIntensity;
    s32              angle;
    s32              halfStepAngle;
    s32              nextAngle;
    s32              bladeAngle;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(RoomDiscScratch);

    // Project the centre once; screen radii use its camera depth.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->sx);
    gte_stflg(&scratch->flag);
    if (scratch->flag >= 0) {
        gte_stszotz(&scratch->otz);
        pulseSine            = rsin(gDisplayState.animFrame * pulseRate);
        angle                = 0;
        scratch->outerRadius = (radiusScale * GLOW_RADIUS_SCALE) / scratch->otz;
        intensity            = pulseSine / GLOW_PULSE_DIVISOR + GLOW_PULSE_BASE_INTENSITY;
        scratch->innerRadius = (radiusScale * GLOW_INNER_RADIUS_SCALE) / scratch->otz;
        // Pair each outer wedge with a brighter copy at half radius.
        do {
            wedge          = gGpuPrimCursor;
            gGpuPrimCursor = wedge + 1;
            setPolyG4(wedge);
            halfIntensity = (s16)intensity >> 1;
            setRGB0(wedge, 0, 0, 0);
            setRGB1(wedge, 0, 0, 0);
            setRGB2(wedge, 0, halfIntensity, halfIntensity);
            setRGB3(wedge, 0, 0, 0);
            wedge->x0     = scratch->sx + ((scratch->outerRadius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            halfStepAngle = angle + GLOW_SIXTEENTH_TURN;
            wedge->y0     = scratch->sy + ((scratch->outerRadius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            wedge->x1     = scratch->sx + ((scratch->outerRadius * rsin(halfStepAngle)) >> GLOW_TRIG_SHIFT);
            wedge->y1     = scratch->sy + ((scratch->outerRadius * rcos(halfStepAngle)) >> GLOW_TRIG_SHIFT);
            nextAngle     = angle + GLOW_EIGHTH_TURN;
            wedge->x2     = scratch->sx;
            wedge->y2     = scratch->sy;
            wedge->x3     = scratch->sx + ((scratch->outerRadius * rsin(nextAngle)) >> GLOW_TRIG_SHIFT);
            wedge->y3     = scratch->sy + ((scratch->outerRadius * rcos(nextAngle)) >> GLOW_TRIG_SHIFT);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz),
                    wedge);
            gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, scratch->otz);

            wedge          = gGpuPrimCursor;
            gGpuPrimCursor = wedge + 1;
            _shelterB1SterilizationRoomInitGlowWedge(wedge, intensity);
            wedge->x0 = scratch->sx + ((scratch->outerRadius * rsin(angle)) >> (GLOW_TRIG_SHIFT + 1));
            wedge->y0 = scratch->sy + ((scratch->outerRadius * rcos(angle)) >> (GLOW_TRIG_SHIFT + 1));
            wedge->x1 = scratch->sx + ((scratch->outerRadius * rsin(halfStepAngle)) >> (GLOW_TRIG_SHIFT + 1));
            wedge->y1 = scratch->sy + ((scratch->outerRadius * rcos(halfStepAngle)) >> (GLOW_TRIG_SHIFT + 1));
            wedge->x2 = scratch->sx;
            wedge->y2 = scratch->sy;
            wedge->x3 = scratch->sx + ((scratch->outerRadius * rsin(nextAngle)) >> (GLOW_TRIG_SHIFT + 1));
            wedge->y3 = scratch->sy + ((scratch->outerRadius * rcos(nextAngle)) >> (GLOW_TRIG_SHIFT + 1));
            angle     = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz),
                    wedge);
            gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, scratch->otz);
        } while (angle < GLOW_FULL_TURN);

        // Overlay four blades, alternating tips at one and two outer radii.
        intensity = halfIntensity;
        angle     = GLOW_EIGHTH_TURN;
        do {
            wedge          = gGpuPrimCursor;
            gGpuPrimCursor = wedge + 1;
            _shelterB1SterilizationRoomInitGlowWedge(wedge, intensity);
            bladeAngle = angle - GLOW_QUARTER_TURN;
            wedge->x0  = scratch->sx + ((scratch->innerRadius * rsin(bladeAngle)) >> (GLOW_TRIG_SHIFT + 1));
            wedge->y0  = scratch->sy + ((scratch->innerRadius * rcos(bladeAngle)) >> (GLOW_TRIG_SHIFT + 1));
            wedge->x1  = scratch->sx + ((scratch->outerRadius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            wedge->y1  = scratch->sy + ((scratch->outerRadius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            bladeAngle = angle + GLOW_QUARTER_TURN;
            wedge->x2  = scratch->sx;
            wedge->y2  = scratch->sy;
            wedge->x3  = scratch->sx + ((scratch->innerRadius * rsin(bladeAngle)) >> (GLOW_TRIG_SHIFT + 1));
            wedge->y3  = scratch->sy + ((scratch->innerRadius * rcos(bladeAngle)) >> (GLOW_TRIG_SHIFT + 1));
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz),
                    wedge);
            gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, scratch->otz);

            wedge          = gGpuPrimCursor;
            gGpuPrimCursor = wedge + 1;
            _shelterB1SterilizationRoomInitGlowWedge(wedge, intensity);
            wedge->x0  = scratch->sx + ((scratch->innerRadius * rsin(angle)) >> GLOW_TRIG_SHIFT);
            wedge->y0  = scratch->sy + ((scratch->innerRadius * rcos(angle)) >> GLOW_TRIG_SHIFT);
            wedge->x1  = scratch->sx + ((scratch->outerRadius * rsin(bladeAngle)) >> (GLOW_TRIG_SHIFT - 1));
            wedge->y1  = scratch->sy + ((scratch->outerRadius * rcos(bladeAngle)) >> (GLOW_TRIG_SHIFT - 1));
            bladeAngle = angle + GLOW_HALF_TURN;
            wedge->x2  = scratch->sx;
            wedge->y2  = scratch->sy;
            wedge->x3  = scratch->sx + ((scratch->innerRadius * rsin(bladeAngle)) >> GLOW_TRIG_SHIFT);
            wedge->y3  = scratch->sy + ((scratch->innerRadius * rcos(bladeAngle)) >> GLOW_TRIG_SHIFT);
            angle      = bladeAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz),
                    wedge);
            gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, scratch->otz);
        } while (angle < GLOW_FULL_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomDiscScratch);
}
