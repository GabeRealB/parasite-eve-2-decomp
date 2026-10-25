#include "helicopter_landing_pad_no_9_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/model_objects.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "stages/akropolis/rooms/helicopter_landing_pad.h"
#include "../../../../shared/actor/actor_messages.h"

static void _no9GolemAimHead(const Task* actor);
static void _no9GolemDrawShadow(const Task* task);

/// Body animation slots selected by the handlers below.
///
/// Event requests use their request index plus 0x1B. Slots 0x1C and 0x1D
/// leave the prop's previous visibility intact; 0x1E reveals it and 0x1F tosses it.
enum {
    HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE            = 1,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_WALK_START             = 2,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_WALK                   = 3,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_WALK_FIRE              = 4,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_BACKSTEP               = 5,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_CATCH_UP               = 6,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_LETHAL_STRIKE          = 7,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_SLASH_FIRST            = 8,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_SLASH_SECOND           = 9,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_SLASH_WINDUP           = 10,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_DASH_START             = 11,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_DASH_RUN               = 12,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_DASH_STRIKE            = 13,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_FLAME_WINDUP           = 14,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_FLAME_LUNGE            = 15,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_RECOIL                 = 16,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_FLINCH                 = 17,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_SPARK_RECOIL           = 18,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_STUN                   = 19,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_STUN_RECOVER           = 20,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_SHOTGUN_RESPONSE_START = 21,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_SHOTGUN_RESPONSE_WALK  = 22,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_SHOTGUN_RESPONSE_END   = 23,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_DEATH                  = 24,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_SLASH_RECOVER          = 25,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_FIGHT_START            = 26,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_GRENADE_THROW          = 27,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_EVENT_1                = 28,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_EVENT_2                = 29,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_EVENT_PROP_REVEAL      = 30,
    HELICOPTER_LANDING_PAD_NO_9_ANIM_EVENT_PROP_TOSS        = 31,
};

/// Steps within the selected combat state; values are stored as signed halfwords.
enum {
    HELICOPTER_LANDING_PAD_NO_9_OPENING_INTRO                 = 0,
    HELICOPTER_LANDING_PAD_NO_9_OPENING_WINDUP                = 1,
    HELICOPTER_LANDING_PAD_NO_9_OPENING_FIRST_SLASH           = 2,
    HELICOPTER_LANDING_PAD_NO_9_OPENING_SECOND_SLASH          = 3,
    HELICOPTER_LANDING_PAD_NO_9_OPENING_LUNGE                 = 4,
    HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT                   = 0,
    HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK_START             = 1,
    HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK                   = 2,
    HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK_FIRE              = 3,
    HELICOPTER_LANDING_PAD_NO_9_PATROL_CATCH_UP               = 4,
    HELICOPTER_LANDING_PAD_NO_9_PATROL_SHOTGUN_RESPONSE_START = 5,
    HELICOPTER_LANDING_PAD_NO_9_PATROL_SHOTGUN_RESPONSE_WALK  = 6,
    HELICOPTER_LANDING_PAD_NO_9_PATROL_SHOTGUN_RESPONSE_END   = 7,
    HELICOPTER_LANDING_PAD_NO_9_PATROL_BACKSTEP               = 8,
    HELICOPTER_LANDING_PAD_NO_9_SLASH_WINDUP                  = 0,
    HELICOPTER_LANDING_PAD_NO_9_SLASH_FIRST                   = 1,
    HELICOPTER_LANDING_PAD_NO_9_SLASH_SECOND                  = 2,
    HELICOPTER_LANDING_PAD_NO_9_SLASH_RECOVER                 = 3,
    HELICOPTER_LANDING_PAD_NO_9_FLAME_LUNGE_WINDUP            = 0,
    HELICOPTER_LANDING_PAD_NO_9_FLAME_LUNGE_CHARGE            = 1,
    HELICOPTER_LANDING_PAD_NO_9_DASH_START                    = 0,
    HELICOPTER_LANDING_PAD_NO_9_DASH_RUN                      = 1,
    HELICOPTER_LANDING_PAD_NO_9_DASH_STRIKE                   = 2,
    HELICOPTER_LANDING_PAD_NO_9_RECOIL_ENTER                  = 0,
    HELICOPTER_LANDING_PAD_NO_9_RECOIL_PLAY                   = 1,
    HELICOPTER_LANDING_PAD_NO_9_DEATH_ENTER                   = 0,
    HELICOPTER_LANDING_PAD_NO_9_SPARK_RECOIL_ENTER            = 0,
    HELICOPTER_LANDING_PAD_NO_9_SPARK_RECOIL_PLAY             = 1,
    HELICOPTER_LANDING_PAD_NO_9_SPARK_STUN_ENTER              = 0,
    HELICOPTER_LANDING_PAD_NO_9_SPARK_STUN_PLAY               = 1,
    HELICOPTER_LANDING_PAD_NO_9_SPARK_STUN_RECOVER            = 2,
    HELICOPTER_LANDING_PAD_NO_9_BUILDUP_STUN_WAIT             = 0,
    HELICOPTER_LANDING_PAD_NO_9_BUILDUP_STUN_RECOVER          = 1,
    HELICOPTER_LANDING_PAD_NO_9_FLINCH_ENTER                  = 0,
    HELICOPTER_LANDING_PAD_NO_9_FLINCH_PLAY                   = 1,
};

/// Attack-table indices packed into both weapon and forearm collision keys.
enum {
    HELICOPTER_LANDING_PAD_NO_9_ATTACK_DASH        = 0,
    HELICOPTER_LANDING_PAD_NO_9_ATTACK_FLAME_LUNGE = 1,
    HELICOPTER_LANDING_PAD_NO_9_ATTACK_SLASH       = 2,
    HELICOPTER_LANDING_PAD_NO_9_ATTACK_LETHAL      = 3,
    HELICOPTER_LANDING_PAD_NO_9_ATTACK_GRENADE     = 4,
    HELICOPTER_LANDING_PAD_NO_9_ATTACK_IGNITION    = 5,
};

/// Script keys before the placement index is packed into bits 8..11.
enum {
    HELICOPTER_LANDING_PAD_NO_9_SOUND_STEP_CUE_2       = 0x40780001,
    HELICOPTER_LANDING_PAD_NO_9_SOUND_STEP_CUE_1       = 0x40780002,
    HELICOPTER_LANDING_PAD_NO_9_SOUND_SHOTGUN_RESPONSE = 0x40780003,
    HELICOPTER_LANDING_PAD_NO_9_SOUND_RECOIL           = 0x40780005,
    HELICOPTER_LANDING_PAD_NO_9_SOUND_WINDUP           = 0x40780006,
    HELICOPTER_LANDING_PAD_NO_9_SOUND_FLAME_STRIKE     = 0x40780007,
    HELICOPTER_LANDING_PAD_NO_9_SOUND_BACKSTEP         = 0x40780008,
    HELICOPTER_LANDING_PAD_NO_9_SOUND_SPARK_LOOP       = 0x40780009,
    HELICOPTER_LANDING_PAD_NO_9_SOUND_SLASH_RECOVER    = 0x4078000A,
    HELICOPTER_LANDING_PAD_NO_9_SOUND_SLASH            = 0x4078000B,
    HELICOPTER_LANDING_PAD_NO_9_SOUND_DASH_STRIKE      = 0x4078000C,
    HELICOPTER_LANDING_PAD_NO_9_SOUND_FLAME_LOOP       = 0x4078000D,
    HELICOPTER_LANDING_PAD_NO_9_SOUND_FIRE             = 0x4078000E,
    HELICOPTER_LANDING_PAD_NO_9_SOUND_DASH_START       = 0x4078000F,
};

/// Frame thresholds shared by the spark and buildup reactions.
enum {
    HELICOPTER_LANDING_PAD_NO_9_SPARK_MIX_INTERVAL     = 3,
    HELICOPTER_LANDING_PAD_NO_9_STUN_RECOVER_END_FRAME = 59,
};

/// Lap and range thresholds in world units; speeds are world units per frame.
enum {
    HELICOPTER_LANDING_PAD_NO_9_LIGHT_0_SLASH_DISTANCE = 0x4B00,
    HELICOPTER_LANDING_PAD_NO_9_LIGHT_1_SLASH_DISTANCE = 0x7F00,
    HELICOPTER_LANDING_PAD_NO_9_LIGHT_SLASH_WINDOW     = 0x120,
    HELICOPTER_LANDING_PAD_NO_9_LAST_ATTACK_DISTANCE   = 0xAE9C,
    HELICOPTER_LANDING_PAD_NO_9_LETHAL_ATTACK_DISTANCE = 0xB477,
    HELICOPTER_LANDING_PAD_NO_9_PATROL_END_DISTANCE    = 0xB478,
    HELICOPTER_LANDING_PAD_NO_9_SLASH_RANGE            = 0xAF0,
    HELICOPTER_LANDING_PAD_NO_9_FLAME_LUNGE_RANGE      = 0xED8,
    HELICOPTER_LANDING_PAD_NO_9_MIXED_ATTACK_RANGE     = 0x12C0,
    HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK_SPEED      = 0x1D,
    HELICOPTER_LANDING_PAD_NO_9_DASH_SPEED             = 0x84,
    HELICOPTER_LANDING_PAD_NO_9_MAX_LUNGE_DISTANCE     = 0x1388,
    HELICOPTER_LANDING_PAD_NO_9_LAP_MIN_DISTANCE       = 0xC8,
    HELICOPTER_LANDING_PAD_NO_9_LAP_MAX_DISTANCE       = 0xB66C,
};

/// The latch's two positive values select helipad light 0 or 1 for a slash.
enum {
    HELICOPTER_LANDING_PAD_NO_9_LIGHT_SLASH_NONE    = 0,
    HELICOPTER_LANDING_PAD_NO_9_LIGHT_SLASH_LIGHT_0 = 1,
    HELICOPTER_LANDING_PAD_NO_9_LIGHT_SLASH_LIGHT_1 = 2,
};

/// The ammunition row interval that triggers the patrol's shotgun response.
enum {
    HELICOPTER_LANDING_PAD_NO_9_SHOTGUN_FIRST_AMMO_ROW = 10,
    HELICOPTER_LANDING_PAD_NO_9_SHOTGUN_AMMO_ROW_COUNT = 3,
};

/// The three attached models have initialization and update states only.
enum {
    HELICOPTER_LANDING_PAD_NO_9_ATTACHMENT_INIT        = 0,
    HELICOPTER_LANDING_PAD_NO_9_ATTACHMENT_UPDATE      = 1,
    HELICOPTER_LANDING_PAD_NO_9_ATTACHMENT_STATE_COUNT = 2,
    HELICOPTER_LANDING_PAD_NO_9_WEAPON_PART            = 8,
    HELICOPTER_LANDING_PAD_NO_9_CHEST_PART             = 3,
    HELICOPTER_LANDING_PAD_NO_9_PROP_HAND_PART         = 12,
};

/// Script message selecting this golem's show, fight-start or hide command.
enum { HELICOPTER_LANDING_PAD_NO_9_MESSAGE_SET_ACTIVATION = 2007 };

/// Task states of the grenade and the two stationary child hazards.
enum {
    HELICOPTER_LANDING_PAD_NO_9_CHILD_TASK_RUNNING = 1,
    HELICOPTER_LANDING_PAD_NO_9_GRENADE_TASK_BURST = 2,
};

/// Progress of the lethal attack; once struck, incoming damage cannot kill the golem.
enum {
    HELICOPTER_LANDING_PAD_NO_9_LETHAL_PHASE_NONE   = 0,
    HELICOPTER_LANDING_PAD_NO_9_LETHAL_PHASE_WINDUP = 1,
    HELICOPTER_LANDING_PAD_NO_9_LETHAL_PHASE_STRUCK = 2,
};

/// Hazard rows whose blast contacts this golem consumes.
enum {
    HELICOPTER_LANDING_PAD_NO_9_HAZARD_LIGHT_BLAST   = 2,
    HELICOPTER_LANDING_PAD_NO_9_HAZARD_SOURCE_RECOIL = 3,
    HELICOPTER_LANDING_PAD_NO_9_HAZARD_SOURCE_STUN   = 4,
};

/// States sent to the helipad light's adopted spark controller.
enum {
    HELICOPTER_LANDING_PAD_NO_9_LIGHT_SPARKS_BURSTS_ONLY = 1,
    HELICOPTER_LANDING_PAD_NO_9_LIGHT_SPARKS_RELEASE     = 2,
};

/// States sent to the blast source's adopted flare controller.
enum {
    HELICOPTER_LANDING_PAD_NO_9_SOURCE_FLARE_FLICKERING = 0,
    HELICOPTER_LANDING_PAD_NO_9_SOURCE_FLARE_RAW_COLOR  = 1,
    HELICOPTER_LANDING_PAD_NO_9_SOURCE_FLARE_EMBERS     = 2,
    HELICOPTER_LANDING_PAD_NO_9_SOURCE_FLARE_RELEASE    = 3,
};

static s32 _helicopterLandingPadNo9SetActivation(Task* task, s32 messageId, s32 command, s32 unusedArg);

static s32 _helicopterLandingPadNo9ReleaseGrenadeHold(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);

static s32 _helicopterLandingPadNo9PlayEventAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedSecondArg);

static s32 _helicopterLandingPadNo9SetModelDraw(Task* task, s32 messageId, s32 drawEnabled, s32 unusedSecondArg);

static s32 _helicopterLandingPadNo9IsPresent(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);

enum {
    HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_COUNT     = 3,
    HELICOPTER_LANDING_PAD_NO_9_HELIPAD_VIEWS_PER_LIGHT = 3,
};

/// Mapped camera-view IDs that enable each helipad light's active update.
///
/// Rows belong to light indices 0..2. A match with any entry enables the light;
/// repeated IDs are allowed. The blast source shares the first entry of light 2.
/// The table remains live for the actor overlay lifetime. The final two stored
/// bytes have no observed access and their role is unproven.
typedef struct {
    u16  lightViews[HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_COUNT][HELICOPTER_LANDING_PAD_NO_9_HELIPAD_VIEWS_PER_LIGHT]; // Mapped camera-view IDs per light; compared with the current view's low byte
    byte field_12[2];                                                                                                      // Unaccessed zero bytes; original type and role unproven
} _HelicopterLandingPadNo9HelipadViews;
STATIC_ASSERT_SIZEOF(_HelicopterLandingPadNo9HelipadViews, 0x14);

/// Scratch block of the step that carries the golem's three wall faces in the
/// room's collision grid to where the golem stands.
///
/// The faces are stored as twelve corners around an origin of their own, which
/// lies at a fixed offset from the golem in the golem's axes. The step turns
/// that offset by the golem's facing to place the origin in the world, then
/// turns each corner the same way and adds the origin to it. Reserve one
/// complete block and release it when the step is done. The fourth halfword of
/// either vector is never written.
typedef struct {
    SVECTOR origin;  // The faces' origin: first its offset from the golem in the golem's axes, then its world position, held within 0x1770 of the world origin in X and Z
    SVECTOR rotated; // Latest vector turned by the golem's facing: the origin's offset, then each corner in turn
} _HelicopterLandingPadNo9GridFacesScratch;
STATIC_ASSERT_SIZEOF(_HelicopterLandingPadNo9GridFacesScratch, 0x10);

/// Values of `_HelicopterLandingPadNo9HelipadLightWork::state`. Light 2 also copies its
/// state to `HelicopterLandingPadNo9Work::light2State` every frame.
enum {
    HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_INTACT   = 0, // Whole: a shot or the golem's slash breaks it
    HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_SPARKING = 1, // Broken and throwing sparks; its blast can strike once
    HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_DONE     = 2, // The sparks have run out, or the golem is gone
};

/// Values of `_HelicopterLandingPadNo9HelipadLightWork::status`.
enum {
    HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_STATUS_INTACT   = 0, // Not broken yet
    HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_STATUS_SPARKING = 1, // Broken, sparks still running
    HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_STATUS_SPENT    = 2, // Broken, sparks over
    HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_STATUS_STOPPED  = 3, // Never broken: the golem was gone first
};

/// Work block of one of the three helipad lights, kept at `Task::work` of the
/// light model's task.
///
/// A light stands intact until the player shoots it or the golem's slash
/// reaches it. It then breaks: the model changes to its second animation, a
/// spark effect starts, and for as long as the sparks last a blast sphere
/// around the light can strike the golem, which drives it back in a shower of
/// sparks. The blast is switched off at its first contact. A light runs only
/// while the camera is in one of its three views.
///
/// The block is cleared at allocation.
typedef struct {
    AnimationContext      anim;                                   // Context bound to `slots`, `poses` and the model's part coordinates
    AnimationSlot         slots[11];                              // Playback state of the model part at the same index; slots 1..10 are driven
    u8                    poses[11][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose of the slot at the same index
    MATRIX                colorMtx;                               // Colour matrix lent to the light's model
    MATRIX                lightMtx;                               // Light matrix lent to the same model
    WorldCollisionBody    body;                                   // Sphere of radius 0xC8 on model part 10, on list 2, receiving attacks; enabled only while the light is intact and in view
    WorldCollisionContact bodyContacts[1];                        // Contact of `body`, read and cleared every frame the light is intact
    WorldCollisionBody    blast;                                  // Sphere of radius 0x15E on the same part, on list 8, keyed 0x50002; enabled from the break until its first contact or the end of the sparks
    WorldCollisionContact blastContacts[1];                       // Contact of `blast`
    Task*                 sparksTask;                             // Spark effect started at the break and adopted as a child of the light's task; `NULL` before the break, if it did not spawn, or once it has been told to stop
    s16                   state;                                  // `HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_*`
    s16                   sparkFrames;                            // Frames of sparks left, 0x78 at the break; a frame the camera spends in another view also takes one off
    s16                   lightIndex;                             // Which of the three lights this is (0..2), from the spawn argument; picks its place, facing and views
    s16                   status;                                 // `HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_STATUS_*`, published every frame in view: by lights 0 and 1 to game-flag nibbles 0xB and 0xC, by light 2 to `HelicopterLandingPadNo9Work::light2Status`
} _HelicopterLandingPadNo9HelipadLightWork;
STATIC_ASSERT_SIZEOF(_HelicopterLandingPadNo9HelipadLightWork, 0x338);

/// Values of `_HelicopterLandingPadNo9GrenadeWork::phase` while the grenade flies.
enum {
    HELICOPTER_LANDING_PAD_NO_9_GRENADE_FLIGHT_BELOW_PEAK = 0, // Has not been above height 0x514 yet
    HELICOPTER_LANDING_PAD_NO_9_GRENADE_FLIGHT_PEAKED     = 1, // Has been above it: dropping back below bursts the grenade
};

/// Values of `_HelicopterLandingPadNo9GrenadeWork::phase` once the grenade has burst.
enum {
    HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_SPREADING = 0, // 0x10 frames with the attack sphere off, then it widens to radius 0x258
    HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_CATCHING  = 1, // Up to 0x1F frames in which the widened sphere can catch the player
    HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_HOLDING   = 2, // The player is caught: `holdStep` runs
    HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_ENDING    = 3, // 0x1F frames, then the grenade's task ends
};

/// Values of `_HelicopterLandingPadNo9GrenadeWork::holdStep`.
enum {
    HELICOPTER_LANDING_PAD_NO_9_GRENADE_HOLD_REQUEST = 0, // Asks the player for the hold; on acceptance deals the damage and starts the stunned animation
    HELICOPTER_LANDING_PAD_NO_9_GRENADE_HOLD_STUNNED = 1, // Until the player has escaped, 0x3C frames have passed or the golem is gone; then starts the recovery animation
    HELICOPTER_LANDING_PAD_NO_9_GRENADE_HOLD_RECOVER = 2, // 0x14 frames, then waits for the animation to end and releases the player
};

/// Work block of the golem's stun grenade, kept at `Task::work` of the
/// grenade model's task.
///
/// The grenade leaves the golem's chest pitching forward a fixed step every
/// frame while it moves along its own axis, so the step sets how far the arc
/// carries; it is chosen from the distance to the player at the throw. It
/// bursts when it comes back down through height 0x514, touches the player or
/// meets the room's collision grid. The burst then spreads into a wider sphere
/// that holds a player it catches until they have pressed enough buttons, the
/// hold times out or the golem is gone.
///
/// The block is cleared at allocation.
typedef struct {
    MATRIX                colorMtx;             // Colour matrix lent to the grenade's model
    MATRIX                lightMtx;             // Light matrix lent to the same model
    WorldCollisionBody    attack;               // Sphere on list 3 at the grenade: radius 0xC8 in flight, 0x258 once the burst has spread; a player contact bursts the grenade or starts the hold
    WorldCollisionContact attackContacts[1];    // Contact of `attack`
    WorldCollisionBody    gridProbe;            // Capsule on list 3 tested against the room's collision grid only; unlinked at the burst
    WorldCollisionCapsule gridProbeCapsule;     // Shape of `gridProbe`: 0x1F4 along the grenade's Y axis from its origin, radius 1
    WorldCollisionContact gridProbeContacts[1]; // Contact of `gridProbe`; any contact bursts the grenade
    u16                   frames;               // Frames counted in the current phase; in flight, frames since the last smoke puff of the trail (one every third frame)
    s16                   phase;                // `HELICOPTER_LANDING_PAD_NO_9_GRENADE_FLIGHT_*` in flight, `HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_*` after the burst
    s16                   holdStep;             // `HELICOPTER_LANDING_PAD_NO_9_GRENADE_HOLD_*`
    s16                   phaseCounter;         // In flight, the pitch added each frame (4096 a turn); during the hold, frames spent in `holdStep`
} _HelicopterLandingPadNo9GrenadeWork;
STATIC_ASSERT_SIZEOF(_HelicopterLandingPadNo9GrenadeWork, 0xD0);

/// Values of `_HelicopterLandingPadNo9BlastSourceWork::state`.
enum {
    HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_DORMANT = 0, // Waiting for helipad light 2 to break
    HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_ARMED   = 1, // Can be shot
    HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_SHOT    = 2, // The frame after the shot: the flare is given its starting state
    HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_FLARING = 3, // `flareFrames` runs down
    HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_SPENT   = 4, // The flare has been told to end
    HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_STOPPED = 5, // The golem is gone and the flare released
};

/// Work block of the blast source beside helipad light 2, kept at
/// `Task::work` of a task that has a coordinate and no model.
///
/// The source lies dormant until light 2 breaks. A shot then sets it off: a
/// flare starts, and for as long as it burns a blast sphere can strike the
/// golem once - stunning it in a shower of sparks while light 2 is still
/// sparking, driving it back along the lap once the light is spent. The
/// source runs in one view only and gives off embers there the whole time.
/// What the source is in the scene is unproven.
///
/// The block is cleared at allocation. No access to `pad_7A` has been
/// observed; whether it is a member or tail padding is unproven.
typedef struct {
    WorldCollisionBody    body;             // Sphere of radius 0x12C at the source on list 2, receiving attacks; enabled only while armed and in view
    WorldCollisionContact bodyContacts[1];  // Contact of `body`, read and cleared every frame the source is armed
    WorldCollisionBody    blast;            // Sphere of radius 0x200 raised 0x200 above the source, on list 8; keyed 0x50003 or 0x50004 by the shot and enabled from it until its first contact or the end of the flare
    WorldCollisionContact blastContacts[1]; // Contact of `blast`
    Task*                 flareTask;        // Flare effect started by the shot and adopted as a child of the source's task; `NULL` before the shot, if it did not spawn, or once it has been released
    s16                   state;            // `HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_*`
    s16                   flareFrames;      // Frames the flare still burns, 0x3C at the shot; a frame the camera spends in another view also takes one off
    s16                   flareTaskState;   // `Task::state` the flare starts in, chosen at the shot (1 if light 2 was already spent, 0 if it was still sparking)
    byte                  pad_7A[0x2];
} _HelicopterLandingPadNo9BlastSourceWork;
STATIC_ASSERT_SIZEOF(_HelicopterLandingPadNo9BlastSourceWork, 0x7C);

/// One corner of the square lap the golem walks, in world X and Z. A side of
/// the lap starts at its corner.
typedef struct {
    s16 x; // World X of the corner
    s16 z; // World Z of the corner
} _HelicopterLandingPadNo9LapCorner;
STATIC_ASSERT_SIZEOF(_HelicopterLandingPadNo9LapCorner, 0x4);

/// The direction one side of the lap runs in, from its corner to the next:
/// the world X and Z the golem covers per unit of distance along the side.
/// Every side is parallel to an axis, so one member is 1 or -1 and the other 0.
typedef struct {
    s8 x; // World X covered per unit of distance along the side
    s8 z; // World Z covered per unit of distance along the side
} _HelicopterLandingPadNo9LapDirection;
STATIC_ASSERT_SIZEOF(_HelicopterLandingPadNo9LapDirection, 0x2);

/// The strip of the landing pad that runs along one side of the lap, as a
/// world-space rectangle; all four edges are exclusive.
///
/// The golem finds the side the player is on by testing the player's position
/// against the four strips in order. The strips overlap at the pad's corners,
/// where the first one that holds the player counts.
typedef struct {
    s16 minX; // Edge of the strip towards negative X
    s16 maxX; // Edge towards positive X
    s16 minZ; // Edge towards negative Z
    s16 maxZ; // Edge towards positive Z
} _HelicopterLandingPadNo9LapStrip;
STATIC_ASSERT_SIZEOF(_HelicopterLandingPadNo9LapStrip, 0x8);

/// The three face normals `helicopterLandingPadNo9RestoreGridFaces` copies into
/// `gWorldCollisionActiveGrid->normals`, restoring the collision grid this actor edited.
static SVECTOR _gHelicopterLandingPadNo9Collision35DA4[3];

/// The twelve face corners `helicopterLandingPadNo9RestoreGridFaces` copies into
/// `gWorldCollisionActiveGrid->vertices`.
static SVECTOR _gHelicopterLandingPadNo9Collision35DBC[12];

/// The three `WorldCollisionGridFace` records `helicopterLandingPadNo9RestoreGridFaces` copies into
/// `gWorldCollisionActiveGrid->faces`.
static WorldCollisionGridFace _gHelicopterLandingPadNo9Collision35E1C[3];

static s32  _helicopterLandingPadNo9TrySelectAttack(Task* task);
static void _helicopterLandingPadNo9UpdatePlayerRange(Task* task);
static void _helicopterLandingPadNo9TurnAlongLap(Task* task);
static void _helicopterLandingPadNo9AdvanceAlongLap(Task* task);
static void _helicopterLandingPadNo9PlayStepSounds(Task* task);
static void _helicopterLandingPadNo9ApplyHitTwist(Task* task);
static void _helicopterLandingPadNo9UpdateGridFaces(Task* task);
static void _helicopterLandingPadNo9TickCombat(Enemy* enemy, Task* task);
static void _helicopterLandingPadNo9ConsumeReactionFlags(Task* task);
static void _helicopterLandingPadNo9TickCombatState(Task* task);
static void _helicopterLandingPadNo9TickBuildupStun(Task* task);
static void _helicopterLandingPadNo9TickFlinch(Task* task);
static void _helicopterLandingPadNo9UpdateBodyAnimation(Task* task);
static void _helicopterLandingPadNo9UpdateFlameJet(Task* task);

static void _helicopterLandingPadNo9ExitHelipadLight(Task* task);
static void _helicopterLandingPadNo9ExitBlastSource(Task* task);

static void _helicopterLandingPadNo9BodyTask(Task* task);
static void _helicopterLandingPadNo9PropTask(Task* task);
static void _helicopterLandingPadNo9WeaponTask(Task* task);
static void _helicopterLandingPadNo9ChestModelTask(Task* task);
static void _helicopterLandingPadNo9GrenadeTask(Task* task);
static void _helicopterLandingPadNo9HelipadLightTask(Task* task);
static void _helicopterLandingPadNo9BlastSourceTask(Task* task);

EnemyParams gHelicopterLandingPadNo9EnemyParams = { gHelicopterLandingPadNo9Attacks, 1600, 500, 800, 30, 50, 3, 0, 0 };

/// Walk distances in world units, sampled with bits 16..19 of a fresh LCG draw.
///
/// Normal and shotgun-response walks subtract 29 world units per frame.
static s16 _gHelicopterLandingPadNo9PatrolWalkDistances[16] = {
    2000,
    2100,
    2200,
    2300,
    2400,
    2500,
    2600,
    2700,
    2800,
    2900,
    3000,
    3200,
    3400,
    3600,
    3800,
    4000,
};

/// Patrol waiting times in frames after a walk, indexed by LCG bits 16..19.
static s16 _gHelicopterLandingPadNo9PatrolWaitFrames[16] = {
    14,
    16,
    18,
    20,
    22,
    24,
    26,
    28,
    30,
    32,
    35,
    40,
    45,
    50,
    55,
    60,
};

/// Patrol waiting times in frames after a missed or completed attack.
///
/// Slash, lunge, grenade, dash and lethal-strike exits sample LCG bits 16..19.
static u16 _gHelicopterLandingPadNo9PostAttackWaitFrames[16] = {
    5,
    5,
    5,
    5,
    10,
    10,
    10,
    10,
    15,
    15,
    15,
    15,
    20,
    20,
    30,
    30,
};

/// Patrol waiting times in frames after a damage reaction, sampled by LCG bits 16..19.
///
/// Also used when simultaneous player death cancels the golem's death state.
static u16 _gHelicopterLandingPadNo9ReactionWaitFrames[16] = {
    15,
    15,
    15,
    15,
    25,
    25,
    25,
    25,
    35,
    35,
    35,
    35,
    45,
    45,
    45,
    45,
};

/// Slash continuation thresholds indexed by the completed-stroke counter 0..3.
///
/// A fresh four-bit random sample strictly greater than the entry ends the
/// sequence. The second-stroke exit also stops at the final entry regardless
/// of the sample; entry 3 is not followed by another first stroke.
static s16 _gHelicopterLandingPadNo9SlashContinuationThresholds[4] = {
    14,
    12,
    10,
    0,
};

TaskDesc gHelicopterLandingPadNo9TaskDescriptors[7] = {
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _helicopterLandingPadNo9BodyTask, { .model = &gGolemNo9BodyAkropolis } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _helicopterLandingPadNo9PropTask, { .model = &gGolemNo9AkropolisProp } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _helicopterLandingPadNo9WeaponTask, { .model = &gGolemNo9AkropolisSword } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _helicopterLandingPadNo9ChestModelTask, { .model = &gGolemNo9AkropolisChestPiece } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _helicopterLandingPadNo9GrenadeTask, { .model = &gGolemGrenade } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _helicopterLandingPadNo9HelipadLightTask, { .model = &gHelicopterLandingPadNo9Model10C8C } },
    { { { TASK_BODY_COORD, TASK_PRIORITY_ACTOR } }, _helicopterLandingPadNo9BlastSourceTask, { .value = 0 } },
};

TaskMessageEntry gHelicopterLandingPadNo9BodyMessages[7] = {
    { ACTOR_MESSAGE_RELEASE_HOLD, _helicopterLandingPadNo9ReleaseGrenadeHold },
    { ACTOR_MESSAGE_PLAY_ANIMATION, _helicopterLandingPadNo9PlayEventAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRotMatrix },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _helicopterLandingPadNo9SetModelDraw },
    { HELICOPTER_LANDING_PAD_NO_9_MESSAGE_SET_ACTIVATION, _helicopterLandingPadNo9SetActivation },
    { ACTOR_MESSAGE_IS_PRESENT, _helicopterLandingPadNo9IsPresent },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationSet* gHelicopterLandingPadNo9BodyAnimationSets[34] = {
    NULL,
    &_gHelicopterLandingPadNo9Animation11CC0,
    &_gHelicopterLandingPadNo9Animation11FB8,
    &_gHelicopterLandingPadNo9Animation129B0,
    &_gHelicopterLandingPadNo9Animation13884,
    &_gHelicopterLandingPadNo9Animation142A0,
    &_gHelicopterLandingPadNo9Animation14E4C,
    &_gHelicopterLandingPadNo9Animation172DC,
    &_gHelicopterLandingPadNo9Animation17CB8,
    &_gHelicopterLandingPadNo9Animation186E0,
    &_gHelicopterLandingPadNo9Animation18B5C,
    &_gHelicopterLandingPadNo9Animation1A250,
    &_gHelicopterLandingPadNo9Animation1A994,
    &_gHelicopterLandingPadNo9Animation1BFA8,
    &_gHelicopterLandingPadNo9Animation1CBA8,
    &_gHelicopterLandingPadNo9Animation1D868,
    &_gHelicopterLandingPadNo9Animation1E56C,
    &_gHelicopterLandingPadNo9Animation1EF7C,
    &_gHelicopterLandingPadNo9Animation2035C,
    &_gHelicopterLandingPadNo9Animation2093C,
    &_gHelicopterLandingPadNo9Animation213E4,
    &_gHelicopterLandingPadNo9Animation217A4,
    &_gHelicopterLandingPadNo9Animation22074,
    &_gHelicopterLandingPadNo9Animation223F8,
    &_gHelicopterLandingPadNo9Animation24128,
    &_gHelicopterLandingPadNo9Animation24FE8,
    &_gHelicopterLandingPadNo9Animation267A8,
    &_gHelicopterLandingPadNo9Animation273B8,
    &_gHelicopterLandingPadNo9Animation2A068,
    &_gHelicopterLandingPadNo9Animation2B800,
    &_gHelicopterLandingPadNo9Animation2E9DC,
    &_gHelicopterLandingPadNo9Animation2BC04,
    &_gHelicopterLandingPadNo9Animation324C0,
    &_gHelicopterLandingPadNo9Animation353AC,
};

/// Player clips borrowed during the stun grenade's hold: 0 unused, 1 stunned, 2 recovery.
///
/// The player retains this bank for playback; keep this overlay loaded until
/// the hold returns the player to ordinary control.
static AnimationSet* _gHelicopterLandingPadNo9PlayerGrenadeAnimationSets[3] = {
    NULL,
    &gHelicopterLandingPadNo9Animation27994,
    &gHelicopterLandingPadNo9Animation27FDC,
};

/// Whole-frame transition durations indexed by the body's animation ID 0..33.
///
/// Uses the same slot order as `gHelicopterLandingPadNo9BodyAnimationSets`; zero starts
/// without a transition pose.
static s16 _gHelicopterLandingPadNo9BodyAnimationBlendFrames[34] = {
    0,
    8,
    8,
    4,
    4,
    0,
    8,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    4,
    3,
    3,
    3,
    3,
    0,
    8,
    4,
    4,
    8,
    8,
    8,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

/// Reusable splatter-spray argument for the lethal strike's repeated player hit effect.
///
/// The player root coordinate is refreshed before each spawn and borrowed by
/// the effects. Low half 300 sets initial speed to floor(300 * 3 / 16);
/// high half 1 gives four emission frames. The record itself is not retained.
static EffectSpawnArg _gHelicopterLandingPadNo9LethalHitEffectArg = { NULL, 300, 1 };

/// World X/Z origins of lap sides 0..3, in walking order.
static _HelicopterLandingPadNo9LapCorner _gHelicopterLandingPadNo9LapCorners[4] = {
    { -6600, -6600 },
    { 6600, -6600 },
    { 6600, 6600 },
    { -6600, 6600 },
};

/// Axis-aligned unit X/Z directions from each lap corner to the next.
static _HelicopterLandingPadNo9LapDirection _gHelicopterLandingPadNo9LapDirections[4] = {
    { 1, 0 },
    { 0, 1 },
    { -1, 0 },
    { 0, -1 },
};

/// Facing angles of lap sides 0..3, in units of 4096 per turn.
///
/// A moving golem targets the next entry within 1000 world units of a corner.
/// On side 3 the terminal distance clamp leaves at least 6100 units, so that
/// lookahead never indexes past the four headings.
static u16 _gHelicopterLandingPadNo9LapYaws[4] = {
    ACTOR_TRANSFORM_ANGLE_TURN / 4,
    0,
    ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4,
    ACTOR_TRANSFORM_ANGLE_HALF_TURN,
};

/// Exclusive world X/Z rectangles assigning the player to lap sides 0..3.
///
/// The first containing strip wins where rectangles overlap. Outside them
/// all, the player's preceding side is retained.
static _HelicopterLandingPadNo9LapStrip _gHelicopterLandingPadNo9LapStrips[4] = {
    { -7200, 7200, -7200, -6000 },
    { 6000, 7200, -6000, 7200 },
    { -7200, 6000, 6000, 7200 },
    { -7200, -4608, -6000, 6000 },
};

static SVECTOR _gHelicopterLandingPadNo9Collision35DA4[3] = {
#include "assets/helicopter_landing_pad_no_9_collision_35DA4.inc"
};

static SVECTOR _gHelicopterLandingPadNo9Collision35DBC[12] = {
#include "assets/helicopter_landing_pad_no_9_collision_35DBC.inc"
};

static WorldCollisionGridFace _gHelicopterLandingPadNo9Collision35E1C[3] = {
#include "assets/helicopter_landing_pad_no_9_collision_35E1C.inc"
};

/// Q12 normal of the extra landing-pad collision face, installed at normal index 3.
static SVECTOR _gHelicopterLandingPadNo9ExtraGridNormals[1] = {
#include "assets/helicopter_landing_pad_no_9_collision_35E40.inc"
};

/// Four world-coordinate vertices of the extra landing-pad face, installed at indices 12..15.
static SVECTOR _gHelicopterLandingPadNo9ExtraGridVertices[4] = {
#include "assets/helicopter_landing_pad_no_9_collision_35E48.inc"
};

/// Extra landing-pad face linking vertices 12..15 to normal 3.
///
/// Enabling copies this record; disabling collapses its vertices and normal
/// without changing these indices.
static WorldCollisionGridFace _gHelicopterLandingPadNo9ExtraGridFaces[1] = {
#include "assets/helicopter_landing_pad_no_9_collision_35E68.inc"
};

/// Grenade pitch-step magnitudes per flight frame, in angle units of 4096 per turn.
///
/// Index by horizontal launch-to-player distance divided by 1000 world units;
/// distances of 11000 or more use the final entry of this twelve-entry bank.
/// Flight rotates the launch basis by the negative of the selected step.
static u16 _gHelicopterLandingPadNo9GrenadePitchStepsByRange[12] = {
    25,
    20,
    15,
    12,
    10,
    9,
    8,
    7,
    6,
    5,
    5,
    5,
};

/// Helipad-light animation bank: 0 unused, 1 intact, 2 breaking.
///
/// Light-owned animation contexts borrow the bank for the light task's lifetime.
static AnimationSet* _gHelicopterLandingPadNo9HelipadLightAnimationSets[3] = {
    NULL,
    &gHelicopterLandingPadNo9Animation35474,
    &gHelicopterLandingPadNo9Animation35B20,
};

/// Root positions of helipad lights 0..2, in room-coordinate units beneath `gGfxViewCoord`.
static SVECTOR _gHelicopterLandingPadNo9HelipadLightPositions[3] = {
    { 6458, -2658, 535, 0 },
    { -1336, -2658, 6552, 0 },
    { -6441, -2658, -100, 0 },
};

/// Root Y rotations of helipad lights 0..2, in angle units of 4096 per turn.
///
/// The fourth stored entry is unread; its role is unproven.
static u16 _gHelicopterLandingPadNo9HelipadLightYaws[4] = {
    ACTOR_TRANSFORM_ANGLE_HALF_TURN,
    ACTOR_TRANSFORM_ANGLE_TURN / 4,
    0,
    0,
};

/// Mapped-view rows shared by the three helipad lights and the light-2 blast source.
static _HelicopterLandingPadNo9HelipadViews _gHelicopterLandingPadNo9HelipadViews = {
    { { 5, 6, 7 }, { 7, 8, 9 }, { 10, 10, 10 } },
    { 0 },
};

/// Status encodings emitted by the source/light pair; the flag's reader is unproven.
enum {
    HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_INTACT   = 0,
    HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_SPARKING = 1,
    HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_SPENT    = 2,
    HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_SOURCE_FLARING = 4,
    HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_SOURCE_SPENT   = 5,
    HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_STOPPED  = 6,
};

/// Game-flag nibble values combining blast-source phase with light-2 status.
///
/// Rows 0..5 are `HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_*`; columns are intact, sparking,
/// spent and stopped. Row 6 is stored but has no observed state selecting it.
/// The consumer of the published flag and the unused row's role are unproven.
static u16 _gHelicopterLandingPadNo9HelipadStatusFlagValues[7][4] = {
    { HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_INTACT, HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_SPARKING, HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_SPENT, HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_STOPPED },
    { HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_INTACT, HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_SPARKING, HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_SPENT, HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_STOPPED },
    { HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_INTACT, HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_SOURCE_FLARING, HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_SOURCE_FLARING, HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_STOPPED },
    { HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_INTACT, HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_SOURCE_FLARING, HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_SOURCE_FLARING, HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_STOPPED },
    { HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_INTACT, HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_SOURCE_SPENT, HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_SOURCE_SPENT, HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_STOPPED },
    { HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_INTACT, HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_SOURCE_SPENT, HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_SOURCE_SPENT, HELICOPTER_LANDING_PAD_NO_9_HELIPAD_FLAG_LIGHT_STOPPED },
    { 0, 0, 0, 0 },
};

static void _helicopterLandingPadNo9TickGrenadeHold(Task* task);

static void _helicopterLandingPadNo9TickHelipadLightBreak(Task* task);

static s32 _helicopterLandingPadNo9CheckHelipadLightView(Task* task);

static void _helicopterLandingPadNo9UpdateHelipadLightLighting(Task* task, const GfxCoord* rootCoord);

static void _helicopterLandingPadNo9TickBlastSourcePhases(Task* task);

static void _helicopterLandingPadNo9TickBody(Enemy* enemy, Task* task);

static void _helicopterLandingPadNo9InitProp(Enemy* enemy, Task* task);

static void _helicopterLandingPadNo9InitWeapon(Enemy* enemy, Task* task);

static void _helicopterLandingPadNo9UpdateWeapon(Enemy* enemy, Task* task);

static void _helicopterLandingPadNo9InitChestModel(Enemy* enemy, Task* task);

static void _helicopterLandingPadNo9UpdateChestModel(Enemy* enemy, Task* task);

static void _helicopterLandingPadNo9ResolveBodyContacts(Task* task);
static void _helicopterLandingPadNo9TickOpening(Task* task);
static void _helicopterLandingPadNo9TickPatrol(Task* task);
static void _helicopterLandingPadNo9TickSlash(Task* task);
static void _helicopterLandingPadNo9TickFlameLunge(Task* task);
static void _helicopterLandingPadNo9TickGrenadeThrow(Task* task);
static void _helicopterLandingPadNo9TickDash(Task* task);
static void _helicopterLandingPadNo9TickLethalAttack(Task* task);
static void _helicopterLandingPadNo9TickRecoil(Task* task);
static void _helicopterLandingPadNo9TickSparkRecoil(Task* task);
static void _helicopterLandingPadNo9TickSparkStun(Task* task);
static void _helicopterLandingPadNo9TickDeath(Task* task);
static void _helicopterLandingPadNo9UpdateProp(Enemy* enemy, Task* task);
static void _helicopterLandingPadNo9InitGrenade(Enemy* enemy, Task* task);
static void _helicopterLandingPadNo9TickGrenadeFlight(Enemy* enemy, Task* task);
static void _helicopterLandingPadNo9TickGrenadeBurst(Enemy* enemy, Task* task);
static void _helicopterLandingPadNo9InitHelipadLight(Enemy* enemy, Task* task);
static void _helicopterLandingPadNo9TickHelipadLight(Enemy* enemy, Task* task);
static void _helicopterLandingPadNo9InitBlastSource(Enemy* enemy, Task* task);
static void _helicopterLandingPadNo9TickBlastSource(Enemy* enemy, Task* task);

/// Enables the weapon and forearm attack spheres with the same damage-table key.
///
/// `work` must be the live body work; `attackIndex` is 0..5 in this
/// golem's attack table. Neither sphere's contacts are cleared here.
static inline void _helicopterLandingPadNo9ArmAttack(HelicopterLandingPadNo9Work* work, s32 attackIndex)
{
    s32 attackKey;

    work->weaponAttack.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->forearmAttack.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    attackKey                  = damagePackAttackKey(gHelicopterLandingPadNo9Attacks, attackIndex);
    work->weaponAttack.key     = attackKey;
    work->forearmAttack.key    = attackKey;
}

/// Attaches the prop at the off-hand origin with identity local rotation.
///
/// Both coordinates must be live; the hand remains the prop's borrowed parent
/// until it is tossed or reattached. Composition is invalidated by the caller.
static inline void _helicopterLandingPadNo9AttachProp(GfxCoord* propCoord, GfxCoord* handCoord)
{
    gfxSetRotIdentity(&propCoord->coord);
    propCoord->coord.t[0] = 0;
    propCoord->coord.t[1] = 0;
    propCoord->coord.t[2] = 0;
    propCoord->parent     = handCoord;
}

/// Spawns a flash burst along the body's negative Y axis at a masked random offset.
///
/// Advances the shared random sequence once. `offset` is a live scratch
/// vector; this effect copies its position at spawn and does not follow the
/// retained offset pointer on later frames. The fourth halfword is unused.
static inline void _helicopterLandingPadNo9SpawnReactionSpark(GfxCoord* bodyCoord, SVECTOR* offset)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_SPARK_HEIGHT_MASK   = 0x2FF,
        HELICOPTER_LANDING_PAD_NO_9_SPARK_HEIGHT_OFFSET = 1664,
        HELICOPTER_LANDING_PAD_NO_9_SPARK_BASE_SIZE     = 256,
    };
    offset->vx = 0;
    offset->vz = 0;
    RANDOM_LCG_ADVANCE();
    offset->vy = (RANDOM_LCG_DRAW() & HELICOPTER_LANDING_PAD_NO_9_SPARK_HEIGHT_MASK) - HELICOPTER_LANDING_PAD_NO_9_SPARK_HEIGHT_OFFSET;
    effectSpawn(EFFECT_FLASH_BURST, bodyCoord, HELICOPTER_LANDING_PAD_NO_9_SPARK_BASE_SIZE, offset);
}

/// Ends grenade catching when the golem parent disappears.
///
/// Requires the live grenade model and its owned work. Disables its attack,
/// unlinks the grid probe and resets the ending-frame counter while keeping the
/// grenade task alive for delayed teardown. Grid unlink is safe after flight
/// already removed the probe.
static inline void _helicopterLandingPadNo9CancelGrenadeCatch(Task* task, _HelicopterLandingPadNo9GrenadeWork* work)
{
    work->attack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionUnlinkBody(&work->gridProbe);
    work->frames           = 0;
    task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    task->state            = HELICOPTER_LANDING_PAD_NO_9_GRENADE_TASK_BURST;
    work->phase            = HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_ENDING;
}

/// Places a grenade at the chest's launch offset in the view coordinate's frame.
///
/// Both coordinates must be live and distinct; `placement` is caller-owned scratch.
/// Uses (-165, -565, 160) chest-axis units and pitch -352 (4096 units per turn).
/// Refreshes both source caches and borrows the view coordinate as the new parent.
static inline void _helicopterLandingPadNo9PlaceGrenade(GfxCoord* grenadeCoord, GfxCoord* launchCoord, ActorChildPlaceScratch* placement)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_LAUNCH_X     = -165,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_LAUNCH_Y     = -565,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_LAUNCH_Z     = 160,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_LAUNCH_PITCH = -352,
    };

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    launchCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(launchCoord);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &launchCoord->workm, &grenadeCoord->coord);

    placement->operand.vx = HELICOPTER_LANDING_PAD_NO_9_GRENADE_LAUNCH_X;
    placement->operand.vy = HELICOPTER_LANDING_PAD_NO_9_GRENADE_LAUNCH_Y;
    placement->operand.vz = HELICOPTER_LANDING_PAD_NO_9_GRENADE_LAUNCH_Z;
    gte_SetRotMatrix(&grenadeCoord->coord);
    gte_ldv0(&placement->operand);
    gte_rtv0();
    gte_stlvnl(&placement->offset);
    grenadeCoord->parent      = &gGfxViewCoord;
    grenadeCoord->coord.t[0] += placement->offset.vx;
    grenadeCoord->coord.t[1] += placement->offset.vy;
    grenadeCoord->coord.t[2] += placement->offset.vz;

    placement->operand.vx = HELICOPTER_LANDING_PAD_NO_9_GRENADE_LAUNCH_PITCH;
    placement->operand.vy = 0;
    placement->operand.vz = 0;
    RotMatrix(&placement->operand, &placement->rotation);
    // Store product columns while the GTE retains the original launch basis.
    gte_MulMatrix0(&grenadeCoord->coord, &placement->rotation, &grenadeCoord->coord);
}

/// Post-multiplies the chest's local rotation by its residual hit twist.
///
/// Both matrices must be word-aligned and distinct, with 12 fractional bits
/// per coefficient. Preserves translation and changes the GTE rotation state;
/// the enclosing animation update owns coordinate-cache validity.
static inline void _helicopterLandingPadNo9ComposeHitRotation(MATRIX* chestRotation, const MATRIX* hitRotation)
{
    gte_MulMatrix0(chestRotation, hitRotation, chestRotation);
}

/// Seeds the chest's residual pitch and yaw for a hit twist.
///
/// Requires live body work. One LCG draw chooses signed pitch and yaw magnitudes
/// of 64..191 angle units (4096 per turn); the yaw uses the draw's signed upper byte.
/// Replaces any preceding residuals and marks the twist active; roll is unchanged.
static inline void _helicopterLandingPadNo9StartHitTwist(HelicopterLandingPadNo9Work* work)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_HIT_TWIST_MIN_ANGLE  = 64,
        HELICOPTER_LANDING_PAD_NO_9_HIT_TWIST_ANGLE_MASK = 127,
    };
    u32 randomState;
    u32 randomBits;
    s32 pitchTwist;
    s32 yawBits;
    s32 yawTwist;

    randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
    randomBits      = randomState >> 16;
    pitchTwist      = (randomBits & HELICOPTER_LANDING_PAD_NO_9_HIT_TWIST_ANGLE_MASK) + HELICOPTER_LANDING_PAD_NO_9_HIT_TWIST_MIN_ANGLE;
    gRandomLcgState = randomState;
    if (!(randomBits & 1)) {
        pitchTwist = -pitchTwist;
    }
    work->hitTwist.vx = pitchTwist;
    yawBits           = (s16)randomBits >> 8;
    yawTwist          = (yawBits & HELICOPTER_LANDING_PAD_NO_9_HIT_TWIST_ANGLE_MASK) + HELICOPTER_LANDING_PAD_NO_9_HIT_TWIST_MIN_ANGLE;
    if (!(yawBits & 1)) {
        yawTwist = -yawTwist;
    }
    work->hitTwist.vy    = yawTwist;
    work->hitTwistActive = 1;
}

/// Selects the lethal strike's forward and retreat movement from its frame windows.
///
/// Requires live boss work. Callers pass the unsigned low half of the animation
/// frame: 75..90 advances at 60, 143..157 uses `retreatSpeed`, and other frames
/// stop. Positive speed follows the lap and negative speed retreats, in game
/// coordinate units per running update. The first subtraction is compared as
/// an unsigned word; the retreat-window subtraction wraps to sixteen bits.
static inline void _helicopterLandingPadNo9SetLethalLapSpeed(HelicopterLandingPadNo9Work* work, s32 frame, s32 retreatSpeed)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_FORWARD_FIRST_FRAME = 75,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_FORWARD_FRAME_COUNT = 16,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_RETREAT_FIRST_FRAME = 143,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_RETREAT_FRAME_COUNT = 15,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_FORWARD_SPEED       = 60,
    };
    if (frame - HELICOPTER_LANDING_PAD_NO_9_LETHAL_FORWARD_FIRST_FRAME < (u32)HELICOPTER_LANDING_PAD_NO_9_LETHAL_FORWARD_FRAME_COUNT) {
        work->lapSpeed = HELICOPTER_LANDING_PAD_NO_9_LETHAL_FORWARD_SPEED;
    } else if (((frame - HELICOPTER_LANDING_PAD_NO_9_LETHAL_RETREAT_FIRST_FRAME) & 0xFFFF) < (u32)HELICOPTER_LANDING_PAD_NO_9_LETHAL_RETREAT_FRAME_COUNT) {
        work->lapSpeed = retreatSpeed;
    } else {
        work->lapSpeed = 0;
    }
}

/// Applies incoming attack and helipad-hazard contacts and latches outgoing hits.
///
/// Requires the live body model, owned work and enemy. Weapon attacks respect the
/// hit cooldown; hazard rows 2..4 apply fixed damage without that gate. Damage and
/// cooldown results narrow to signed halfwords. Once the lethal strike starts,
/// damage leaves at least one HP. Clears body contacts and occupied attack contacts;
/// hit effects use the chest, with consecutive equal attack keys sharing one effect.
static void _helicopterLandingPadNo9ResolveBodyContacts(Task* task)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_HIT_TWIST                       = 0,
        HELICOPTER_LANDING_PAD_NO_9_HIT_FLINCH                      = 1,
        HELICOPTER_LANDING_PAD_NO_9_HIT_RECOIL                      = 2,
        HELICOPTER_LANDING_PAD_NO_9_ATTACK_ATTRIBUTE_REDUCED_DAMAGE = 4,
        HELICOPTER_LANDING_PAD_NO_9_ATTACK_ATTRIBUTE_DOUBLE_DAMAGE  = 5,
        HELICOPTER_LANDING_PAD_NO_9_SOUND_HIT_FLINCH                = 0x40780004,
        HELICOPTER_LANDING_PAD_NO_9_ATTACHMENT_KEY_BIT              = 0x8000,
    };
    s32                          lastEffectKey;
    VECTOR*                      playerOffset;
    HelicopterLandingPadNo9Work* work;
    Enemy*                       enemy;
    GfxCoord*                    bodyCoord;
    s32                          hitReaction;
    s32                          attackAttribute;
    s32                          contactIndex;
    s16                          damage;
    s16                          hazardApplies;
    s32                          unscaledDamage;
    s16                          hazardDamage;
    s32                          soundKey;
    s32                          audioPan;
    s16                          cooldownFrames;

    SCRATCH_STACK_RESERVE_BYTES(sizeof(VECTOR));
    playerOffset  = SCRATCH_STACK_CURSOR(VECTOR);
    bodyCoord     = task->extra.tmd->coords;
    work          = task->work;
    enemy         = task->spawnArg2.pointer;
    hitReaction   = HELICOPTER_LANDING_PAD_NO_9_HIT_TWIST;
    lastEffectKey = 0;
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    // Apply receiving contacts before consuming the weapon/forearm contact.
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->bodyContacts); contactIndex++) {
        switch ((u16)(work->bodyContacts[contactIndex].key.value >> 16)) {
            case 0:
            case WORLD_COLLISION_CONTACT_PLAYER_BODY >> 16:
            case WORLD_COLLISION_CONTACT_ENEMY_BODY >> 16:
            case DAMAGE_ATTACK_CATEGORY >> 16:
                break;
            case WORLD_COLLISION_CONTACT_ATTACK >> 16:
                if (work->hitCooldown != 0) {
                    break;
                }
                attackAttribute = damageGetPlayerAttackReaction(work->bodyContacts[contactIndex].key.value);
                if (work->bodyContacts[contactIndex].key.value & HELICOPTER_LANDING_PAD_NO_9_ATTACHMENT_KEY_BIT) {
                    if ((u8)work->bodyContacts[contactIndex].key.value - 1 < 6U) {
                        hitReaction = HELICOPTER_LANDING_PAD_NO_9_HIT_RECOIL;
                    }
                    damage = (s16)damageComputePlayerAttack(work->bodyContacts[contactIndex].key.value, 0, 0, 0) >> 1;
                    damageAccumulateLifeDrainHp(task->spawnArg2.pointer, work->bodyContacts[contactIndex].key.value, damage, 0);
                } else {
                    playerOffset->vx = gPlayerStatus.coordMtx->t[0] - bodyCoord->coord.t[0];
                    playerOffset->vy = gPlayerStatus.coordMtx->t[1] - bodyCoord->coord.t[1];
                    playerOffset->vz = gPlayerStatus.coordMtx->t[2] - bodyCoord->coord.t[2];
                    unscaledDamage   = damageComputePlayerAttack(work->bodyContacts[contactIndex].key.value, SquareRoot0(playerOffset->vx * playerOffset->vx + playerOffset->vy * playerOffset->vy + playerOffset->vz * playerOffset->vz), 0, 0);
                    damage           = unscaledDamage;
                    if ((u16)attackAttribute == HELICOPTER_LANDING_PAD_NO_9_ATTACK_ATTRIBUTE_DOUBLE_DAMAGE) {
                        damage = unscaledDamage * 2;
                        effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[3], 2, NULL);
                    }
                    if (damageRollCriticalHit(enemy, work->bodyContacts[contactIndex].key.value, 0) != 0) {
                        damage *= 4;
                        if ((u16)attackAttribute != HELICOPTER_LANDING_PAD_NO_9_ATTACK_ATTRIBUTE_DOUBLE_DAMAGE) {
                            effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[3], 0, NULL);
                        }
                        if (work->buildupStunned == 0) {
                            hitReaction = HELICOPTER_LANDING_PAD_NO_9_HIT_FLINCH;
                        }
                    }
                }
                switch ((u16)attackAttribute) {
                    case DAMAGE_PLAYER_REACTION_NONE:
                    case HELICOPTER_LANDING_PAD_NO_9_ATTACK_ATTRIBUTE_DOUBLE_DAMAGE:
                    case DAMAGE_PLAYER_REACTION_EXPLOSION:
                    case DAMAGE_PLAYER_REACTION_INCENDIARY:
                    case 8:
                    case 9:
                        break;
                    case DAMAGE_PLAYER_REACTION_STAGGER:
                        if (work->buildupStunned == 0) {
                            RANDOM_LCG_ADVANCE();
                            if (RANDOM_LCG_DRAW() & 1) {
                                hitReaction = HELICOPTER_LANDING_PAD_NO_9_HIT_FLINCH;
                            }
                        }
                        break;
                    case DAMAGE_PLAYER_REACTION_BUILDUP:
                        if (work->lethalAttackPhase == HELICOPTER_LANDING_PAD_NO_9_LETHAL_PHASE_NONE) {
                            damageStartEnemyBuildup(enemy, work->bodyContacts[contactIndex].key.value, 0);
                        }
                        break;
                    case DAMAGE_PLAYER_REACTION_POISON:
                        if (work->buildupStunned == 0) {
                            hitReaction = HELICOPTER_LANDING_PAD_NO_9_HIT_RECOIL;
                        }
                        break;
                    case HELICOPTER_LANDING_PAD_NO_9_ATTACK_ATTRIBUTE_REDUCED_DAMAGE:
                        damage = damage * 75 / 100;
                        break;
                }
                enemy->hp -= damage;
                worldTargetAddReadoutAmount(&enemy->node, damage, 0);
                if (enemy->hp <= 0) {
                    if (work->lethalAttackPhase < HELICOPTER_LANDING_PAD_NO_9_LETHAL_PHASE_STRUCK) {
                        work->lethalAttackPhase = HELICOPTER_LANDING_PAD_NO_9_LETHAL_PHASE_NONE;
                    } else {
                        enemy->hp = 1;
                    }
                }
                if (hitReaction != HELICOPTER_LANDING_PAD_NO_9_HIT_TWIST) {
                    RANDOM_LCG_ADVANCE();
                    if (!(RANDOM_LCG_DRAW() & 1)) {
                        soundKey = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_HIT_FLINCH;
                    } else {
                        soundKey = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_RECOIL;
                    }
                    audioPan = (s8)worldCoordGetOriginAudioPan(bodyCoord);
                    sndEvtRequestScriptStart(soundKey, audioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                }
                if (work->sparkSound != 0) {
                    sndEvtRequestScriptStop(work->sparkSound, SOUND_SCRIPT_STOP_NO_FADE);
                    work->sparkSound = 0;
                }
                if (work->lethalAttackPhase == HELICOPTER_LANDING_PAD_NO_9_LETHAL_PHASE_WINDUP) {
                    hitReaction = HELICOPTER_LANDING_PAD_NO_9_HIT_TWIST;
                }
                switch (hitReaction) {
                    case HELICOPTER_LANDING_PAD_NO_9_HIT_TWIST: {
                        _helicopterLandingPadNo9StartHitTwist(work);
                        if (enemy->hp <= 0) {
                            work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_DEATH;
                            work->subState    = HELICOPTER_LANDING_PAD_NO_9_DEATH_ENTER;
                            work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_DEATH;
                            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        }
                        break;
                    }
                    case HELICOPTER_LANDING_PAD_NO_9_HIT_FLINCH:
                        work->state                = HELICOPTER_LANDING_PAD_NO_9_STATE_FLINCH;
                        work->subState             = HELICOPTER_LANDING_PAD_NO_9_FLINCH_ENTER;
                        work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        if (enemy->hp <= 0) {
                            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        }
                        break;
                    case HELICOPTER_LANDING_PAD_NO_9_HIT_RECOIL:
                        work->state                = HELICOPTER_LANDING_PAD_NO_9_STATE_RECOIL;
                        work->subState             = HELICOPTER_LANDING_PAD_NO_9_RECOIL_ENTER;
                        work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        if (enemy->hp <= 0) {
                            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        }
                        break;
                }
                if (lastEffectKey != work->bodyContacts[contactIndex].key.value) {
                    lastEffectKey = work->bodyContacts[contactIndex].key.value;
                    effectSpawnHit(damageGetPlayerAttackEffectId(lastEffectKey), &task->extra.tmd->coords[3], NULL, &work->hitEffectArg);
                }
                cooldownFrames = damageGetPlayerAttackHitCooldown(work->bodyContacts[contactIndex].key.value);
                if (cooldownFrames > 0) {
                    work->hitCooldown = cooldownFrames;
                }
                break;
            case DAMAGE_HAZARD_CATEGORY >> 16:
                hazardApplies = 0;
                switch ((u32)(u16)work->bodyContacts[contactIndex].key.value) {
                    case HELICOPTER_LANDING_PAD_NO_9_HAZARD_LIGHT_BLAST:
                        hazardApplies  = 1;
                        work->state    = HELICOPTER_LANDING_PAD_NO_9_STATE_SPARK_RECOIL;
                        work->subState = HELICOPTER_LANDING_PAD_NO_9_SPARK_RECOIL_ENTER;
                        break;
                    case HELICOPTER_LANDING_PAD_NO_9_HAZARD_SOURCE_RECOIL:
                        hazardApplies  = 1;
                        work->state    = HELICOPTER_LANDING_PAD_NO_9_STATE_RECOIL;
                        work->subState = HELICOPTER_LANDING_PAD_NO_9_RECOIL_ENTER;
                        break;
                    case HELICOPTER_LANDING_PAD_NO_9_HAZARD_SOURCE_STUN:
                        hazardApplies  = 1;
                        work->state    = HELICOPTER_LANDING_PAD_NO_9_STATE_SPARK_STUN;
                        work->subState = HELICOPTER_LANDING_PAD_NO_9_SPARK_STUN_ENTER;
                        break;
                }
                if (hazardApplies) {
                    work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    hazardDamage               = damageGetHazardDamage((u16)work->bodyContacts[contactIndex].key.value, DAMAGE_HAZARD_VICTIM_ENEMY);
                    enemy->hp                 -= hazardDamage;
                    worldTargetAddReadoutAmount(&enemy->node, hazardDamage, 0);
                    if (enemy->hp <= 0) {
                        if (work->lethalAttackPhase < HELICOPTER_LANDING_PAD_NO_9_LETHAL_PHASE_STRUCK) {
                            work->lethalAttackPhase = HELICOPTER_LANDING_PAD_NO_9_LETHAL_PHASE_NONE;
                            work->body.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        } else {
                            enemy->hp = 1;
                        }
                    }
                }
                break;
        }
    }
    worldCollisionClearContacts(work->bodyContacts);
    if (work->attackContacts[0].flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
        if ((work->attackContacts[0].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
            work->attackLanded         = 1;
            work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
        worldCollisionClearContacts(work->attackContacts);
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(VECTOR));
}

/// Runs the fight's opening slash pair and hands off to the flame charge.
///
/// The opening uses attack index 2. A player hit interrupts either slash into
/// slash recovery; otherwise the flame-lunge state starts directly at its charge.
static void _helicopterLandingPadNo9TickOpening(Task* task)
{
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoord;
    s32                          soundKey;

    work      = task->work;
    bodyCoord = task->extra.tmd->coords;
    switch (work->subState) {
        case HELICOPTER_LANDING_PAD_NO_9_OPENING_INTRO:
            if (work->animationFrame == 0xA) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_WINDUP;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x64) {
                work->subState    = HELICOPTER_LANDING_PAD_NO_9_OPENING_WINDUP;
                work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_SLASH_WINDUP;
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_OPENING_WINDUP:
            if (work->animationFrame >= 0x14) {
                work->subState    = HELICOPTER_LANDING_PAD_NO_9_OPENING_FIRST_SLASH;
                work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_SLASH_FIRST;
                _helicopterLandingPadNo9ArmAttack(work, HELICOPTER_LANDING_PAD_NO_9_ATTACK_SLASH);
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_OPENING_FIRST_SLASH:
            work->lapSpeed = ((u32)((u16)work->animationFrame - 9) < 0x13U) ? 0x38 : 0;
            if (work->attackLanded != 0) {
                work->lapSpeed = 0;
            }
            if (work->animationFrame == 0xA) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_SLASH;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x1D) {
                if (work->attackLanded != 0) {
                    work->state                = HELICOPTER_LANDING_PAD_NO_9_STATE_SLASH;
                    work->subState             = HELICOPTER_LANDING_PAD_NO_9_SLASH_RECOVER;
                    work->animationId          = HELICOPTER_LANDING_PAD_NO_9_ANIM_SLASH_RECOVER;
                    work->attackLanded         = 0;
                    work->flameFrames          = 0;
                    work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    work->subState    = HELICOPTER_LANDING_PAD_NO_9_OPENING_SECOND_SLASH;
                    work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_SLASH_SECOND;
                }
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_OPENING_SECOND_SLASH:
            work->lapSpeed = ((u32)((u16)work->animationFrame - 5) < 0x13U) ? 0x38 : 0;
            if (work->attackLanded != 0) {
                work->lapSpeed = 0;
            }
            if (work->animationFrame == 6) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_SLASH;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x27) {
                if (work->attackLanded != 0) {
                    work->state                = HELICOPTER_LANDING_PAD_NO_9_STATE_SLASH;
                    work->subState             = HELICOPTER_LANDING_PAD_NO_9_SLASH_RECOVER;
                    work->animationId          = HELICOPTER_LANDING_PAD_NO_9_ANIM_SLASH_RECOVER;
                    work->attackLanded         = 0;
                    work->flameFrames          = 0;
                    work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    work->subState = HELICOPTER_LANDING_PAD_NO_9_OPENING_LUNGE;
                }
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_OPENING_LUNGE:
            work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_FLAME_LUNGE;
            work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_FLAME_LUNGE;
            work->subState    = HELICOPTER_LANDING_PAD_NO_9_FLAME_LUNGE_CHARGE;
            break;
    }
}

/// Walks the lap, fires from the weapon and selects attacks between walks.
///
/// `stateCounter` counts waiting frames or remaining walk distance, depending
/// on the step. A targeted player using shotgun ammunition triggers the separate
/// response walk. Catch-up and backstep movement remain inside the current side.
static void _helicopterLandingPadNo9TickPatrol(Task* task)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK_WITHOUT_FIRE    = 0,
        HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK_WITH_FIRE       = 1,
        HELICOPTER_LANDING_PAD_NO_9_PATROL_FIRST_SHOT_FRAMES    = 29,
        HELICOPTER_LANDING_PAD_NO_9_PATROL_SHOT_INTERVAL_FRAMES = 40,
    };
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoord;
    EffectWork*                  muzzleFlash;
    s32                          soundKey;
    s16                          catchUpSpeed;

    work      = task->work;
    bodyCoord = task->extra.tmd->coords;
    if (work->playerSide != work->lapSide && work->subState != HELICOPTER_LANDING_PAD_NO_9_PATROL_BACKSTEP && work->sideRemaining >= 0xBB8) {
        work->subState = HELICOPTER_LANDING_PAD_NO_9_PATROL_CATCH_UP;
    }
    switch (work->subState) {
        // Pick an attack only at a wait boundary; otherwise prepare another walk.
        case HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT:
            work->lapSpeed     = 0;
            work->attackLanded = 0;
            if (--work->stateCounter <= 0) {
                if (work->flameMode == HELICOPTER_LANDING_PAD_NO_9_FLAME_BLAST) {
                    work->flameMode = HELICOPTER_LANDING_PAD_NO_9_FLAME_BURNING;
                }
                if (_helicopterLandingPadNo9TrySelectAttack(task) == 0) {
                    if (work->sideRemaining < 0x7D0) {
                        work->subState     = HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK_START;
                        work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_WALK_START;
                        work->stateCounter = HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK_WITHOUT_FIRE;
                    } else if (((Enemy*)task->spawnArg2.pointer)->node.state.parts.targeted == 1 && (u32)(gPlayerStatus.weaponSlotItem - HELICOPTER_LANDING_PAD_NO_9_SHOTGUN_FIRST_AMMO_ROW) < (u32)HELICOPTER_LANDING_PAD_NO_9_SHOTGUN_AMMO_ROW_COUNT) {
                        work->subState    = HELICOPTER_LANDING_PAD_NO_9_PATROL_SHOTGUN_RESPONSE_START;
                        work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_SHOTGUN_RESPONSE_START;
                        soundKey          = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_SHOTGUN_RESPONSE;
                        sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                                 (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                    } else {
                        work->subState     = HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK_START;
                        work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_WALK_START;
                        work->stateCounter = HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK_WITH_FIRE;
                    }
                }
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK_START:
            work->lapSpeed = 0;
            if (work->animationFrame >= 0xD) {
                if (work->stateCounter == HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK_WITHOUT_FIRE) {
                    work->subState    = HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK;
                    work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_WALK;
                } else {
                    work->subState      = HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK_FIRE;
                    work->animationId   = HELICOPTER_LANDING_PAD_NO_9_ANIM_WALK_FIRE;
                    work->shotCountdown = HELICOPTER_LANDING_PAD_NO_9_PATROL_FIRST_SHOT_FRAMES;
                }
                work->stateCounter = _gHelicopterLandingPadNo9PatrolWalkDistances[((RANDOM_LCG_ADVANCE()) >> 16) & 0xF];
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK:
            work->lapSpeed      = HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK_SPEED;
            work->stateCounter -= HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK_SPEED;
            if (work->stateCounter <= 0 || work->lapDistance > (u32)HELICOPTER_LANDING_PAD_NO_9_PATROL_END_DISTANCE) {
                work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
                work->subState     = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
                work->stateCounter = _gHelicopterLandingPadNo9PatrolWaitFrames[((RANDOM_LCG_ADVANCE()) >> 16) & 0xF];
            }
            // The binary retains this empty lap-distance interval.
            if (work->sideRemaining >= 0x7D0 && work->playerDistance < 0x3E8 && work->lapDistance <= (u32)HELICOPTER_LANDING_PAD_NO_9_LAST_ATTACK_DISTANCE &&
                work->lapDistance > (u32)HELICOPTER_LANDING_PAD_NO_9_LETHAL_ATTACK_DISTANCE) {
                work->subState     = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
                work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
                work->stateCounter = 0;
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK_FIRE:
            work->lapSpeed      = HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK_SPEED;
            work->stateCounter -= HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK_SPEED;
            if (--work->shotCountdown == 0) {
                muzzleFlash = effectSpawn((EFFECT_NO9_MUZZLE_FLASH | EFFECT_SPAWN_UNLIMITED), &task->extra.tmd->coords[HELICOPTER_LANDING_PAD_NO_9_WEAPON_PART], 0, NULL);
                if (muzzleFlash != NULL) {
                    taskReparent(task, muzzleFlash->task);
                }
                work->shotCountdown = HELICOPTER_LANDING_PAD_NO_9_PATROL_SHOT_INTERVAL_FRAMES;
                soundKey            = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_FIRE;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (((Enemy*)task->spawnArg2.pointer)->node.state.parts.targeted == 1 && (u32)(gPlayerStatus.weaponSlotItem - HELICOPTER_LANDING_PAD_NO_9_SHOTGUN_FIRST_AMMO_ROW) < (u32)HELICOPTER_LANDING_PAD_NO_9_SHOTGUN_AMMO_ROW_COUNT) {
                work->subState    = HELICOPTER_LANDING_PAD_NO_9_PATROL_SHOTGUN_RESPONSE_WALK;
                work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_SHOTGUN_RESPONSE_START;
                soundKey          = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_SHOTGUN_RESPONSE;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->stateCounter <= 0 || work->lapDistance > (u32)HELICOPTER_LANDING_PAD_NO_9_PATROL_END_DISTANCE) {
                work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
                work->subState     = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
                work->stateCounter = _gHelicopterLandingPadNo9PatrolWaitFrames[((RANDOM_LCG_ADVANCE()) >> 16) & 0xF];
            }
            if (work->sideRemaining >= 0x7D0 && work->playerDistance < 0x3E8 && work->lapDistance <= (u32)HELICOPTER_LANDING_PAD_NO_9_LAST_ATTACK_DISTANCE &&
                work->lapDistance > (u32)HELICOPTER_LANDING_PAD_NO_9_LETHAL_ATTACK_DISTANCE) {
                work->subState     = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
                work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
                work->stateCounter = 0;
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_PATROL_CATCH_UP:
            catchUpSpeed      = 0;
            work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_CATCH_UP;
            if (work->animationFrame >= 8) {
                catchUpSpeed = 0xA0;
            }
            work->lapSpeed = catchUpSpeed;
            if (work->sideRemaining < 0x5DC) {
                work->subState     = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
                work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
                work->stateCounter = 0;
            }
            if (work->sideRemaining >= 0x7D0 && work->playerDistance < 0x3E8 && work->lapDistance <= (u32)HELICOPTER_LANDING_PAD_NO_9_LAST_ATTACK_DISTANCE &&
                work->lapDistance > (u32)HELICOPTER_LANDING_PAD_NO_9_LETHAL_ATTACK_DISTANCE) {
                work->subState     = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
                work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
                work->stateCounter = 0;
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_PATROL_SHOTGUN_RESPONSE_START:
            if (work->animationFrame >= 0xF) {
                work->subState     = HELICOPTER_LANDING_PAD_NO_9_PATROL_SHOTGUN_RESPONSE_WALK;
                work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_SHOTGUN_RESPONSE_WALK;
                work->stateCounter = _gHelicopterLandingPadNo9PatrolWalkDistances[((RANDOM_LCG_ADVANCE()) >> 16) & 0xF];
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_PATROL_SHOTGUN_RESPONSE_WALK:
            work->lapSpeed      = HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK_SPEED;
            work->stateCounter -= HELICOPTER_LANDING_PAD_NO_9_PATROL_WALK_SPEED;
            if (work->stateCounter <= 0 || work->lapDistance > (u32)HELICOPTER_LANDING_PAD_NO_9_PATROL_END_DISTANCE ||
                (work->sideRemaining >= 0x7D0 && work->playerDistance < 0x3E8 && work->lapDistance <= (u32)HELICOPTER_LANDING_PAD_NO_9_LAST_ATTACK_DISTANCE)) {
                work->subState    = HELICOPTER_LANDING_PAD_NO_9_PATROL_SHOTGUN_RESPONSE_END;
                work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_SHOTGUN_RESPONSE_END;
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_PATROL_SHOTGUN_RESPONSE_END:
            work->lapSpeed = 0;
            if (work->animationFrame >= 0xD) {
                work->subState    = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
                work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
                if (work->sideRemaining >= 0x7D0 && work->playerDistance < 0x3E8 && work->lapDistance <= (u32)HELICOPTER_LANDING_PAD_NO_9_LAST_ATTACK_DISTANCE &&
                    work->lapDistance > (u32)HELICOPTER_LANDING_PAD_NO_9_LETHAL_ATTACK_DISTANCE) {
                    work->stateCounter = 0;
                } else {
                    work->stateCounter = _gHelicopterLandingPadNo9PatrolWaitFrames[((RANDOM_LCG_ADVANCE()) >> 16) & 0xF];
                }
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_PATROL_BACKSTEP:
            if (work->sideTravelled < 0xC8 || work->playerSide != work->lapSide || work->animationFrame < 0xB ||
                work->animationFrame >= 0x18) {
                work->lapSpeed = 0;
            } else {
                work->lapSpeed = -0xA7;
            }
            if (work->animationFrame == 0x19) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_BACKSTEP;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x32) {
                work->state        = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                work->subState     = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
                work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
                work->stateCounter = 0;
            }
            break;
    }
}

/// Selects an attack from lap progress, player range and relative facing.
///
/// Returns 1 after setting the next state, step and animation, or 0 to continue
/// patrolling. The light-slash latch permits one scripted light slash until the
/// light consumes it. Range choices apply only on the player's side of the lap;
/// the end-of-lap lethal attack takes precedence over them.
static s32 _helicopterLandingPadNo9TrySelectAttack(Task* task)
{
    enum { HELICOPTER_LANDING_PAD_NO_9_ATTACK_INITIAL_STEP = 0 };
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoord;
    s32                          lightDistanceError;
    s32                          selected;
    s16                          playerDistance;

    work      = task->work;
    bodyCoord = task->extra.tmd->coords;
    // Scripted light strikes and the lethal attack precede range-based choices.
    if (work->slashedLight == HELICOPTER_LANDING_PAD_NO_9_LIGHT_SLASH_NONE) {
        lightDistanceError = work->lapDistance - HELICOPTER_LANDING_PAD_NO_9_LIGHT_0_SLASH_DISTANCE;
        if (lightDistanceError < 0) {
            lightDistanceError = -lightDistanceError;
        }
        if (lightDistanceError < HELICOPTER_LANDING_PAD_NO_9_LIGHT_SLASH_WINDOW) {
            work->slashedLight = HELICOPTER_LANDING_PAD_NO_9_LIGHT_SLASH_LIGHT_0;
            work->state        = HELICOPTER_LANDING_PAD_NO_9_STATE_SLASH;
            work->subState     = HELICOPTER_LANDING_PAD_NO_9_SLASH_WINDUP;
            work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_SLASH_WINDUP;
            return 1;
        }
        lightDistanceError = work->lapDistance - HELICOPTER_LANDING_PAD_NO_9_LIGHT_1_SLASH_DISTANCE;
        if (lightDistanceError < 0) {
            lightDistanceError = -lightDistanceError;
        }
        if (lightDistanceError < HELICOPTER_LANDING_PAD_NO_9_LIGHT_SLASH_WINDOW) {
            work->slashedLight = HELICOPTER_LANDING_PAD_NO_9_LIGHT_SLASH_LIGHT_1;
            work->state        = HELICOPTER_LANDING_PAD_NO_9_STATE_SLASH;
            work->subState     = HELICOPTER_LANDING_PAD_NO_9_SLASH_WINDUP;
            work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_SLASH_WINDUP;
            return 1;
        }
    }
    if (work->lapDistance > (u32)HELICOPTER_LANDING_PAD_NO_9_LETHAL_ATTACK_DISTANCE) {
        work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_LETHAL_ATTACK;
        work->subState    = HELICOPTER_LANDING_PAD_NO_9_ATTACK_INITIAL_STEP;
        work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_FLAME_WINDUP;
        return 1;
    }
    if (work->lapDistance > (u32)HELICOPTER_LANDING_PAD_NO_9_LAST_ATTACK_DISTANCE) {
        return 0;
    }
    if (work->lapSide != work->playerSide) {
        return 0;
    }
    // A negative forward-axis dot product means the player faces the golem.
    if (gPlayerStatus.coordMtx->m[0][2] * bodyCoord->coord.m[0][2] +
            gPlayerStatus.coordMtx->m[2][2] * bodyCoord->coord.m[2][2] <
        0) {
        playerDistance = work->playerDistance;
        if (playerDistance < HELICOPTER_LANDING_PAD_NO_9_SLASH_RANGE) {
            selected          = 1;
            work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_SLASH;
            work->subState    = HELICOPTER_LANDING_PAD_NO_9_SLASH_WINDUP;
            work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_SLASH_WINDUP;
        } else if (playerDistance < HELICOPTER_LANDING_PAD_NO_9_FLAME_LUNGE_RANGE) {
            selected          = 1;
            work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_FLAME_LUNGE;
            work->subState    = HELICOPTER_LANDING_PAD_NO_9_FLAME_LUNGE_WINDUP;
            work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_FLAME_WINDUP;
        } else if (playerDistance < HELICOPTER_LANDING_PAD_NO_9_MIXED_ATTACK_RANGE) {
            RANDOM_LCG_ADVANCE();
            if ((gRandomLcgState >> 0x10) & 1) {
                work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_FLAME_LUNGE;
                work->subState    = HELICOPTER_LANDING_PAD_NO_9_FLAME_LUNGE_WINDUP;
                work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_FLAME_WINDUP;
            } else {
                work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_GRENADE;
                work->subState    = HELICOPTER_LANDING_PAD_NO_9_ATTACK_INITIAL_STEP;
                work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_GRENADE_THROW;
            }
            selected = 1;
        } else {
            selected = 0;
            if (work->grenadeLive == 0) {
                selected          = 1;
                work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_GRENADE;
                work->subState    = HELICOPTER_LANDING_PAD_NO_9_ATTACK_INITIAL_STEP;
                work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_GRENADE_THROW;
            }
        }
    } else {
        if (work->playerDistance < HELICOPTER_LANDING_PAD_NO_9_SLASH_RANGE) {
            work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_SLASH;
            work->subState    = HELICOPTER_LANDING_PAD_NO_9_SLASH_WINDUP;
            work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_SLASH_WINDUP;
        } else {
            RANDOM_LCG_ADVANCE();
            if (((gRandomLcgState >> 0x10) & 0xF) < 0xAU) {
                work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_GRENADE;
                work->subState    = HELICOPTER_LANDING_PAD_NO_9_ATTACK_INITIAL_STEP;
                work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_GRENADE_THROW;
            } else {
                work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_DASH;
                work->subState    = HELICOPTER_LANDING_PAD_NO_9_DASH_START;
                work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_DASH_START;
            }
        }
        selected = 1;
    }
    return selected;
}

/// Advances alternating slash cycles and their recovery after a player hit.
///
/// Movement is confined to each slash's animation window. The first stroke
/// also signals a selected helipad light. Random continuation thresholds are
/// indexed by cycle count 0..3, after which the golem returns to patrol.
static void _helicopterLandingPadNo9TickSlash(Task* task)
{
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoord;
    s32                          soundKey;

    work      = task->work;
    bodyCoord = task->extra.tmd->coords;
    switch (work->subState) {
        case HELICOPTER_LANDING_PAD_NO_9_SLASH_WINDUP:
            work->lapSpeed = 0;
            if (work->animationFrame >= 0x14) {
                work->subState     = HELICOPTER_LANDING_PAD_NO_9_SLASH_FIRST;
                work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_SLASH_FIRST;
                work->stateCounter = 0;
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_SLASH_FIRST:
            work->lapSpeed = ((u32)((u16)work->animationFrame - 9) < 0x13U) ? 0x38 : 0;
            if (work->attackLanded != 0) {
                work->lapSpeed = 0;
            }
            if (work->animationFrame == 0xA) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_SLASH;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if ((work->slashedLight > HELICOPTER_LANDING_PAD_NO_9_LIGHT_SLASH_NONE) && (work->animationFrame == 8)) {
                work->lightSlashStruck = 1;
            }
            if (work->animationFrame == 9) {
                _helicopterLandingPadNo9ArmAttack(work, HELICOPTER_LANDING_PAD_NO_9_ATTACK_SLASH);
            }
            if (work->animationFrame >= 0x1D) {
                if (work->attackLanded != 0) {
                    work->subState             = HELICOPTER_LANDING_PAD_NO_9_SLASH_RECOVER;
                    work->animationId          = HELICOPTER_LANDING_PAD_NO_9_ANIM_SLASH_RECOVER;
                    work->attackLanded         = 0;
                    work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    RANDOM_LCG_ADVANCE();
                    if (_gHelicopterLandingPadNo9SlashContinuationThresholds[work->stateCounter] < (s32)((gRandomLcgState >> 0x10) & 0xF)) {
                        u16* patrolWaitFrames = _gHelicopterLandingPadNo9PostAttackWaitFrames;
                        work->state           = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                        work->animationId     = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
                        work->subState        = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
                        RANDOM_LCG_ADVANCE();
                        work->stateCounter         = patrolWaitFrames[(gRandomLcgState >> 0x10) & 0xF];
                        work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    } else {
                        work->subState    = HELICOPTER_LANDING_PAD_NO_9_SLASH_SECOND;
                        work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_SLASH_SECOND;
                        work->stateCounter++;
                    }
                }
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_SLASH_SECOND:
            work->lapSpeed = ((u32)((u16)work->animationFrame - 5) < 0x13U) ? 0x38 : 0;
            if (work->attackLanded != 0) {
                work->lapSpeed = 0;
            }
            if (work->animationFrame == 6) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_SLASH;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x27) {
                if (work->attackLanded != 0) {
                    work->subState             = HELICOPTER_LANDING_PAD_NO_9_SLASH_RECOVER;
                    work->animationId          = HELICOPTER_LANDING_PAD_NO_9_ANIM_SLASH_RECOVER;
                    work->attackLanded         = 0;
                    work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    if ((_gHelicopterLandingPadNo9SlashContinuationThresholds[work->stateCounter] <
                         (s32)(((RANDOM_LCG_ADVANCE()) >> 0x10) & 0xF)) ||
                        (work->stateCounter >= (s32)ARRAY_SIZE(_gHelicopterLandingPadNo9SlashContinuationThresholds) - 1)) {
                        u16* patrolWaitFrames = _gHelicopterLandingPadNo9PostAttackWaitFrames;
                        work->state           = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                        work->subState        = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
                        work->animationId     = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
                        RANDOM_LCG_ADVANCE();
                        work->stateCounter         = patrolWaitFrames[(gRandomLcgState >> 0x10) & 0xF];
                        work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    } else {
                        work->stateCounter++;
                        work->subState    = HELICOPTER_LANDING_PAD_NO_9_SLASH_FIRST;
                        work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_SLASH_FIRST;
                    }
                }
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_SLASH_RECOVER:
            if (work->animationFrame == 0x2D) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_SLASH_RECOVER;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x5A) {
                u16* patrolWaitFrames = _gHelicopterLandingPadNo9PostAttackWaitFrames;
                work->state           = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                work->subState        = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
                work->animationId     = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
                RANDOM_LCG_ADVANCE();
                work->stateCounter = patrolWaitFrames[(gRandomLcgState >> 0x10) & 0xF];
            }
            break;
    }
}

/// Ignites the weapon, charges toward the player and recovers into patrol.
///
/// The charge speed is (min(player distance, 5000) - 1200) / 24 world units
/// per frame, narrowed to `stateCounter`. A player hit selects patrol's backstep;
/// a miss selects its wait. Both exits disable the weapon and forearm attacks.
static void _helicopterLandingPadNo9TickFlameLunge(Task* task)
{
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoord;
    s32                          soundKey;
    s32                          lungeDistance;
    s32                          playerDistance;
    u32                          randomState;

    work      = task->work;
    bodyCoord = task->extra.tmd->coords;
    switch (work->subState) {
        case HELICOPTER_LANDING_PAD_NO_9_FLAME_LUNGE_WINDUP:
            work->lapSpeed = 0;
            if (work->animationFrame == 0x39) {
                work->flameMode   = HELICOPTER_LANDING_PAD_NO_9_FLAME_BURNING;
                work->flameFrames = 0x55;
                soundKey          = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_FIRE;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                work->flameSound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_FLAME_LOOP;
                sndEvtRequestScriptStart(work->flameSound, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                _helicopterLandingPadNo9ArmAttack(work, HELICOPTER_LANDING_PAD_NO_9_ATTACK_IGNITION);
            }
            if (work->animationFrame == 0x41) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_WINDUP;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x5A) {
                work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_FLAME_LUNGE;
                work->subState    = HELICOPTER_LANDING_PAD_NO_9_FLAME_LUNGE_CHARGE;
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_FLAME_LUNGE_CHARGE:
            if (work->animationFrame == 0xE) {
                // Keep the raw distance separate from the default clamp result.
                playerDistance = work->playerDistance;
                lungeDistance  = HELICOPTER_LANDING_PAD_NO_9_MAX_LUNGE_DISTANCE;
                if (playerDistance < HELICOPTER_LANDING_PAD_NO_9_MAX_LUNGE_DISTANCE + 1) {
                    lungeDistance = playerDistance;
                }
                work->stateCounter = (lungeDistance - 0x4B0) / 24;
            }
            work->lapSpeed = ((u32)((u16)work->animationFrame - 0x17) < 0x19U) ? work->stateCounter : 0;
            if (work->animationFrame == 0x19) {
                _helicopterLandingPadNo9ArmAttack(work, HELICOPTER_LANDING_PAD_NO_9_ATTACK_FLAME_LUNGE);
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_FLAME_STRIKE;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame == 0x2F) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_BACKSTEP;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x46) {
                if (work->attackLanded != 0) {
                    work->state        = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                    work->subState     = HELICOPTER_LANDING_PAD_NO_9_PATROL_BACKSTEP;
                    work->attackLanded = 0;
                    work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_BACKSTEP;
                } else {
                    work->state        = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                    work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
                    work->subState     = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
                    work->stateCounter = _gHelicopterLandingPadNo9PostAttackWaitFrames[((u32)(randomState = RANDOM_LCG_NEXT(gRandomLcgState)) >> 0x10) & 0xF];
                    gRandomLcgState    = randomState;
                }
                work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
    }
}

/// Throws the stun grenade, then chooses a dash or a return to patrol.
///
/// Requires the live body and enemy with an existing area placement. Frame 20
/// spawns descriptor 4 and lends it the body's placement textures; frame 70
/// chooses a dash with 12/16 probability when the player is at least 3751 units
/// away. The grenade joins the body's task tree and clears its live latch on exit.
static void _helicopterLandingPadNo9TickGrenadeThrow(Task* task)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_THROW_FRAME       = 20,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_THROW_END_FRAME   = 70,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_DESCRIPTOR        = 4,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_DASH_MIN_DISTANCE = 3751,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_DASH_ROLL_LIMIT   = 12,
        HELICOPTER_LANDING_PAD_NO_9_SOUND_GRENADE_THROW       = 0x40780012,
    };
    HelicopterLandingPadNo9Work* work;
    Enemy*                       enemy;
    GfxCoord*                    bodyCoord;
    GameLocationKey*             liveLocation;
    TmdObject*                   grenadeModel;
    AreaVariant*                 areaVariant;
    AreaPlacement*               placement;
    GameLocationKey              location;
    s32                          placeIndex;
    s32                          soundKey;
    s32                          audioPan;
    s32                          dashNext;
    u32                          randomState;

    dashNext  = 0;
    bodyCoord = task->extra.tmd->coords;
    work      = task->work;
    enemy     = task->spawnArg2.pointer;

    work->lapSpeed = 0;
    if (work->animationFrame == HELICOPTER_LANDING_PAD_NO_9_GRENADE_THROW_FRAME) {
        work->grenadeLive  = 1;
        grenadeModel       = enemySpawnFromTable(gHelicopterLandingPadNo9TaskDescriptors, HELICOPTER_LANDING_PAD_NO_9_GRENADE_DESCRIPTOR, dashNext, enemy)->task->extra.tmd;
        placeIndex         = (u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        liveLocation       = &gGameSession->location;
        location.stage     = liveLocation->stage;
        location.area      = liveLocation->area;
        location.roomState = liveLocation->roomState;
        location.view      = gGameSession->location.view;
        areaSyncLocationVariant(&location);
        areaVariant                     = areaGetVariant(&location);
        placement                       = AREA_PLACEMENT_AT(areaVariant->placements, placeIndex);
        grenadeModel->texturePageOffset = placement->texturePageOffset;
        grenadeModel->clutRowOffset     = placement->clutRowOffset;
        if (grenadeModel->buffer != NULL) {
            tmdBuildBufferHalf(grenadeModel);
            tmdBuildBufferHalf(grenadeModel);
        }
        soundKey = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_GRENADE_THROW;
        audioPan = (s8)worldCoordGetOriginAudioPan(bodyCoord);
        sndEvtRequestScriptStart(soundKey, audioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
    }
    if (work->animationFrame >= HELICOPTER_LANDING_PAD_NO_9_GRENADE_THROW_END_FRAME) {
        if (work->playerDistance >= HELICOPTER_LANDING_PAD_NO_9_GRENADE_DASH_MIN_DISTANCE) {
            RANDOM_LCG_ADVANCE();
            if (((gRandomLcgState >> 0x10) & 0xF) < (u32)HELICOPTER_LANDING_PAD_NO_9_GRENADE_DASH_ROLL_LIMIT) {
                dashNext = 1;
            }
        }
        if (dashNext == 0) {
            work->state        = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
            work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
            work->subState     = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
            work->stateCounter = _gHelicopterLandingPadNo9PostAttackWaitFrames[((u32)(randomState = RANDOM_LCG_NEXT(gRandomLcgState)) >> 0x10) & 0xF];
            gRandomLcgState    = randomState;
            return;
        }
        work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_DASH;
        work->subState    = HELICOPTER_LANDING_PAD_NO_9_DASH_START;
        work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_DASH_START;
    }
}

/// Dashes along the lap until the player is close enough for a strike.
///
/// The run consumes a distance budget at 132 world units per frame and aborts
/// near a corner. A completed strike enters patrol's backstep; an aborted run
/// returns to its wait. Both exits disable the two attack spheres.
static void _helicopterLandingPadNo9TickDash(Task* task)
{
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoord;
    s32                          soundKey;
    s32                          patrolWaitFrames;

    work      = task->work;
    bodyCoord = task->extra.tmd->coords;
    switch (work->subState) {
        case HELICOPTER_LANDING_PAD_NO_9_DASH_START:
            work->lapSpeed = (work->animationFrame < 0x47) ? 0 : HELICOPTER_LANDING_PAD_NO_9_DASH_SPEED;
            if (work->animationFrame == 0x4A) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_DASH_START;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x52) {
                work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_DASH_RUN;
                work->subState     = HELICOPTER_LANDING_PAD_NO_9_DASH_RUN;
                work->stateCounter = 0xEA6;
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_DASH_RUN:
            work->lapSpeed = HELICOPTER_LANDING_PAD_NO_9_DASH_SPEED;
            if (work->playerDistance < 0x514) {
                work->subState    = HELICOPTER_LANDING_PAD_NO_9_DASH_STRIKE;
                work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_DASH_STRIKE;
                _helicopterLandingPadNo9ArmAttack(work, HELICOPTER_LANDING_PAD_NO_9_ATTACK_DASH);
            } else {
                work->stateCounter -= HELICOPTER_LANDING_PAD_NO_9_DASH_SPEED;
                if (work->stateCounter < 0 || work->sideRemaining < 0x384) {
                    work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                    work->subState    = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
                    work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
                    RANDOM_LCG_ADVANCE();
                    patrolWaitFrames           = _gHelicopterLandingPadNo9PostAttackWaitFrames[RANDOM_LCG_DRAW() & 0xF];
                    work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->stateCounter         = patrolWaitFrames;
                }
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_DASH_STRIKE:
            work->lapSpeed = 0;
            if (work->animationFrame == 0xE) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_DASH_STRIKE;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x3B) {
                work->subState             = HELICOPTER_LANDING_PAD_NO_9_PATROL_BACKSTEP;
                work->animationId          = HELICOPTER_LANDING_PAD_NO_9_ANIM_BACKSTEP;
                work->state                = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
    }
}

/// Runs the lap-ending ignition, lethal strike and player-death sequence.
///
/// Requires the live body, player and landing-pad resources. The strike holds the
/// death sound and restart timers, kills the player on a latched contact, then
/// spawns splatter every six updates and loads the death sound. A missed strike
/// returns to patrol; angle, frame and damage storage keep their halfword widths.
static void _helicopterLandingPadNo9TickLethalAttack(Task* task)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_IGNITE              = 0,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_WINDUP              = 1,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_STRIKE              = 2,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_PLAYER_DEAD         = 3,
        HELICOPTER_LANDING_PAD_NO_9_DEATH_SOUND_IDLE           = 0,
        HELICOPTER_LANDING_PAD_NO_9_DEATH_SOUND_REQUEST        = 1,
        HELICOPTER_LANDING_PAD_NO_9_DEATH_SOUND_WAIT           = 2,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_IGNITION_FRAME      = 57,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_IGNITION_END_FRAME  = 90,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_WINDUP_SOUND_FRAME  = 10,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_WINDUP_END_FRAME    = 100,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_ATTACK_ON_FRAME     = 80,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_ATTACK_SOUND_FRAME  = 83,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_VIBRATION_FRAME     = 88,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_ATTACK_OFF_FRAME    = 96,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_CONTACT_FIRST_FRAME = 81,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_MISS_END_FRAME      = 179,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_RECOVER_FRAME       = 170,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_FLAME_FRAMES        = 270,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_RESTART_DELAY       = 128,
        HELICOPTER_LANDING_PAD_NO_9_LETHAL_HIT_EFFECT_INTERVAL = 6,
        HELICOPTER_LANDING_PAD_NO_9_PLAYER_DEATH_SOUND_FILE    = 30,
        HELICOPTER_LANDING_PAD_NO_9_SOUND_LETHAL_FLAME_STRIKE  = 0x40780010,
    };
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoord;
    s32                          soundKey;
    s32                          animationValue; // Holds either the signed animation ID or the unsigned frame
    u32                          randomState;

    bodyCoord = task->extra.tmd->coords;
    work      = task->work;

    // Keep the balanced scratch reservation used by this sequence.
    SCRATCH_STACK_RESERVE_BYTES(0x10);

    switch (work->subState) {
        case HELICOPTER_LANDING_PAD_NO_9_LETHAL_IGNITE:
            work->lethalAttackPhase = HELICOPTER_LANDING_PAD_NO_9_LETHAL_PHASE_WINDUP;
            work->lapSpeed          = 0;
            if (work->animationFrame == HELICOPTER_LANDING_PAD_NO_9_LETHAL_IGNITION_FRAME) {
                work->flameMode   = HELICOPTER_LANDING_PAD_NO_9_FLAME_BURNING;
                work->flameFrames = HELICOPTER_LANDING_PAD_NO_9_LETHAL_FLAME_FRAMES;
                soundKey          = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_FIRE;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                work->flameSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_FLAME_LOOP;
                sndEvtRequestScriptStart(work->flameSound, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                _helicopterLandingPadNo9ArmAttack(work, HELICOPTER_LANDING_PAD_NO_9_ATTACK_IGNITION);
            }
            if (work->animationFrame >= HELICOPTER_LANDING_PAD_NO_9_LETHAL_IGNITION_END_FRAME) {
                work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_FIGHT_START;
                work->subState    = HELICOPTER_LANDING_PAD_NO_9_LETHAL_WINDUP;
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_LETHAL_WINDUP:
            if (work->animationFrame == HELICOPTER_LANDING_PAD_NO_9_LETHAL_WINDUP_SOUND_FRAME) {
                soundKey = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_WINDUP;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= HELICOPTER_LANDING_PAD_NO_9_LETHAL_WINDUP_END_FRAME) {
                work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_LETHAL_STRIKE;
                work->subState    = HELICOPTER_LANDING_PAD_NO_9_LETHAL_STRIKE;
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_LETHAL_STRIKE:
            if (work->animationFrame == HELICOPTER_LANDING_PAD_NO_9_LETHAL_ATTACK_ON_FRAME) {
                // Arm the fatal hit before the death-delay and sound holds.
                work->lethalAttackPhase = HELICOPTER_LANDING_PAD_NO_9_LETHAL_PHASE_STRUCK;
                _helicopterLandingPadNo9ArmAttack(work, HELICOPTER_LANDING_PAD_NO_9_ATTACK_LETHAL);

                gGameSession->deathRestartDelay   = HELICOPTER_LANDING_PAD_NO_9_LETHAL_RESTART_DELAY;
                gGameSession->deathSoundCountdown = GAME_SESSION_DEATH_SOUND_HOLD;
            }
            if (work->animationFrame == HELICOPTER_LANDING_PAD_NO_9_LETHAL_ATTACK_SOUND_FRAME) {
                if (work->flameMode == HELICOPTER_LANDING_PAD_NO_9_FLAME_BURNING) {
                    soundKey = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_LETHAL_FLAME_STRIKE;
                    sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                             (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                } else {
                    soundKey = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_DASH_STRIKE;
                    sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                             (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                }
            }
            if (work->animationFrame == HELICOPTER_LANDING_PAD_NO_9_LETHAL_VIBRATION_FRAME) {
                padScriptSpawn(gAkropolisHelicopterLandingPadLethalStrikeVibrationCommands, &gAkropolisHelicopterLandingPadLethalStrikeVibrationSegment);
            }
            if (work->animationFrame == HELICOPTER_LANDING_PAD_NO_9_LETHAL_ATTACK_OFF_FRAME) {
                work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            animationValue = (u16)work->animationFrame;
            _helicopterLandingPadNo9SetLethalLapSpeed(work, animationValue, -0x40);
            if (work->animationFrame >= HELICOPTER_LANDING_PAD_NO_9_LETHAL_CONTACT_FIRST_FRAME) {
                if (work->attackLanded == 1) {
                    work->subState           = HELICOPTER_LANDING_PAD_NO_9_LETHAL_PLAYER_DEAD;
                    work->attackLanded       = 0;
                    work->deathSoundLoadStep = HELICOPTER_LANDING_PAD_NO_9_DEATH_SOUND_REQUEST;
                    gPlayerStatus.hp         = 0;
                    roomEffectRequestCancelPe();
                }
            } else {
                work->attackLanded = 0;
            }
            if (work->animationFrame >= HELICOPTER_LANDING_PAD_NO_9_LETHAL_MISS_END_FRAME) {
                work->state        = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
                work->subState     = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
                work->stateCounter = _gHelicopterLandingPadNo9PostAttackWaitFrames[((u32)(randomState = RANDOM_LCG_NEXT(gRandomLcgState)) >> 0x10) & 0xF];
                gRandomLcgState    = randomState;
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_LETHAL_PLAYER_DEAD:
            animationValue = work->animationId;
            if (animationValue == HELICOPTER_LANDING_PAD_NO_9_ANIM_LETHAL_STRIKE) {
                animationValue = (u16)work->animationFrame;
                _helicopterLandingPadNo9SetLethalLapSpeed(work, animationValue, -0x10);
                if (work->animationFrame >= HELICOPTER_LANDING_PAD_NO_9_LETHAL_RECOVER_FRAME) {
                    work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_SLASH_RECOVER;
                }
            }
            work->stateCounter++;
            if (work->stateCounter >= HELICOPTER_LANDING_PAD_NO_9_LETHAL_HIT_EFFECT_INTERVAL) {
                work->stateCounter                                = 0;
                _gHelicopterLandingPadNo9LethalHitEffectArg.coord = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
                effectSpawnHit(EFFECT_HIT_KIND_SPLATTER, &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[4], NULL,
                               &_gHelicopterLandingPadNo9LethalHitEffectArg);
            }
            switch (work->deathSoundLoadStep) {
                case HELICOPTER_LANDING_PAD_NO_9_DEATH_SOUND_IDLE:
                    break;
                case HELICOPTER_LANDING_PAD_NO_9_DEATH_SOUND_REQUEST:
                    cdCmdEnqueueDisplayResource(9, HELICOPTER_LANDING_PAD_NO_9_PLAYER_DEATH_SOUND_FILE, CD_COMMAND_DISPLAY_LOAD_DEFAULT);
                    work->deathSoundLoadStep = HELICOPTER_LANDING_PAD_NO_9_DEATH_SOUND_WAIT;
                    break;
                case HELICOPTER_LANDING_PAD_NO_9_DEATH_SOUND_WAIT:
                    if (cdCmdIsIdle() == 1) {
                        bodyCoord = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
                        sndEvtRequestScriptStart(SOUND_PLAYER_DEATH, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                                 (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                        work->deathSoundLoadStep = HELICOPTER_LANDING_PAD_NO_9_DEATH_SOUND_IDLE;
                    }
                    break;
            }
            break;
    }

    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// Steps the hit-recoil animation backward along the current lap side.
///
/// Retreat requires at least 200 world units already travelled on the player's
/// side. At the animation's end, zero HP selects death; otherwise patrol resumes
/// with a random waiting time in frames.
static void _helicopterLandingPadNo9TickRecoil(Task* task)
{
    HelicopterLandingPadNo9Work* work;
    Enemy*                       enemy;
    GfxCoord*                    bodyCoord;
    s32                          soundKey;
    s32                          audioPan;
    s32                          randomState;

    work      = task->work;
    enemy     = task->spawnArg2.pointer;
    bodyCoord = task->extra.tmd->coords;
    switch (work->subState) {
        case HELICOPTER_LANDING_PAD_NO_9_RECOIL_ENTER:
            work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_RECOIL;
            work->lapSpeed    = 0;
            work->subState    = HELICOPTER_LANDING_PAD_NO_9_RECOIL_PLAY;
            soundKey          = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_RECOIL;
            audioPan          = (s8)worldCoordGetOriginAudioPan(bodyCoord);
            sndEvtRequestScriptStart(soundKey, audioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            break;
        case HELICOPTER_LANDING_PAD_NO_9_RECOIL_PLAY:
            if (work->sideTravelled >= 0xC8 && work->playerSide == work->lapSide) {
                if ((u16)work->animationFrame >= 3 && (u16)work->animationFrame < 13) {
                    work->lapSpeed = -0x2C;
                } else if ((u16)work->animationFrame >= 0x12 && (u16)work->animationFrame < 0x27) {
                    work->lapSpeed = -0x1E;
                } else {
                    work->lapSpeed = 0;
                }
            } else {
                work->lapSpeed = 0;
            }
            if (work->animationFrame >= 0x53) {
                if (enemy->hp <= 0) {
                    work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_DEATH;
                    work->subState    = 0;
                    work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_DEATH;
                } else {
                    work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                    work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
                    work->subState    = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
                    work->stateCounter =
                        _gHelicopterLandingPadNo9ReactionWaitFrames[((u32)(randomState = RANDOM_LCG_NEXT(gRandomLcgState)) >> 16) & 0xF];
                    gRandomLcgState = randomState;
                }
            }
            break;
    }
}

/// Steps the helipad-blast recoil, trailing sparks and retreating along the lap.
///
/// Retreat occurs only on the player's side with at least 200 world units
/// behind the golem. Sparks occur on odd animation frames and the loop sound's
/// spatial mix updates every third tick. Death or a random patrol wait follows.
static void _helicopterLandingPadNo9TickSparkRecoil(Task* task)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_SPARK_RECOIL_RETREAT_FRAME  = 71,
        HELICOPTER_LANDING_PAD_NO_9_SPARK_RECOIL_RETREAT_FRAMES = 7,
        HELICOPTER_LANDING_PAD_NO_9_SPARK_RECOIL_RETREAT_SPEED  = -83,
        HELICOPTER_LANDING_PAD_NO_9_SPARK_RECOIL_END_FRAME      = 81,
    };
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoord;
    Enemy*                       enemy;
    SVECTOR*                     sparkOffset;
    s32                          startAudioPan;
    s32                          loopAudioPan;
    s16                          retreatSpeed;
    u32                          randomState;

    sparkOffset = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    work        = task->work;
    enemy       = task->spawnArg2.pointer;
    bodyCoord   = task->extra.tmd->coords;
    switch (work->subState) {
        case HELICOPTER_LANDING_PAD_NO_9_SPARK_RECOIL_ENTER:
            work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_SPARK_RECOIL;
            work->lapSpeed     = 0;
            work->subState     = HELICOPTER_LANDING_PAD_NO_9_SPARK_RECOIL_PLAY;
            work->stateCounter = 0;
            work->sparkSound   = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_SPARK_LOOP;
            startAudioPan      = (s8)worldCoordGetOriginAudioPan(bodyCoord);
            sndEvtRequestScriptStart(work->sparkSound, startAudioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            break;
        case HELICOPTER_LANDING_PAD_NO_9_SPARK_RECOIL_PLAY:
            if (work->animationFrame & 1) {
                _helicopterLandingPadNo9SpawnReactionSpark(bodyCoord, sparkOffset);
            }
            work->stateCounter++;
            if (work->stateCounter >= HELICOPTER_LANDING_PAD_NO_9_SPARK_MIX_INTERVAL) {
                loopAudioPan = (s8)worldCoordGetOriginAudioPan(bodyCoord);
                sndEvtRequestScriptMix(work->sparkSound, loopAudioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                work->stateCounter = 0;
            }
            if ((work->sideTravelled < HELICOPTER_LANDING_PAD_NO_9_LAP_MIN_DISTANCE) || (work->playerSide != work->lapSide)) {
                work->lapSpeed = 0;
            } else {
                retreatSpeed = 0;
                if ((u32)((u16)work->animationFrame - HELICOPTER_LANDING_PAD_NO_9_SPARK_RECOIL_RETREAT_FRAME) < HELICOPTER_LANDING_PAD_NO_9_SPARK_RECOIL_RETREAT_FRAMES) {
                    retreatSpeed = HELICOPTER_LANDING_PAD_NO_9_SPARK_RECOIL_RETREAT_SPEED;
                }
                work->lapSpeed = retreatSpeed;
            }
            if (work->animationFrame >= HELICOPTER_LANDING_PAD_NO_9_SPARK_RECOIL_END_FRAME) {
                if (work->sparkSound != 0) {
                    sndEvtRequestScriptStop(work->sparkSound, SOUND_SCRIPT_STOP_NO_FADE);
                    work->sparkSound = 0;
                }
                if (enemy->hp <= 0) {
                    work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_DEATH;
                    work->subState    = HELICOPTER_LANDING_PAD_NO_9_DEATH_ENTER;
                    work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_DEATH;
                } else {
                    work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                    work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
                    work->subState    = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
                    work->stateCounter =
                        _gHelicopterLandingPadNo9ReactionWaitFrames[((u32)(randomState = RANDOM_LCG_NEXT(gRandomLcgState)) >> 0x10) & 0xF];
                    gRandomLcgState = randomState;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Plays the stationary spark stun, followed by its recovery animation.
///
/// Sparks occur on odd animation frames; the loop sound follows the body every
/// third tick and stops at recovery. Recovery selects death or a random patrol
/// wait. Requires a live body task and its enemy and work records.
static void _helicopterLandingPadNo9TickSparkStun(Task* task)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_SPARK_STUN_END_FRAME = 30,
    };
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoord;
    Enemy*                       enemy;
    SVECTOR*                     sparkOffset;
    s32                          startAudioPan;
    s32                          loopAudioPan;
    u32                          randomState;

    sparkOffset = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    work        = task->work;
    enemy       = task->spawnArg2.pointer;
    bodyCoord   = task->extra.tmd->coords;
    switch (work->subState) {
        case HELICOPTER_LANDING_PAD_NO_9_SPARK_STUN_ENTER:
            work->lapSpeed     = 0;
            work->animationId  = HELICOPTER_LANDING_PAD_NO_9_ANIM_STUN;
            work->subState     = HELICOPTER_LANDING_PAD_NO_9_SPARK_STUN_PLAY;
            work->stateCounter = 0;
            work->sparkSound   = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_SPARK_LOOP;
            startAudioPan      = (s8)worldCoordGetOriginAudioPan(bodyCoord);
            sndEvtRequestScriptStart(work->sparkSound, startAudioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            break;
        case HELICOPTER_LANDING_PAD_NO_9_SPARK_STUN_PLAY:
            if (work->animationFrame & 1) {
                _helicopterLandingPadNo9SpawnReactionSpark(bodyCoord, sparkOffset);
            }
            work->stateCounter++;
            if (work->stateCounter >= HELICOPTER_LANDING_PAD_NO_9_SPARK_MIX_INTERVAL) {
                loopAudioPan = (s8)worldCoordGetOriginAudioPan(bodyCoord);
                sndEvtRequestScriptMix(work->sparkSound, loopAudioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                work->stateCounter = 0;
            }
            if (work->animationFrame >= HELICOPTER_LANDING_PAD_NO_9_SPARK_STUN_END_FRAME) {
                if (work->sparkSound != 0) {
                    sndEvtRequestScriptStop(work->sparkSound, SOUND_SCRIPT_STOP_NO_FADE);
                    work->sparkSound = 0;
                }
                work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_STUN_RECOVER;
                work->subState    = HELICOPTER_LANDING_PAD_NO_9_SPARK_STUN_RECOVER;
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_SPARK_STUN_RECOVER:
            if (work->animationFrame >= HELICOPTER_LANDING_PAD_NO_9_STUN_RECOVER_END_FRAME) {
                if (enemy->hp <= 0) {
                    work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_DEATH;
                    work->subState    = HELICOPTER_LANDING_PAD_NO_9_DEATH_ENTER;
                    work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_DEATH;
                } else {
                    work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                    work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
                    work->subState    = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
                    work->stateCounter =
                        _gHelicopterLandingPadNo9ReactionWaitFrames[((u32)(randomState = RANDOM_LCG_NEXT(gRandomLcgState)) >> 0x10) & 0xF];
                    gRandomLcgState = randomState;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Silences the dying golem, removes its target and marks it absent.
///
/// If the player has already died, restores one enemy HP and resumes patrol.
/// Otherwise the death animation's frame 112 queues the flame-strike sound.
/// Model and collision teardown remain the task's responsibility.
static void _helicopterLandingPadNo9TickDeath(Task* task)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_DEATH_SOUND_FRAME = 112,
    };
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoord;
    s32                          soundKey;
    s32                          audioPan;
    u32                          randomState;

    work      = task->work;
    bodyCoord = task->extra.tmd->coords;
    // A simultaneous player death keeps the golem's fight alive.
    if (gPlayerStatus.hp <= 0) {
        ((Enemy*)task->spawnArg2.pointer)->hp = 1;
        work->state                           = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
        work->animationId                     = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
        work->subState                        = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
        work->stateCounter                    = _gHelicopterLandingPadNo9ReactionWaitFrames[((u32)(randomState = RANDOM_LCG_NEXT(gRandomLcgState)) >> 16) & 0xF];
        gRandomLcgState                       = randomState;
        return;
    }
    if (work->flameMode != HELICOPTER_LANDING_PAD_NO_9_FLAME_OFF) {
        work->flameMode = HELICOPTER_LANDING_PAD_NO_9_FLAME_DYING;
    }
    work->flameFrames = 0;
    if (work->sparkSound != 0) {
        sndEvtRequestScriptStop(work->sparkSound, SOUND_SCRIPT_STOP_NO_FADE);
        work->sparkSound = 0;
    }
    if (work->flameSound != 0) {
        sndEvtRequestScriptStop(work->flameSound, SOUND_SCRIPT_STOP_NO_FADE);
        work->flameSound = 0;
    }
    if (work->eventFlameSound != 0) {
        sndEvtRequestScriptStop(work->eventFlameSound, SOUND_SCRIPT_STOP_NO_FADE);
        work->eventFlameSound = 0;
    }
    ((Enemy*)task->spawnArg2.pointer)->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    if (work->animationFrame == HELICOPTER_LANDING_PAD_NO_9_DEATH_SOUND_FRAME) {
        soundKey = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_FLAME_STRIKE;
        audioPan = (s8)worldCoordGetOriginAudioPan(bodyCoord);
        sndEvtRequestScriptStart(soundKey, audioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
    }
    gAttachmentState.flags &= ATTACHMENT_FLAG_EVENT_LOCK;
    work->present           = 0;
}

/// Updates the player's lap side and the horizontal ranges used by combat.
///
/// The first strip containing the player's world X/Z wins; if none contains
/// them, the previous side is retained. Distances use world units and the body
/// root's local translation, which is in the same world frame here. Side
/// ranges refer to the current lap position before this frame's movement.
static void _helicopterLandingPadNo9UpdatePlayerRange(Task* task)
{
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoord;
    VECTOR*                      playerOffset;
    s32                          stripIndex;
    s32                          deltaX;
    s32                          deltaZ;

    work         = task->work;
    bodyCoord    = task->extra.tmd->coords;
    playerOffset = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);

    // Outside all strips, preserve the last side that contained the player.
    for (stripIndex = 0; stripIndex < ARRAY_SIZE(_gHelicopterLandingPadNo9LapStrips); stripIndex++) {
        if (_gHelicopterLandingPadNo9LapStrips[stripIndex].minX < gPlayerStatus.coordMtx->t[0] &&
            gPlayerStatus.coordMtx->t[0] < _gHelicopterLandingPadNo9LapStrips[stripIndex].maxX &&
            _gHelicopterLandingPadNo9LapStrips[stripIndex].minZ < gPlayerStatus.coordMtx->t[2] &&
            gPlayerStatus.coordMtx->t[2] < _gHelicopterLandingPadNo9LapStrips[stripIndex].maxZ) {
            work->playerSide = stripIndex;
            break;
        }
    }

    work->sideTravelled = __builtin_abs(work->lapSide * HELICOPTER_LANDING_PAD_NO_9_LAP_SIDE_LENGTH - work->lapDistance);
    work->sideRemaining = __builtin_abs((work->lapSide + 1) * HELICOPTER_LANDING_PAD_NO_9_LAP_SIDE_LENGTH - work->lapDistance);

    deltaX               = gPlayerStatus.coordMtx->t[0] - bodyCoord->coord.t[0];
    playerOffset->vx     = deltaX;
    playerOffset->vy     = 0;
    deltaZ               = gPlayerStatus.coordMtx->t[2] - bodyCoord->coord.t[2];
    playerOffset->vz     = deltaZ;
    work->playerDistance = SquareRoot0(deltaX * deltaX + deltaZ * deltaZ);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Turns a moving golem towards its current or approaching lap side's heading.
///
/// Within 1000 world units of a corner, targets the following side. Turns by
/// at most 30 angle units per frame (4096 per turn), wrapping across zero.
/// The lap's terminal clamp keeps the following-side index within four entries.
/// Replaces root rotation only; the caller invalidates coordinate composition.
static void _helicopterLandingPadNo9TurnAlongLap(Task* task)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_LAP_TURN_DISTANCE = 1000,
        HELICOPTER_LANDING_PAD_NO_9_LAP_TURN_STEP     = 30,
    };
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoord;
    SVECTOR*                     rotation;
    u16                          targetYaw;
    u16                          currentYaw;
    s32                          yawDifference;
    s16                          shortDifference;
    s32                          signedDifference;
    s32                          differenceMagnitude;
    s32                          previousYaw;
    s32                          wrappedPreviousYaw;

    work      = task->work;
    bodyCoord = task->extra.tmd->coords;
    if (work->lapSpeed != 0) {
        rotation = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
        if (work->sideRemaining < HELICOPTER_LANDING_PAD_NO_9_LAP_TURN_DISTANCE) {
            targetYaw = _gHelicopterLandingPadNo9LapYaws[work->lapSide + 1];
        } else {
            targetYaw = _gHelicopterLandingPadNo9LapYaws[work->lapSide];
        }
        currentYaw          = ratan2(bodyCoord->coord.m[0][2], bodyCoord->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
        yawDifference       = targetYaw - currentYaw;
        shortDifference     = yawDifference;
        signedDifference    = (s16)yawDifference;
        differenceMagnitude = __builtin_abs(signedDifference);
        work->yaw           = currentYaw;
        if (differenceMagnitude < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
            if (differenceMagnitude < (HELICOPTER_LANDING_PAD_NO_9_LAP_TURN_STEP + 1)) {
                work->yaw = targetYaw;
            } else {
                previousYaw = work->yaw;
                if (signedDifference > 0) {
                    work->yaw = previousYaw + HELICOPTER_LANDING_PAD_NO_9_LAP_TURN_STEP;
                } else {
                    work->yaw = previousYaw - HELICOPTER_LANDING_PAD_NO_9_LAP_TURN_STEP;
                }
            }
        } else if (signedDifference > 0 ? (ACTOR_TRANSFORM_ANGLE_TURN - signedDifference) < (HELICOPTER_LANDING_PAD_NO_9_LAP_TURN_STEP + 1) : (signedDifference + ACTOR_TRANSFORM_ANGLE_TURN) < (HELICOPTER_LANDING_PAD_NO_9_LAP_TURN_STEP + 1)) {
            work->yaw = targetYaw;
        } else {
            wrappedPreviousYaw = work->yaw;
            if (shortDifference > 0) {
                work->yaw = wrappedPreviousYaw - HELICOPTER_LANDING_PAD_NO_9_LAP_TURN_STEP;
            } else {
                work->yaw = wrappedPreviousYaw + HELICOPTER_LANDING_PAD_NO_9_LAP_TURN_STEP;
            }
        }
        rotation->vx = 0;
        rotation->vy = work->yaw;
        rotation->vz = 0;
        RotMatrix(rotation, &bodyCoord->coord);
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    }
}

/// Advances the body along its square lap and writes its world position.
///
/// Adds the signed speed modulo 65536, then clamps distance to 200..46700
/// world units. The quotient and remainder by the 13200-unit side length select
/// side 0..3 and the distance from its corner. Height is reset to zero.
static void _helicopterLandingPadNo9AdvanceAlongLap(Task* task)
{
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoord;
    s16                          lapSide;
    s16                          sideDistance;
    u16                          nextDistance;

    work      = task->work;
    bodyCoord = task->extra.tmd->coords;

    // Narrow the sum before applying the lap's terminal bounds.
    nextDistance      = work->lapDistance + (u16)work->lapSpeed;
    work->lapDistance = nextDistance;
    if (nextDistance < HELICOPTER_LANDING_PAD_NO_9_LAP_MIN_DISTANCE) {
        work->lapDistance = HELICOPTER_LANDING_PAD_NO_9_LAP_MIN_DISTANCE;
    } else if (nextDistance > HELICOPTER_LANDING_PAD_NO_9_LAP_MAX_DISTANCE) {
        work->lapDistance = HELICOPTER_LANDING_PAD_NO_9_LAP_MAX_DISTANCE;
    }

    lapSide       = work->lapDistance / HELICOPTER_LANDING_PAD_NO_9_LAP_SIDE_LENGTH;
    work->lapSide = lapSide;
    sideDistance  = work->lapDistance % HELICOPTER_LANDING_PAD_NO_9_LAP_SIDE_LENGTH;

    bodyCoord->coord.t[0] = _gHelicopterLandingPadNo9LapCorners[lapSide].x + (sideDistance * _gHelicopterLandingPadNo9LapDirections[lapSide].x);
    bodyCoord->coord.t[1] = 0;
    bodyCoord->coord.t[2] =
        _gHelicopterLandingPadNo9LapCorners[work->lapSide].z + (sideDistance * _gHelicopterLandingPadNo9LapDirections[work->lapSide].z);
}

/// Plays step sounds when slot 1's animation cue bits fall.
///
/// Cue 2 and cue 1 select separate placement-keyed scripts, spatially mixed at
/// the body coordinate. Saves the current cue mask only when a record exists;
/// a missing record preserves the preceding frame's latch.
static void _helicopterLandingPadNo9PlayStepSounds(Task* task)
{
    s32                          soundKey;
    s32                          cue2AudioPan;
    s32                          cue1AudioPan;
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoord;
    const AnimationRecord*       record;

    work      = task->work;
    bodyCoord = task->extra.tmd->coords;
    record    = animationGetCurrentRecord(&work->rig.anim, &work->rig.slots[1]);
    if (record != NULL) {
        if (!(record->flags & ANIMATION_RECORD_CUE_2) && (work->lastCueFlags & ANIMATION_RECORD_CUE_2)) {
            soundKey     = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_STEP_CUE_2;
            cue2AudioPan = (s8)worldCoordGetOriginAudioPan(bodyCoord);
            sndEvtRequestScriptStart(soundKey, cue2AudioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
        }
        if (!(record->flags & ANIMATION_RECORD_CUE_1) && (work->lastCueFlags & ANIMATION_RECORD_CUE_1)) {
            soundKey     = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_STEP_CUE_1;
            cue1AudioPan = (s8)worldCoordGetOriginAudioPan(bodyCoord);
            sndEvtRequestScriptStart(soundKey, cue1AudioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
        }
        work->lastCueFlags = (u16)(record->flags & ANIMATION_RECORD_CUE_MASK);
    }
}

#include "../../../../shared/creatures/golem/no9_golem_aim_head.inc.c"

/// Applies the residual hit rotation to the chest and relaxes pitch and yaw.
///
/// Post-multiplies model part 3's animated rotation, then moves each residual
/// angle up to 32 units towards zero (4096 per turn). Clears `hitTwistActive`
/// when both residuals have reached zero. Requires the live body animation pose.
static void _helicopterLandingPadNo9ApplyHitTwist(Task* task)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_HIT_TWIST_STEP = 32,
    };
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoords;
    MATRIX*                      twistMatrix;
    s32                          pitch;
    s32                          yaw;
    s32                          pitchMagnitude;
    s32                          nextPitch;
    s32                          yawMagnitude;
    s32                          nextYaw;
    s32                          stillTwisted;

    twistMatrix  = SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    stillTwisted = 0;
    work         = task->work;
    bodyCoords   = task->extra.tmd->coords;
    // Apply this frame's residual before relaxing it for the next frame.
    RotMatrix(&work->hitTwist, twistMatrix);
    _helicopterLandingPadNo9ComposeHitRotation(&bodyCoords[HELICOPTER_LANDING_PAD_NO_9_CHEST_PART].coord, twistMatrix);

    pitch = work->hitTwist.vx;
    if (pitch != 0) {
        pitchMagnitude = __builtin_abs(pitch);
        if (pitchMagnitude < (HELICOPTER_LANDING_PAD_NO_9_HIT_TWIST_STEP + 1)) {
            work->hitTwist.vx = 0;
        } else {
            nextPitch = pitch - HELICOPTER_LANDING_PAD_NO_9_HIT_TWIST_STEP;
            if (pitch <= 0) {
                nextPitch = pitch + HELICOPTER_LANDING_PAD_NO_9_HIT_TWIST_STEP;
            }
            work->hitTwist.vx = nextPitch;
            stillTwisted      = 1;
        }
    }

    yaw = work->hitTwist.vy;
    if (yaw != 0) {
        yawMagnitude = __builtin_abs(yaw);
        if (yawMagnitude < (HELICOPTER_LANDING_PAD_NO_9_HIT_TWIST_STEP + 1)) {
            work->hitTwist.vy = 0;
        } else {
            nextYaw = yaw - HELICOPTER_LANDING_PAD_NO_9_HIT_TWIST_STEP;
            if (yaw <= 0) {
                nextYaw = yaw + HELICOPTER_LANDING_PAD_NO_9_HIT_TWIST_STEP;
            }
            work->hitTwist.vy = nextYaw;
            stillTwisted      = 1;
        }
    }

    if (stillTwisted == 0) {
        work->hitTwistActive = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

/// Carries the golem's three wall faces to its current position and facing.
///
/// The room grid must provide the first twelve vertices and three normals.
/// Rotates the faces' local origin offset, translates it by the body and clamps
/// its world X/Z to +/-6000. Writes rotated corners around that origin and
/// rotated normals; face indices and vector fourth halfwords stay intact.
static void _helicopterLandingPadNo9UpdateGridFaces(Task* task)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_GRID_ORIGIN_OFFSET_X = -600,
        HELICOPTER_LANDING_PAD_NO_9_GRID_ORIGIN_OFFSET_Z = 640,
        HELICOPTER_LANDING_PAD_NO_9_GRID_ORIGIN_LIMIT    = 6000,
    };
    _HelicopterLandingPadNo9GridFacesScratch* scratch;
    GfxCoord*                                 bodyCoord;
    SVECTOR*                                  normals;
    SVECTOR*                                  vertices;
    s32                                       vectorIndex;

    scratch   = SCRATCH_STACK_RESERVE_BLOCK(_HelicopterLandingPadNo9GridFacesScratch);
    bodyCoord = task->extra.tmd->coords;
    normals   = gWorldCollisionActiveGrid->normals;
    vertices  = gWorldCollisionActiveGrid->vertices;

    scratch->origin.vx = HELICOPTER_LANDING_PAD_NO_9_GRID_ORIGIN_OFFSET_X;
    scratch->origin.vy = 0;
    scratch->origin.vz = HELICOPTER_LANDING_PAD_NO_9_GRID_ORIGIN_OFFSET_Z;

    gte_SetRotMatrix(&bodyCoord->coord);
    gte_ldv0(&scratch->origin);
    gte_rtv0();
    gte_stsv(&scratch->rotated);

    scratch->origin.vx = bodyCoord->coord.t[0] + scratch->rotated.vx;
    scratch->origin.vy = bodyCoord->coord.t[1] + scratch->rotated.vy;
    scratch->origin.vz = bodyCoord->coord.t[2] + scratch->rotated.vz;

    if (scratch->origin.vx > HELICOPTER_LANDING_PAD_NO_9_GRID_ORIGIN_LIMIT) {
        scratch->origin.vx = HELICOPTER_LANDING_PAD_NO_9_GRID_ORIGIN_LIMIT;
    } else if (scratch->origin.vx < -HELICOPTER_LANDING_PAD_NO_9_GRID_ORIGIN_LIMIT) {
        scratch->origin.vx = -HELICOPTER_LANDING_PAD_NO_9_GRID_ORIGIN_LIMIT;
    }
    if (scratch->origin.vz > HELICOPTER_LANDING_PAD_NO_9_GRID_ORIGIN_LIMIT) {
        scratch->origin.vz = HELICOPTER_LANDING_PAD_NO_9_GRID_ORIGIN_LIMIT;
    } else if (scratch->origin.vz < -HELICOPTER_LANDING_PAD_NO_9_GRID_ORIGIN_LIMIT) {
        scratch->origin.vz = -HELICOPTER_LANDING_PAD_NO_9_GRID_ORIGIN_LIMIT;
    }

    for (vectorIndex = 0; vectorIndex < ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DBC); vectorIndex++) {
        gte_SetRotMatrix(&bodyCoord->coord);
        gte_ldv0(&_gHelicopterLandingPadNo9Collision35DBC[vectorIndex]);
        gte_rtv0();
        gte_stsv(&scratch->rotated);
        vertices[vectorIndex].vx = scratch->rotated.vx + scratch->origin.vx;
        vertices[vectorIndex].vy = scratch->rotated.vy + scratch->origin.vy;
        vertices[vectorIndex].vz = scratch->rotated.vz + scratch->origin.vz;
    }

    for (vectorIndex = 0; vectorIndex < ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DA4); vectorIndex++) {
        gte_SetRotMatrix(&bodyCoord->coord);
        gte_ldv0(&_gHelicopterLandingPadNo9Collision35DA4[vectorIndex]);
        gte_rtv0();
        gte_stsv(&normals[vectorIndex]);
    }

    SCRATCH_STACK_RELEASE_BLOCK(_HelicopterLandingPadNo9GridFacesScratch);
}

/// Handles message 2007: show (0), start the fight (1), or hide (2).
///
/// Requires the live body, weapon and chest-model tasks and the helipad grid.
/// Showing acquires a battle reference. Starting resets the lap and animation
/// and ignites the flame only if it has never burned. Hiding releases the flame
/// task, disables the three spheres and restores the grid's first three faces,
/// then clears the fourth face's normal and corners. Other commands do nothing.
/// The message ID and second payload are unused. Returns 0.
static s32 _helicopterLandingPadNo9SetActivation(Task* task, s32 messageId, s32 command, s32 unusedArg)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_ACTIVATION_SHOW        = 0,
        HELICOPTER_LANDING_PAD_NO_9_ACTIVATION_START_FIGHT = 1,
        HELICOPTER_LANDING_PAD_NO_9_ACTIVATION_HIDE        = 2,
        HELICOPTER_LANDING_PAD_NO_9_PUT_AWAY               = 0,
        HELICOPTER_LANDING_PAD_NO_9_SHOWN                  = 1,
        HELICOPTER_LANDING_PAD_NO_9_FIGHTING               = 2,
        HELICOPTER_LANDING_PAD_NO_9_FIGHT_START_DISTANCE   = 4600,
        HELICOPTER_LANDING_PAD_NO_9_FIGHT_START_YAW        = 1024,
        HELICOPTER_LANDING_PAD_NO_9_INITIAL_FLAME_FRAMES   = 240,
    };
    HelicopterLandingPadNo9Work* work;
    TmdObject*                   bodyModel;
    GfxCoord*                    bodyCoord;
    Enemy*                       enemy;
    SVECTOR*                     rotation;
    SVECTOR*                     normals;
    SVECTOR*                     vertices;
    WorldCollisionGridFace*      faces;
    s32                          slotIndex;
    s32                          vectorIndex;
    s32                          cornerIndex;
    SVECTOR*                     vector;
    SVECTOR*                     extraVertices;

    rotation  = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    bodyModel = task->extra.tmd;
    work      = task->work;
    enemy     = task->spawnArg2.pointer;
    bodyCoord = bodyModel->coords;

    switch (command) {
        case HELICOPTER_LANDING_PAD_NO_9_ACTIVATION_SHOW:
            sceneAcquireBattleRef(0x1B);
            bodyModel->flags                       = 0;
            work->weaponTask->extra.tmd->flags     = 0;
            work->chestModelTask->extra.tmd->flags = 0;
            work->activation                       = HELICOPTER_LANDING_PAD_NO_9_SHOWN;
            break;
        case HELICOPTER_LANDING_PAD_NO_9_ACTIVATION_START_FIGHT:
            bodyModel->flags                       = 0;
            work->weaponTask->extra.tmd->flags     = 0;
            work->chestModelTask->extra.tmd->flags = 0;
            work->activation                       = HELICOPTER_LANDING_PAD_NO_9_FIGHTING;
            work->lapDistance                      = HELICOPTER_LANDING_PAD_NO_9_FIGHT_START_DISTANCE;
            vector                                 = rotation;
            work->animationId                      = HELICOPTER_LANDING_PAD_NO_9_ANIM_FIGHT_START;
            work->seededAnimationId                = HELICOPTER_LANDING_PAD_NO_9_ANIM_FIGHT_START;
            work->yaw                              = HELICOPTER_LANDING_PAD_NO_9_FIGHT_START_YAW;
            work->state                            = HELICOPTER_LANDING_PAD_NO_9_STATE_OPENING;
            work->subState                         = HELICOPTER_LANDING_PAD_NO_9_OPENING_INTRO;
            work->lapSpeed                         = 0;
            work->animationFrame                   = 0;
            work->body.flags                      |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            enemy->node.state.parts.flags          = WORLD_TARGET_HIDE_HP;
            vector->vx                             = 0;
            vector->vy                             = work->yaw;
            vector->vz                             = 0;
            RotMatrix(vector, &bodyCoord->coord);
            bodyCoord->coord.t[0] = 0;
            bodyCoord->coord.t[1] = 0;
            bodyCoord->coord.t[2] = 0;
            for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationResetSlot(&work->rig.anim, slotIndex, work->animationId);
            }
            if (work->flameMode == HELICOPTER_LANDING_PAD_NO_9_FLAME_OFF) {
                work->flameMode = HELICOPTER_LANDING_PAD_NO_9_FLAME_BURNING;
                if (work->flameJetTask != NULL) {
                    work->flameJetTask->spawnArg1.value = HELICOPTER_LANDING_PAD_NO_9_FLAME_BURNING;
                }
                work->flameFrames = HELICOPTER_LANDING_PAD_NO_9_INITIAL_FLAME_FRAMES;
                _helicopterLandingPadNo9ArmAttack(work, HELICOPTER_LANDING_PAD_NO_9_ATTACK_IGNITION);
                work->flameSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_FLAME_LOOP;
                sndEvtRequestScriptStart(work->flameSound, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_ACTIVATION_HIDE:
            bodyModel->flags                       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->weaponTask->extra.tmd->flags     = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->chestModelTask->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->activation                       = HELICOPTER_LANDING_PAD_NO_9_PUT_AWAY;
            if (work->flameJetTask != NULL) {
                work->flameJetTask->spawnArg1.value = HELICOPTER_LANDING_PAD_NO_9_FLAME_RELEASED;
            }
            work->flameJetTask            = NULL;
            work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->weaponAttack.flags     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->forearmAttack.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;

            // Restore the moving faces before clearing the additional face.
            normals  = gWorldCollisionActiveGrid->normals;
            vertices = gWorldCollisionActiveGrid->vertices;
            faces    = gWorldCollisionActiveGrid->faces;
            for (vectorIndex = 0; vectorIndex < ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DBC); vectorIndex++) {
                vertices[vectorIndex] = _gHelicopterLandingPadNo9Collision35DBC[vectorIndex];
            }
            for (vectorIndex = 0; vectorIndex < ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DA4); vectorIndex++) {
                normals[vectorIndex] = _gHelicopterLandingPadNo9Collision35DA4[vectorIndex];
                faces[vectorIndex]   = _gHelicopterLandingPadNo9Collision35E1C[vectorIndex];
            }

            do {
                vector = gWorldCollisionActiveGrid->normals;
            } while (0);
            extraVertices                                                  = gWorldCollisionActiveGrid->vertices;
            vector[ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DA4)].vx = 0;
            vector[ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DA4)].vy = 0;
            vector[ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DA4)].vz = 0;
            for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(_gHelicopterLandingPadNo9ExtraGridVertices); cornerIndex++) {
                extraVertices[ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DBC) + cornerIndex].vx = 0;
                extraVertices[ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DBC) + cornerIndex].vy = 0;
                extraVertices[ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DBC) + cornerIndex].vz = 0;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    return 0;
}

/// Reveals and tosses the off-hand prop during the parent's event animations.
///
/// `task->parent` must own a live nineteen-part golem model and work block.
/// The prop borrows that work without allocating its own. Each 40-frame toss
/// captures the hand transform relative to the view at frame 14, follows a
/// parabola in that frame, then reattaches to the hand. The enemy argument is
/// unused but retains the two-argument enemy-state callback contract.
static void _helicopterLandingPadNo9UpdateProp(Enemy* enemy, Task* task)
{
    // The reserved 32-byte scratch frame has no directly accessed object here.
    enum { HELICOPTER_LANDING_PAD_NO_9_PROP_SCRATCH_BYTES = 0x20 };
    TmdObject*                   propModel;
    HelicopterLandingPadNo9Work* parentWork;
    GfxCoord*                    propCoord;
    GfxCoord*                    handCoord;
    s16                          animationFrame;
    s16                          tossFrame;
    s32                          heightOffset;

    parentWork = task->parent->work;
    propModel  = task->extra.tmd;
    propCoord  = propModel->coords;
    if (parentWork->animationId < HELICOPTER_LANDING_PAD_NO_9_ANIM_EVENT_1) {
        propModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        return;
    }
    SCRATCH_STACK_RESERVE_BYTES(HELICOPTER_LANDING_PAD_NO_9_PROP_SCRATCH_BYTES);
    switch (parentWork->animationId) {
        case HELICOPTER_LANDING_PAD_NO_9_ANIM_EVENT_1:
        case HELICOPTER_LANDING_PAD_NO_9_ANIM_EVENT_2:
            break;

        case HELICOPTER_LANDING_PAD_NO_9_ANIM_EVENT_PROP_REVEAL:
            if (parentWork->animationFrame == 0xC8) {
                propModel->flags = 0;
            }
            break;

        case HELICOPTER_LANDING_PAD_NO_9_ANIM_EVENT_PROP_TOSS:
            handCoord      = &task->parent->extra.tmd->coords[HELICOPTER_LANDING_PAD_NO_9_PROP_HAND_PART];
            animationFrame = parentWork->animationFrame;
            if (animationFrame < 0x50) {
                tossFrame = animationFrame % 40;
                if (tossFrame < 0xF) {
                    _helicopterLandingPadNo9AttachProp(propCoord, handCoord);
                    if (tossFrame == 0xE) {
                        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &handCoord->workm, &parentWork->propTossMtx);
                    }
                } else {
                    // Release into the view frame from the cached hand transform.
                    tossFrame              = tossFrame - 0xF;
                    heightOffset           = ((tossFrame - 0xC) * (tossFrame - 0xC) * 3) - 0x1B0;
                    propCoord->coord       = parentWork->propTossMtx;
                    propCoord->parent      = &gGfxViewCoord;
                    propCoord->coord.t[1] += heightOffset;
                }
                propCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(propCoord);
            } else if (animationFrame == 0x52) {
                _helicopterLandingPadNo9AttachProp(propCoord, handCoord);
                propCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(propCoord);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(HELICOPTER_LANDING_PAD_NO_9_PROP_SCRATCH_BYTES);
}

/// Initializes the golem's chest-launched stun grenade and its collision bodies.
///
/// Requires a live parent body and player. Allocates cleared grenade-owned work
/// and lighting, then selects its pitch step from horizontal player distance in
/// 1000-unit buckets, clamped to the final table entry. Failure destroys the
/// grenade enemy and task. Successful initialization starts flight. The body
/// remains its task parent; burst effects are adopted as grenade children.
static void _helicopterLandingPadNo9InitGrenade(Enemy* enemy, Task* task)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_PITCH_BUCKET_DISTANCE = 1000,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_FLIGHT_RADIUS         = 200,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_GRID_PROBE_LENGTH     = 500,
    };

    _HelicopterLandingPadNo9GrenadeWork* work;
    ActorChildPlaceScratch*              placement;
    TmdObject*                           grenadeModel;
    GfxCoord*                            grenadeCoord;
    GfxCoord*                            bodyCoords;
    GfxCoord*                            chestCoord;
    s32                                  playerDeltaX;
    s32                                  playerDeltaZ;
    s32                                  pitchBucket;

    grenadeModel = task->extra.tmd;
    grenadeCoord = grenadeModel->coords;
    bodyCoords   = task->parent->extra.tmd->coords;
    chestCoord   = &bodyCoords[HELICOPTER_LANDING_PAD_NO_9_CHEST_PART];
    work         = memCalloc(sizeof(_HelicopterLandingPadNo9GrenadeWork), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work             = work;
    grenadeModel->flags    = 0;
    placement              = SCRATCH_STACK_RESERVE_BLOCK(ActorChildPlaceScratch);
    grenadeModel->lightMtx = &work->lightMtx;
    grenadeModel->colorMtx = &work->colorMtx;

    _helicopterLandingPadNo9PlaceGrenade(grenadeCoord, chestCoord, placement);

    playerDeltaX         = gPlayerStatus.coordMtx->t[0] - grenadeCoord->coord.t[0];
    placement->offset.vy = 0;
    placement->offset.vx = playerDeltaX;
    playerDeltaZ         = gPlayerStatus.coordMtx->t[2] - grenadeCoord->coord.t[2];
    placement->offset.vz = playerDeltaZ;
    pitchBucket          = SquareRoot0((playerDeltaX * playerDeltaX) + (playerDeltaZ * playerDeltaZ)) / HELICOPTER_LANDING_PAD_NO_9_GRENADE_PITCH_BUCKET_DISTANCE;
    if (pitchBucket >= ARRAY_SIZE(_gHelicopterLandingPadNo9GrenadePitchStepsByRange)) {
        pitchBucket = ARRAY_SIZE(_gHelicopterLandingPadNo9GrenadePitchStepsByRange) - 1;
    }

    work->phaseCounter = _gHelicopterLandingPadNo9GrenadePitchStepsByRange[pitchBucket];
    // A sphere catches the player; the capsule tests the room grid only.
    work->attack.coord            = grenadeCoord;
    work->attack.context.contacts = work->attackContacts;
    work->attack.pos.vx           = 0;
    work->attack.pos.vy           = 0;
    work->attack.pos.vz           = 0;
    work->attack.key              = 0;
    work->attack.radius           = HELICOPTER_LANDING_PAD_NO_9_GRENADE_FLIGHT_RADIUS;
    work->attack.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attack);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);

    work->gridProbeCapsule.ends[0].vx = 0;
    work->gridProbeCapsule.ends[0].vy = 0;
    work->gridProbeCapsule.ends[0].vz = 0;
    work->gridProbeCapsule.ends[1].vx = 0;
    work->gridProbeCapsule.ends[1].vy = HELICOPTER_LANDING_PAD_NO_9_GRENADE_GRID_PROBE_LENGTH;
    work->gridProbeCapsule.ends[1].vz = 0;
    work->gridProbeCapsule.end0Radius = 1;
    work->gridProbeCapsule.end1Radius = 1;
    work->gridProbeCapsule.contacts   = work->gridProbeContacts;
    work->gridProbe.context.capsule   = &work->gridProbeCapsule;
    work->gridProbe.coord             = grenadeCoord;
    work->gridProbe.pos.vx            = 0;
    work->gridProbe.pos.vy            = 0;
    work->gridProbe.pos.vz            = 0;
    work->gridProbe.key               = 0;
    work->gridProbe.radius            = 0;
    work->gridProbe.flags             = WORLD_COLLISION_BODY_CAPSULE;
    work->attack.flags               |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->gridProbe);
    worldCollisionInitContacts(work->gridProbeContacts, ARRAY_SIZE(work->gridProbeContacts), 0);
    work->gridProbe.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;

    task->state = HELICOPTER_LANDING_PAD_NO_9_CHILD_TASK_RUNNING;
    SCRATCH_STACK_RELEASE_BLOCK(ActorChildPlaceScratch);
}

/// Flies the grenade along its pitching launch axis until it bursts.
///
/// Requires the live grenade model, owned work and body parent; the enemy argument
/// is unused. Moves 150 coordinate units per update, emitting smoke every third
/// update. Descending through the height threshold, a player contact or a grid
/// contact hides the model and starts spreading or holding. The impact effect is
/// adopted beneath the grenade. Parent disappearance starts delayed teardown;
/// paused combat refreshes colour and hidden combat only hides the model.
static void _helicopterLandingPadNo9TickGrenadeFlight(Enemy* enemy, Task* task)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_FLIGHT_STEP            = 150,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_ROTATION_FRACTION_BITS = 12,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_HEIGHT           = 1300,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_TRAIL_INTERVAL         = 3,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_TRAIL_OFFSET_Y         = 100,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_TRAIL_OPTIONS          = 0x01001600,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_EXPLOSION_OPTIONS      = 0x10002200,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_SMOKE_OPTIONS    = 0xC1001200,
        HELICOPTER_LANDING_PAD_NO_9_SOUND_GRENADE_BURST            = 0x51100009,
    };
    VECTOR                               samplePosition;
    EffectWork*                          burstEffect;
    GfxCoord*                            grenadeCoord;
    ActorEulerTurnScratch*               pitchScratch;
    TmdObject*                           grenadeModel;
    _HelicopterLandingPadNo9GrenadeWork* work;
    HelicopterLandingPadNo9Work*         parentWork;
    s32                                  pitchStep;
    s32                                  soundKey;
    s32                                  crossedBurstHeight;
    u16                                  trailFrames;

    grenadeModel       = task->extra.tmd;
    work               = task->work;
    grenadeCoord       = grenadeModel->coords;
    parentWork         = task->parent->work;
    crossedBurstHeight = 0;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            grenadeModel->flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            samplePosition.vx = grenadeCoord->workm.t[0];
            samplePosition.vy = grenadeCoord->workm.t[1];
            samplePosition.vz = grenadeCoord->workm.t[2];
            worldCoordUpdateActorColor(task->spawnArg2.pointer, &samplePosition, 0, 0);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            grenadeModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }

    // Pitch the launch basis and travel along its negative Y axis.
    pitchScratch            = SCRATCH_STACK_RESERVE_BLOCK(ActorEulerTurnScratch);
    pitchStep               = -work->phaseCounter;
    pitchScratch->angles.vx = pitchStep;
    pitchScratch->angles.vy = 0;
    pitchScratch->angles.vz = 0;
    RotMatrix(&pitchScratch->angles, &pitchScratch->rotation);
    gte_MulMatrix0(&grenadeCoord->coord, &pitchScratch->rotation, &grenadeCoord->coord);
    grenadeCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    grenadeCoord->coord.t[0]  += -(grenadeCoord->coord.m[0][1] * HELICOPTER_LANDING_PAD_NO_9_GRENADE_FLIGHT_STEP) >> HELICOPTER_LANDING_PAD_NO_9_GRENADE_ROTATION_FRACTION_BITS;
    grenadeCoord->coord.t[1]  += -(grenadeCoord->coord.m[1][1] * HELICOPTER_LANDING_PAD_NO_9_GRENADE_FLIGHT_STEP) >> HELICOPTER_LANDING_PAD_NO_9_GRENADE_ROTATION_FRACTION_BITS;
    grenadeCoord->coord.t[2]  += -(grenadeCoord->coord.m[2][1] * HELICOPTER_LANDING_PAD_NO_9_GRENADE_FLIGHT_STEP) >> HELICOPTER_LANDING_PAD_NO_9_GRENADE_ROTATION_FRACTION_BITS;
    trailFrames                = work->frames + 1;
    work->frames               = trailFrames;
    if (trailFrames >= HELICOPTER_LANDING_PAD_NO_9_GRENADE_TRAIL_INTERVAL) {
        pitchScratch->angles.vx = 0;
        pitchScratch->angles.vy = HELICOPTER_LANDING_PAD_NO_9_GRENADE_TRAIL_OFFSET_Y;
        pitchScratch->angles.vz = 0;
        effectSpawn(EFFECT_SMOKE_PUFF, grenadeCoord, HELICOPTER_LANDING_PAD_NO_9_GRENADE_TRAIL_OPTIONS, &pitchScratch->angles);
        work->frames = 0;
    }
    samplePosition.vx = grenadeCoord->workm.t[0];
    samplePosition.vy = grenadeCoord->workm.t[1];
    samplePosition.vz = grenadeCoord->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &samplePosition, 0, 0);
    if (grenadeCoord->coord.t[1] < -HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_HEIGHT) {
        work->phase = HELICOPTER_LANDING_PAD_NO_9_GRENADE_FLIGHT_PEAKED;
    }
    if (work->phase != HELICOPTER_LANDING_PAD_NO_9_GRENADE_FLIGHT_BELOW_PEAK && grenadeCoord->coord.t[1] >= -HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_HEIGHT + 1) {
        crossedBurstHeight = 1;
    }
    // Hide the model, adopt the impact effect, and retain the spreading/hold task.
    if (crossedBurstHeight != 0 || (work->attackContacts[0].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY || work->gridProbeContacts[0].key.value != 0) {
        effectSpawn(EFFECT_EXPLOSION, grenadeCoord, HELICOPTER_LANDING_PAD_NO_9_GRENADE_EXPLOSION_OPTIONS, NULL);
        effectSpawn(EFFECT_SMOKE_PUFF, grenadeCoord, HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_SMOKE_OPTIONS, NULL);
        burstEffect = effectSpawn((EFFECT_HELICOPTER_LANDING_PAD_NO_9_IMPACT_BURST | EFFECT_SPAWN_UNLIMITED), grenadeCoord, 0, NULL);
        if (burstEffect != NULL) {
            taskReparent(task, burstEffect->task);
        }
        if (work->attackContacts[0].key.value != 0) {
            work->phase = HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_HOLDING;
        } else {
            work->phase = HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_SPREADING;
        }
        work->attack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(work->attackContacts);
        worldCollisionUnlinkBody(&work->gridProbe);
        work->frames           = 0;
        task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        soundKey               = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_GRENADE_BURST;
        sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(grenadeCoord), (s8)worldCoordGetOriginAudioDepth(grenadeCoord));
        task->state = HELICOPTER_LANDING_PAD_NO_9_GRENADE_TASK_BURST;
    }
    worldCollisionClearContacts(work->attackContacts);
    if (parentWork->present == 0) {
        _helicopterLandingPadNo9CancelGrenadeCatch(task, work);
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorEulerTurnScratch);
}

/// Steps the hidden grenade's spreading blast, player hold and delayed teardown.
///
/// Runs only while scene actors are running. After sixteen frames the sphere
/// widens to radius 600, then gets 31 frames to catch the player. A player contact
/// starts the button-press hold; the ending phase waits 31 frames before unlinking
/// the attack and destroying the grenade. Requires the body parent and its
/// work to remain live through teardown; the burst effects are grenade children.
static void _helicopterLandingPadNo9TickGrenadeBurst(Enemy* enemy, Task* task)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_SPREAD_FRAMES = 16,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_CATCH_FRAMES  = 31,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_END_FRAMES    = 31,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_RADIUS  = 600,
    };

    _HelicopterLandingPadNo9GrenadeWork* work;
    HelicopterLandingPadNo9Work*         parent;
    u16                                  phaseFrames;

    work   = task->work;
    parent = task->parent->work;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        switch (work->phase) {
            case HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_SPREADING:
                phaseFrames  = work->frames + 1;
                work->frames = phaseFrames;
                if (phaseFrames >= HELICOPTER_LANDING_PAD_NO_9_GRENADE_SPREAD_FRAMES) {
                    work->attack.radius = HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_RADIUS;
                    work->phase         = HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_CATCHING;
                    work->frames        = 0;
                    work->attack.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    return;
                }
                if (parent->present == 0) {
                    _helicopterLandingPadNo9CancelGrenadeCatch(task, work);
                    return;
                }
                break;
            case HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_CATCHING:
                if ((work->attackContacts[0].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
                    work->phase         = HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_HOLDING;
                    work->holdStep      = HELICOPTER_LANDING_PAD_NO_9_GRENADE_HOLD_REQUEST;
                    work->attack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    phaseFrames  = work->frames + 1;
                    work->frames = phaseFrames;
                    // A timed-out catch keeps pair tests enabled during the ending delay.
                    if (phaseFrames >= HELICOPTER_LANDING_PAD_NO_9_GRENADE_CATCH_FRAMES) {
                        work->phase  = HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_ENDING;
                        work->frames = 0;
                    } else if (parent->present == 0) {
                        _helicopterLandingPadNo9CancelGrenadeCatch(task, work);
                    }
                }
                worldCollisionClearContacts(work->attackContacts);
                return;
            case HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_HOLDING:
                _helicopterLandingPadNo9TickGrenadeHold(task);
                return;
            case HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_ENDING:
                phaseFrames  = work->frames + 1;
                work->frames = phaseFrames;
                if (phaseFrames >= HELICOPTER_LANDING_PAD_NO_9_GRENADE_END_FRAMES) {
                    parent->grenadeLive = 0;
                    worldCollisionUnlinkBody(&work->attack);
                    enemyDestroy(enemy, task);
                }
                break;
        }
    }
}

/// Steps the stun grenade's button-press hold and recovery of the player.
///
/// Requires a live grenade task, body parent and player. Requests twelve button
/// presses, deals grenade damage and installs the stunned clip on acceptance.
/// A refused request ends the burst. Escape, loss of the golem or the 61st held
/// tick starts recovery; after 21 ticks, its animation must finish before release.
/// Message payloads borrow scratch storage only during synchronous dispatch.
static void _helicopterLandingPadNo9TickGrenadeHold(Task* task)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_HOLD_PRESS_COUNT    = 12,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_HOLD_STUN_LIMIT     = 61,
        HELICOPTER_LANDING_PAD_NO_9_GRENADE_HOLD_RECOVER_LIMIT  = 21,
        HELICOPTER_LANDING_PAD_NO_9_PLAYER_ANIM_GRENADE_STUN    = 1,
        HELICOPTER_LANDING_PAD_NO_9_PLAYER_ANIM_GRENADE_RECOVER = 2,
        HELICOPTER_LANDING_PAD_NO_9_SOUND_GRENADE_HOLD          = 0x5110000A,
    };
    _HelicopterLandingPadNo9GrenadeWork* work;
    HelicopterLandingPadNo9Work*         parent;
    Task*                                player;
    ActorPlayerHoldScratch*              scratch;
    GfxCoord*                            playerCoord;
    u16                                  nextFrame;
    s32                                  soundKey;
    s32                                  audioPan;

    work    = task->work;
    parent  = task->parent->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(ActorPlayerHoldScratch);

    switch (work->holdStep) {
        case HELICOPTER_LANDING_PAD_NO_9_GRENADE_HOLD_REQUEST:
            if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
                // All hold and animation payloads are borrowed during dispatch only.
                scratch->buttonPressHold.pressCount = HELICOPTER_LANDING_PAD_NO_9_GRENADE_HOLD_PRESS_COUNT;
                if (TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &scratch->buttonPressHold, 0) != 0) {
                    work->phase = HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_ENDING;
                    break;
                }
                taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(gHelicopterLandingPadNo9Attacks, HELICOPTER_LANDING_PAD_NO_9_ATTACK_GRENADE), 0);
                scratch->playerAnim.source.sets          = _gHelicopterLandingPadNo9PlayerGrenadeAnimationSets;
                scratch->playerAnim.animationId          = HELICOPTER_LANDING_PAD_NO_9_PLAYER_ANIM_GRENADE_STUN;
                scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;
                scratch->playerAnim.blendFrames          = 0;
                scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &scratch->playerAnim, 0);
                work->holdStep     = HELICOPTER_LANDING_PAD_NO_9_GRENADE_HOLD_STUNNED;
                work->phaseCounter = 0;
                playerCoord        = player->extra.tmd->coords;
                soundKey           = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_GRENADE_HOLD;
                audioPan           = (s8)worldCoordGetOriginAudioPan(playerCoord);
                sndEvtRequestScriptStart(soundKey, audioPan, (s8)worldCoordGetOriginAudioDepth(playerCoord));
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_GRENADE_HOLD_STUNNED:
            nextFrame          = work->phaseCounter + 1;
            work->phaseCounter = nextFrame;
            if ((s16)nextFrame < HELICOPTER_LANDING_PAD_NO_9_GRENADE_HOLD_STUN_LIMIT && parent->playerEscaped != 1 && parent->present != 0) {
                break;
            }
            parent->playerEscaped                    = 0;
            scratch->playerAnim.source.sets          = _gHelicopterLandingPadNo9PlayerGrenadeAnimationSets;
            scratch->playerAnim.animationId          = HELICOPTER_LANDING_PAD_NO_9_PLAYER_ANIM_GRENADE_RECOVER;
            scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;
            scratch->playerAnim.blendFrames          = 0;
            scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &scratch->playerAnim, 0);
            work->holdStep     = HELICOPTER_LANDING_PAD_NO_9_GRENADE_HOLD_RECOVER;
            work->phaseCounter = 0;
            break;
        case HELICOPTER_LANDING_PAD_NO_9_GRENADE_HOLD_RECOVER:
            nextFrame          = work->phaseCounter + 1;
            work->phaseCounter = nextFrame;
            if ((s16)nextFrame >= HELICOPTER_LANDING_PAD_NO_9_GRENADE_HOLD_RECOVER_LIMIT) {
                if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                    taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                    work->phase = HELICOPTER_LANDING_PAD_NO_9_GRENADE_BURST_ENDING;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorPlayerHoldScratch);
}

/// Initializes one of the three destructible helipad lights and its blast sphere.
///
/// `task->spawnArg1.value` must be a light index 0..2. Allocates cleared owned
/// animation and lighting storage, places the model from the index tables, and
/// registers a target and two collision bodies at part 10. Both bodies start
/// with pair tests disabled; the frame handler enables them in the appropriate
/// phase and view. Allocation failure destroys the light enemy and task.
static void _helicopterLandingPadNo9InitHelipadLight(Enemy* enemy, Task* task)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_ANIM_INTACT  = 1,
        HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_CONTACT_PART = 10,
        HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_BODY_RADIUS  = 200,
        HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_BLAST_RADIUS = 350,
        HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_HAZARD_ID    = 2,
    };

    TmdObject*                                model;
    GfxCoord*                                 partCoords;
    _HelicopterLandingPadNo9HelipadLightWork* work;
    GfxCoord*                                 contactCoord;
    SVECTOR*                                  spawnRotation;
    s32                                       slotIndex;

    model        = task->extra.tmd;
    partCoords   = model->coords;
    work         = memCalloc(sizeof(_HelicopterLandingPadNo9HelipadLightWork), false);
    contactCoord = &partCoords[HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_CONTACT_PART];
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work               = work;
    model->flags             = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    partCoords->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx          = &work->lightMtx;
    model->colorMtx          = &work->colorMtx;
    enemy->field_4           = &partCoords->coord;
    spawnRotation            = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    enemy->field_48          = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->coord                  = contactCoord;
    enemy->bodyPos.vx             = -HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_BODY_RADIUS;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    work->lightIndex              = task->spawnArg1.value;
    spawnRotation->vx             = 0;
    spawnRotation->vy             = _gHelicopterLandingPadNo9HelipadLightYaws[work->lightIndex];
    spawnRotation->vz             = 0;
    RotMatrix(spawnRotation, &partCoords->coord);
    slotIndex              = 1;
    partCoords->coord.t[0] = _gHelicopterLandingPadNo9HelipadLightPositions[work->lightIndex].vx;
    partCoords->coord.t[1] = _gHelicopterLandingPadNo9HelipadLightPositions[work->lightIndex].vy;
    partCoords->coord.t[2] = _gHelicopterLandingPadNo9HelipadLightPositions[work->lightIndex].vz;
    partCoords->parent     = &gGfxViewCoord;
    animationInitContext(&work->anim, _gHelicopterLandingPadNo9HelipadLightAnimationSets, model, work->poses, work->slots);
    do {
        animationResetSlot(&work->anim, slotIndex, HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_ANIM_INTACT);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->slots));
    work->body.pos.vx           = -HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_BODY_RADIUS;
    work->body.coord            = contactCoord;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.context.contacts = work->bodyContacts;
    work->body.key              = 0;
    work->body.radius           = HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_BODY_RADIUS;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);
    work->blast.key              = DAMAGE_HAZARD_CATEGORY | HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_HAZARD_ID;
    work->blast.coord            = contactCoord;
    work->blast.pos.vx           = 0;
    work->blast.pos.vy           = 0;
    work->blast.pos.vz           = 0;
    work->blast.context.contacts = work->blastContacts;
    work->blast.radius           = HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_BLAST_RADIUS;
    work->blast.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->body.flags            &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_BLASTS, &work->blast);
    worldCollisionInitContacts(work->blastContacts, ARRAY_SIZE(work->blastContacts), 0);
    work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->exitCallback = _helicopterLandingPadNo9ExitHelipadLight;
    task->state        = HELICOPTER_LANDING_PAD_NO_9_CHILD_TASK_RUNNING;
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Advances a helipad light's animation, break state and published status.
///
/// Requires its initialized eleven-part model/work and live parent body work.
/// Running lights outside their selected views tick only slots 1..10. In view,
/// break processing publishes lights 0/1 to saved flag nibbles 11/12 and light 2
/// to the parent, then composes the root and refreshes lighting. Paused actors
/// refresh lighting alone; hidden actors hide the model and target.
static void _helicopterLandingPadNo9TickHelipadLight(Enemy* enemy, Task* task)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_PERSISTENT_LIGHT_COUNT = 2,
        HELICOPTER_LANDING_PAD_NO_9_LIGHT_FLAG_BASE        = 11,
    };
    TmdObject*                                model;
    _HelicopterLandingPadNo9HelipadLightWork* work;
    GfxCoord*                                 rootCoord;
    HelicopterLandingPadNo9Work*              bodyWork;
    s32                                       slotIndex;

    model     = task->extra.tmd;
    work      = task->work;
    rootCoord = model->coords;
    bodyWork  = task->parent->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (_helicopterLandingPadNo9CheckHelipadLightView(task) == 0) {
                slotIndex = 1;
                do {
                    animationTickSlot(&work->anim, slotIndex);
                    slotIndex++;
                } while (slotIndex < ARRAY_SIZE(work->slots));
                return;
            }
            task->extra.tmd->flags        = 0;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _helicopterLandingPadNo9UpdateHelipadLightLighting(task, rootCoord);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    // Publish damage progress only on the full in-view update.
    _helicopterLandingPadNo9TickHelipadLightBreak(task);
    if (work->lightIndex < HELICOPTER_LANDING_PAD_NO_9_PERSISTENT_LIGHT_COUNT) {
        gameFlagSetNibble(work->lightIndex + HELICOPTER_LANDING_PAD_NO_9_LIGHT_FLAG_BASE, work->status);
    } else {
        bodyWork->light2Status = work->status;
    }
    slotIndex = 1;
    do {
        animationTickSlot(&work->anim, slotIndex);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->slots));
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    _helicopterLandingPadNo9UpdateHelipadLightLighting(task, rootCoord);
}

/// Breaks a shot or slashed helipad light and runs its sparks and one-hit blast.
///
/// Requires the live light model, enemy and body parent. A targeted damaging shot
/// or the parent's slash latch starts animation 2 and 120 frames of sparks. Blast
/// contacts disable further hits. Expiry leaves intermittent bursts; loss of the
/// parent releases the adopted spark controller. Light 2 publishes its phase to
/// the parent for the blast source beside it.
static void _helicopterLandingPadNo9TickHelipadLightBreak(Task* task)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_LIGHT_BREAK_ANIMATION = 2,
        HELICOPTER_LANDING_PAD_NO_9_LIGHT_SPARK_FRAMES    = 120,
        HELICOPTER_LANDING_PAD_NO_9_LIGHT_SPARK_OFFSET_Y  = 128,
        HELICOPTER_LANDING_PAD_NO_9_LIGHT_EXPLOSION_SIZE  = 512,
        HELICOPTER_LANDING_PAD_NO_9_LIGHT_SHOT_DISTANCE   = 1000,
        HELICOPTER_LANDING_PAD_NO_9_SOUND_LIGHT_BREAK     = 0x51100004,
    };
    _HelicopterLandingPadNo9HelipadLightWork* work;
    HelicopterLandingPadNo9Work*              parent;
    Enemy*                                    enemy;
    ActorFaceScratch*                         sparkScratch;
    ActorFaceScratch*                         scratchEnd;
    GfxCoord*                                 contactCoord;
    EffectWork*                               sparksEffect;
    Task*                                     sparksTask;
    s16                                       doneState;
    s32                                       breakRequested;
    s32                                       slotIndex;
    s32                                       soundKey;
    s32                                       audioPan;
    s32                                       attackKey;
    s16                                       lightState;

    breakRequested                         = 0;
    scratchEnd                             = SCRATCH_STACK_CURSOR(ActorFaceScratch);
    contactCoord                           = &task->extra.tmd->coords[10];
    SCRATCH_STACK_CURSOR(ActorFaceScratch) = scratchEnd - 1;
    sparkScratch                           = scratchEnd - 1;
    work                                   = task->work;
    enemy                                  = task->spawnArg2.pointer;
    lightState                             = work->state;
    parent                                 = task->parent->work;
    switch (lightState) {
        case HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_INTACT:
            if (parent->present == 0) {
                work->status = HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_STATUS_STOPPED;
                work->state  = HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_DONE;
                break;
            }
            enemy->node.state.parts.flags = gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED;
            attackKey                     = work->bodyContacts[0].key.value;
            work->body.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            if ((attackKey & (WORLD_COLLISION_CONTACT_KIND_MASK | 0x8000)) == WORLD_COLLISION_CONTACT_ATTACK && enemy->node.state.parts.targeted == 1 &&
                damageComputePlayerAttack(attackKey, HELICOPTER_LANDING_PAD_NO_9_LIGHT_SHOT_DISTANCE, 0, 0) != 0) {
                breakRequested = 1;
            }
            if (parent->slashedLight == work->lightIndex + 1 && parent->lightSlashStruck == 1) {
                breakRequested       = 1;
                parent->slashedLight = -1;
            }
            if (breakRequested == 1) {
                work->status = HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_STATUS_SPARKING;
                work->state  = HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_SPARKING;
                slotIndex    = 1;
                do {
                    animationSeekSlotWithBlend(&work->anim, slotIndex, HELICOPTER_LANDING_PAD_NO_9_LIGHT_BREAK_ANIMATION, 0, 0);
                    slotIndex++;
                } while (slotIndex < ARRAY_SIZE(work->slots));
                sparkScratch->rot.vx = 0;
                sparkScratch->rot.vy = HELICOPTER_LANDING_PAD_NO_9_LIGHT_SPARK_OFFSET_Y;
                sparkScratch->rot.vz = 0;
                sparksEffect         = effectSpawn((EFFECT_HELIPAD_LIGHT_SPARKS | EFFECT_SPAWN_UNLIMITED), contactCoord, 0, &sparkScratch->rot);
                if (sparksEffect != NULL) {
                    sparksTask       = sparksEffect->task;
                    work->sparksTask = sparksTask;
                    taskReparent(task, sparksTask);
                }
                effectSpawn(EFFECT_EXPLOSION, contactCoord, HELICOPTER_LANDING_PAD_NO_9_LIGHT_EXPLOSION_SIZE, &sparkScratch->rot);
                work->sparkFrames  = HELICOPTER_LANDING_PAD_NO_9_LIGHT_SPARK_FRAMES;
                work->body.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->blast.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                soundKey           = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_LIGHT_BREAK;
                audioPan           = (s8)worldCoordGetOriginAudioPan(contactCoord);
                sndEvtRequestScriptStart(soundKey, audioPan, (s8)worldCoordGetOriginAudioDepth(contactCoord));
            }
            worldCollisionClearContacts(work->bodyContacts);
            break;
        case HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_SPARKING:
            if (worldCollisionFindContactIndex(work->blastContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            worldCollisionClearContacts(work->blastContacts);
            akropolisHelicopterLandingPadDrawLowerSparkLine(&task->extra.tmd->coords[9]);
            akropolisHelicopterLandingPadDrawLowerSparkLine(&task->extra.tmd->coords[8]);
            akropolisHelicopterLandingPadDrawLowerSparkLine(&task->extra.tmd->coords[7]);
            akropolisHelicopterLandingPadDrawLowerSparkLine(&task->extra.tmd->coords[6]);
            work->sparkFrames--;
            doneState = HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_DONE;
            if (work->sparkFrames <= 0) {
                Task* sparksTask;

                work->status       = HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_STATUS_SPENT;
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                sparksTask         = work->sparksTask;
                if (sparksTask != NULL) {
                    sparksTask->state = HELICOPTER_LANDING_PAD_NO_9_LIGHT_SPARKS_BURSTS_ONLY;
                }
            } else {
                Task* sparksTask;

                if (parent->present != 0) {
                    break;
                }
                work->status       = HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_STATUS_SPENT;
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                sparksTask         = work->sparksTask;
                if (sparksTask != NULL) {
                    sparksTask->state = HELICOPTER_LANDING_PAD_NO_9_LIGHT_SPARKS_RELEASE;
                    work->sparksTask  = NULL;
                }
            }
            work->state = doneState;
            break;
        case HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_DONE:
            if (parent->present == 0) {
                Task* sparksTask;

                sparksTask = work->sparksTask;
                if (sparksTask != NULL) {
                    sparksTask->state = HELICOPTER_LANDING_PAD_NO_9_LIGHT_SPARKS_RELEASE;
                    work->sparksTask  = NULL;
                }
            }
            break;
    }
    if (work->lightIndex == 2) {
        parent->light2State = work->state;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

/// Initializes the coordinate-only blast source beside helipad light 2.
///
/// Requires a live parent body. Allocates cleared owned work, places the source
/// at (-6100, -1110, 380) in the view coordinate's frame, and registers a target,
/// a radius-300 receiving sphere and a radius-512 blast raised 512 units above it.
/// Pair tests start disabled; allocation failure destroys the enemy and task.
static void _helicopterLandingPadNo9InitBlastSource(Enemy* enemy, Task* task)
{
    GfxCoord*                                sourceCoord;
    _HelicopterLandingPadNo9BlastSourceWork* work;

    sourceCoord = task->extra.coordBody->coord;
    work        = memCalloc(sizeof(_HelicopterLandingPadNo9BlastSourceWork), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work = work;
    gfxSetRotIdentity(&sourceCoord->coord);
    sourceCoord->coord.t[0]   = -0x17D4;
    sourceCoord->coord.t[1]   = -0x456;
    sourceCoord->coord.t[2]   = 0x17C;
    sourceCoord->parent       = &gGfxViewCoord;
    sourceCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    enemy->field_4            = &sourceCoord->coord;
    enemy->field_48           = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = sourceCoord;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    work->body.coord              = sourceCoord;
    work->body.pos.vx             = 0;
    work->body.pos.vy             = 0;
    work->body.pos.vz             = 0;
    work->body.context.contacts   = work->bodyContacts;
    work->body.key                = 0;
    work->body.radius             = 0x12C;
    work->body.flags              = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);
    work->blast.pos.vy           = -0x200;
    work->blast.coord            = sourceCoord;
    work->blast.pos.vx           = 0;
    work->blast.pos.vz           = 0;
    work->blast.context.contacts = work->blastContacts;
    work->blast.key              = 0;
    work->blast.radius           = 0x200;
    work->blast.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->body.flags            &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_BLASTS, &work->blast);
    worldCollisionInitContacts(work->blastContacts, ARRAY_SIZE(work->blastContacts), 0);
    work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->exitCallback = _helicopterLandingPadNo9ExitBlastSource;
    task->state        = HELICOPTER_LANDING_PAD_NO_9_CHILD_TASK_RUNNING;
}

/// Updates the helipad blast source only while its view and combat are active.
///
/// Requires the initialized coordinate-body task and its live body parent. Leaving
/// the source's mapped view disables both collision spheres and releases its flare,
/// while taking one frame from its burn timer. In view, steps the shot phases,
/// publishes the source/light status flag and emits an ember with 1/3 probability.
/// Paused or hidden combat does not advance the source.
static void _helicopterLandingPadNo9TickBlastSource(Enemy* enemy, Task* task)
{
    _HelicopterLandingPadNo9BlastSourceWork* work;
    HelicopterLandingPadNo9Work*             parentWork;
    u32                                      randomState;
    Task*                                    flareTask;

    work       = task->work;
    parentWork = task->parent->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if ((viewGetMappedIndex() & 0xFF) != _gHelicopterLandingPadNo9HelipadViews.lightViews[2][0]) {
                enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->blast.flags            &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                if (work->flareFrames != 0) {
                    work->flareFrames--;
                }
                flareTask = work->flareTask;
                if (flareTask != NULL) {
                    flareTask->state = HELICOPTER_LANDING_PAD_NO_9_SOURCE_FLARE_RELEASE;
                    work->flareTask  = NULL;
                }
                return;
            }
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    // Publish the new phase before drawing this frame's ambient ember.
    _helicopterLandingPadNo9TickBlastSourcePhases(task);
    gameFlagSetNibble(GAME_FLAG_00D, _gHelicopterLandingPadNo9HelipadStatusFlagValues[work->state][parentWork->light2Status]);
    randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
    gRandomLcgState = randomState;
    if ((u16)((randomState >> 16) % 3) == 0) {
        effectSpawn(EFFECT_ACROPOLIS_HELIPAD_EMBER, task->extra.coordBody->coord, 0, NULL);
    }
}

/// Grenade task-state callbacks: 0 initialize, 1 flight, 2 hidden burst and player hold.
///
/// Copied as a complete three-pointer value before dispatch; no terminator.
/// The live enemy and task are lent to the selected handler, which may release
/// them. The grenade and its adopted effects require the overlay to stay loaded.
static const EnemyTaskFuncTable3 _gHelicopterLandingPadNo9GrenadeTaskStates = {
    { _helicopterLandingPadNo9InitGrenade, _helicopterLandingPadNo9TickGrenadeFlight, _helicopterLandingPadNo9TickGrenadeBurst },
};

/// Arms the helipad blast source after light 2 breaks, then runs its shot flare.
///
/// Requires the live coordinate body, owned enemy/work and golem parent. A targeted
/// damaging shot starts a 60-frame flare and one-hit hazard blast: row 4 while the
/// light sparks, row 3 once it is done. The flare is adopted beneath the source;
/// expiry switches it to embers and parent disappearance releases it.
static void _helicopterLandingPadNo9TickBlastSourcePhases(Task* task)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_SOURCE_FLARE_FRAMES  = 60,
        HELICOPTER_LANDING_PAD_NO_9_SOURCE_SHOT_DISTANCE = 1000,
        HELICOPTER_LANDING_PAD_NO_9_SOUND_SOURCE_SHOT    = 0x51100002,
    };
    _HelicopterLandingPadNo9BlastSourceWork* work;
    GfxCoord*                                sourceCoord;
    HelicopterLandingPadNo9Work*             parentWork;
    Enemy*                                   enemy;
    EffectWork*                              flareEffect;
    Task*                                    flareAfterShot;
    Task*                                    endingFlare;
    Task*                                    stoppedFlare;
    Task*                                    releasedFlare;
    Task*                                    spawnedFlare;
    s32                                      attackKey;
    s32                                      targeted;
    s32                                      soundKey;
    s32                                      audioPan;
    u16                                      flareFrames;

    work        = task->work;
    sourceCoord = task->extra.coordBody->coord;
    parentWork  = task->parent->work;
    enemy       = task->spawnArg2.pointer;

    switch (work->state) {
        case HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_DORMANT:
            if (parentWork->light2State == HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_SPARKING) {
                work->state = HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_ARMED;
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_ARMED:
            enemy->node.state.parts.flags = gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED;
            attackKey                     = work->bodyContacts[0].key.value;
            work->body.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            if ((attackKey & ~0x7FFF) == WORLD_COLLISION_CONTACT_ATTACK) {
                targeted = enemy->node.state.parts.targeted;
                if (targeted == 1 && damageComputePlayerAttack(attackKey, HELICOPTER_LANDING_PAD_NO_9_SOURCE_SHOT_DISTANCE, 0, 0) != 0) {
                    work->state       = HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_SHOT;
                    work->flareFrames = HELICOPTER_LANDING_PAD_NO_9_SOURCE_FLARE_FRAMES;
                    effectSpawn(EFFECT_IMPACT_SPARK, sourceCoord, 0, NULL);
                    flareEffect = effectSpawn((EFFECT_05F | EFFECT_SPAWN_UNLIMITED), sourceCoord, 0, NULL);
                    if (flareEffect != NULL) {
                        spawnedFlare    = flareEffect->task;
                        work->flareTask = spawnedFlare;
                        taskReparent(task, spawnedFlare);
                    }
                    work->body.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->blast.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    if (parentWork->light2State == HELICOPTER_LANDING_PAD_NO_9_HELIPAD_LIGHT_DONE) {
                        work->flareTaskState = HELICOPTER_LANDING_PAD_NO_9_SOURCE_FLARE_RAW_COLOR;
                        work->blast.key      = DAMAGE_HAZARD_CATEGORY | HELICOPTER_LANDING_PAD_NO_9_HAZARD_SOURCE_RECOIL;
                    } else {
                        work->flareTaskState = HELICOPTER_LANDING_PAD_NO_9_SOURCE_FLARE_FLICKERING;
                        work->blast.key      = DAMAGE_HAZARD_CATEGORY | HELICOPTER_LANDING_PAD_NO_9_HAZARD_SOURCE_STUN;
                    }
                    soundKey = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | HELICOPTER_LANDING_PAD_NO_9_SOUND_SOURCE_SHOT;
                    audioPan = (s8)worldCoordGetOriginAudioPan(sourceCoord);
                    sndEvtRequestScriptStart(soundKey, audioPan, (s8)worldCoordGetOriginAudioDepth(sourceCoord));
                }
            }
            worldCollisionClearContacts(work->bodyContacts);
            break;
        case HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_SHOT:
            flareAfterShot = work->flareTask;
            work->state    = HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_FLARING;
            if (flareAfterShot != NULL) {
                flareAfterShot->state = work->flareTaskState;
            }
            if (worldCollisionFindContactIndex(work->blastContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            worldCollisionClearContacts(work->blastContacts);
            break;
        case HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_FLARING:
            if (worldCollisionFindContactIndex(work->blastContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            worldCollisionClearContacts(work->blastContacts);
            flareFrames       = work->flareFrames - 1;
            work->flareFrames = flareFrames;
            if ((s16)flareFrames <= 0) {
                work->state        = HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_SPENT;
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                endingFlare        = work->flareTask;
                if (endingFlare != NULL) {
                    endingFlare->state = HELICOPTER_LANDING_PAD_NO_9_SOURCE_FLARE_EMBERS;
                }
                break;
            }
            if (parentWork->present == 0) {
                work->state        = HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_STOPPED;
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                stoppedFlare       = work->flareTask;
                if (stoppedFlare != NULL) {
                    stoppedFlare->state = HELICOPTER_LANDING_PAD_NO_9_SOURCE_FLARE_RELEASE;
                    work->flareTask     = NULL;
                }
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_SPENT:
            if (parentWork->present == 0) {
                work->state   = HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_STOPPED;
                releasedFlare = work->flareTask;
                if (releasedFlare != NULL) {
                    releasedFlare->state = HELICOPTER_LANDING_PAD_NO_9_SOURCE_FLARE_RELEASE;
                    work->flareTask      = NULL;
                }
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_BLAST_SOURCE_STOPPED:
            break;
    }
}

/// Dispatches the Akropolis No. 9 GOLEM body's initialization and frame update.
///
/// Task state must be 0 (initialize) or 1 (update), with a live enemy in
/// spawnArg2.pointer and the nineteen-part descriptor model. The handlers
/// own body work, collision registrations, attachments and adopted effects.
static void _helicopterLandingPadNo9BodyTask(Task* task)
{
    EnemyTaskFunc handlers[2] = { helicopterLandingPadNo9InitBody, _helicopterLandingPadNo9TickBody };

    handlers[task->state](task->spawnArg2.pointer, task);
}

void helicopterLandingPadNo9SetExtraGridFace(s32 enabled)
{
    s32                     vertexIndex;
    SVECTOR*                normals  = gWorldCollisionActiveGrid->normals;
    SVECTOR*                vertices = gWorldCollisionActiveGrid->vertices;
    WorldCollisionGridFace* faces    = gWorldCollisionActiveGrid->faces;

    if (enabled == true) {
        for (vertexIndex = 0; vertexIndex < ARRAY_SIZE(_gHelicopterLandingPadNo9ExtraGridVertices); vertexIndex++) {
            vertices[ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DBC) + vertexIndex] = _gHelicopterLandingPadNo9ExtraGridVertices[vertexIndex];
        }
        normals[ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DA4)] = _gHelicopterLandingPadNo9ExtraGridNormals[0];
        faces[ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35E1C)]   = _gHelicopterLandingPadNo9ExtraGridFaces[0];
    } else {
        normals[ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DA4)].vx = 0;
        normals[ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DA4)].vy = 0;
        normals[ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DA4)].vz = 0;
        for (vertexIndex = 0; vertexIndex < ARRAY_SIZE(_gHelicopterLandingPadNo9ExtraGridVertices); vertexIndex++) {
            vertices[ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DBC) + vertexIndex].vx = 0;
            vertices[ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DBC) + vertexIndex].vy = 0;
            vertices[ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DBC) + vertexIndex].vz = 0;
        }
    }
}

void helicopterLandingPadNo9RestoreGridFaces(Task* unusedTask)
{
    s32                     elementIndex;
    SVECTOR*                normals  = gWorldCollisionActiveGrid->normals;
    SVECTOR*                vertices = gWorldCollisionActiveGrid->vertices;
    WorldCollisionGridFace* faces    = gWorldCollisionActiveGrid->faces;

    for (elementIndex = 0; elementIndex < ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DBC); elementIndex++) {
        vertices[elementIndex] = _gHelicopterLandingPadNo9Collision35DBC[elementIndex];
    }

    for (elementIndex = 0; elementIndex < ARRAY_SIZE(_gHelicopterLandingPadNo9Collision35DA4); elementIndex++) {
        normals[elementIndex] = _gHelicopterLandingPadNo9Collision35DA4[elementIndex];
        faces[elementIndex]   = _gHelicopterLandingPadNo9Collision35E1C[elementIndex];
    }
}

void helicopterLandingPadNo9ExitBody(Task* task)
{
    HelicopterLandingPadNo9Work* work = task->work;

    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->weaponAttack);
    worldCollisionUnlinkBody(&work->forearmAttack);
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// Selects event playback or combat for the initialized No. 9 golem body.
///
/// Requires live body work, model and enemy. Any nonzero session event state
/// runs scripted animation/flame playback; zero selects combat, whose own
/// activation and actor-control gates determine whether it advances.
static void _helicopterLandingPadNo9TickBody(Enemy* enemy, Task* task)
{
    if (gGameSession->eventState != 0) {
        helicopterLandingPadNo9TickEvent(enemy, task);
        return;
    }
    _helicopterLandingPadNo9TickCombat(enemy, task);
}

/// Runs the active golem's combat, motion, animation and presentation update.
///
/// Requires the initialized body model, owned work and enemy. Activation zero
/// skips the update; paused combat updates shadow and lighting, hidden combat
/// hides the model and target. Running combat consumes reactions and contacts
/// before selecting movement, then composes the rig and refreshes shadow,
/// lighting, collision-grid faces and flame state.
static void _helicopterLandingPadNo9TickCombat(Enemy* enemy, Task* task)
{
    GfxCoord*                    bodyCoord;
    TmdObject*                   bodyModel;
    HelicopterLandingPadNo9Work* work;

    work      = task->work;
    bodyModel = task->extra.tmd;
    bodyCoord = bodyModel->coords;
    if (work->activation != 0) {
        switch (gSceneCombatState.actorControl) {
            case SCENE_COMBAT_ACTORS_RUNNING:
                bodyModel->flags              = 0;
                enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
                break;
            case SCENE_COMBAT_ACTORS_PAUSED:
                _no9GolemDrawShadow(task);
                helicopterLandingPadNo9UpdateLighting(task, bodyCoord);
                return;
            case SCENE_COMBAT_ACTORS_HIDDEN:
                bodyModel->flags              = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                return;
        }
        if (enemy->reactionFlags != 0) {
            _helicopterLandingPadNo9ConsumeReactionFlags(task);
        }
        _helicopterLandingPadNo9ResolveBodyContacts(task);
        _helicopterLandingPadNo9UpdatePlayerRange(task);
        _helicopterLandingPadNo9TickCombatState(task);
        _helicopterLandingPadNo9TurnAlongLap(task);
        _helicopterLandingPadNo9AdvanceAlongLap(task);
        _helicopterLandingPadNo9PlayStepSounds(task);
        _helicopterLandingPadNo9UpdateBodyAnimation(task);
        _no9GolemAimHead(task);
        if (work->hitTwistActive != 0) {
            _helicopterLandingPadNo9ApplyHitTwist(task);
        }
        bodyCoord->composeStamp                 = GRAPHICS_COORD_DIRTY;
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(bodyCoord);
        _no9GolemDrawShadow(task);
        helicopterLandingPadNo9UpdateLighting(task, bodyCoord);
        _helicopterLandingPadNo9UpdateGridFaces(task);
        _helicopterLandingPadNo9UpdateFlameJet(task);
    }
}

/// Consumes enemy reaction requests and enters the golem's buildup stun.
///
/// Requires the live body task and enemy. Clears stagger and damage-over-time
/// requests without starting those reactions; buildup sets the stun state and
/// its latch. Other reaction bits remain intact.
static void _helicopterLandingPadNo9ConsumeReactionFlags(Task* task)
{
    HelicopterLandingPadNo9Work* work;
    Enemy*                       enemy;
    u8                           reactionFlags;

    enemy         = task->spawnArg2.pointer;
    reactionFlags = enemy->reactionFlags;
    work          = task->work;
    if (reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags = reactionFlags & ENEMY_REACTION_STAGGER_CLEAR;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->state           = HELICOPTER_LANDING_PAD_NO_9_STATE_BUILDUP_STUN;
        work->subState        = HELICOPTER_LANDING_PAD_NO_9_BUILDUP_STUN_WAIT;
        work->buildupStunned  = 1;
    }
    reactionFlags = enemy->reactionFlags;
    if (reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        enemy->reactionFlags = reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
    }
}

/// Dispatches the golem's thirteen combat states to their per-frame handlers.
///
/// Requires live body work with a state in OPENING..DEATH; other values do nothing.
/// Each handler owns its substate and selects this frame's movement and animation.
static void _helicopterLandingPadNo9TickCombatState(Task* task)
{
    HelicopterLandingPadNo9Work* work = task->work;
    s16                          state;

    state = work->state;
    switch (state) {
        case HELICOPTER_LANDING_PAD_NO_9_STATE_OPENING:
            _helicopterLandingPadNo9TickOpening(task);
            return;
        case HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL:
            _helicopterLandingPadNo9TickPatrol(task);
            return;
        case HELICOPTER_LANDING_PAD_NO_9_STATE_SLASH:
            _helicopterLandingPadNo9TickSlash(task);
            return;
        case HELICOPTER_LANDING_PAD_NO_9_STATE_FLAME_LUNGE:
            _helicopterLandingPadNo9TickFlameLunge(task);
            return;
        case HELICOPTER_LANDING_PAD_NO_9_STATE_GRENADE:
            _helicopterLandingPadNo9TickGrenadeThrow(task);
            return;
        case HELICOPTER_LANDING_PAD_NO_9_STATE_DASH:
            _helicopterLandingPadNo9TickDash(task);
            return;
        case HELICOPTER_LANDING_PAD_NO_9_STATE_LETHAL_ATTACK:
            _helicopterLandingPadNo9TickLethalAttack(task);
            return;
        case HELICOPTER_LANDING_PAD_NO_9_STATE_BUILDUP_STUN:
            _helicopterLandingPadNo9TickBuildupStun(task);
            return;
        case HELICOPTER_LANDING_PAD_NO_9_STATE_FLINCH:
            _helicopterLandingPadNo9TickFlinch(task);
            return;
        case HELICOPTER_LANDING_PAD_NO_9_STATE_RECOIL:
            _helicopterLandingPadNo9TickRecoil(task);
            return;
        case HELICOPTER_LANDING_PAD_NO_9_STATE_SPARK_RECOIL:
            _helicopterLandingPadNo9TickSparkRecoil(task);
            return;
        case HELICOPTER_LANDING_PAD_NO_9_STATE_SPARK_STUN:
            _helicopterLandingPadNo9TickSparkStun(task);
            return;
        case HELICOPTER_LANDING_PAD_NO_9_STATE_DEATH:
            _helicopterLandingPadNo9TickDeath(task);
        default:
            return;
    }
}

/// Holds the golem still until status buildup expires, then plays recovery.
///
/// Recovery clears the buildup latch and ends at animation frame 59, resuming
/// patrol with a random waiting time. Requires the live body work and enemy.
static void _helicopterLandingPadNo9TickBuildupStun(Task* task)
{
    HelicopterLandingPadNo9Work* work;
    s32                          subState;
    s32                          randomState;

    work     = task->work;
    subState = work->subState;
    switch (subState) {
        case HELICOPTER_LANDING_PAD_NO_9_BUILDUP_STUN_WAIT:
            work->lapSpeed    = 0;
            work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_STUN;
            if (damageTickEnemyBuildup(task->spawnArg2.pointer) != 0) {
                work->animationId    = HELICOPTER_LANDING_PAD_NO_9_ANIM_STUN_RECOVER;
                work->subState       = HELICOPTER_LANDING_PAD_NO_9_BUILDUP_STUN_RECOVER;
                work->buildupStunned = 0;
            }
            break;
        case HELICOPTER_LANDING_PAD_NO_9_BUILDUP_STUN_RECOVER:
            if (work->animationFrame >= HELICOPTER_LANDING_PAD_NO_9_STUN_RECOVER_END_FRAME) {
                work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                work->subState    = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
                work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
                work->stateCounter =
                    _gHelicopterLandingPadNo9ReactionWaitFrames[((u32)(randomState = RANDOM_LCG_NEXT(gRandomLcgState)) >> 16) & 0xF];
                gRandomLcgState = randomState;
            }
            break;
    }
}

/// Plays the stationary flinch and selects death or patrol when it finishes.
///
/// At animation frame 80, zero enemy HP selects death; otherwise patrol resumes
/// with a random wait. Requires the live body work and enemy record.
static void _helicopterLandingPadNo9TickFlinch(Task* task)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_FLINCH_END_FRAME = 80,
    };
    HelicopterLandingPadNo9Work* work;
    s32                          subState;
    Enemy*                       enemy;
    s32                          randomState;

    work     = task->work;
    subState = work->subState;
    enemy    = task->spawnArg2.pointer;
    switch (subState) {
        case HELICOPTER_LANDING_PAD_NO_9_FLINCH_ENTER:
            work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_FLINCH;
            work->lapSpeed    = 0;
            work->subState    = HELICOPTER_LANDING_PAD_NO_9_FLINCH_PLAY;
            break;
        case HELICOPTER_LANDING_PAD_NO_9_FLINCH_PLAY:
            if (work->animationFrame >= HELICOPTER_LANDING_PAD_NO_9_FLINCH_END_FRAME) {
                if (enemy->hp <= 0) {
                    work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_DEATH;
                    work->subState    = HELICOPTER_LANDING_PAD_NO_9_DEATH_ENTER;
                    work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_DEATH;
                } else {
                    work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                    work->subState    = HELICOPTER_LANDING_PAD_NO_9_PATROL_WAIT;
                    work->animationId = HELICOPTER_LANDING_PAD_NO_9_ANIM_PATROL_IDLE;
                    work->stateCounter =
                        _gHelicopterLandingPadNo9ReactionWaitFrames[((u32)(randomState = RANDOM_LCG_NEXT(gRandomLcgState)) >> 16) & 0xF];
                    gRandomLcgState = randomState;
                }
            }
            break;
    }
}

/// Restarts the body slots on an animation change, otherwise advances their poses.
///
/// Requires the initialized body rig and an animation ID present in both its
/// set table and the per-animation blend-duration table. Drives slots 1..18;
/// the root slot is skipped. A restart resets the frame counter, while an
/// unchanged animation advances the stored signed-halfword counter by one.
static void _helicopterLandingPadNo9UpdateBodyAnimation(Task* task)
{
    HelicopterLandingPadNo9Work* work;
    s32                          slotIndex;
    s32                          blendFrames;

    work = task->work;
    if (work->animationId != work->seededAnimationId) {
        work->seededAnimationId = work->animationId;
        work->animationFrame    = 0;
        blendFrames             = _gHelicopterLandingPadNo9BodyAnimationBlendFrames[work->animationId];
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animationId, 0, blendFrames);
        }
    } else {
        work->animationFrame++;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
}

#include "../../../../shared/creatures/golem/no9_golem_draw_shadow.inc.c"

void helicopterLandingPadNo9UpdateLighting(Task* task, const GfxCoord* coord)
{
    VECTOR3 samplePosition;

    samplePosition.vx = coord->workm.t[0];
    samplePosition.vy = coord->workm.t[1];
    samplePosition.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &samplePosition, 0, 0);
}

/// Expires the golem's flame attack and forwards mode changes to its flame jet.
///
/// Requires live body work. Burning and blast modes count down frames; expiry
/// stops both flame sounds and disables the paired attack spheres. Sends each
/// changed mode through the borrowed effect task's first payload when it exists;
/// a missing effect still updates the last-sent mode.
static void _helicopterLandingPadNo9UpdateFlameJet(Task* task)
{
    HelicopterLandingPadNo9Work* work = task->work;

    // Only the two active flame modes consume burn time.
    if ((u16)(work->flameMode - HELICOPTER_LANDING_PAD_NO_9_FLAME_BURNING) < 2) {
        if (--work->flameFrames <= 0) {
            work->flameMode = HELICOPTER_LANDING_PAD_NO_9_FLAME_DYING;
            if (work->flameSound != 0) {
                sndEvtRequestScriptStop(work->flameSound, SOUND_SCRIPT_STOP_NO_FADE);
                work->flameSound = 0;
            }
            if (work->eventFlameSound != 0) {
                sndEvtRequestScriptStop(work->eventFlameSound, SOUND_SCRIPT_STOP_NO_FADE);
                work->eventFlameSound = 0;
            }
            work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        }
    }
    if (work->flameMode != work->sentFlameMode) {
        if (work->flameJetTask != NULL) {
            work->flameJetTask->spawnArg1.value = work->flameMode;
        }
        work->sentFlameMode = work->flameMode;
    }
}

/// Latches an early release of the grenade hold while the player is alive.
///
/// Handles `ACTOR_MESSAGE_RELEASE_HOLD` on the live body task. Both payloads
/// are unused. The grenade hold consumes the latch later; returns zero.
static s32 _helicopterLandingPadNo9ReleaseGrenadeHold(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    HelicopterLandingPadNo9Work* work;

    if (gPlayerStatus.hp > 0) {
        work                = task->work;
        work->playerEscaped = 1;
    }
    return 0;
}

/// Starts the golem's event animation selected by a borrowed playback request.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION`; script IDs 1..6 select body sets
/// 28..33 by adding 27. The resulting ID must select a loaded body set. Resets slots 1..18 at record zero, using zero
/// blend frames for RESET and eight for any other choice. Ignores the request's
/// source, blend duration and collision choice, and leaves the seeded animation
/// ID unchanged. Borrows the request through dispatch; returns zero.
static s32 _helicopterLandingPadNo9PlayEventAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedSecondArg)
{
    enum {
        HELICOPTER_LANDING_PAD_NO_9_EVENT_BLEND_FRAMES   = 8,
        HELICOPTER_LANDING_PAD_NO_9_EVENT_ANIMATION_BASE = 27,
    };

    HelicopterLandingPadNo9Work* work;
    s32                          blendFrames;
    s32                          slotIndex;

    blendFrames       = (request->blend != ANIMATION_BLEND_RESET) * HELICOPTER_LANDING_PAD_NO_9_EVENT_BLEND_FRAMES;
    work              = task->work;
    work->animationId = request->animationId + HELICOPTER_LANDING_PAD_NO_9_EVENT_ANIMATION_BASE;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animationId, 0, blendFrames);
    }
    work->animationFrame = 0;
    return 0;
}

#include "../../../../shared/actor/actor_messages_place_rot_matrix.inc.c"

/// Sets whether the body model participates in active drawing.
///
/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` on a live TMD body task. A zero
/// first payload replaces all model flags with SKIP_ACTIVE_DRAW; any nonzero
/// payload replaces them with zero. The second payload is unused; returns zero.
static s32 _helicopterLandingPadNo9SetModelDraw(Task* task, s32 messageId, s32 drawEnabled, s32 unusedSecondArg)
{
    TmdObject* bodyModel;

    bodyModel = task->extra.tmd;
    if (drawEnabled == 0) {
        bodyModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        bodyModel->flags = 0;
    }
    return 0;
}

/// Returns the body work's presence latch for `ACTOR_MESSAGE_IS_PRESENT`.
///
/// Requires the live body task and work. Both payloads are unused. Presence
/// lasts from initialization until death and is independent of model drawing.
static s32 _helicopterLandingPadNo9IsPresent(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    const HelicopterLandingPadNo9Work* work = task->work;

    return work->present;
}

/// Dispatches initialization or update of the golem's off-hand prop model.
///
/// `task->state` must be 0 (initialize) or 1 (update). The parent model and its
/// work block must remain live; the child borrows their coordinate and lighting.
static void _helicopterLandingPadNo9PropTask(Task* task)
{
    EnemyTaskFunc states[HELICOPTER_LANDING_PAD_NO_9_ATTACHMENT_STATE_COUNT] = { _helicopterLandingPadNo9InitProp, _helicopterLandingPadNo9UpdateProp };

    states[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the initially hidden prop to the off hand and borrows body lighting.
///
/// Advances its palette by two rows and rebuilds both model buffer halves.
/// Requires the live parent golem model and work block. The enemy argument is unused.
static void _helicopterLandingPadNo9InitProp(Enemy* enemy, Task* task)
{
    TmdObject*                   propModel;
    HelicopterLandingPadNo9Work* parentWork;
    GfxCoord*                    propCoord;

    propModel                 = task->extra.tmd;
    parentWork                = task->parent->work;
    propCoord                 = propModel->coords;
    propModel->clutRowOffset += 2;
    tmdBuildBufferHalf(propModel);
    tmdBuildBufferHalf(propModel);
    propCoord->parent       = &task->parent->extra.tmd->coords[HELICOPTER_LANDING_PAD_NO_9_PROP_HAND_PART];
    propCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    propModel->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    propModel->lightMtx     = &parentWork->light;
    propModel->colorMtx     = &parentWork->color;
    task->state             = HELICOPTER_LANDING_PAD_NO_9_ATTACHMENT_UPDATE;
}

/// Dispatches initialization or update of the golem's attached weapon model.
///
/// `task->state` must be 0 (initialize) or 1 (update). The parent model and work
/// block must remain live; the weapon borrows their coordinate and lighting.
static void _helicopterLandingPadNo9WeaponTask(Task* task)
{
    EnemyTaskFunc states[HELICOPTER_LANDING_PAD_NO_9_ATTACHMENT_STATE_COUNT] = { _helicopterLandingPadNo9InitWeapon, _helicopterLandingPadNo9UpdateWeapon };

    states[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the hidden weapon to body part 8 and borrows body lighting.
///
/// Requires the live parent golem model and work block. No child work is allocated;
/// the enemy argument is unused. The task advances to the attachment update state.
static void _helicopterLandingPadNo9InitWeapon(Enemy* enemy, Task* task)
{
    TmdObject*                   weaponModel;
    HelicopterLandingPadNo9Work* parentWork;

    weaponModel                 = task->extra.tmd;
    parentWork                  = task->parent->work;
    weaponModel->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    weaponModel->coords->parent = &task->parent->extra.tmd->coords[HELICOPTER_LANDING_PAD_NO_9_WEAPON_PART];
    weaponModel->lightMtx       = &parentWork->light;
    weaponModel->colorMtx       = &parentWork->color;
    task->state                 = HELICOPTER_LANDING_PAD_NO_9_ATTACHMENT_UPDATE;
}

/// Copies body visibility to the weapon and composes its attached coordinate.
///
/// Requires the live parent model established at initialization; the enemy
/// argument is unused. All model flags are copied, not just the hidden bit.
static void _helicopterLandingPadNo9UpdateWeapon(Enemy* enemy, Task* task)
{
    task->extra.tmd->flags                = task->parent->extra.tmd->flags;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
}

/// Dispatches initialization or update of the golem's attached chest model.
///
/// `task->state` must be 0 (initialize) or 1 (update). The parent model and work
/// block must remain live; the child borrows their coordinate and lighting.
static void _helicopterLandingPadNo9ChestModelTask(Task* task)
{
    EnemyTaskFunc states[HELICOPTER_LANDING_PAD_NO_9_ATTACHMENT_STATE_COUNT] = { _helicopterLandingPadNo9InitChestModel, _helicopterLandingPadNo9UpdateChestModel };

    states[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the hidden chest model to body part 3 and borrows body lighting.
///
/// Requires the live parent golem model and work block. No child work is allocated;
/// the enemy argument is unused. The task advances to the attachment update state.
static void _helicopterLandingPadNo9InitChestModel(Enemy* enemy, Task* task)
{
    TmdObject*                   chestModel;
    HelicopterLandingPadNo9Work* parentWork;

    chestModel                 = task->extra.tmd;
    parentWork                 = task->parent->work;
    chestModel->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    chestModel->coords->parent = &task->parent->extra.tmd->coords[HELICOPTER_LANDING_PAD_NO_9_CHEST_PART];
    chestModel->lightMtx       = &parentWork->light;
    chestModel->colorMtx       = &parentWork->color;
    task->state                = HELICOPTER_LANDING_PAD_NO_9_ATTACHMENT_UPDATE;
}

/// Copies body visibility to the chest model and composes its attached coordinate.
///
/// Requires the live parent model established at initialization; the enemy
/// argument is unused. All model flags are copied, not just the hidden bit.
static void _helicopterLandingPadNo9UpdateChestModel(Enemy* enemy, Task* task)
{
    task->extra.tmd->flags                = task->parent->extra.tmd->flags;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
}

/// Dispatches the grenade's initialization, flight and hidden burst phases.
///
/// Requires task state 0..2 and the live enemy in the second spawn argument.
/// Copies the three callback pointers before dispatch; burst/ending may destroy
/// the grenade and its adopted effects.
static void _helicopterLandingPadNo9GrenadeTask(Task* task)
{
    EnemyTaskFuncTable3 states;

    states = _gHelicopterLandingPadNo9GrenadeTaskStates;
    states.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Dispatches a destructible helipad light's initialization and frame update.
///
/// Task state must be 0 (initialize) or 1 (update), with a live light enemy in
/// spawnArg2.pointer, its descriptor model and the parent GOLEM task. The first
/// spawn argument is the light index 0..2. Work, collision bodies and adopted
/// sparks remain owned by the light task until teardown.
static void _helicopterLandingPadNo9HelipadLightTask(Task* task)
{
    EnemyTaskFunc handlers[2] = {
        _helicopterLandingPadNo9InitHelipadLight,
        _helicopterLandingPadNo9TickHelipadLight,
    };

    handlers[task->state](task->spawnArg2.pointer, task);
}

/// Returns one when a helipad light runs in the current mapped camera view.
///
/// Requires the light model, work and enemy with light index 0..2. Checks its three
/// view IDs. Outside those views, hides it, disables both collision spheres,
/// releases the adopted sparks, decrements a nonzero spark timer and returns zero.
/// The caller then advances animation without running the break state machine.
static s32 _helicopterLandingPadNo9CheckHelipadLightView(Task* task)
{
    TmdObject*                                model;
    _HelicopterLandingPadNo9HelipadLightWork* work;
    Enemy*                                    enemy;
    s32                                       viewIndex;
    u8                                        viewMisses;
    u8                                        mappedView;

    model      = task->extra.tmd;
    work       = task->work;
    enemy      = task->spawnArg2.pointer;
    viewMisses = 0;
    mappedView = viewGetMappedIndex();
    for (viewIndex = 0; viewIndex < ARRAY_SIZE(_gHelicopterLandingPadNo9HelipadViews.lightViews[0]); viewIndex++) {
        if (mappedView != _gHelicopterLandingPadNo9HelipadViews.lightViews[work->lightIndex][viewIndex]) {
            viewMisses++;
        }
    }

    if (viewMisses != ARRAY_SIZE(_gHelicopterLandingPadNo9HelipadViews.lightViews[0])) {
        return 1;
    }

    model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->blast.flags            &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    if (work->sparksTask != NULL) {
        work->sparksTask->state = HELICOPTER_LANDING_PAD_NO_9_LIGHT_SPARKS_RELEASE;
        work->sparksTask        = NULL;
    }
    if (work->sparkFrames != 0) {
        work->sparkFrames--;
    }
    return 0;
}

/// Refreshes helipad-light model lighting at its composed root position.
///
/// Requires a live light model and current root work matrix. Uses composed-coordinate
/// translation in game units and leaves composition to the caller.
static void _helicopterLandingPadNo9UpdateHelipadLightLighting(Task* task, const GfxCoord* rootCoord)
{
    VECTOR composedPosition;

    composedPosition.vx = rootCoord->workm.t[0];
    composedPosition.vy = rootCoord->workm.t[1];
    composedPosition.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(task->extra.tmd, &composedPosition, 0, 3);
}

/// Unregisters the helipad light's target and collision bodies, then destroys it.
///
/// Requires the successfully initialized light task and its live enemy and work.
/// Target unlink precedes body unlink and enemy-owned work/task destruction;
/// adopted spark effects follow the task tree's teardown.
static void _helicopterLandingPadNo9ExitHelipadLight(Task* task)
{
    Enemy*                                    enemy = task->spawnArg2.pointer;
    _HelicopterLandingPadNo9HelipadLightWork* work  = task->work;

    worldTargetUnlinkNode(&enemy->node);
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->blast);
    enemyDestroy(enemy, task);
}

/// Dispatches the coordinate-only helipad blast source's initialization and update.
///
/// Requires task state 0..1 and the live enemy in the second spawn argument.
/// Initialization allocates the source work; the update runs its view-gated phases.
static void _helicopterLandingPadNo9BlastSourceTask(Task* task)
{
    EnemyTaskFunc states[2] = {
        _helicopterLandingPadNo9InitBlastSource,
        _helicopterLandingPadNo9TickBlastSource,
    };

    states[task->state](task->spawnArg2.pointer, task);
}

/// Unregisters the blast source's target and collision bodies, then destroys it.
///
/// Requires the successfully initialized source task and its live enemy and work.
/// Target unlink precedes body unlink and enemy-owned work/task destruction;
/// adopted flare effects follow the task tree's teardown.
static void _helicopterLandingPadNo9ExitBlastSource(Task* task)
{
    Enemy*                                   enemy = task->spawnArg2.pointer;
    _HelicopterLandingPadNo9BlastSourceWork* work  = task->work;

    worldTargetUnlinkNode(&enemy->node);
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->blast);
    enemyDestroy(enemy, task);
}
