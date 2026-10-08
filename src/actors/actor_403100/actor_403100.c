#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/gpu_image_upload.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
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

#include "overlay.h"

#include "rooms/dryfield_night_motel_balcony.h"
#include "../../shared/coord_math.h"

/// Stride and impact cues used by the combat walk and its follow-up clips.
///
/// Stride phase is a half-turn in 4096 angle units per turn.
/// Rumble counts eligible pad-script updates, shake counts running actor updates,
/// and intensity endpoints are bytes (0..255).
enum {
    ACTOR_403100_STRIDE_STEP                = 32,
    ACTOR_403100_STRIDE_MASK                = ACTOR_TRANSFORM_ANGLE_HALF_TURN - 1,
    ACTOR_403100_FOOTFALL_SOUND             = SOUND_CHARACTER(SOUND_BANK_BURNER, 1),
    ACTOR_403100_JAW_IMPACT_SOUND           = SOUND_CHARACTER(SOUND_BANK_BURNER, 2),
    ACTOR_403100_PART_JAW                   = 4,
    ACTOR_403100_FOOTFALL_FEEDBACK_FRAMES   = 30,
    ACTOR_403100_IMPACT_FEEDBACK_FRAMES     = 18,
    ACTOR_403100_FEEDBACK_START_INTENSITY   = 255,
    ACTOR_403100_FEEDBACK_END_INTENSITY     = 8,
    ACTOR_403100_IMPACT_CUE_FRAME           = 49,
    ACTOR_403100_RECOVERY_SMOKE_START_FRAME = 50,
    ACTOR_403100_RECOVERY_SMOKE_FRAMES      = 11,
    ACTOR_403100_RECOVERY_SMOKE_ARG         = -0x3FFCB400,
};

/// Timing and packed room-effect arguments shared by the two puff emitters.
///
/// The low twelve bits are the sizing numerator, bits 16..17 choose palette
/// row 2, and bit 31 chooses rising rather than horizontal generated drift.
enum {
    ACTOR_403100_PUFF_EMISSION_TICKS = 90,
    ACTOR_403100_PUFF_INTERVAL_MASK  = 7,
    ACTOR_403100_PUFF_SIZE           = 1024,
    ACTOR_403100_PUFF_PALETTE_ROW    = 2,
    ACTOR_403100_PUFF_RISING         = 0x80000000U,
    ACTOR_403100_PUFF_DRIFT_ARG      = (ACTOR_403100_PUFF_PALETTE_ROW << 16) | ACTOR_403100_PUFF_SIZE,
    ACTOR_403100_PUFF_RISING_ARG     = ACTOR_403100_PUFF_RISING | ACTOR_403100_PUFF_DRIFT_ARG,
};

/// Descriptor and room-state selections used by the defeat and flame updates.
enum {
    ACTOR_403100_FLOOR_PUFF_DESCRIPTOR   = 1,
    ACTOR_403100_BALCONY_SECTION_DAMAGED = 1,
    ACTOR_403100_FLAME_BASE_SIZE         = 150,
    ACTOR_403100_BUFFER_RELEASE_DISABLED = -1,
    ACTOR_403100_SCENE_SPRITE_CUE_0      = 0,
    ACTOR_403100_SCENE_SPRITE_CUE_1      = 1,
};

extern GpuImageUpload D_actor_403100_801555EC[2];

static void _actor403100EmitFlame(Task* task, SVECTOR* offset, const SVECTOR* velocity, s32 disableCollision);
static void _actor403100AimHead(Task* task, s16 aimMode);
static void _actor403100StepHeadAimPitch(SVECTOR* targetAngles, s32 pitchStep, s32 upperPitchLimit, s16 lowerPitchLimit);
static void _actor403100StepHeadAimYaw(const SVECTOR* targetAngles, s32 headYawStep, s32 bodyYawStep, s32 bodyCatchupStep, s32 bodyTurnStep);
static void _actor403100RelaxHeadAimRoll(void);

/// Player-region ids used by the attack range gate and attack picker.
///
/// The floor rectangles can share an id; 0 lies outside all listed regions.
enum {
    ACTOR_403100_PLAYER_REGION_1 = 1,
    ACTOR_403100_PLAYER_REGION_2 = 2,
    ACTOR_403100_PLAYER_REGION_3 = 3,
    ACTOR_403100_PLAYER_REGION_4 = 4,
    ACTOR_403100_PLAYER_REGION_5 = 5,
    ACTOR_403100_PLAYER_REGION_6 = 6,
};

/// Whether a newly emitted flame participates in grid and body-pair collision.
enum {
    ACTOR_403100_FLAME_COLLISION_ENABLED  = 0,
    ACTOR_403100_FLAME_COLLISION_DISABLED = 1,
};

/// Sound selection of an explicit jaw/head pitch-kick request.
enum {
    ACTOR_403100_PITCH_KICK_KEEP_SOUND          = 0,
    ACTOR_403100_PITCH_KICK_SELECT_ATTACK_SOUND = 1,
};

/// Burner skeleton coordinates used by the head and arm pose helpers.
enum {
    ACTOR_403100_PART_LOWER_TRUNK  = 1,
    ACTOR_403100_PART_MIDDLE_TRUNK = 2,
    ACTOR_403100_PART_HEAD         = 3,
    ACTOR_403100_PART_FOREARM      = 7,
    ACTOR_403100_PART_HAND         = 8,
};

/// Model-chain joints and room dimensions of the two arm-shadow strips.
enum {
    ACTOR_403100_ARM_SHADOW_BASE_PART   = 6,
    ACTOR_403100_ARM_SHADOW_MIDDLE_PART = 7,
    ACTOR_403100_ARM_SHADOW_TIP_PART    = 8,
    ACTOR_403100_ARM_SHADOW_HALF_WIDTH  = 1024,
    ACTOR_403100_ARM_SHADOW_FLOOR_Y     = -3200,
};

/// Head aim's inclusive pitch/yaw dead zone, in 4096 units per turn.
enum { ACTOR_403100_AIM_DEAD_ZONE = 0x20 };

/// Ends a `_Actor403100Zone` table, in the `id` of its last entry.
enum { ACTOR_403100_ZONE_END = -1 };

/// One entry of a zone table: a rectangle of the balcony floor and the id a
/// lookup returns for a point inside it.
///
/// A lookup takes the first entry containing the point, both far edges
/// included, and answers 0 when none does, so 0 is never a zone's id. This is
/// the layout of `ActorZone` with a 32-bit id.
typedef struct {
    s16 x;     // Near corner along world X
    s16 z;     // Near corner along world Z
    s16 width; // Extent along X
    s16 depth; // Extent along Z
    s32 id;    // Value returned for a point inside, or ACTOR_403100_ZONE_END
} _Actor403100Zone;
STATIC_ASSERT_SIZEOF(_Actor403100Zone, 0xC);

extern EnemyParams D_actor_403100_8014762C;

/// One puff of the flame the Burner breathes.
///
/// A puff leaves the head with a velocity fixed at that moment, drifts along
/// it and grows with age while drawn as a screen-facing sprite. Its sphere
/// carries the flame's attack to whatever it touches; a puff whose contact
/// table holds a contact stops advancing and rises instead.
///
/// Slots are handed out lowest first and the update relies on that: it walks
/// only the first `Actor403100Work::flameLifetime` slots, which hold every
/// live puff while no more than one is emitted per frame of ageing.
typedef struct {
    s16                   active;       // 1 while the puff is alive and its body linked; 0 for a free slot
    s16                   age;          // Running frames lived; the puff is released at `Actor403100Work::flameLifetime`
    s16                   spriteStep;   // Animation step of the sprite: starts at a random 0..15 and advances with `age`
    SVECTOR               velocity;     // World-space movement per frame
    SVECTOR               position;     // World-space centre
    byte                  field_16[10]; // Never accessed; role unproven
    GfxCoord              coord;        // World placement of `body`: identity rotation, translation following `position`
    WorldCollisionBody    body;         // Damaging sphere at the centre; its radius is a third of the drawn size
    WorldCollisionContact contacts[4];  // Contact table of `body`
} _Actor403100Flame;
STATIC_ASSERT_SIZEOF(_Actor403100Flame, 0xF0);

extern s16               D_actor_403100_80155810;
extern _Actor403100Flame D_actor_403100_80155814[28];

/// Exact damage-latch value accepted by the hit-reaction and pitch-kick helpers.
enum { ACTOR_403100_HIT_TAKEN = 1 };

/// Values of `Actor403100Work::jawPitchPhase` and `headPitchPhase`.
///
/// Each byte runs one kick of a model part's pitch: a quick eased swing away
/// from the animated pose followed by a slower linear return. The two parts
/// are kicked together, and a new kick is accepted only while both are at
/// rest.
enum {
    ACTOR_403100_PITCH_PHASE_REST   = 0, // No kick: any leftover offset eases back to zero
    ACTOR_403100_PITCH_PHASE_START  = 1, // A kick was requested; taken up on the next update
    ACTOR_403100_PITCH_PHASE_SWING  = 2, // Easing out to the kick's extreme
    ACTOR_403100_PITCH_PHASE_RETURN = 3, // Stepping back to zero at a fixed rate
};

/// Values of `Actor403100Work::animationRequest`.
enum {
    ACTOR_403100_ANIMATION_REQUEST_BLEND   = 1, // Blend into `animationId` over `animationBlendFrames` frames; only retimes the slots if it is already applied
    ACTOR_403100_ANIMATION_REQUEST_RESET   = 2, // Cut straight to the start of `animationId`
    ACTOR_403100_ANIMATION_REQUEST_PLAYING = 3, // `animationId` has been applied and is playing
};

/// Clips requested by the approach and build-up stun steps.
enum {
    ACTOR_403100_ANIMATION_WALK    = 1,
    ACTOR_403100_ANIMATION_STAGGER = 10,
};

/// Fight clips shared by the arm combo, jump slam and grab initializers.
enum {
    ACTOR_403100_ANIMATION_ARM_ATTACK   = 4,
    ACTOR_403100_ANIMATION_ARM_RECOVERY = 5,
    ACTOR_403100_ANIMATION_AIMED_FLAME  = 7,
    ACTOR_403100_ANIMATION_GRAB_REACH   = 9,
};

/// Clips and camera shared by the held-player squeeze and throw steps.
enum {
    ACTOR_403100_ANIMATION_SQUEEZE           = 12,
    ACTOR_403100_ANIMATION_THROW_HELD_PLAYER = 14,
    ACTOR_403100_HELD_PLAYER_THROW_VIEW      = 12,
};

/// Shoulder yaw limits and signed 12-bit turn-angle easing used by arm attacks.
enum {
    ACTOR_403100_ARM_YAW_MIN            = -0x160,
    ACTOR_403100_ARM_YAW_MAX            = 0xD0,
    ACTOR_403100_ARM_SIGNED_ANGLE_SHIFT = 20,
    ACTOR_403100_ARM_EASE_SHIFT         = 3,
};

/// Clip used for the entrance and aimed impact in the balcony scene.
enum { ACTOR_403100_ANIMATION_SCENE_IMPACT = 3 };

/// Recovery timing shared by the aimed and side flame attacks.
///
/// Blend durations count normal animation frames; the rate counts sixteenths
/// of a frame per tick. The sound fade counts audio updates, including PAL extras.
enum {
    ACTOR_403100_FLAME_RECOVERY_FRAMES               = 14,
    ACTOR_403100_BREATH_SOUND                        = SOUND_CHARACTER(SOUND_BANK_BURNER, 4),
    ACTOR_403100_FLAME_CONTACT_RECOVERY_BLEND_FRAMES = 20,
    ACTOR_403100_FLAME_CONTACT_RECOVERY_RATE         = ANIMATION_RATE_ONE * 7 / 4,
    ACTOR_403100_FLAME_CONTACT_STOP_FADE_UPDATES     = 10,
};

/// Walking phases selected by the approach, stun and low-health scene steps.
enum {
    ACTOR_403100_WALK_APPROACH      = 0,
    ACTOR_403100_WALK_FINISH_STRIDE = 4,
    ACTOR_403100_WALK_STOPPED       = 5,
};

/// Fight behaviour that closes on the player's region and selects an attack.
enum { ACTOR_403100_BEHAVIOUR_ATTACK_APPROACH = 1 };

/// Camera held throughout the low-health scene and restored on completion.
enum { ACTOR_403100_LOW_HEALTH_SCENE_VIEW = 24 };

/// Values of `Actor403100Work::aimMode`: how the head is turned each frame.
///
/// The tracking modes step `headAim` toward the direction of `aimTarget` and
/// turn the root with it, straightening the roll; a target outside the head's
/// reach lets the pitch drop back and turns the root alone. The others let
/// the animation have the head back.
enum {
    ACTOR_403100_AIM_TRACK         = 0, // Track at 8 a frame
    ACTOR_403100_AIM_YAW_ONLY      = 1, // Track in yaw at 8 a frame; pitch and roll ease to the head's animated angles
    ACTOR_403100_AIM_ANIMATED      = 2, // All three angles ease to the animated angles of the head and the two parts under it
    ACTOR_403100_AIM_TRACK_FAST    = 3, // Track at 16 a frame
    ACTOR_403100_AIM_YAW_ONLY_FAST = 4, // As `ACTOR_403100_AIM_YAW_ONLY` at 16 a frame
    ACTOR_403100_AIM_TRACK_STEPPED = 5, // As `ACTOR_403100_AIM_TRACK_FAST`, the yaw stepping by `aimYawStep`
};

/// Work block of the Burner, the boss fought on the night motel's balcony.
///
/// The task allocates it zeroed and keeps it in `Task::work`; the package
/// reaches it through one global pointer instead, so only one Burner can
/// exist. `Task::state` selects the scripted scenes, the fight, the defeat
/// and the teardown; within each, `state` picks a handler and `subState` the
/// step that handler is on.
///
/// The model has fifteen parts. The ones this block poses on top of the
/// animation are the head (part 3, on the trunk parts 1 and 2) with its jaw
/// (part 4), and the long arm: upper arm (5), forearm (6), hand (7) and claw
/// (8). During the fight the head is aimed at a point (`aimMode`), the arm is
/// swung from the shoulder (`armPitch`, `armYaw`), and while the hand holds
/// the player the forearm is turned and slid along the arm in strokes.
///
/// The bytes named `field_XX` have no access anywhere in the package; whether
/// they are members at all is unproven.
typedef struct {
    MATRIX                color;                  // Colour matrix lent to the model
    MATRIX                light;                  // Light matrix lent to the model
    MATRIX                savedRootMatrix;        // Root coordinate's matrix, kept across the low-health sequence
    byte                  field_60[0x20];
    SVECTOR               rotation;               // Rotation of the root (4096 a turn); only `vy`, the heading, is applied, wrapped to 12 bits
    SVECTOR               savedRotation;          // `rotation`, kept across the low-health sequence
    SVECTOR               playerPosition;         // Translation of the player's root coordinate, sampled each fight frame
    SVECTOR               aimTarget;              // Point the head turns toward: `playerPosition` unless the running step replaces it
    SVECTOR               forearmTurn;            // Euler angles added to the forearm's animated rotation
    SVECTOR               savedForearmTurn;       // `forearmTurn` of the hold pose kept while the pose is run ahead
    SVECTOR               headAim;                // Pitch, yaw and roll of the head relative to the body, applied in Z-X-Y order in place of its animated rotation
    ActorAnimRig15        rig;                    // Playback storage of the fifteen-part model; slots 1 to 14 are driven
    WorldCollisionBody    trunkBody;              // Sphere of radius 0x800 at part 1 on list 2, receiving attacks into `hitContacts`
    WorldCollisionContact initializedContacts[3]; // Initialized at setup and bound to no body
    WorldCollisionBody    headBody;               // Sphere ahead of the head on list 2, receiving attacks into `hitContacts`; radius 0x400, 0x500 while the player is held
    WorldCollisionContact hitContacts[8];         // Contacts of both receiving spheres, also lent to the enemy record; each frame's hits are read from them and cleared
    WorldCollisionBody    handAttack;             // Sphere of radius 0x3A0 on list 3 at the hand; a player contact sets `handTouchedPlayer`
    WorldCollisionContact handContacts[1];        // Contact of `handAttack`
    WorldCollisionBody    forearmAttack;          // Sphere of radius 0x3A0 on list 3 at the forearm; a player contact sets `forearmTouchedPlayer`
    WorldCollisionContact forearmContacts[1];     // Contact of `forearmAttack`
    byte                  field_5CC[4];
    s32                   fightFramesLeft;        // Frames of the fight left (5400 at its start), counted down to -1; once negative, low health hands over to an event script instead of the low-health sequence
    s32                   savedForearmX;          // Forearm's X translation of the hold pose, kept while the pose is run ahead
    s16                   defeatScale;            // Scale of the root in the defeat sequence (4096 = 1.0)
    s16                   animationRequest;       // `ACTOR_403100_ANIMATION_REQUEST_*`, 0 before the first request
    s16                   appliedAnimation;       // Animation the slots were last started on
    s16                   animationId;            // Requested animation: index into the package's animation table
    u16                   animationFrames;        // Frames since the request was applied
    s16                   animationRate;          // Playback rate of slots 1 to 14; `ANIMATION_RATE_ONE` is normal speed, negative plays backwards
    s16                   hitCooldown;            // Frames before another hit is taken; set from the hit's id parameter 2
    s16                   engageDelay;            // Frames (30 from the fight's start) until the battle is flagged engaged, if the Burner still lives
    s16                   jawPitchOffset;         // Pitch added to the jaw's animated rotation by its kick
    s16                   headPitchOffset;        // Pitch added to the head by its kick
    u16                   stateFrames;            // Frames spent in the current step
    s16                   auxFrames;              // Second counter of the running step: cues played, frames since the walk clip restarted, or frames of forearm strokes
    s16                   savedAuxFrames;         // `auxFrames` of the hold pose, kept while the pose is run ahead
    s16                   playerReactionStage;    // `ACTOR_403100_PLAYER_REACTION_*`: the player's reaction to an arm hit
    s16                   playerReactionFrames;   // Frames left of stage 1, 23 when the hit lands
    s16                   walkStage;              // Walk along the balcony (0 closing on the player's region, 1 finishing the stride then closing again, 4 finishing the stride then stopping, 5 stopped; 2 and 3 idle)
    u16                   state;                  // Index into the handler table of the current task state
    u16                   subState;               // Step of the current `state`, numbered separately by each
    s16                   animationBlendFrames;   // Frames a blend request takes; cleared when it is applied
    s16                   shakeFrames;            // Frames the screen still shakes vertically, harder above 15
    s16                   stridePhase;            // Phase of the walking stride (half a turn, 0..0x7FF, 0x20 a frame); the root bobs on its sine and a footfall sounds as it wraps
    s16                   previousStridePhase;    // `stridePhase` as the frame began
    s16                   armPitch;               // Rotation about X given the upper arm, in the root's frame
    s16                   savedArmPitch;          // `armPitch` of the hold pose, kept while the pose is run ahead
    s16                   armYaw;                 // Rotation about Y given the upper arm, in the root's frame
    s16                   savedArmYaw;            // `armYaw` of the hold pose, kept while the pose is run ahead
    s16                   hitColorFrames;         // Frames until the hit tint returns to the default colour; 0 when idle
    s16                   overlayX;               // Screen X offset of the two foreground quads drawn while the player is held
    s16                   overlayY;               // Screen Y offset of the same quads
    s16                   recentStates[3];        // The last three attack states picked; a third of a kind in a row is swapped for another
    s16                   sceneScale;             // Scale of the root in the scripted scenes (4096 = 1.0)
    byte                  field_61A[2];
    s16                   aimMode;                // `ACTOR_403100_AIM_*`
    s16                   jumpAcceleration;       // Change of `jumpSpeed` per frame, itself stepped by 4
    s16                   jumpSpeed;              // Height the root gains per frame of the jump
    s16                   savedView;              // Camera view in use when the block was set up or the low-health sequence began; restored after the latter
    byte                  field_624[2];
    s16                   armYawTarget;           // Yaw the arm swings toward: the head's as the swing began, kept within -0x160..0xD0
    s16                   playerRegion;           // Region of the balcony the player stands in (1 to 6, 0 outside all)
    s16                   hitReaction;            // Reaction the last hit asks for (0 none, 1 flinch, 2 stagger, 3 build-up stun)
    s16                   walkSpeed;              // Distance walked per frame
    s16                   playerDistance;         // Horizontal distance from the root to the player
    s16                   hitDistance;            // Horizontal distance from the player to its own offset from the root read in the head's frame; halved into the damage roll
    s16                   field_632;              // Cleared as the grab begins and as the hold ends; never read, role unproven
    u16                   previousAnimationFlags; // Slot 1's ANIMATION_SLOT_* results as the last running update's tick left them
    s16                   flameLifetime;          // Frames a flame puff lives (28 or 20); also how many slots of the flame pool the update walks
    s16                   repromptDelay;          // Frames until the held player is prompted for button presses again; 0 while a prompt runs
    s16                   squeezeFrames;          // Frames of the hold since its last damage; 180 deal the next
    s16                   sectionDamaged[9];      // Nonzero once the balcony section of that index has been switched to its damaged look
    byte                  field_64E[6];
    s16                   playerDeathFrames;      // Frames since the held player was killed
    s16                   holdStartHp;            // Burner's hit points as the hold began
    s16                   bufferReleaseDelay;     // Frames until the model's draw buffers are freed in a scripted scene; -1 when idle
    s16                   aimYawStep;             // Yaw step of `ACTOR_403100_AIM_TRACK_STEPPED` (8, or 16 at low health)
    u8                    lowHealth;              // 1 while hit points are under 35% of the maximum
    s8                    playerAnimationId;      // Animation last requested of the player from the package's table; never read
    u8                    stateCounter;           // Scratch of the current state: arm swings left, or prompts reissued during the hold
    u8                    releaseRequested;       // Set by message 2014: the player finished the prompted button presses or was killed
    u8                    promptPending;          // Set when the held player is due a new button prompt
    u8                    phaseChangeDone;        // Set once the low-health sequence has played, so that it plays once
    byte                  field_662[3];
    u8                    jawPitchPhase;          // ACTOR_403100_PITCH_PHASE_* of the kick added to the jaw's pitch
    u8                    headPitchPhase;         // ACTOR_403100_PITCH_PHASE_* of the kick added to the head's pitch
    u8                    forearmStrokeDone;      // 1 once the forearm has slid to the end of its current stroke; 0 from the stroke's start
    u8                    handTouchedPlayer;      // Latched when the claw or `handAttack` reaches the player; cleared by the step that acts on it
    u8                    forearmTouchedPlayer;   // Latched when the hand or `forearmAttack` reaches the player; cleared the same way
    byte                  field_66A[2];
    s8                    hitTaken;               // 1 when a hit or a status tick dealt damage this frame; lets `hitReaction` be consumed
    u8                    recentStateCursor;      // Entry of `recentStates` the next pick overwrites
    u8                    swingConnected;         // Set when a swing of the arm combo has hit the player
    u8                    jawKickSound;           // Sound the next jaw kick plays as it starts (0 none, 1 the Burner's sound 9, 2 its sound 2 or 5 at random)
    u8                    playerKilled;           // Set when an attack of the grab or the hold has killed the player
    u8                    holdingPlayer;          // Set from the grab until the player is let go; keeps the low-health sequence from starting
    byte                  field_672;
    u8                    vulnerable;             // Set while the player is held and during the low-health sequence: hits are doubled and defeat is deferred
    byte                  field_674[4];
} Actor403100Work;
STATIC_ASSERT_SIZEOF(Actor403100Work, 0x678);

static void _actor403100UpdateFlames(Task* unusedTask, s32 sizeBias);

static void _actor403100BeginDefeat(Task* task);
static void _actor403100StepDefeatRise(Task* task);
static void _actor403100StepDefeatBurn(Task* task);
static void _actor403100BeginDefeatRewardWait(Task* unusedTask);
static void _actor403100StepDefeatRewardWait(Task* task);

static void _actor403100DrawArmShadow(Task* task, s16 startPartIndex, s16 endPartIndex, s16 halfWidth, s16 floorY);

static void _actor403100UpdateAnimation();
static void _actor403100TurnUpperArm(Task* task);
static void _actor403100BeginLowHealthScene(Task* task);
static void _actor403100StepLowHealthScene(Task* task);
static void _actor403100RefreshBreathLight(Task* task);
static void _actor403100PlacePlayer(s16 x, s16 y, s16 z, s16 yaw);
static void _actor403100PlayPlayerAnimation(s16 animationId, s16 messageId);
static void _actor403100RequestHitPitchKick(void);
static s32  _actor403100GetWorldRotation(const GfxCoord* joint, MATRIX* rotation);
static s32  _actorRenderAccumulateJointRotation(const GfxCoord* joint, MATRIX* rotation, const GfxCoord* excludedAncestor);
static void _actor403100TurnForearm(Task* task);
static void _actor403100RequestAnimationBlend(s16 animationId, s16 rate, s16 blendFrames);
static void _actor403100BeginStagger(void);
static void _actor403100FinishStagger(void);

extern DamageAttack D_actor_403100_80147614[6];
static void         _actor403100BreakBalconyAtArm(Task* task);
static void         _actor403100ProbeArmPlayerContact(Task* task);

extern u8 D_actor_403100_801557A8[];

extern TaskDesc D_actor_403100_8015560C[];
extern s16      D_actor_403100_80155794[5][2];

static void _actor403100SceneIdleState0(Task* unusedTask);
static void _actor403100DispatchSceneEntrance(Task* task);
static void _actor403100DispatchSceneAimAndImpact(Task* task);
static void _actor403100DispatchSceneAdvance(Task* task);
static void _actor403100DispatchSceneFlameBreath(Task* task);
static void _actor403100DispatchSceneBalconyImpact(Task* task);
static void _actor403100DispatchSceneTurn(Task* task);
static void _actor403100DispatchSceneFlameRetreat(Task* task);
static void _actor403100DispatchSceneDeparture(Task* task);
static void _actor403100SceneIdleState9(Task* unusedTask);

/// Values of `Actor403100Work::playerReactionStage`: how far the player is
/// through the reaction to a hit of the arm.
///
/// The hit starts the package's hit clip on the player; the stages then
/// follow one another until the player is handed back to its own control.
enum {
    ACTOR_403100_PLAYER_REACTION_NONE       = 0, // No reaction running
    ACTOR_403100_PLAYER_REACTION_HIT_HELD   = 1, // Hit clip restarted every frame while `playerReactionFrames` runs down; a dead player stays here
    ACTOR_403100_PLAYER_REACTION_HIT_ENDING = 2, // Hit clip left to play out; the recovery clip of the equipped weapon follows it
    ACTOR_403100_PLAYER_REACTION_RECOVERING = 3, // Recovery clip playing; its end releases the player
    ACTOR_403100_PLAYER_REACTION_COUNT,
};

/// The handlers of the player's reaction, indexed by
/// `ACTOR_403100_PLAYER_REACTION_*`.
///
/// The fight's update copies the table to the stack and calls the entry of
/// the current stage once a frame, ahead of the Burner's own state. The call
/// is unconditional, so every stage has a handler; they take no argument and
/// reach the work block through the package's pointer.
typedef struct {
    void (*handlers[ACTOR_403100_PLAYER_REACTION_COUNT])(void); // Handler of each stage
} _Actor403100PlayerReactionTable;
STATIC_ASSERT_SIZEOF(_Actor403100PlayerReactionTable, 0x10);

extern _Actor403100Zone D_actor_403100_80155638[];
extern EvsCommand       D_actor_335800_80166098[];
static s32              _actor403100FindZoneId(s16 x, s16 z, const _Actor403100Zone* zone);

/// Rows of `_Actor403100AttackPickStorage::states`.
enum {
    ACTOR_403100_ATTACK_ODDS_NORMAL   = 0, // The player has at least half their hit points and the Burner is not at low health
    ACTOR_403100_ATTACK_ODDS_WEAKENED = 1, // Either of them is weakened
    ACTOR_403100_ATTACK_ODDS_COUNT,
};

/// Draws in one row of `_Actor403100AttackPickStorage::states`; a power of
/// two, the draw being that many low bits of a random number.
enum { ACTOR_403100_ATTACK_DRAWS = 16 };

/// Static allocation of the odds the Burner picks its next attack by.
///
/// Each row lists `ACTOR_403100_ATTACK_DRAWS` equally likely values of
/// `Actor403100Work::state`, so a state's share of a row is its chance. The
/// pick is a first choice: the picker then swaps a state the situation rules
/// out, or a third of a kind in a row, for another.
///
/// Sixteen zero bytes follow the rows. No access to them is recovered, so
/// whether they are an unused third row or a separate unreferenced variable
/// is unproven; they stay in this allocation only to keep the data after it
/// at its address.
typedef struct {
    u8 states[ACTOR_403100_ATTACK_ODDS_COUNT][ACTOR_403100_ATTACK_DRAWS]; // Attack states by `ACTOR_403100_ATTACK_ODDS_*` row, one per draw
    u8 unknown_20[16];                                                    // Zero in the image; no access established and role unproven
} _Actor403100AttackPickStorage;
STATIC_ASSERT_SIZEOF(_Actor403100AttackPickStorage, 0x30);

extern _Actor403100AttackPickStorage D_actor_403100_801557B0;

extern _Actor403100Zone D_actor_403100_80155698[];

static AnimationSet _gActor403100Animation1AF4C;
static AnimationSet _gActor403100Animation1BB74;
static AnimationSet _gActor403100Animation1BCAC;
static AnimationSet _gActor403100Animation1C240;
static AnimationSet _gActor403100Animation1CDF0;
static AnimationSet _gActor403100Animation1D8EC;
static void         _actor403100ApplyCommand(Task* task, s32 unusedMessageId, const ActorCommand* command, s32 unusedSecondArg);
static void         _actor403100RequestPlayerRelease(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);
static void         _actor403100SetModelDrawMode(Task* task, s32 unusedMessageId, s32 drawMode, s32 unusedSecondArg);

static AnimationSet _gActor403100Animation15BB0;
static AnimationSet _gActor403100Animation163CC;
static AnimationSet _gActor403100Animation16A80;
static AnimationSet _gActor403100Animation17258;
static AnimationSet _gActor403100Animation17584;
static AnimationSet _gActor403100Animation17690;
static AnimationSet _gActor403100Animation180D8;
static AnimationSet _gActor403100Animation18A88;
static AnimationSet _gActor403100Animation197D8;
static AnimationSet _gActor403100Animation19C90;
static AnimationSet _gActor403100Animation19F88;
static AnimationSet _gActor403100Animation1A138;
static AnimationSet _gActor403100Animation1A318;
static AnimationSet _gActor403100Animation1A7C0;
static AnimationSet _gActor403100Animation1A9CC;
static AnimationSet _gActor403100Animation1E600;
static AnimationSet _gActor403100Animation1FA78;
static AnimationSet _gActor403100Animation20004;
static AnimationSet _gActor403100Animation2014C;
static AnimationSet _gActor403100Animation20624;
static AnimationSet _gActor403100Animation20AD4;
static AnimationSet _gActor403100Animation20FA4;
static AnimationSet _gActor403100Animation2142C;
static AnimationSet _gActor403100Animation2192C;

static TmdSource _gActor403100BurnerBody;
static void      _actor403100FloorPuffEmitterTask(Task* task);
static void      _actor403100DriftPuffEmitterTask(Task* task);
static void      _actor403100Task(Task* task);

extern u_long D_actor_403100_80153774[1950];

static TmdBone _gActor403100BurnerBodySkeleton[15] = {
#include "assets/burner_body_skeleton.inc"
};

static u32 _gActor403100BurnerBodyPartVerts[15] = {
#include "assets/burner_body_partVerts.inc"
};

static SVECTOR _gActor403100BurnerBodyVerts[524] = {
#include "assets/burner_body_verts.inc"
};

static SVECTOR _gActor403100BurnerBodyNormals[543] = {
#include "assets/burner_body_normals.inc"
};

static u32 _gActor403100BurnerBodyStream[5791] = {
#include "assets/burner_body_stream.inc"
};

static TmdSource _gActor403100BurnerBody = {
    0,
    32572,
    8568,
    15,
    _gActor403100BurnerBodyPartVerts,
    _gActor403100BurnerBodyVerts,
    _gActor403100BurnerBodyNormals,
    _gActor403100BurnerBodySkeleton,
    _gActor403100BurnerBodyStream,
};

DamageAttack D_actor_403100_80147614[6] = {
    { 20, 0 },
    { 25, 6 },
    { 30, 0 },
    { 8, 0 },
    { 10, 6 },
    { 15, 0 },
};

EnemyParams D_actor_403100_8014762C = { D_actor_403100_80147614, 4650, 2000, 1000, 100, 200, 20, 100, 5 };

static AnimationPackedPose _gActor403100Animation15BB0Bank1[4] = {
#include "assets/actor_403100_animation_15BB0_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation15BB0Bank4[76] = {
#include "assets/actor_403100_animation_15BB0_bank4.inc"
};

static AnimationRecord _gActor403100Animation15BB0Records[133] = {
#include "assets/actor_403100_animation_15BB0_records.inc"
};

static u16 _gActor403100Animation15BB0Indices[16] = {
#include "assets/actor_403100_animation_15BB0_indices.inc"
};

static AnimationSet _gActor403100Animation15BB0 = {
    _gActor403100Animation15BB0Records,
    _gActor403100Animation15BB0Indices,
    { NULL, _gActor403100Animation15BB0Bank1, NULL, NULL, _gActor403100Animation15BB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation163CCBank1[21] = {
#include "assets/actor_403100_animation_163CC_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation163CCBank4[186] = {
#include "assets/actor_403100_animation_163CC_bank4.inc"
};

static AnimationRecord _gActor403100Animation163CCRecords[252] = {
#include "assets/actor_403100_animation_163CC_records.inc"
};

static u16 _gActor403100Animation163CCIndices[16] = {
#include "assets/actor_403100_animation_163CC_indices.inc"
};

static AnimationSet _gActor403100Animation163CC = {
    _gActor403100Animation163CCRecords,
    _gActor403100Animation163CCIndices,
    { NULL, _gActor403100Animation163CCBank1, NULL, NULL, _gActor403100Animation163CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation16A80Bank1[16] = {
#include "assets/actor_403100_animation_16A80_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation16A80Bank4[159] = {
#include "assets/actor_403100_animation_16A80_bank4.inc"
};

static AnimationRecord _gActor403100Animation16A80Records[204] = {
#include "assets/actor_403100_animation_16A80_records.inc"
};

static u16 _gActor403100Animation16A80Indices[16] = {
#include "assets/actor_403100_animation_16A80_indices.inc"
};

static AnimationSet _gActor403100Animation16A80 = {
    _gActor403100Animation16A80Records,
    _gActor403100Animation16A80Indices,
    { NULL, _gActor403100Animation16A80Bank1, NULL, NULL, _gActor403100Animation16A80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation17258Bank1[18] = {
#include "assets/actor_403100_animation_17258_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation17258Bank4[191] = {
#include "assets/actor_403100_animation_17258_bank4.inc"
};

static AnimationRecord _gActor403100Animation17258Records[239] = {
#include "assets/actor_403100_animation_17258_records.inc"
};

static u16 _gActor403100Animation17258Indices[16] = {
#include "assets/actor_403100_animation_17258_indices.inc"
};

static AnimationSet _gActor403100Animation17258 = {
    _gActor403100Animation17258Records,
    _gActor403100Animation17258Indices,
    { NULL, _gActor403100Animation17258Bank1, NULL, NULL, _gActor403100Animation17258Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation17584Bank1[7] = {
#include "assets/actor_403100_animation_17584_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation17584Bank4[69] = {
#include "assets/actor_403100_animation_17584_bank4.inc"
};

static AnimationRecord _gActor403100Animation17584Records[95] = {
#include "assets/actor_403100_animation_17584_records.inc"
};

static u16 _gActor403100Animation17584Indices[16] = {
#include "assets/actor_403100_animation_17584_indices.inc"
};

static AnimationSet _gActor403100Animation17584 = {
    _gActor403100Animation17584Records,
    _gActor403100Animation17584Indices,
    { NULL, _gActor403100Animation17584Bank1, NULL, NULL, _gActor403100Animation17584Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation17690Bank1[2] = {
#include "assets/actor_403100_animation_17690_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation17690Bank4[13] = {
#include "assets/actor_403100_animation_17690_bank4.inc"
};

static AnimationRecord _gActor403100Animation17690Records[30] = {
#include "assets/actor_403100_animation_17690_records.inc"
};

static u16 _gActor403100Animation17690Indices[16] = {
#include "assets/actor_403100_animation_17690_indices.inc"
};

static AnimationSet _gActor403100Animation17690 = {
    _gActor403100Animation17690Records,
    _gActor403100Animation17690Indices,
    { NULL, _gActor403100Animation17690Bank1, NULL, NULL, _gActor403100Animation17690Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation180D8Bank1[29] = {
#include "assets/actor_403100_animation_180D8_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation180D8Bank4[244] = {
#include "assets/actor_403100_animation_180D8_bank4.inc"
};

static AnimationRecord _gActor403100Animation180D8Records[309] = {
#include "assets/actor_403100_animation_180D8_records.inc"
};

static u16 _gActor403100Animation180D8Indices[16] = {
#include "assets/actor_403100_animation_180D8_indices.inc"
};

static AnimationSet _gActor403100Animation180D8 = {
    _gActor403100Animation180D8Records,
    _gActor403100Animation180D8Indices,
    { NULL, _gActor403100Animation180D8Bank1, NULL, NULL, _gActor403100Animation180D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation18A88Bank1[23] = {
#include "assets/actor_403100_animation_18A88_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation18A88Bank4[242] = {
#include "assets/actor_403100_animation_18A88_bank4.inc"
};

static AnimationRecord _gActor403100Animation18A88Records[291] = {
#include "assets/actor_403100_animation_18A88_records.inc"
};

static u16 _gActor403100Animation18A88Indices[16] = {
#include "assets/actor_403100_animation_18A88_indices.inc"
};

static AnimationSet _gActor403100Animation18A88 = {
    _gActor403100Animation18A88Records,
    _gActor403100Animation18A88Indices,
    { NULL, _gActor403100Animation18A88Bank1, NULL, NULL, _gActor403100Animation18A88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation197D8Bank1[37] = {
#include "assets/actor_403100_animation_197D8_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation197D8Bank4[327] = {
#include "assets/actor_403100_animation_197D8_bank4.inc"
};

static AnimationRecord _gActor403100Animation197D8Records[396] = {
#include "assets/actor_403100_animation_197D8_records.inc"
};

static u16 _gActor403100Animation197D8Indices[16] = {
#include "assets/actor_403100_animation_197D8_indices.inc"
};

static AnimationSet _gActor403100Animation197D8 = {
    _gActor403100Animation197D8Records,
    _gActor403100Animation197D8Indices,
    { NULL, _gActor403100Animation197D8Bank1, NULL, NULL, _gActor403100Animation197D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation19C90Bank1[10] = {
#include "assets/actor_403100_animation_19C90_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation19C90Bank4[112] = {
#include "assets/actor_403100_animation_19C90_bank4.inc"
};

static AnimationRecord _gActor403100Animation19C90Records[142] = {
#include "assets/actor_403100_animation_19C90_records.inc"
};

static u16 _gActor403100Animation19C90Indices[16] = {
#include "assets/actor_403100_animation_19C90_indices.inc"
};

static AnimationSet _gActor403100Animation19C90 = {
    _gActor403100Animation19C90Records,
    _gActor403100Animation19C90Indices,
    { NULL, _gActor403100Animation19C90Bank1, NULL, NULL, _gActor403100Animation19C90Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation19F88Bank1[4] = {
#include "assets/actor_403100_animation_19F88_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation19F88Bank4[67] = {
#include "assets/actor_403100_animation_19F88_bank4.inc"
};

static AnimationRecord _gActor403100Animation19F88Records[93] = {
#include "assets/actor_403100_animation_19F88_records.inc"
};

static u16 _gActor403100Animation19F88Indices[16] = {
#include "assets/actor_403100_animation_19F88_indices.inc"
};

static AnimationSet _gActor403100Animation19F88 = {
    _gActor403100Animation19F88Records,
    _gActor403100Animation19F88Indices,
    { NULL, _gActor403100Animation19F88Bank1, NULL, NULL, _gActor403100Animation19F88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation1A138Bank1[3] = {
#include "assets/actor_403100_animation_1A138_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation1A138Bank4[17] = {
#include "assets/actor_403100_animation_1A138_bank4.inc"
};

static AnimationRecord _gActor403100Animation1A138Records[64] = {
#include "assets/actor_403100_animation_1A138_records.inc"
};

static u16 _gActor403100Animation1A138Indices[16] = {
#include "assets/actor_403100_animation_1A138_indices.inc"
};

static AnimationSet _gActor403100Animation1A138 = {
    _gActor403100Animation1A138Records,
    _gActor403100Animation1A138Indices,
    { NULL, _gActor403100Animation1A138Bank1, NULL, NULL, _gActor403100Animation1A138Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation1A318Bank1[5] = {
#include "assets/actor_403100_animation_1A318_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation1A318Bank4[31] = {
#include "assets/actor_403100_animation_1A318_bank4.inc"
};

static AnimationRecord _gActor403100Animation1A318Records[56] = {
#include "assets/actor_403100_animation_1A318_records.inc"
};

static u16 _gActor403100Animation1A318Indices[16] = {
#include "assets/actor_403100_animation_1A318_indices.inc"
};

static AnimationSet _gActor403100Animation1A318 = {
    _gActor403100Animation1A318Records,
    _gActor403100Animation1A318Indices,
    { NULL, _gActor403100Animation1A318Bank1, NULL, NULL, _gActor403100Animation1A318Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation1A7C0Bank1[9] = {
#include "assets/actor_403100_animation_1A7C0_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation1A7C0Bank4[107] = {
#include "assets/actor_403100_animation_1A7C0_bank4.inc"
};

static AnimationRecord _gActor403100Animation1A7C0Records[146] = {
#include "assets/actor_403100_animation_1A7C0_records.inc"
};

static u16 _gActor403100Animation1A7C0Indices[16] = {
#include "assets/actor_403100_animation_1A7C0_indices.inc"
};

static AnimationSet _gActor403100Animation1A7C0 = {
    _gActor403100Animation1A7C0Records,
    _gActor403100Animation1A7C0Indices,
    { NULL, _gActor403100Animation1A7C0Bank1, NULL, NULL, _gActor403100Animation1A7C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation1A9CCBank1[3] = {
#include "assets/actor_403100_animation_1A9CC_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation1A9CCBank4[41] = {
#include "assets/actor_403100_animation_1A9CC_bank4.inc"
};

static AnimationRecord _gActor403100Animation1A9CCRecords[63] = {
#include "assets/actor_403100_animation_1A9CC_records.inc"
};

static u16 _gActor403100Animation1A9CCIndices[16] = {
#include "assets/actor_403100_animation_1A9CC_indices.inc"
};

static AnimationSet _gActor403100Animation1A9CC = {
    _gActor403100Animation1A9CCRecords,
    _gActor403100Animation1A9CCIndices,
    { NULL, _gActor403100Animation1A9CCBank1, NULL, NULL, _gActor403100Animation1A9CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation1AF4CBank1[2] = {
#include "assets/actor_403100_animation_1AF4C_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation1AF4CBank4[135] = {
#include "assets/actor_403100_animation_1AF4C_bank4.inc"
};

static AnimationRecord _gActor403100Animation1AF4CRecords[191] = {
#include "assets/actor_403100_animation_1AF4C_records.inc"
};

static u16 _gActor403100Animation1AF4CIndices[20] = {
#include "assets/actor_403100_animation_1AF4C_indices.inc"
};

static AnimationSet _gActor403100Animation1AF4C = {
    _gActor403100Animation1AF4CRecords,
    _gActor403100Animation1AF4CIndices,
    { NULL, _gActor403100Animation1AF4CBank1, NULL, NULL, _gActor403100Animation1AF4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation1BB74Bank1[23] = {
#include "assets/actor_403100_animation_1BB74_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation1BB74Bank4[309] = {
#include "assets/actor_403100_animation_1BB74_bank4.inc"
};

static AnimationRecord _gActor403100Animation1BB74Records[380] = {
#include "assets/actor_403100_animation_1BB74_records.inc"
};

static u16 _gActor403100Animation1BB74Indices[20] = {
#include "assets/actor_403100_animation_1BB74_indices.inc"
};

static AnimationSet _gActor403100Animation1BB74 = {
    _gActor403100Animation1BB74Records,
    _gActor403100Animation1BB74Indices,
    { NULL, _gActor403100Animation1BB74Bank1, NULL, NULL, _gActor403100Animation1BB74Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation1BCACBank1[2] = {
#include "assets/actor_403100_animation_1BCAC_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation1BCACBank4[14] = {
#include "assets/actor_403100_animation_1BCAC_bank4.inc"
};

static AnimationRecord _gActor403100Animation1BCACRecords[38] = {
#include "assets/actor_403100_animation_1BCAC_records.inc"
};

static u16 _gActor403100Animation1BCACIndices[20] = {
#include "assets/actor_403100_animation_1BCAC_indices.inc"
};

static AnimationSet _gActor403100Animation1BCAC = {
    _gActor403100Animation1BCACRecords,
    _gActor403100Animation1BCACIndices,
    { NULL, _gActor403100Animation1BCACBank1, NULL, NULL, _gActor403100Animation1BCACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation1C240Bank1[4] = {
#include "assets/actor_403100_animation_1C240_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation1C240Bank4[144] = {
#include "assets/actor_403100_animation_1C240_bank4.inc"
};

static AnimationRecord _gActor403100Animation1C240Records[181] = {
#include "assets/actor_403100_animation_1C240_records.inc"
};

static u16 _gActor403100Animation1C240Indices[20] = {
#include "assets/actor_403100_animation_1C240_indices.inc"
};

static AnimationSet _gActor403100Animation1C240 = {
    _gActor403100Animation1C240Records,
    _gActor403100Animation1C240Indices,
    { NULL, _gActor403100Animation1C240Bank1, NULL, NULL, _gActor403100Animation1C240Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation1CDF0Bank1[20] = {
#include "assets/actor_403100_animation_1CDF0_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation1CDF0Bank4[310] = {
#include "assets/actor_403100_animation_1CDF0_bank4.inc"
};

static AnimationRecord _gActor403100Animation1CDF0Records[358] = {
#include "assets/actor_403100_animation_1CDF0_records.inc"
};

static u16 _gActor403100Animation1CDF0Indices[20] = {
#include "assets/actor_403100_animation_1CDF0_indices.inc"
};

static AnimationSet _gActor403100Animation1CDF0 = {
    _gActor403100Animation1CDF0Records,
    _gActor403100Animation1CDF0Indices,
    { NULL, _gActor403100Animation1CDF0Bank1, NULL, NULL, _gActor403100Animation1CDF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation1D8ECBank1[26] = {
#include "assets/actor_403100_animation_1D8EC_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation1D8ECBank4[260] = {
#include "assets/actor_403100_animation_1D8EC_bank4.inc"
};

static AnimationRecord _gActor403100Animation1D8ECRecords[345] = {
#include "assets/actor_403100_animation_1D8EC_records.inc"
};

static u16 _gActor403100Animation1D8ECIndices[20] = {
#include "assets/actor_403100_animation_1D8EC_indices.inc"
};

static AnimationSet _gActor403100Animation1D8EC = {
    _gActor403100Animation1D8ECRecords,
    _gActor403100Animation1D8ECIndices,
    { NULL, _gActor403100Animation1D8ECBank1, NULL, NULL, _gActor403100Animation1D8ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation1E600Bank1[26] = {
#include "assets/actor_403100_animation_1E600_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation1E600Bank4[331] = {
#include "assets/actor_403100_animation_1E600_bank4.inc"
};

static AnimationRecord _gActor403100Animation1E600Records[410] = {
#include "assets/actor_403100_animation_1E600_records.inc"
};

static u16 _gActor403100Animation1E600Indices[16] = {
#include "assets/actor_403100_animation_1E600_indices.inc"
};

static AnimationSet _gActor403100Animation1E600 = {
    _gActor403100Animation1E600Records,
    _gActor403100Animation1E600Indices,
    { NULL, _gActor403100Animation1E600Bank1, NULL, NULL, _gActor403100Animation1E600Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation1FA78Bank1[22] = {
#include "assets/actor_403100_animation_1FA78_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation1FA78Bank4[556] = {
#include "assets/actor_403100_animation_1FA78_bank4.inc"
};

static AnimationRecord _gActor403100Animation1FA78Records[670] = {
#include "assets/actor_403100_animation_1FA78_records.inc"
};

static u16 _gActor403100Animation1FA78Indices[16] = {
#include "assets/actor_403100_animation_1FA78_indices.inc"
};

static AnimationSet _gActor403100Animation1FA78 = {
    _gActor403100Animation1FA78Records,
    _gActor403100Animation1FA78Indices,
    { NULL, _gActor403100Animation1FA78Bank1, NULL, NULL, _gActor403100Animation1FA78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation20004Bank1[10] = {
#include "assets/actor_403100_animation_20004_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation20004Bank4[131] = {
#include "assets/actor_403100_animation_20004_bank4.inc"
};

static AnimationRecord _gActor403100Animation20004Records[176] = {
#include "assets/actor_403100_animation_20004_records.inc"
};

static u16 _gActor403100Animation20004Indices[16] = {
#include "assets/actor_403100_animation_20004_indices.inc"
};

static AnimationSet _gActor403100Animation20004 = {
    _gActor403100Animation20004Records,
    _gActor403100Animation20004Indices,
    { NULL, _gActor403100Animation20004Bank1, NULL, NULL, _gActor403100Animation20004Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation2014CBank1[2] = {
#include "assets/actor_403100_animation_2014C_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation2014CBank4[13] = {
#include "assets/actor_403100_animation_2014C_bank4.inc"
};

static AnimationRecord _gActor403100Animation2014CRecords[45] = {
#include "assets/actor_403100_animation_2014C_records.inc"
};

static u16 _gActor403100Animation2014CIndices[16] = {
#include "assets/actor_403100_animation_2014C_indices.inc"
};

static AnimationSet _gActor403100Animation2014C = {
    _gActor403100Animation2014CRecords,
    _gActor403100Animation2014CIndices,
    { NULL, _gActor403100Animation2014CBank1, NULL, NULL, _gActor403100Animation2014CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation20624Bank1[2] = {
#include "assets/actor_403100_animation_20624_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation20624Bank4[121] = {
#include "assets/actor_403100_animation_20624_bank4.inc"
};

static AnimationRecord _gActor403100Animation20624Records[165] = {
#include "assets/actor_403100_animation_20624_records.inc"
};

static u16 _gActor403100Animation20624Indices[16] = {
#include "assets/actor_403100_animation_20624_indices.inc"
};

static AnimationSet _gActor403100Animation20624 = {
    _gActor403100Animation20624Records,
    _gActor403100Animation20624Indices,
    { NULL, _gActor403100Animation20624Bank1, NULL, NULL, _gActor403100Animation20624Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation20AD4Bank1[2] = {
#include "assets/actor_403100_animation_20AD4_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation20AD4Bank4[119] = {
#include "assets/actor_403100_animation_20AD4_bank4.inc"
};

static AnimationRecord _gActor403100Animation20AD4Records[157] = {
#include "assets/actor_403100_animation_20AD4_records.inc"
};

static u16 _gActor403100Animation20AD4Indices[16] = {
#include "assets/actor_403100_animation_20AD4_indices.inc"
};

static AnimationSet _gActor403100Animation20AD4 = {
    _gActor403100Animation20AD4Records,
    _gActor403100Animation20AD4Indices,
    { NULL, _gActor403100Animation20AD4Bank1, NULL, NULL, _gActor403100Animation20AD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation20FA4Bank1[13] = {
#include "assets/actor_403100_animation_20FA4_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation20FA4Bank4[102] = {
#include "assets/actor_403100_animation_20FA4_bank4.inc"
};

static AnimationRecord _gActor403100Animation20FA4Records[149] = {
#include "assets/actor_403100_animation_20FA4_records.inc"
};

static u16 _gActor403100Animation20FA4Indices[16] = {
#include "assets/actor_403100_animation_20FA4_indices.inc"
};

static AnimationSet _gActor403100Animation20FA4 = {
    _gActor403100Animation20FA4Records,
    _gActor403100Animation20FA4Indices,
    { NULL, _gActor403100Animation20FA4Bank1, NULL, NULL, _gActor403100Animation20FA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation2142CBank1[9] = {
#include "assets/actor_403100_animation_2142C_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation2142CBank4[95] = {
#include "assets/actor_403100_animation_2142C_bank4.inc"
};

static AnimationRecord _gActor403100Animation2142CRecords[150] = {
#include "assets/actor_403100_animation_2142C_records.inc"
};

static u16 _gActor403100Animation2142CIndices[16] = {
#include "assets/actor_403100_animation_2142C_indices.inc"
};

static AnimationSet _gActor403100Animation2142C = {
    _gActor403100Animation2142CRecords,
    _gActor403100Animation2142CIndices,
    { NULL, _gActor403100Animation2142CBank1, NULL, NULL, _gActor403100Animation2142CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor403100Animation2192CBank1[7] = {
#include "assets/actor_403100_animation_2192C_bank1.inc"
};

static AnimationPackedRotation _gActor403100Animation2192CBank4[95] = {
#include "assets/actor_403100_animation_2192C_bank4.inc"
};

static AnimationRecord _gActor403100Animation2192CRecords[186] = {
#include "assets/actor_403100_animation_2192C_records.inc"
};

static u16 _gActor403100Animation2192CIndices[16] = {
#include "assets/actor_403100_animation_2192C_indices.inc"
};

static AnimationSet _gActor403100Animation2192C = {
    _gActor403100Animation2192CRecords,
    _gActor403100Animation2192CIndices,
    { NULL, _gActor403100Animation2192CBank1, NULL, NULL, _gActor403100Animation2192CBank4, NULL, NULL, NULL },
};

u_long D_actor_403100_80153774[1950] = {
#include "assets/actor_403100_image_21954.inc"
};

GpuImageUpload D_actor_403100_801555EC[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 384, 256, 30, 130 }, D_actor_403100_80153774 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

TaskDesc D_actor_403100_8015560C[2] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _actor403100Task, { .model = &_gActor403100BurnerBody } },
    { { { TASK_BODY_COORD, 96 } }, _actor403100FloorPuffEmitterTask, { .value = 0 } },
};

TaskDesc D_actor_403100_80155624 = { { { TASK_BODY_COORD, 96 } }, _actor403100DriftPuffEmitterTask, { .value = 0 } };

EffectSpawnArg D_actor_403100_80155630 = { NULL, 1536, 3 };

_Actor403100Zone D_actor_403100_80155638[8] = {
    { -7100, 9200, 3600, 1900, 1 },
    { -7100, 6450, 1900, 2750, 1 },
    { -3500, 9200, 3500, 1900, 5 },
    { -7100, -1000, 1900, 7450, 3 },
    { -7100, -5100, 1900, 4100, 4 },
    { -0x32C8, 900, 4000, 2200, 2 },
    { -9000, 900, 1900, 2200, 6 },
    { 0, 0, 0, 0, ACTOR_403100_ZONE_END },
};

_Actor403100Zone D_actor_403100_80155698[7] = {
    { -2000, 9200, 2000, 1900, 3 },
    { -5500, 9200, 3500, 1900, 4 },
    { -7100, 5900, 3000, 3400, 5 },
    { -7100, 2900, 3000, 3400, 6 },
    { -7100, -100, 3000, 3400, 7 },
    { -7100, -3400, 3000, 3700, 8 },
    { 0, 0, 0, 0, ACTOR_403100_ZONE_END },
};

TaskMessageEntry D_actor_403100_801556EC[4] = {
    { ACTOR_MESSAGE_RELEASE_HOLD, _actor403100RequestPlayerRelease },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor403100ApplyCommand },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor403100SetModelDrawMode },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Borrowed player animation table with a dynamically selected clip in entry four.
static AnimationSet* _gActor403100PlayerAnimationSets[8] = { NULL, &_gActor403100Animation1AF4C, &_gActor403100Animation1BB74, &_gActor403100Animation1BCAC, NULL, &_gActor403100Animation1CDF0, &_gActor403100Animation1C240, &_gActor403100Animation1D8EC };

AnimationSet* D_actor_403100_8015572C[26] = {
    NULL,
    &_gActor403100Animation15BB0,
    &_gActor403100Animation163CC,
    &_gActor403100Animation16A80,
    &_gActor403100Animation17258,
    &_gActor403100Animation17584,
    &_gActor403100Animation17690,
    &_gActor403100Animation180D8,
    &_gActor403100Animation18A88,
    &_gActor403100Animation197D8,
    &_gActor403100Animation19C90,
    &_gActor403100Animation19F88,
    &_gActor403100Animation1A138,
    &_gActor403100Animation1A318,
    &_gActor403100Animation1A7C0,
    &_gActor403100Animation1A9CC,
    &_gActor403100Animation1E600,
    &_gActor403100Animation20004,
    &_gActor403100Animation2014C,
    &_gActor403100Animation1FA78,
    &_gActor403100Animation20624,
    &_gActor403100Animation20AD4,
    &_gActor403100Animation20FA4,
    &_gActor403100Animation2142C,
    &_gActor403100Animation2192C,
    NULL,
};

s16 D_actor_403100_80155794[5][2] = {
    { -1312, 6720 },
    { -3488, 5024 },
    { -3136, 3744 },
    { -1536, 672 },
    { -2016, 1952 },
};

u8 D_actor_403100_801557A8[8] = {
    0,
    1,
    2,
    1,
    2,
    1,
    0,
    0,
};

_Actor403100AttackPickStorage D_actor_403100_801557B0 = {
    {
        { 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 7, 7, 7 },
        { 4, 3, 3, 3, 3, 3, 3, 3, 5, 5, 5, 5, 5, 7, 7, 7 },
    },
    { 0 },
};

SpriteSource D_actor_403100_801557E0[2] = {
    { 142, 0x3FC0, { .fields = { 151, 64 } }, 8, 56, 2250, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 167, 184 } }, -160, -64, 2250, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

Actor403100Work* D_actor_403100_80155808 = NULL;

Enemy* D_actor_403100_8015580C = NULL;

s16 D_actor_403100_80155810 = 0;

_Actor403100Flame D_actor_403100_80155814[28] = { 0 };

/// Overlay-wide work block; `Task::extra` is a `TmdObject` whose `coords` is
/// this actor's `GfxCoord`.
extern Actor403100Work* D_actor_403100_80155808;

extern Enemy* D_actor_403100_8015580C;

extern EffectSpawnArg D_actor_403100_80155630;

extern TaskMessageEntry D_actor_403100_801556EC[4];

extern AnimationSet* D_actor_403100_8015572C[26];

extern EvsCommand D_actor_335800_80165FC0[];

static void _actor403100ApplyAnimationBlend(void);

static void _actor403100BeginFight(Task* task);

static void _actor403100DrawHoldForeground(s16 offsetX, s16 offsetY);

static void _actor403100ResetHoldCycle(Task* task);

static s16 _actor403100HandleHitReaction(void);

static void _actor403100SpawnDefeatFloorPuffs(Task* task, Task* unusedTask);

static void _actor403100EmitFloorPuffs(Task* task);

static void _actor403100WaitForFightStart(Task* task);

static void _actor403100PlayerReactionNone(void);

static void _actor403100HoldPlayerHitPose(void);

static void _actor403100BeginPlayerHitRecovery(void);

static void _actor403100FinishPlayerHitRecovery(void);

static void _actor403100UpdateWalk(Task* task);

static void _actor403100StepPlayerSqueeze(Task* task);

static void _actor403100StepJawPitchKick(Task* task);

static void _actor403100StepHeadPitchKick(void);

static void _actor403100RequestPitchKick(s16 selectAttackSound);

static void _actor403100SaveHoldPose(Task* task);

static void _actor403100RestoreHoldPose(Task* task);

static void _actor403100StepFight(Task* task);

static void _actor403100WaitForPlayerInAttackRange(Task* task);

static void _actor403100ChooseAttack(Task* task);

static void _actor403100WalkApproach(Task* task);

static void _actor403100FinishApproachStride(Task* task);

static void _actor403100FinishStoppingStride(Task* task);

static void _actor403100Destroy(Task* task);

static void _actor403100BeginTimeoutEvent(Task* task);

static void _actor403100StepFightInitialization(Task* task);

static void _actor403100StepAttackApproach(Task* task);

static void _actor403100StepArmSwingCombo(Task* task);

static void _actor403100StepAimedFlameAttack(Task* task);

static void _actor403100StepJumpSlam(Task* task);

static void _actor403100StepGrabAndSqueeze(Task* task);

static void _actor403100StepSideFlameAttack(Task* task);

static void _actor403100StepGrabAndDrag(Task* task);

static void _actor403100StepStagger(Task* unusedTask);

static void _actor403100DispatchLowHealthScene(Task* task);

static void _actor403100StepBuildupStun(Task* task);

static void _actor403100BeginFloorPuffEmission(Task* task);

static void _actor403100WaitAfterFloorPuffEmission(Task* task);

static void _actor403100BeginDriftPuffEmission(Task* task);

static void _actor403100EmitDriftPuffs(Task* task);

static void _actor403100WaitAfterDriftPuffEmission(Task* task);

static void _actor403100BeginSceneEntrance(Task* task);

static void _actor403100WaitAfterSceneEntrance(Task* unusedTask);

static void _actor403100BeginSceneAimAndImpact(Task* task);

static void _actor403100WaitAfterSceneAimAndImpact(Task* task);

static void _actor403100StepSceneAdvance(Task* task);

static void _actor403100WaitAfterSceneAdvance(Task* task);

static void _actor403100WaitAfterSceneFlameBreath(Task* task);

static void _actor403100BeginSceneBalconyImpact(Task* task);

static void _actor403100WaitAfterSceneBalconyImpact(Task* task);

static void _actor403100BlendSceneWalk(Task* task);

static void _actor403100WaitAfterSceneTurn(Task* task);

static void _actor403100BeginSceneFlameRetreat(Task* task);

static void _actor403100BeginSceneDeparture(Task* task);

static void _actor403100FinishSceneDeparture(Task* task);

static void _actor403100BeginAttackApproach(Task* task);

static void _actor403100FinishArmSwingRecovery(Task* unusedTask);

static void _actor403100WaitAfterAimedFlameAttack(Task* unusedTask);

static void _actor403100BeginAimedFlameContactRecovery(Task* unusedTask);

static void _actor403100PrepareJumpSlam(Task* unusedTask);

static void _actor403100BeginJumpSlam(Task* unusedTask);

static void _actor403100WaitAfterJumpSlam(Task* unusedTask);

static void _actor403100BeginHeldPlayerLift(Task* task);

static void _actor403100FinishGrabRewind(Task* unusedTask);

static void _actor403100WaitAfterSideFlameAttack(Task* unusedTask);

static void _actor403100BeginSideFlameContactRecovery(Task* unusedTask);

static void _actor403100BeginGrabAndDrag(Task* unusedTask);

static void _actor403100BeginBuildupStun(Task* task);

static void _actor403100FinishBuildupStunAnimation(Task* task);

static void _actor403100WaitAfterBuildupStun(Task* task);

static void _actor403100WalkIdleState2(Task* task);

static void _actor403100WalkIdleState3(Task* task);

static void _actor403100WalkStopped(Task* task);

/// Scratch-stack block of the head aim: the rotations the head's local
/// matrix is rebuilt from, and the direction of the aim target.
///
/// The head hangs from the two trunk parts, so the rotation it is to have in
/// the root's frame is multiplied by the inverses of theirs to give the
/// rotation stored in its own coordinate.
typedef struct {
    MATRIX  aim;                        // Rotation the head is to have in the root's frame, built from `Actor403100Work::headAim`
    byte    unknown_20[sizeof(MATRIX)]; // One matrix's worth of bytes that is never accessed; role unproven
    MATRIX  lowerInverse;               // Transpose of the lower trunk part's rotation
    MATRIX  local;                      // Transpose of the middle trunk part's rotation, multiplied up into the head's own rotation
    SVECTOR targetAngles;               // Pitch (`vx`) and yaw (`vy`) of the aim target seen from the head, in the root's frame (4096 a turn, wrapped to -0x800..0x7FF), `vz` 0; the tracking modes' pitch step lowers `vx` by 0x140 in place
} _Actor403100HeadAimScratch;
STATIC_ASSERT_SIZEOF(_Actor403100HeadAimScratch, 0x88);

static __inline__ s32  _actor403100FindBalconySection(s16 x, s16 z);
static __inline__ s32  _actor403100FindPlayerRegion(s16 x, s16 z);
static __inline__ s32  _actor403100LocalizeRotation(const GfxCoord* joint, MATRIX* rotation, const GfxCoord* excludedAncestor);
static __inline__ s16  _actor403100PreviousAnimationAtBoundaryOrJump(void);
static __inline__ s16  _actor403100AnimationAtBoundaryOrJump(void);
static __inline__ s16  _actor403100Slot2AnimationSettled(void);
static void            _actor403100InitCombatBodies(Task* task);
static void            _actor403100PlaceHeldPlayer(Task* task);
static void            _actor403100SamplePlayerTarget(Task* task);
static inline void     _actor403100SetRootYaw(Task* task);
static inline void     _actor403100ScaleRoot(Task* task, s16 scaleQ12);
static void            _actor403100StepDefeat(Task* task);
static __inline__ void _actor403100PlaceSpawned(GfxCoord* coord, s32 spawnPointIndex);
static __inline__ void _actor403100MaskCollisionFlags(WorldCollisionBody* body, s32 keepMask, s32 setBits);
static void            _actor403100StepScene(Task* task);
static void            _actor403100StepSceneEntrance(Task* task);
static void            _actor403100StepSceneAimAndImpact(Task* task);
static void            _actor403100BeginSceneAdvance(Task* task);
static void            _actor403100StepSceneAdvanceImpact(Task* task);
static void            _actor403100BeginSceneFlameBreath(Task* task);
static void            _actor403100StepSceneFlameBreath(Task* task);
static void            _actor403100StepSceneBalconyImpact(Task* task);
static void            _actor403100BeginSceneTurn(Task* task);
static void            _actor403100StepSceneTurn(Task* task);
static void            _actor403100StepSceneFlameRetreatBursts(Task* task);
static void            _actor403100StepSceneFlameRetreatRecovery(Task* task);
static void            _actor403100Initialize(Task* task);
static inline void     _actor403100StepPlayerReaction(void);
static inline void     _actor403100TurnForearmInline(Task* task);
static inline void     _actor403100ApplyJawAndHeadPitch(Task* task);
static void            _actor403100BeginArmSwingCombo(Task* task);
static void            _actor403100StepFirstArmSwing(Task* task);
static void            _actor403100StepRepeatedArmSwing(Task* task);
static void            _actor403100FinishArmSwingCombo(Task* task);
static void            _actor403100BeginAimedFlameAttack(Task* task);
static void            _actor403100StepAimedFlameBreath(Task* task);
static void            _actor403100CrouchForJumpSlam(Task* task);
static void            _actor403100AccelerateJumpSlam(Task* task);
static void            _actor403100DecelerateJumpSlam(Task* task);
static void            _actor403100DescendJumpSlam(Task* task);
static void            _actor403100StepJumpSlamImpact(Task* task);
static void            _actor403100FinishJumpSlamRecovery(Task* task);
static void            _actor403100BeginGrabAndSqueeze(Task* task);
static void            _actor403100StepGrabAndSqueezeReach(Task* task);
static void            _actor403100BeginHeldPlayerSqueeze(Task* task);
static void            _actor403100StepHeldPlayerSqueeze(Task* task);
static void            _actor403100PrepareHeldPlayerFlameBreath(Task* task);
static void            _actor403100StepHeldPlayerFlameBreath(Task* task);
static void            _actor403100RecoverHeldPlayerFromFlameBreath(Task* task);
static void            _actor403100ThrowHeldPlayer(Task* task);
static void            _actor403100FinishHeldPlayerThrow(Task* task);
static void            _actor403100BeginSideFlameAttack(Task* task);
static void            _actor403100StepGrabAndDragReach(Task* task);
static void            _actor403100FinishPlayerDrag(Task* task);
static inline void     _actor403100StepApproachRoot(TmdObject* model, GfxCoord* rootCoord);
static inline s32      _actor403100PointToWorld(const GfxCoord* coord, SVECTOR* point);
static inline void     _actor403100RequestAnimationBlendInline(s16 animationId, s16 rate, s16 blendFrames);
static s32             _actorRenderLocalizeRotationToAncestor(const GfxCoord* joint, MATRIX* rotation, const GfxCoord* excludedAncestor);

static __inline__ void _actor403100CopyRotation(MATRIX* destination, const MATRIX* source);

/// Pre-multiplies a rotation by a parent basis and normalizes the product.
///
/// All arguments are stable, side-effect-free pointers to word-aligned MATRIX
/// storage: parentRotation is read, rotation is writable, normalizedRotation
/// supplies a separate writable temporary. Coefficients have 12 fractional
/// bits. Only the resulting 3x3 is valid; the whole-matrix copy also overwrites
/// translation and alignment bytes. The arguments can be evaluated repeatedly.
/// GTE working registers change.
#define ACTOR_403100_PREMULTIPLY_NORMALIZED_ROTATION(parentRotation, rotation, normalizedRotation) \
    do {                                                                                           \
        gte_SetRotMatrix(parentRotation);                                                          \
        MulRotMatrix(rotation);                                                                    \
        MatrixNormal(rotation, normalizedRotation);                                                \
        *(rotation) = *(normalizedRotation);                                                       \
    } while (0)

/// Finds the first breakable balcony section containing a world-space X/Z point.
///
/// Both far rectangle edges are included. Returns section 3..8, or 0 outside
/// the section table; overlapping edges take the earlier entry.
static __inline__ s32 _actor403100FindBalconySection(s16 x, s16 z)
{
    enum { ACTOR_403100_BALCONY_SECTION_NONE = 0 };
    const _Actor403100Zone* zone;
    for (zone = D_actor_403100_80155698; zone->id != ACTOR_403100_ZONE_END; zone++) {
        if (x >= zone->x && x <= zone->x + zone->width &&
            z >= zone->z && z <= zone->z + zone->depth) {
            return zone->id;
        }
    }
    return ACTOR_403100_BALCONY_SECTION_NONE;
}

/// Finds the first player-region rectangle containing a world X/Z point.
///
/// Returns region 1..6, or 0 outside the table. Both far edges are included;
/// shared edges take the earlier entry. Flame scenery checks use the same regions.
static __inline__ s32 _actor403100FindPlayerRegion(s16 x, s16 z)
{
    enum { ACTOR_403100_PLAYER_REGION_NONE = 0 };
    const _Actor403100Zone* zone;
    for (zone = D_actor_403100_80155638; zone->id != ACTOR_403100_ZONE_END; zone++) {
        if (x >= zone->x && x <= zone->x + zone->width && z >= zone->z && z <= zone->z + zone->depth) {
            return zone->id;
        }
    }
    return ACTOR_403100_PLAYER_REGION_NONE;
}

/// Copies the nine rotation coefficients, retaining translation and alignment bytes.
///
/// Source and destination are separate word-aligned matrices. The source's
/// 3x3 is initialized and the destination is writable; no cache stamp changes.
static __inline__ void _actor403100CopyRotation(MATRIX* destination, const MATRIX* source)
{
    destination->m[0][0] = source->m[0][0];
    destination->m[0][1] = source->m[0][1];
    destination->m[0][2] = source->m[0][2];
    destination->m[1][0] = source->m[1][0];
    destination->m[1][1] = source->m[1][1];
    destination->m[1][2] = source->m[1][2];
    destination->m[2][0] = source->m[2][0];
    destination->m[2][1] = source->m[2][1];
    destination->m[2][2] = source->m[2][2];
}

/// Converts an ancestor-frame rotation into the joint's parent frame.
///
/// Includes the immediate parent and intervening ancestors, excluding
/// `excludedAncestor`, then pre-multiplies `rotation` by their transpose.
/// Parent products are normalized; the final inverse multiply is not.
/// Coefficients have 12 fractional bits. Only rotation and matrix alignment
/// bytes change; translation is retained. Returns 1 after conversion or 0
/// unchanged if the parent is the view or the chain reaches NULL first.
/// The joint needs a non-NULL parent and a live acyclic chain. For success
/// the excluded ancestor must lie above that parent. The output is separate
/// writable word-aligned storage; GTE working registers change.
static __inline__ s32 _actor403100LocalizeRotation(const GfxCoord* joint, MATRIX* rotation, const GfxCoord* excludedAncestor)
{
    MATRIX          parentRotation;
    MATRIX          normalizedRotation;
    MATRIX          inverseParentRotation;
    const GfxCoord* ancestor;

    ancestor = joint->parent;
    if (ancestor == &gGfxViewCoord) {
        return 0;
    }
    parentRotation = ancestor->coord;
    while (1) {
        ancestor = ancestor->parent;
        if (ancestor == NULL) {
            return 0;
        }
        if (ancestor == excludedAncestor) {
            break;
        }
        ACTOR_403100_PREMULTIPLY_NORMALIZED_ROTATION(&ancestor->coord, &parentRotation, &normalizedRotation);
    }
    gte_TransposeMatrix(&parentRotation, &inverseParentRotation);
    gte_SetRotMatrix(&inverseParentRotation);
    MulRotMatrix(rotation);
    return 1;
}

/// Tests slot 1's previous running update for a boundary, jump or held boundary pose.
///
/// Returns 0 or 1. A followed jump qualifies even when playback keeps running;
/// this is the behaviour transition test, rather than a playback-stop test.
static __inline__ s16 _actor403100PreviousAnimationAtBoundaryOrJump(void)
{
    if (D_actor_403100_80155808->previousAnimationFlags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        return 1;
    }
    if ((D_actor_403100_80155808->previousAnimationFlags & ANIMATION_SLOT_FOLLOWED_JUMP) ||
        (D_actor_403100_80155808->previousAnimationFlags & ANIMATION_SLOT_SETTLED)) {
        return 1;
    }
    return 0;
}

/// Tests live slot 1 for a boundary, jump or held boundary pose.
///
/// Returns 0 or 1. Behaviour steps can advance on any of these results,
/// including a jump whose destination continues playback.
static __inline__ s16 _actor403100AnimationAtBoundaryOrJump(void)
{
    if (D_actor_403100_80155808->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        return 1;
    }
    if ((D_actor_403100_80155808->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) ||
        (D_actor_403100_80155808->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
        return 1;
    }
    return 0;
}

/// Tests whether live animation slot 2 holds its boundary pose.
///
/// Returns 0 or 1 from ANIMATION_SLOT_SETTLED; reaching a boundary or following
/// a control jump alone does not qualify.
static __inline__ s16 _actor403100Slot2AnimationSettled(void)
{
    if (D_actor_403100_80155808->rig.slots[2].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        return 1;
    }
    return 0;
}

/// Per-frame hooks run before the behaviour mode, indexed by `playerReactionStage`.
static const _Actor403100PlayerReactionTable D_actor_403100_80131E24 = {
    {
        _actor403100PlayerReactionNone,
        _actor403100HoldPlayerHitPose,
        _actor403100BeginPlayerHitRecovery,
        _actor403100FinishPlayerHitRecovery,
    },
};

/// Emits one head-local flame puff into the lowest free pool slot.
///
/// `offset` and `velocity` use signed halfword game coordinates in head part 3's
/// frame. On emission, `offset` becomes world space; `velocity` stays intact.
/// The input vectors are separate. A full pool changes neither input.
/// Only `disableCollision`'s low 16 bits count:
/// zero enables grid/pair tests, nonzero disables both. The pool owns the linked
/// attack body until the puff update or teardown unlinks it. Requires the live
/// actor model and global work, view hierarchy, attack table and flame pool.
static void _actor403100EmitFlame(Task* task, SVECTOR* offset, const SVECTOR* velocity, s32 disableCollision)
{
    enum { ACTOR_403100_FLAME_RADIUS = 50 };
    SVECTOR                velocityEndpoint;
    GfxMatrix              worldTransform;
    u16                    collisionDisabled;
    GfxMatrix*             identityTransform;
    GfxCoord*              headCoord;
    s32                    flameIndex;
    u32                    spriteRandom;
    WorldCollisionContact* contacts;
    WorldCollisionBody*    collisionBody;
    GfxCoord*              flameCoord;

    flameIndex        = 0;
    collisionDisabled = disableCollision;
    headCoord         = &task->extra.tmd->coords[ACTOR_403100_PART_HEAD];
    // Transform both endpoints so the velocity includes the head's orientation.
    velocityEndpoint.vx = offset->vx + velocity->vx;
    velocityEndpoint.vy = offset->vy + velocity->vy;
    velocityEndpoint.vz = offset->vz + velocity->vz;
    for (; flameIndex < ARRAY_SIZE(D_actor_403100_80155814); flameIndex++) {
        if (D_actor_403100_80155814[flameIndex].active == 0) {
            flameCoord                                     = &D_actor_403100_80155814[flameIndex].coord;
            D_actor_403100_80155814[flameIndex].active     = 1;
            D_actor_403100_80155814[flameIndex].age        = 0;
            spriteRandom                                   = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState                                = spriteRandom;
            D_actor_403100_80155814[flameIndex].spriteStep = (s16)((spriteRandom >> 0x10) & 0xF);
            _actorRenderTransformPointToWorld(headCoord, offset);
            identityTransform = &worldTransform;
            _actorRenderTransformPointToWorld(headCoord, &velocityEndpoint);
            D_actor_403100_80155814[flameIndex].velocity.vx = (s16)(velocityEndpoint.vx - offset->vx);
            D_actor_403100_80155814[flameIndex].velocity.vy = (s16)(velocityEndpoint.vy - offset->vy);
            D_actor_403100_80155814[flameIndex].velocity.vz = (s16)(velocityEndpoint.vz - offset->vz);
            D_actor_403100_80155814[flameIndex].position.vx = (u16)offset->vx;
            D_actor_403100_80155814[flameIndex].position.vy = (u16)offset->vy;
            D_actor_403100_80155814[flameIndex].position.vz = (u16)offset->vz;
            // Each puff owns a world-space coordinate and a linked attack sphere.
            D_actor_403100_80155814[flameIndex].coord.parent          = &gGfxViewCoord;
            worldTransform.rotationWords.m00M01                       = ONE;
            worldTransform.rotationWords.m02M10                       = 0;
            identityTransform->rotationWords.m11M12                   = ONE;
            worldTransform.rotationWords.m20M21                       = 0;
            identityTransform->rotationWords.m22                      = ONE;
            worldTransform.mat.t[0]                                   = (s32)(s16)offset->vx;
            worldTransform.mat.t[1]                                   = (s32)(s16)offset->vy;
            contacts                                                  = D_actor_403100_80155814[flameIndex].contacts;
            worldTransform.mat.t[2]                                   = (s32)(s16)offset->vz;
            D_actor_403100_80155814[flameIndex].coord.coord           = worldTransform.mat;
            D_actor_403100_80155814[flameIndex].body.coord            = flameCoord;
            D_actor_403100_80155814[flameIndex].body.context.contacts = contacts;
            D_actor_403100_80155814[flameIndex].body.pos.vx           = 0;
            D_actor_403100_80155814[flameIndex].body.pos.vy           = 0;
            D_actor_403100_80155814[flameIndex].body.pos.vz           = 0;
            D_actor_403100_80155814[flameIndex].body.key              = damagePackAttackKey(D_actor_403100_80147614, 1);
            D_actor_403100_80155814[flameIndex].body.radius           = ACTOR_403100_FLAME_RADIUS;
            D_actor_403100_80155814[flameIndex].body.flags            = WORLD_COLLISION_BODY_SPHERE;
            collisionBody                                             = &D_actor_403100_80155814[flameIndex].body;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, collisionBody);
            worldCollisionInitContacts(contacts, ARRAY_SIZE(D_actor_403100_80155814[flameIndex].contacts), 0);
            if ((collisionDisabled << 0x10) == 0) {
                collisionBody->flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            } else {
                collisionBody->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            actorRenderComposeCoord(&D_actor_403100_80155814[flameIndex].coord);
            D_actor_403100_80155814[flameIndex].coord.composeStamp = GRAPHICS_COORD_DIRTY;
            D_actor_403100_80155814[flameIndex].coord.coord.t[0]   = (s32)(s16)D_actor_403100_80155814[flameIndex].position.vx;
            D_actor_403100_80155814[flameIndex].coord.coord.t[1]   = (s32)(s16)D_actor_403100_80155814[flameIndex].position.vy;
            D_actor_403100_80155814[flameIndex].coord.coord.t[2]   = (s32)(s16)D_actor_403100_80155814[flameIndex].position.vz;
            break;
        }
    }
}

/// Initializes and links the Burner's two receiving and two arm-contact spheres.
///
/// Requires the live fifteen-part model and zeroed singleton work, before any
/// of these bodies is linked. Head and trunk share hitContacts; hand and forearm
/// each borrow their own one-entry table. All four report enemy-body identity
/// 31. Only the receiving spheres start with body-pair tests enabled.
/// Work and model coordinates must remain live until teardown unlinks the bodies.
static void _actor403100InitCombatBodies(Task* task)
{
    enum { ACTOR_403100_COMBAT_BODY_ID      = 0x1F,
           ACTOR_403100_COMBAT_HAND_PART    = 7,
           ACTOR_403100_COMBAT_FOREARM_PART = 6 };
    // Receiving spheres share one hit table; the adjacent three-entry table is only initialized.
    D_actor_403100_80155808->headBody.coord            = &task->extra.tmd->coords[ACTOR_403100_PART_HEAD];
    D_actor_403100_80155808->headBody.context.contacts = D_actor_403100_80155808->hitContacts;
    D_actor_403100_80155808->headBody.pos.vz           = 0x300;
    D_actor_403100_80155808->headBody.pos.vx           = 0;
    D_actor_403100_80155808->headBody.pos.vy           = 0;
    D_actor_403100_80155808->headBody.key              = (WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_403100_COMBAT_BODY_ID);
    D_actor_403100_80155808->headBody.radius           = 0x400;
    D_actor_403100_80155808->headBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &D_actor_403100_80155808->headBody);
    worldCollisionInitContacts(D_actor_403100_80155808->hitContacts, ARRAY_SIZE(D_actor_403100_80155808->hitContacts), 0);
    D_actor_403100_80155808->headBody.flags            |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    D_actor_403100_80155808->trunkBody.coord            = &task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK];
    D_actor_403100_80155808->trunkBody.context.contacts = D_actor_403100_80155808->hitContacts;
    D_actor_403100_80155808->trunkBody.pos.vx           = 0;
    D_actor_403100_80155808->trunkBody.pos.vy           = 0;
    D_actor_403100_80155808->trunkBody.pos.vz           = 0;
    D_actor_403100_80155808->trunkBody.key              = (WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_403100_COMBAT_BODY_ID);
    D_actor_403100_80155808->trunkBody.radius           = 0x800;
    D_actor_403100_80155808->trunkBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &D_actor_403100_80155808->trunkBody);
    worldCollisionInitContacts(D_actor_403100_80155808->initializedContacts, ARRAY_SIZE(D_actor_403100_80155808->initializedContacts), 0);
    // Arm spheres are linked now, with pair tests disabled until an attack enables them.
    D_actor_403100_80155808->handAttack.key              = (WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_403100_COMBAT_BODY_ID);
    D_actor_403100_80155808->trunkBody.flags            |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    D_actor_403100_80155808->handAttack.coord            = &task->extra.tmd->coords[ACTOR_403100_COMBAT_HAND_PART];
    D_actor_403100_80155808->handAttack.context.contacts = D_actor_403100_80155808->handContacts;
    D_actor_403100_80155808->handAttack.pos.vx           = -0x200;
    D_actor_403100_80155808->handAttack.pos.vy           = 0;
    D_actor_403100_80155808->handAttack.pos.vz           = 0x200;
    D_actor_403100_80155808->handAttack.radius           = 0x3A0;
    D_actor_403100_80155808->handAttack.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &D_actor_403100_80155808->handAttack);
    worldCollisionInitContacts(D_actor_403100_80155808->handContacts, ARRAY_SIZE(D_actor_403100_80155808->handContacts), 0);
    D_actor_403100_80155808->forearmAttack.key              = (WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_403100_COMBAT_BODY_ID);
    D_actor_403100_80155808->handAttack.flags              &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    D_actor_403100_80155808->forearmAttack.coord            = &task->extra.tmd->coords[ACTOR_403100_COMBAT_FOREARM_PART];
    D_actor_403100_80155808->forearmAttack.context.contacts = D_actor_403100_80155808->forearmContacts;
    D_actor_403100_80155808->forearmAttack.pos.vx           = -0x200;
    D_actor_403100_80155808->forearmAttack.pos.vy           = 0;
    D_actor_403100_80155808->forearmAttack.pos.vz           = 0x180;
    D_actor_403100_80155808->forearmAttack.radius           = 0x3A0;
    D_actor_403100_80155808->forearmAttack.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &D_actor_403100_80155808->forearmAttack);
    worldCollisionInitContacts(D_actor_403100_80155808->forearmContacts, ARRAY_SIZE(D_actor_403100_80155808->forearmContacts), 0);
    D_actor_403100_80155808->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}
/// Places the held player at the arm's grip sampled two animation ticks ahead.
///
/// Uses coordinate 7's world rotation and a fixed local grip offset, writing the
/// player's root and stored Euler angles. Both live models and the actor's
/// animation rig are required. Reverse playback undoes the sampled advance and
/// restores the requested rate. Sampling also advances animation bookkeeping and
/// updates the pose helpers and composition caches.
static void _actor403100PlaceHeldPlayer(Task* task)
{
    SVECTOR    heldPosition;
    SVECTOR    heldRotation;
    MATRIX     heldWorldRotation;
    GameActor* playerActor;
    GfxCoord*  gripCoord;
    GfxCoord*  playerCoord;
    s32        originalRate;
    s32        animationId;
    u16        savedRateBits;
    GfxCoord*  actorCoords;

    animationId   = D_actor_403100_80155808->animationId;
    playerCoord   = (*gPlayerActorTasks)->extra.tmd->coords;
    actorCoords   = task->extra.tmd->coords;
    playerActor   = (*gPlayerActorTasks)->work;
    originalRate  = D_actor_403100_80155808->animationRate;
    savedRateBits = (u16)D_actor_403100_80155808->animationRate;
    // Sample two ticks ahead for the held pose, then rewind the actor below.
    D_actor_403100_80155808->animationRate = originalRate * 2;
    _actor403100RequestAnimationBlend(animationId, D_actor_403100_80155808->animationRate, 0);
    _actor403100UpdateAnimation(task);
    _actor403100TurnUpperArm(task);
    _actor403100TurnForearm(task);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    gripCoord                                           = &actorCoords[ACTOR_403100_PART_FOREARM];
    actorCoords[ACTOR_403100_PART_FOREARM].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(gripCoord);
    heldPosition.vx = -0x290;
    heldPosition.vy = 0x1E8;
    heldPosition.vz = 0x220;
    _actorRenderTransformPointToWorld(gripCoord, &heldPosition);
    _actor403100GetWorldRotation(gripCoord, &heldWorldRotation);
    gfxExtractEulerAngles(&heldWorldRotation, &heldRotation);
    playerActor->rotation.vx = heldRotation.vx;
    playerActor->rotation.vy = heldRotation.vy;
    playerActor->rotation.vz = heldRotation.vz;
    RotMatrix(&heldRotation, &playerCoord->coord);
    playerCoord->coord.t[0]   = heldPosition.vx;
    playerCoord->coord.t[1]   = heldPosition.vy;
    playerCoord->coord.t[2]   = heldPosition.vz;
    playerCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(playerCoord);
    // Reverse the sampled advance and restore the pending playback rate.
    D_actor_403100_80155808->animationRate = (-originalRate) << 1;
    _actor403100RequestAnimationBlend(D_actor_403100_80155808->animationId, D_actor_403100_80155808->animationRate, 0);
    _actor403100UpdateAnimation(task);
    D_actor_403100_80155808->animationRate = (s16)savedRateBits;
    _actor403100RequestAnimationBlend(D_actor_403100_80155808->animationId, originalRate, 0);
}
/// Applies the pending clip and rate to every non-root animation slot.
///
/// A repeated clip only changes rates. A different clip captures each slot
/// into its pose buffer and blends to record offset zero, then consumes the
/// blend duration. Rates narrow to signed bytes (16 is one frame per tick).
/// The singleton's loaded sets, fifteen-part rig and pose buffers must stay live.
static void _actor403100ApplyAnimationBlend(void)
{
    s32 slotIndex;

    if (D_actor_403100_80155808->appliedAnimation == D_actor_403100_80155808->animationId) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(D_actor_403100_80155808->rig.slots); slotIndex++) {
            D_actor_403100_80155808->rig.slots[slotIndex].rate = D_actor_403100_80155808->animationRate;
        }
    } else {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(D_actor_403100_80155808->rig.slots); slotIndex++) {
            D_actor_403100_80155808->rig.slots[slotIndex].rate = D_actor_403100_80155808->animationRate;
            animationSeekSlotWithBlend(&D_actor_403100_80155808->rig.anim, slotIndex, D_actor_403100_80155808->animationId, 0, D_actor_403100_80155808->animationBlendFrames);
        }
        D_actor_403100_80155808->animationBlendFrames = 0;
    }
    D_actor_403100_80155808->appliedAnimation = D_actor_403100_80155808->animationId;
}
/// Applies an animation request, then ticks slots 1 through 14.
///
/// Blend and reset requests start the elapsed-frame counter at zero; playing
/// calls increment the unsigned counter with wrap. Every call ticks the slots,
/// even before the first request. The singleton rig and its borrowed sets
/// must remain live. Callers pass either no arguments or an ignored task pointer.
static void _actor403100UpdateAnimation()
{
    s32 slotIndex;
    if (D_actor_403100_80155808->animationRequest == ACTOR_403100_ANIMATION_REQUEST_BLEND) {
        _actor403100ApplyAnimationBlend();
        D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_PLAYING;
        D_actor_403100_80155808->animationFrames  = 0;
    } else if (D_actor_403100_80155808->animationRequest == ACTOR_403100_ANIMATION_REQUEST_RESET) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(D_actor_403100_80155808->rig.slots); slotIndex++) {
            animationResetSlot(&D_actor_403100_80155808->rig.anim, slotIndex, D_actor_403100_80155808->animationId);
            D_actor_403100_80155808->rig.slots[slotIndex].rate = D_actor_403100_80155808->animationRate;
        }
        D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_PLAYING;
        D_actor_403100_80155808->animationFrames  = 0;
        D_actor_403100_80155808->appliedAnimation = D_actor_403100_80155808->animationId;
    } else if (D_actor_403100_80155808->animationRequest == ACTOR_403100_ANIMATION_REQUEST_PLAYING) {
        D_actor_403100_80155808->animationFrames += 1;
    }
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(D_actor_403100_80155808->rig.slots); slotIndex++) {
        animationTickSlot(&D_actor_403100_80155808->rig.anim, slotIndex);
    }
}
/// Adds the shoulder pitch and yaw in the model root's frame to the upper-arm pose.
///
/// The task must own the live fifteen-part Burner model. Angles use 4096 units
/// per turn, applied X then Y. Only the joint's 3x3 rotation changes; its
/// cached transform is refreshed. Reserves and releases one word-aligned
/// `MATRIX` on the initialized scratch stack; callers provide its capacity.
static void _actor403100TurnUpperArm(Task* task)
{
    GfxCoord* root;
    GfxCoord* upperArm;
    MATRIX*   rootRotation;
    MATRIX*   localRotation;

    root                          = task->extra.tmd->coords;
    SCRATCH_STACK_CURSOR(MATRIX) -= 1;
    rootRotation                  = SCRATCH_STACK_CURSOR(MATRIX);
    upperArm                      = &root[5];
    // Apply the shoulder offsets in the model root's frame.
    _actorRenderAccumulateRotation(upperArm, rootRotation, root);
    RotMatrixX(D_actor_403100_80155808->armPitch, rootRotation);
    RotMatrixY(D_actor_403100_80155808->armYaw, rootRotation);
    _actor403100LocalizeRotation(upperArm, rootRotation, root);
    localRotation = &upperArm->coord;
    _actor403100CopyRotation(localRotation, rootRotation);
    upperArm->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(upperArm);
    SCRATCH_STACK_CURSOR(MATRIX) += 1;
}
/// Queues a subtractive arm-shadow quad from accepted packed XY/depth outputs.
///
/// Requires the current primitive arena (one POLY_FT4 free) and ordering table.
/// Arguments are evaluated once; screen words and depth must be scalar values.
/// Captures the current graphics globals, advances the arena and links the packet.
/// Expands to a braced block, used as a standalone statement. Depth wraps at
/// the ordering-table mask; the packet stays live until frame DMA completes.
#define ACTOR_403100_QUEUE_ARM_SHADOW(screen0, screen1, screen2, screen3, depth)                                                                                      \
    {                                                                                                                                                                 \
        POLY_FT4* shadowQuad;                                                                                                                                         \
        shadowQuad     = gGpuPrimCursor;                                                                                                                              \
        gGpuPrimCursor = shadowQuad + 1;                                                                                                                              \
        setPolyFT4(shadowQuad);                                                                                                                                       \
        setSemiTrans(shadowQuad, true);                                                                                                                               \
        GPU_PRIMITIVE_XY_WORD(shadowQuad, 0) = (screen0);                                                                                                             \
        shadowQuad->tpage                    = getTPage(0, GPU_BLEND_SUBTRACT, 512, 0);                                                                               \
        GPU_PRIMITIVE_XY_WORD(shadowQuad, 1) = (screen1);                                                                                                             \
        shadowQuad->clut                     = getClut(48, 266);                                                                                                      \
        GPU_PRIMITIVE_XY_WORD(shadowQuad, 2) = (screen2);                                                                                                             \
        GPU_PRIMITIVE_XY_WORD(shadowQuad, 3) = (screen3);                                                                                                             \
        setUV4(shadowQuad, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);                                                                                           \
        setRGB0(shadowQuad, 0xFF, 0xFF, 0xFF);                                                                                                                        \
        addPrim((&gGpuCurrentOt[((((u32)((depth) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), shadowQuad); \
    }

/// Draws one elongated ground-shadow strip under two arm joints on the balcony.
///
/// Requires the live fifteen-part model, part indices in 0..14 (callers
/// use 6/7 and 7/8), the view transform, and room for one `POLY_FT4` in the
/// current frame arena. Equal indices draw nothing. `halfWidth` and `floorY`
/// use signed-halfword room game coordinates. The strip spans twice the
/// joints' horizontal separation and is clamped to the balcony's inner edges.
/// Its packet remains in the ordering table until the current frame is drawn.
static void _actor403100DrawArmShadow(Task* task, s16 startPartIndex, s16 endPartIndex, s16 halfWidth, s16 floorY)
{
    enum { ACTOR_403100_BALCONY_INNER_X           = -5200,
           ACTOR_403100_BALCONY_CORNER_Z          = 9200,
           ACTOR_403100_SHADOW_TRIG_FRACTION_BITS = 12 };
    MATRIX    startWorldTransform;
    MATRIX    endWorldTransform;
    SVECTOR   startPosition;
    SVECTOR   endPosition;
    SVECTOR   corner0;
    SVECTOR   corner1;
    SVECTOR   corner2;
    SVECTOR   corner3;
    long      screen0;
    long      screen1;
    long      screen2;
    long      screen3;
    long      perspectiveScale;
    long      projectionFlags;
    s16       lastCornerZ;
    s16       segmentYaw;
    GfxCoord* endCoord;
    GfxCoord* startCoord;
    s32       widthCos0;
    s32       widthCos1;
    s32       widthCos2;
    s32       widthCos3;
    s32       halfDeltaX;
    s32       halfDeltaZ;
    s32       depth;
    GfxCoord* actorCoords;

    actorCoords = task->extra.tmd->coords;
    startCoord  = actorCoords + startPartIndex;
    endCoord    = actorCoords + endPartIndex;
    if (startPartIndex != endPartIndex) {
        // Flatten the two joint origins, extending the strip by half its length at each end.
        actorRenderComposeCoord(startCoord);
        actorRenderComposeCoord(endCoord);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &startCoord->workm, &startWorldTransform);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &endCoord->workm, &endWorldTransform);
        startPosition.vy = floorY;
        endPosition.vy   = floorY;
        startPosition.vx = startWorldTransform.t[0];
        startPosition.vz = startWorldTransform.t[2];
        endPosition.vx   = endWorldTransform.t[0];
        endPosition.vz   = endWorldTransform.t[2];
        segmentYaw       = ratan2((s16)endWorldTransform.t[0] - (s16)startWorldTransform.t[0], (s16)endWorldTransform.t[2] - (s16)startWorldTransform.t[2]);
        halfDeltaX       = (startPosition.vx - endPosition.vx) / 2;
        halfDeltaZ       = (startPosition.vz - endPosition.vz) / 2;
        widthCos0        = rcos(segmentYaw) * halfWidth;
        corner0.vy       = floorY;
        corner0.vx       = halfDeltaX + (startPosition.vx - (widthCos0 >> ACTOR_403100_SHADOW_TRIG_FRACTION_BITS));
        corner0.vz       = halfDeltaZ + (startPosition.vz + ((s32)(rsin(segmentYaw) * halfWidth) >> ACTOR_403100_SHADOW_TRIG_FRACTION_BITS));
        widthCos1        = rcos(segmentYaw) * halfWidth;
        corner1.vy       = floorY;
        corner1.vx       = halfDeltaX + (startPosition.vx + (widthCos1 >> ACTOR_403100_SHADOW_TRIG_FRACTION_BITS));
        corner1.vz       = halfDeltaZ + (startPosition.vz - ((s32)(rsin(segmentYaw) * halfWidth) >> ACTOR_403100_SHADOW_TRIG_FRACTION_BITS));
        widthCos2        = rcos(segmentYaw) * halfWidth;
        corner2.vy       = floorY;
        corner2.vx       = (endPosition.vx - (widthCos2 >> ACTOR_403100_SHADOW_TRIG_FRACTION_BITS)) - halfDeltaX;
        corner2.vz       = (endPosition.vz + ((s32)(rsin(segmentYaw) * halfWidth) >> ACTOR_403100_SHADOW_TRIG_FRACTION_BITS)) - halfDeltaZ;
        widthCos3        = rcos(segmentYaw) * halfWidth;
        corner3.vy       = floorY;
        corner3.vx       = (endPosition.vx + (widthCos3 >> ACTOR_403100_SHADOW_TRIG_FRACTION_BITS)) - halfDeltaX;
        lastCornerZ      = (endPosition.vz - ((s32)(rsin(segmentYaw) * halfWidth) >> ACTOR_403100_SHADOW_TRIG_FRACTION_BITS)) - halfDeltaZ;
        corner3.vz       = lastCornerZ;
        // Keep strips wholly beyond one inner edge on the balcony's L-shaped floor.
        if ((corner0.vz < ACTOR_403100_BALCONY_CORNER_Z) && (corner1.vz < ACTOR_403100_BALCONY_CORNER_Z) && (corner2.vz < ACTOR_403100_BALCONY_CORNER_Z) && (lastCornerZ < ACTOR_403100_BALCONY_CORNER_Z)) {
            if (corner0.vx >= (ACTOR_403100_BALCONY_INNER_X + 1)) {
                corner0.vx = ACTOR_403100_BALCONY_INNER_X;
            }
            if (corner1.vx >= (ACTOR_403100_BALCONY_INNER_X + 1)) {
                corner1.vx = ACTOR_403100_BALCONY_INNER_X;
            }
            if (corner2.vx >= (ACTOR_403100_BALCONY_INNER_X + 1)) {
                corner2.vx = ACTOR_403100_BALCONY_INNER_X;
            }
            if (corner3.vx >= (ACTOR_403100_BALCONY_INNER_X + 1)) {
                corner3.vx = ACTOR_403100_BALCONY_INNER_X;
            }
        } else if ((corner0.vx >= (ACTOR_403100_BALCONY_INNER_X + 1)) && (corner1.vx >= (ACTOR_403100_BALCONY_INNER_X + 1)) && (corner2.vx >= (ACTOR_403100_BALCONY_INNER_X + 1)) && (corner3.vx >= (ACTOR_403100_BALCONY_INNER_X + 1))) {
            if (corner0.vz < ACTOR_403100_BALCONY_CORNER_Z) {
                corner0.vz = ACTOR_403100_BALCONY_CORNER_Z;
            }
            if (corner1.vz < ACTOR_403100_BALCONY_CORNER_Z) {
                corner1.vz = ACTOR_403100_BALCONY_CORNER_Z;
            }
            if (corner2.vz < ACTOR_403100_BALCONY_CORNER_Z) {
                corner2.vz = ACTOR_403100_BALCONY_CORNER_Z;
            }
            if (corner3.vz < ACTOR_403100_BALCONY_CORNER_Z) {
                corner3.vz = ACTOR_403100_BALCONY_CORNER_Z;
            }
        }
        // Project the room-space strip and queue a subtractive blob-texture packet.
        gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&gGfxViewCoord);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        depth = RotTransPers4(&corner0, &corner1, &corner2, &corner3, &screen0, &screen1, &screen2, &screen3, &perspectiveScale, &projectionFlags);
        if (projectionFlags >= 0) {
            ACTOR_403100_QUEUE_ARM_SHADOW(screen0, screen1, screen2, screen3, depth);
        }
    }
}

#undef ACTOR_403100_QUEUE_ARM_SHADOW

/// Samples the player's position, aim target and two horizontal damage/range distances.
///
/// Requires the live Burner model/work and, when present, a live player model.
/// No player leaves the previous sample intact. Root translations and each
/// subtraction narrow to signed halfword game coordinates before distance math.
/// The second distance transforms the root-to-player offset as a head-local
/// point, then measures from that world point back to the player; it is retained
/// as the distance argument used by damage calculation, rather than head range.
static void _actor403100SamplePlayerTarget(Task* task)
{
    SVECTOR         sampledOffset;
    const GfxCoord* headCoord;
    const GfxCoord* playerRoot;
    s16             rootDeltaX;
    s16             damageDeltaX;
    s16             rootDeltaZ;
    s16             damageDeltaZ;
    const GfxCoord* actorCoords;

    actorCoords = task->extra.tmd->coords;
    headCoord   = &actorCoords[ACTOR_403100_PART_HEAD];
    if (*gPlayerActorTasks != NULL) {
        playerRoot                                 = (*gPlayerActorTasks)->extra.tmd->coords;
        D_actor_403100_80155808->playerPosition.vx = (u16)playerRoot->coord.t[0];
        D_actor_403100_80155808->playerPosition.vy = (u16)playerRoot->coord.t[1];
        D_actor_403100_80155808->playerPosition.vz = (u16)playerRoot->coord.t[2];
        D_actor_403100_80155808->aimTarget.vx      = (u16)playerRoot->coord.t[0];
        D_actor_403100_80155808->aimTarget.vy      = (u16)playerRoot->coord.t[1];
        D_actor_403100_80155808->aimTarget.vz      = (u16)playerRoot->coord.t[2];
        rootDeltaX                                 = (u16)playerRoot->coord.t[0] - (u16)actorCoords->coord.t[0];
        sampledOffset.vx                           = rootDeltaX;
        sampledOffset.vy                           = (u16)playerRoot->coord.t[1] - (u16)actorCoords->coord.t[1];
        rootDeltaZ                                 = (u16)playerRoot->coord.t[2] - (u16)actorCoords->coord.t[2];
        sampledOffset.vz                           = rootDeltaZ;
        D_actor_403100_80155808->playerDistance    = SquareRoot0((rootDeltaX * rootDeltaX) + (rootDeltaZ * rootDeltaZ));
        // Preserve the separate head-transformed sample consumed by damage calculation.
        _actorRenderTransformPointToWorld(headCoord, &sampledOffset);
        damageDeltaX                         = (u16)playerRoot->coord.t[0] - (u16)sampledOffset.vx;
        sampledOffset.vx                     = damageDeltaX;
        sampledOffset.vy                     = (u16)playerRoot->coord.t[1] - sampledOffset.vy;
        damageDeltaZ                         = (u16)playerRoot->coord.t[2] - (u16)sampledOffset.vz;
        sampledOffset.vz                     = damageDeltaZ;
        D_actor_403100_80155808->hitDistance = SquareRoot0((damageDeltaX * damageDeltaX) + (damageDeltaZ * damageDeltaZ));
    }
}
/// Clears the receiving and arm-attack contact tables and consumes a hit-cooldown tick.
///
/// Requires live singleton work with three writable, LAST-terminated contact
/// tables. Clearing retains each table and its terminator. A positive signed
/// halfword cooldown loses one tick; zero or negative values become zero.
static inline void _actor403100FinishContactUpdate(void)
{
    // Contact ownership stays with the bodies; only this frame's entries are consumed.
    worldCollisionClearContacts(D_actor_403100_80155808->handContacts);
    worldCollisionClearContacts(D_actor_403100_80155808->forearmContacts);
    worldCollisionClearContacts(D_actor_403100_80155808->hitContacts);
    if (D_actor_403100_80155808->hitCooldown > 0) {
        D_actor_403100_80155808->hitCooldown = (s16)((u16)D_actor_403100_80155808->hitCooldown - 1);
        return;
    }
    D_actor_403100_80155808->hitCooldown = 0;
}

/// Applies received attacks and ongoing damage, then consumes the Burner's contact tables.
///
/// Requires live singleton work/enemy and model coordinates through the jaw.
/// Accepted attacks use half the sampled XZ hit distance, with fourfold critical
/// and doubled vulnerable damage. HP subtraction narrows to a halfword before
/// signed clamping; ongoing damage is quartered with the original unsigned shift.
/// A damage latch ends the attack scan, except attributes that clear that latch.
/// Arm-player contacts are latched for the next behaviour update. All three
/// contact tables are cleared and a positive hit cooldown loses one update.
static void _actor403100ProcessCombatContacts(Task* task)
{
    enum {
        ACTOR_403100_HIT_EFFECT_NONE                 = 0,
        ACTOR_403100_HIT_EFFECT_CRITICAL             = 1,
        ACTOR_403100_HIT_EFFECT_VULNERABLE           = 2,
        ACTOR_403100_CRITICAL_BURST_STYLE            = 0,
        ACTOR_403100_VULNERABLE_BURST_STYLE          = 3,
        ACTOR_403100_HIT_REACTION_NONE               = 0,
        ACTOR_403100_HIT_REACTION_FLINCH             = 1,
        ACTOR_403100_HIT_REACTION_STAGGER            = 2,
        ACTOR_403100_HIT_REACTION_BUILDUP_STUN       = 3,
        ACTOR_403100_ATTACK_REACTION_SUPPRESS_FLINCH = 8,
        ACTOR_403100_ATTACK_REACTION_FORCE_STAGGER   = 9,
        ACTOR_403100_HIT_COLOR_FRAMES                = 16,
        ACTOR_403100_STUN_COLOR_FRAMES               = 90,
    };
    s16 baseDamage;
    s16 appliedDamage;
    s32 attackKey;
    s16 hitEffectKind;
    s32 contactIndex;
    u16 hpBits;
    u32 dotDamage;
    s32 dotExpired;

    hitEffectKind                     = ACTOR_403100_HIT_EFFECT_NONE;
    D_actor_403100_80155808->hitTaken = 0;
    contactIndex                      = 0;
    // Preserve the selected hit effect across any attacks that suppress the latch.
    for (; contactIndex < ARRAY_SIZE(D_actor_403100_80155808->hitContacts); contactIndex++) {
        attackKey = D_actor_403100_80155808->hitContacts[contactIndex].key.value;
        if ((attackKey & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_ATTACK) {
            if (D_actor_403100_80155808->hitCooldown == 0) {
                D_actor_403100_80155808->hitTaken    = ACTOR_403100_HIT_TAKEN;
                baseDamage                           = damageComputePlayerAttack(D_actor_403100_80155808->hitContacts[contactIndex].key.value, D_actor_403100_80155808->hitDistance / 2, 0, 0);
                appliedDamage                        = baseDamage;
                D_actor_403100_80155808->hitCooldown = damageGetPlayerAttackHitCooldown(D_actor_403100_80155808->hitContacts[contactIndex].key.value);
                if (damageRollCriticalHit(D_actor_403100_8015580C, D_actor_403100_80155808->hitContacts[contactIndex].key.value, 0) != 0) {
                    appliedDamage = baseDamage * 4;
                    hitEffectKind = ACTOR_403100_HIT_EFFECT_CRITICAL;
                }
                if (D_actor_403100_80155808->vulnerable != 0) {
                    hitEffectKind = ACTOR_403100_HIT_EFFECT_VULNERABLE;
                    appliedDamage = appliedDamage * 2;
                }
                switch (hitEffectKind) {
                    case ACTOR_403100_HIT_EFFECT_CRITICAL:
                        effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[ACTOR_403100_PART_JAW], ACTOR_403100_CRITICAL_BURST_STYLE, 0);
                        break;
                    case ACTOR_403100_HIT_EFFECT_VULNERABLE:
                        effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[ACTOR_403100_PART_JAW], ACTOR_403100_VULNERABLE_BURST_STYLE, 0);
                        break;
                }
                damageAccumulateLifeDrainHp(D_actor_403100_8015580C, D_actor_403100_80155808->hitContacts[contactIndex].key.value, appliedDamage, 0);
                worldTargetAddReadoutAmount(&D_actor_403100_8015580C->node, appliedDamage, 0);
                hpBits                      = (u16)D_actor_403100_8015580C->hp - appliedDamage;
                D_actor_403100_8015580C->hp = hpBits;
                if ((s16)hpBits < 0) {
                    D_actor_403100_8015580C->hp = 0U;
                }
                effectSpawnHit(damageGetPlayerAttackEffectId(D_actor_403100_80155808->hitContacts[contactIndex].key.value), &task->extra.tmd->coords[ACTOR_403100_PART_JAW], 0, &D_actor_403100_80155630);
                D_actor_403100_80155808->hitReaction = ACTOR_403100_HIT_REACTION_FLINCH;
                switch (damageGetPlayerAttackReaction(D_actor_403100_80155808->hitContacts[contactIndex].key.value) & 0xFFFF) {
                    case DAMAGE_PLAYER_REACTION_NONE:
                        break;
                    case DAMAGE_PLAYER_REACTION_STAGGER:
                        damageStartEnemyStagger(D_actor_403100_8015580C);
                        break;
                    case DAMAGE_PLAYER_REACTION_BUILDUP:
                        damageStartEnemyBuildup(D_actor_403100_8015580C, D_actor_403100_80155808->hitContacts[contactIndex].key.value, 0);
                        break;
                    case DAMAGE_PLAYER_REACTION_POISON:
                        damageTryStartEnemyDamageOverTime(D_actor_403100_8015580C, D_actor_403100_80155808->hitContacts[contactIndex].key.value, 0);
                        break;
                    case 4:
                        D_actor_403100_80155808->hitReaction = ACTOR_403100_HIT_REACTION_FLINCH;
                        break;
                    case 5:
                        D_actor_403100_80155808->hitReaction = ACTOR_403100_HIT_REACTION_FLINCH;
                        break;
                    case DAMAGE_PLAYER_REACTION_EXPLOSION:
                        D_actor_403100_80155808->hitReaction = ACTOR_403100_HIT_REACTION_FLINCH;
                        break;
                    case DAMAGE_PLAYER_REACTION_INCENDIARY:
                        D_actor_403100_80155808->hitReaction = ACTOR_403100_HIT_REACTION_FLINCH;
                        break;
                    case ACTOR_403100_ATTACK_REACTION_SUPPRESS_FLINCH:
                        D_actor_403100_80155808->hitReaction = ACTOR_403100_HIT_REACTION_NONE;
                        D_actor_403100_80155808->hitTaken    = 0;
                        break;
                    case ACTOR_403100_ATTACK_REACTION_FORCE_STAGGER:
                        D_actor_403100_80155808->hitReaction = ACTOR_403100_HIT_REACTION_STAGGER;
                        break;
                }
                if (D_actor_403100_80155808->hitReaction != 0) {
                    D_actor_403100_80155808->hitColorFrames = ACTOR_403100_HIT_COLOR_FRAMES;
                    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);
                }
            } else if ((damageGetPlayerAttackEffectId(attackKey)) == EFFECT_HIT_KIND_LIFE_DRAIN_MOTES) {
                effectSpawnHit(EFFECT_HIT_KIND_LIFE_DRAIN_MOTES, &task->extra.tmd->coords[ACTOR_403100_PART_JAW], 0, &D_actor_403100_80155630);
            }
        }
        if (D_actor_403100_80155808->hitTaken != 0)
            break;
    }
    // Enemy reaction flags can override the direct attack response.
    if (D_actor_403100_8015580C->reactionFlags & ENEMY_REACTION_STAGGER) {
        D_actor_403100_8015580C->reactionFlags &= ~ENEMY_REACTION_STAGGER;
        D_actor_403100_80155808->hitReaction    = ACTOR_403100_HIT_REACTION_STAGGER;
    }
    if (D_actor_403100_8015580C->reactionFlags & ENEMY_REACTION_BUILDUP) {
        D_actor_403100_8015580C->reactionFlags &= ~ENEMY_REACTION_BUILDUP;
        D_actor_403100_80155808->hitColorFrames = ACTOR_403100_STUN_COLOR_FRAMES;
        D_actor_403100_80155808->hitReaction    = ACTOR_403100_HIT_REACTION_BUILDUP_STUN;
    }
    if (D_actor_403100_8015580C->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        dotDamage = (u32)damageTickEnemyDamageOverTime(D_actor_403100_8015580C) >> 2;
        if ((s16)dotDamage != 0) {
            D_actor_403100_8015580C->hp = (u16)((u16)D_actor_403100_8015580C->hp - dotDamage);
            worldTargetAddReadoutAmount(&D_actor_403100_8015580C->node, (s16)dotDamage, 0);
            if ((s16)D_actor_403100_8015580C->hp < 0) {
                D_actor_403100_8015580C->hp = 0U;
            }
            D_actor_403100_80155808->hitTaken    = ACTOR_403100_HIT_TAKEN;
            D_actor_403100_80155808->hitReaction = ACTOR_403100_HIT_REACTION_STAGGER;
        }
        dotExpired = damageIsEnemyDamageOverTimeExpired(D_actor_403100_8015580C);
        if (dotExpired != 0) {
            D_actor_403100_8015580C->reactionFlags = (u8)(D_actor_403100_8015580C->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR);
        }
    }
    if (worldCollisionFindContactIndex(D_actor_403100_80155808->handAttack.context.contacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
        for (contactIndex = 0; contactIndex < ARRAY_SIZE(D_actor_403100_80155808->handContacts); contactIndex++) {
            if ((D_actor_403100_80155808->handContacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
                D_actor_403100_80155808->handTouchedPlayer = 1;
            }
        }
    }
    if (worldCollisionFindContactIndex(D_actor_403100_80155808->forearmAttack.context.contacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
        for (contactIndex = 0; contactIndex < ARRAY_SIZE(D_actor_403100_80155808->forearmContacts); contactIndex++) {
            if ((D_actor_403100_80155808->forearmContacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
                D_actor_403100_80155808->forearmTouchedPlayer = 1;
            }
        }
    }
    _actor403100FinishContactUpdate();
}

/// Consumes a damaged frame's hit reaction and reports whether it interrupts the behaviour.
///
/// A flinch requests the jaw/head pitch kick and returns 0. Stagger or buildup
/// stun also fades the breath sound, enters that behaviour at step zero and
/// returns 1. Other reactions are cleared with a 0 result. Without this frame
/// being marked damaged, the reaction is retained. The damage latch stays set.
static s16 _actor403100HandleHitReaction(void)
{
    enum {
        ACTOR_403100_HIT_REACTION_NONE         = 0,
        ACTOR_403100_HIT_REACTION_STAGGER      = 2,
        ACTOR_403100_HIT_REACTION_BUILDUP_STUN = 3,
        ACTOR_403100_BEHAVIOUR_STAGGER         = 8,
        ACTOR_403100_BEHAVIOUR_BUILDUP_STUN    = 10,
        ACTOR_403100_BREATH_STOP_FADE_UPDATES  = 10,
    };
    s16 reaction;
    s8  hitTaken;

    hitTaken = D_actor_403100_80155808->hitTaken;
    if (hitTaken == ACTOR_403100_HIT_TAKEN) {
        reaction = D_actor_403100_80155808->hitReaction;
        // A flinch shares the damage latch's value and keeps the current behaviour.
        if (reaction == hitTaken) {
            D_actor_403100_80155808->hitReaction = ACTOR_403100_HIT_REACTION_NONE;
            _actor403100RequestHitPitchKick();
            return 0;
        }
        if (reaction == ACTOR_403100_HIT_REACTION_STAGGER) {
            sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), ACTOR_403100_BREATH_STOP_FADE_UPDATES);
            _actor403100RequestHitPitchKick();
            D_actor_403100_80155808->hitReaction = ACTOR_403100_HIT_REACTION_NONE;
            D_actor_403100_80155808->state       = ACTOR_403100_BEHAVIOUR_STAGGER;
            D_actor_403100_80155808->subState    = 0;
            return 1;
        }
        if (reaction == ACTOR_403100_HIT_REACTION_BUILDUP_STUN) {
            sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), ACTOR_403100_BREATH_STOP_FADE_UPDATES);
            _actor403100RequestHitPitchKick();
            D_actor_403100_80155808->hitReaction = ACTOR_403100_HIT_REACTION_NONE;
            D_actor_403100_80155808->state       = ACTOR_403100_BEHAVIOUR_BUILDUP_STUN;
            D_actor_403100_80155808->subState    = 0;
            return 1;
        }
        D_actor_403100_80155808->hitReaction = ACTOR_403100_HIT_REACTION_NONE;
        return 0;
    }
    return 0;
}
/// Rebuilds the Burner's root rotation from its signed, wrapped heading.
///
/// Requires the live model and work block. Heading uses 4096 units per turn
/// and is stored back in -2048..2047. Replaces pitch, roll and scale, retains
/// translation and marks the composed transform dirty; scaling follows this call.
static inline void _actor403100SetRootYaw(Task* task)
{
    MATRIX    yawRotation;
    MATRIX*   rootMatrix;
    GfxCoord* rootCoord;

    rootCoord                            = task->extra.tmd->coords;
    D_actor_403100_80155808->rotation.vy = (s32)((u16)D_actor_403100_80155808->rotation.vy << 20) >> 20;
    gfxSetRotIdentity(&yawRotation);
    RotMatrixY(D_actor_403100_80155808->rotation.vy, &yawRotation);
    rootMatrix = &rootCoord->coord;
    _actor403100CopyRotation(rootMatrix, &yawRotation);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Multiplies the Burner's root rotation by a uniform signed Q12 scale.
///
/// `scaleQ12` uses `ONE` (4096) for 1.0. Requires a live model whose root
/// composition is already dirty. Retains translation and compounds any
/// existing scale; callers rebuild the yaw before applying this frame's scale.
static inline void _actor403100ScaleRoot(Task* task, s16 scaleQ12)
{
    VECTOR    axisScale;
    MATRIX    scaleMatrix;
    GfxCoord* rootCoord;

    rootCoord    = task->extra.tmd->coords;
    axisScale.vx = scaleQ12;
    axisScale.vy = axisScale.vx;
    axisScale.vz = axisScale.vx;
    gfxSetRotIdentity(&scaleMatrix);
    ScaleMatrix(&scaleMatrix, &axisScale);
    MulMatrix(&rootCoord->coord, &scaleMatrix);
}

/// Samples model lighting and applies the Burner's colour mode at a composed joint origin.
///
/// Requires a live enemy/model in `task->spawnArg2.pointer` and a current world
/// transform in `coord->workm`. Copies its three world-unit translations into a
/// 16-byte scratch VECTOR before nested lighting queries; neither input pointer
/// is retained. Restores the scratch cursor and leaves the coordinate unchanged.
static inline void _actor403100UpdateColor(Task* task, const GfxCoord* coord)
{
    VECTOR* worldPosition;

    worldPosition                = SCRATCH_STACK_CURSOR(VECTOR) - 1;
    worldPosition->vx            = coord->workm.t[0];
    worldPosition->vy            = coord->workm.t[1];
    worldPosition->vz            = coord->workm.t[2];
    SCRATCH_STACK_CURSOR(VECTOR) = worldPosition;
    worldCoordUpdateActorColor(task->spawnArg2.pointer, worldPosition, 0, 0);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(VECTOR));
}

/// Latches a normal-rate clip restart for the next animation update.
///
/// `animationId` must select a non-NULL loaded Burner clip (1..24) in the live
/// singleton rig. Restarting cuts to that clip's first pose when the animation
/// driver consumes the request; this helper does not tick a pose.
static inline void _actor403100RequestAnimationReset(s16 animationId)
{
    D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
    D_actor_403100_80155808->animationId      = animationId;
    D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
}

/// Advances the Burner's vertical screen shake and its wrapping halfword timer.
///
/// Requires live singleton work and display state. The shared display frame
/// selects an eight-tick sine wave: offsets reach two display units at timer
/// values >= 16, and one unit otherwise. Only a running caller spends a tick;
/// zero clears the display offset. The signed timer retains its low halfword.
static inline void _actor403100StepScreenShake(void)
{
    enum {
        ACTOR_403100_SHAKE_STRONG_SHIFT = 13,
        ACTOR_403100_SHAKE_WEAK_SHIFT   = 12,
        ACTOR_403100_SHAKE_OUTPUT_SHIFT = 24,
    };
    enum { ACTOR_403100_SHAKE_WEAK_THRESHOLD = 16,
           ACTOR_403100_SHAKE_PHASE_SHIFT    = 9 };
    s32 shakeFrames;
    s32 shakeWave;

    shakeFrames = D_actor_403100_80155808->shakeFrames;
    if (shakeFrames != 0) {
        if (shakeFrames >= ACTOR_403100_SHAKE_WEAK_THRESHOLD) {
            shakeWave = rsin(gDisplayState.animFrame << ACTOR_403100_SHAKE_PHASE_SHIFT) << ACTOR_403100_SHAKE_STRONG_SHIFT;
        } else {
            shakeWave = rsin(gDisplayState.animFrame << ACTOR_403100_SHAKE_PHASE_SHIFT) << ACTOR_403100_SHAKE_WEAK_SHIFT;
        }
        displaySetShakeY(shakeWave >> ACTOR_403100_SHAKE_OUTPUT_SHIFT);
        D_actor_403100_80155808->shakeFrames = (u16)D_actor_403100_80155808->shakeFrames - 1;
    } else {
        displaySetShakeY(0);
    }
}

/// Runs the five defeat stages and refreshes the Burner's scaled pose and shake.
///
/// Requires live singleton work, enemy and model with coordinates 0..8.
/// `state` must be 0..4; dispatch is unchecked. Defeat scale is Q12.
/// Animation advances here; the enclosing task draws the resulting model.
static void _actor403100StepDefeat(Task* task)
{
    TaskFunc handlers[] = {
        _actor403100BeginDefeat,
        _actor403100StepDefeatRise,
        _actor403100StepDefeatBurn,
        _actor403100BeginDefeatRewardWait,
        _actor403100StepDefeatRewardWait
    };
    GfxCoord* coords;
    GfxCoord* jawCoord;
    GfxCoord* headCoord;

    // The defeat callback advances the pose before root and joint composition.
    handlers[(s16)D_actor_403100_80155808->state](task);
    _actor403100UpdateAnimation(task);
    _actor403100SetRootYaw(task);
    _actor403100ScaleRoot(task, D_actor_403100_80155808->defeatScale);
    _actor403100TurnUpperArm(task);
    coords                 = task->extra.tmd->coords;
    coords[8].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[7].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[6].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    jawCoord               = &coords[4];
    headCoord              = &coords[3];
    actorRenderComposeCoord(&coords[8]);
    actorRenderComposeCoord(jawCoord);
    _actor403100UpdateColor(task, headCoord);
    _actor403100StepScreenShake();
}
/// Releases every live puff in the Burner's 28-entry flame pool.
///
/// Requires an initialized pool: every active slot owns a linked attack body.
/// Clearing the active latch before unlinking returns the slot to the emitter;
/// inactive slots are left alone, so repeated teardown is harmless. The package
/// retains the pool storage. Callers manage emission timing and breath audio.
static inline void _actor403100ReleaseFlames(void)
{
    s32 flameIndex;

    for (flameIndex = 0; flameIndex < ARRAY_SIZE(D_actor_403100_80155814); flameIndex++) {
        if (D_actor_403100_80155814[flameIndex].active != 0) {
            D_actor_403100_80155814[flameIndex].active = 0;
            worldCollisionUnlinkBody(&D_actor_403100_80155814[flameIndex].body);
        }
    }
}

/// Withdraws the Burner from combat and starts its defeat animation.
///
/// Requires live singleton work, a linked enemy target and an initialized flame
/// pool. Stops breath audio at both existing boundaries, releases every live
/// flame, blends to clip 17 over 32 frames and resets arm offsets and the timer.
/// The unused task parameter supplies the defeat-state callback signature.
static void _actor403100BeginDefeat(Task* task)
{
    enum { ACTOR_403100_DEFEAT_ANIMATION          = 17,
           ACTOR_403100_DEFEAT_BLEND_FRAMES       = 32,
           ACTOR_403100_DEFEAT_SOUND_FADE_UPDATES = 10 };
    // Withdraw combat targeting and flames before starting the defeat pose.
    sndEvtRequestScriptStop(ACTOR_403100_BREATH_SOUND, ACTOR_403100_DEFEAT_SOUND_FADE_UPDATES);
    worldTargetUnlinkNode(&D_actor_403100_8015580C->node);
    D_actor_403100_80155810 = 0;
    _actor403100ReleaseFlames();
    sndEvtRequestScriptStop(ACTOR_403100_BREATH_SOUND, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    D_actor_403100_80155808->animationBlendFrames = ACTOR_403100_DEFEAT_BLEND_FRAMES;
    D_actor_403100_80155808->animationRate        = ANIMATION_RATE_ONE;
    D_actor_403100_80155808->animationId          = ACTOR_403100_DEFEAT_ANIMATION;
    D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
    D_actor_403100_80155808->defeatScale          = ONE * 5 / 4;
    D_actor_403100_80155808->stateFrames          = 0;
    D_actor_403100_80155808->armPitch             = 0;
    D_actor_403100_80155808->armYaw               = 0;
    D_actor_403100_80155808->state               += 1;
}
/// Raises the defeated Burner, emits floor puffs and places its final burning pose.
///
/// Requires live singleton work/model and a zero-based wrapping halfword timer.
/// After incrementing, tick 130 enables puff bursts and tick 160 starts +Y motion
/// of ten root-parent game units per call. Tick 300 restarts clip 12, changes
/// placement and Q12 scale, clears the timer and selects defeat stage 2.
static void _actor403100StepDefeatRise(Task* task)
{
    enum {
        ACTOR_403100_DEFEAT_PUFF_START_TICK = 130,
        ACTOR_403100_DEFEAT_RISE_TICK       = 160,
        ACTOR_403100_DEFEAT_REPOSITION_TICK = 300,
    };
    GfxCoord* rootCoord;
    u16       nextFrame;

    rootCoord                            = task->extra.tmd->coords;
    nextFrame                            = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = nextFrame;
    if ((s16)nextFrame >= ACTOR_403100_DEFEAT_RISE_TICK) {
        rootCoord->coord.t[1] += 0xA;
    }
    if ((s16)D_actor_403100_80155808->stateFrames >= ACTOR_403100_DEFEAT_PUFF_START_TICK) {
        _actor403100SpawnDefeatFloorPuffs(task, task);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_DEFEAT_REPOSITION_TICK) {
        D_actor_403100_80155808->defeatScale = ONE * 11 / 8;
        rootCoord->coord.t[0]                = -0xA28;
        _actor403100RequestAnimationReset(ACTOR_403100_ANIMATION_SQUEEZE);
        rootCoord->coord.t[1]                = 0x1390;
        rootCoord->coord.t[2]                = 0x1130;
        D_actor_403100_80155808->rotation.vy = -0x6B0;
        D_actor_403100_80155808->armPitch    = -0x140;
        D_actor_403100_80155808->armYaw      = -0x100;
        D_actor_403100_80155808->stateFrames = 0;
        D_actor_403100_80155808->state       = D_actor_403100_80155808->state + 1;
    }
}

/// Places a spawned effect coordinate at one of the five floor emission points.
///
/// `spawnPointIndex` is 0..4. X/Z are parent-frame game coordinates and Y is
/// zero. The caller owns the live coordinate and updates its cache stamp.
static __inline__ void _actor403100PlaceSpawned(GfxCoord* coord, s32 spawnPointIndex)
{
    coord->coord.t[0] = D_actor_403100_80155794[spawnPointIndex][0];
    coord->coord.t[1] = 0;
    coord->coord.t[2] = D_actor_403100_80155794[spawnPointIndex][1];
}

/// Spawns and places one owned floor-puff task, skipping placement on failure.
///
/// Requires the coordinate-only descriptor and a point index in 0..4. Task
/// and coordinate arguments are stable local pointer lvalues, evaluated more
/// than once; pointIndex is evaluated once on success. Captures the package's
/// descriptor table and placement helper. Expands to a standalone braced block.
#define ACTOR_403100_SPAWN_FLOOR_PUFF(spawnedTask, puffCoord, pointIndex)                                      \
    {                                                                                                          \
        (spawnedTask) = taskSpawnFromTable(D_actor_403100_8015560C, ACTOR_403100_FLOOR_PUFF_DESCRIPTOR, 0, 0); \
        if ((spawnedTask) != NULL) {                                                                           \
            (puffCoord) = (spawnedTask)->extra.coordBody->coord;                                               \
            _actor403100PlaceSpawned((puffCoord), (pointIndex));                                               \
        }                                                                                                      \
    }

/// Moves the defeat pose upward and stages floor puffs and corpse-fire bursts.
///
/// Requires live singleton work/model and the coordinate-only puff descriptor.
/// The previous halfword timer selects puffs and fire at ticks 2, 30 and 60;
/// its increment to 190 sets the defeated flag and selects reward setup.
/// Each spawned task owns its coordinate body; failures skip that effect.
/// Fire offsets are in that coordinate's frame and are copied during spawning.
static void _actor403100StepDefeatBurn(Task* task)
{
    enum {
        ACTOR_403100_DEFEAT_PUFF_LONG_MASK    = 127,
        ACTOR_403100_DEFEAT_PUFF_SHORT_MASK   = 63,
        ACTOR_403100_DEFEAT_BURN_FIRST_WAVES  = 3,
        ACTOR_403100_DEFEAT_BURN_SECOND_WAVES = 4,
        ACTOR_403100_DEFEAT_BURN_THIRD_WAVES  = 5,
        ACTOR_403100_DEFEAT_BURN_END_TICK     = 190,
        ACTOR_403100_DEFEAT_PUFF_POINT_0_TICK = 40,
        ACTOR_403100_DEFEAT_PUFF_POINT_1_TICK = 32,
        ACTOR_403100_DEFEAT_PUFF_POINT_2_TICK = 8,
        ACTOR_403100_DEFEAT_PUFF_POINT_4_TICK = 64,
        ACTOR_403100_DEFEAT_BURN_FIRST_TICK   = 2,
        ACTOR_403100_DEFEAT_BURN_SECOND_TICK  = 30,
        ACTOR_403100_DEFEAT_BURN_THIRD_TICK   = 60,
    };
    SVECTOR   burnOffset;
    Task*     spawnedPuff;
    GfxCoord* puffCoord;
    s16       nextFrame;
    GfxCoord* rootCoord;

    rootCoord                          = task->extra.tmd->coords;
    D_actor_403100_80155808->armPitch -= 2;
    rootCoord->composeStamp            = GRAPHICS_COORD_DIRTY;
    rootCoord->coord.t[1]             += 0xC;
    if ((D_actor_403100_80155808->stateFrames & ACTOR_403100_DEFEAT_PUFF_LONG_MASK) == ACTOR_403100_DEFEAT_PUFF_POINT_0_TICK) {
        ACTOR_403100_SPAWN_FLOOR_PUFF(spawnedPuff, puffCoord, 0);
    }
    if ((D_actor_403100_80155808->stateFrames & ACTOR_403100_DEFEAT_PUFF_SHORT_MASK) == ACTOR_403100_DEFEAT_PUFF_POINT_1_TICK) {
        ACTOR_403100_SPAWN_FLOOR_PUFF(spawnedPuff, puffCoord, 1);
    }
    if ((D_actor_403100_80155808->stateFrames & ACTOR_403100_DEFEAT_PUFF_LONG_MASK) == ACTOR_403100_DEFEAT_PUFF_POINT_2_TICK) {
        ACTOR_403100_SPAWN_FLOOR_PUFF(spawnedPuff, puffCoord, 2);
    }
    // These three bursts use offsets relative to the newly placed puff coordinate.
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_DEFEAT_BURN_FIRST_TICK) {
        spawnedPuff = taskSpawnFromTable(D_actor_403100_8015560C, ACTOR_403100_FLOOR_PUFF_DESCRIPTOR, 0, 0);
        if (spawnedPuff != NULL) {
            puffCoord = spawnedPuff->extra.coordBody->coord;
            _actor403100PlaceSpawned(puffCoord, 1);
            burnOffset.vx           = 0x578;
            burnOffset.vy           = -0xFA0;
            burnOffset.vz           = -0xAF0;
            puffCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(puffCoord);
            effectSpawn(EFFECT_CORPSE_BURN, puffCoord, ACTOR_403100_DEFEAT_BURN_FIRST_WAVES, &burnOffset);
        }
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_DEFEAT_BURN_SECOND_TICK) {
        spawnedPuff = taskSpawnFromTable(D_actor_403100_8015560C, ACTOR_403100_FLOOR_PUFF_DESCRIPTOR, 0, 0);
        if (spawnedPuff != NULL) {
            puffCoord = spawnedPuff->extra.coordBody->coord;
            _actor403100PlaceSpawned(puffCoord, 1);
            burnOffset.vx           = 0x3E8;
            burnOffset.vy           = -0xFA0;
            burnOffset.vz           = -0x1130;
            puffCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(puffCoord);
            effectSpawn(EFFECT_CORPSE_BURN, puffCoord, ACTOR_403100_DEFEAT_BURN_SECOND_WAVES, &burnOffset);
        }
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_DEFEAT_BURN_THIRD_TICK) {
        spawnedPuff = taskSpawnFromTable(D_actor_403100_8015560C, ACTOR_403100_FLOOR_PUFF_DESCRIPTOR, 0, 0);
        if (spawnedPuff != NULL) {
            puffCoord = spawnedPuff->extra.coordBody->coord;
            _actor403100PlaceSpawned(puffCoord, 1);
            burnOffset.vx           = 0;
            burnOffset.vy           = -0xFA0;
            burnOffset.vz           = -0xFA0;
            puffCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(puffCoord);
            effectSpawn(EFFECT_CORPSE_BURN, puffCoord, ACTOR_403100_DEFEAT_BURN_THIRD_WAVES, &burnOffset);
        }
    }
    if ((D_actor_403100_80155808->stateFrames & ACTOR_403100_DEFEAT_PUFF_LONG_MASK) == ACTOR_403100_DEFEAT_PUFF_POINT_4_TICK) {
        ACTOR_403100_SPAWN_FLOOR_PUFF(spawnedPuff, puffCoord, 4);
    }
    nextFrame                            = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = nextFrame;
    if (nextFrame == ACTOR_403100_DEFEAT_BURN_END_TICK) {
        gameFlagSetNibble(GAME_FLAG_BURNER_DEFEATED, true);
        D_actor_403100_80155808->state++;
    }
}
/// Breaks the balcony sections beneath the Burner's claw and hand sample points.
///
/// Requires live model coordinates through 8 and singleton section latches.
/// Samples local claw offset (352, 328, 704) and the hand origin in world X/Z;
/// sections 3..8 receive the first-hit or repeat-hit debris effect. Only the
/// first hit changes and persists a section's room-sprite state.
static void _actor403100BreakBalconyAtArm(Task* task)
{
    enum {
        ACTOR_403100_FIRST_ARM_BREAK_SECTION  = 3,
        ACTOR_403100_ARM_CLAW_COORD           = 8,
        ACTOR_403100_ARM_HAND_COORD           = 7,
        ACTOR_403100_BALCONY_BREAK_FIRST_HIT  = 0,
        ACTOR_403100_BALCONY_BREAK_REPEAT_HIT = 1,
    };
    /// Breaks the section under one arm sample, then advances the section cursor.
    ///
    /// Requires a signed cursor starting at 3, live singleton section latches,
    /// stable coordinate/vector lvalues and world-space X/Z. Arguments are
    /// evaluated repeatedly; the cursor is modified. Captures the two local
    /// ACTOR_403100_BALCONY_BREAK_* effect selectors and the singleton work.
    /// The spawned break handler uses its owned coordinate, so stack offsets
    /// are consumed synchronously. Expands to a standalone for statement.
#define ACTOR_403100_BREAK_BALCONY_AT_POINT(limbCoord, localOffset, worldPoint, sectionIndex)                                            \
    for (; (sectionIndex) < (s32)ARRAY_SIZE(D_actor_403100_80155808->sectionDamaged); (sectionIndex)++) {                                \
        if (_actor403100FindBalconySection((worldPoint).vx, (worldPoint).vz) == (sectionIndex)) {                                        \
            if (D_actor_403100_80155808->sectionDamaged[(sectionIndex)] == 0) {                                                          \
                effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_BREAK, (limbCoord), ACTOR_403100_BALCONY_BREAK_FIRST_HIT, &(localOffset));  \
                dryfieldNightMotelBalconySetSectionState((s16)(sectionIndex), ACTOR_403100_BALCONY_SECTION_DAMAGED);                     \
                D_actor_403100_80155808->sectionDamaged[(sectionIndex)] = ACTOR_403100_BALCONY_SECTION_DAMAGED;                          \
            } else {                                                                                                                     \
                effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_BREAK, (limbCoord), ACTOR_403100_BALCONY_BREAK_REPEAT_HIT, &(localOffset)); \
            }                                                                                                                            \
        }                                                                                                                                \
    }

    SVECTOR   clawWorldPoint, handWorldPoint, clawOffset, handOffset;
    GfxCoord* actorCoords;
    GfxCoord* clawCoord;
    GfxCoord* handCoord;
    s32       sectionIndex;

    actorCoords                = task->extra.tmd->coords;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    clawCoord                                             = &actorCoords[ACTOR_403100_ARM_CLAW_COORD];
    actorCoords[ACTOR_403100_ARM_CLAW_COORD].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(clawCoord);
    clawWorldPoint.vx = clawOffset.vx = 0x160;
    clawWorldPoint.vy = clawOffset.vy = 0x148;
    sectionIndex                      = ACTOR_403100_FIRST_ARM_BREAK_SECTION;
    clawWorldPoint.vz = clawOffset.vz = 0x2C0;
    _actorRenderTransformPointToWorld(clawCoord, &clawWorldPoint);
    handCoord         = &actorCoords[ACTOR_403100_ARM_HAND_COORD];
    handWorldPoint.vx = handOffset.vx = 0;
    handWorldPoint.vy = handOffset.vy = 0;
    handWorldPoint.vz = handOffset.vz = 0;
    _actorRenderTransformPointToWorld(handCoord, &handWorldPoint);
    ACTOR_403100_BREAK_BALCONY_AT_POINT(clawCoord, clawOffset, clawWorldPoint, sectionIndex);
    sectionIndex = ACTOR_403100_FIRST_ARM_BREAK_SECTION;
    ACTOR_403100_BREAK_BALCONY_AT_POINT(handCoord, handOffset, handWorldPoint, sectionIndex);
}
#undef ACTOR_403100_BREAK_BALCONY_AT_POINT
/// Spawns defeat floor-puff emitters at all five points on 64-tick boundaries.
///
/// Requires live singleton work, a Burner model and its coordinate-only puff
/// entry. Each successful task owns its body; allocation failures skip that
/// point. The timer is re-read before each spawn. `unusedTask` is ignored.
static void _actor403100SpawnDefeatFloorPuffs(Task* task, Task* unusedTask)
{
    enum {
        ACTOR_403100_DEFEAT_PUFF_BURST_MASK = 63,
    };
    Task*     spawnedPuff;
    GfxCoord* puffCoord;

    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (D_actor_403100_80155808->stateFrames & ACTOR_403100_DEFEAT_PUFF_BURST_MASK) {
        return;
    }
    ACTOR_403100_SPAWN_FLOOR_PUFF(spawnedPuff, puffCoord, 0);
    if (D_actor_403100_80155808->stateFrames & ACTOR_403100_DEFEAT_PUFF_BURST_MASK) {
        return;
    }
    ACTOR_403100_SPAWN_FLOOR_PUFF(spawnedPuff, puffCoord, 1);
    if (D_actor_403100_80155808->stateFrames & ACTOR_403100_DEFEAT_PUFF_BURST_MASK) {
        return;
    }
    ACTOR_403100_SPAWN_FLOOR_PUFF(spawnedPuff, puffCoord, 2);
    if (D_actor_403100_80155808->stateFrames & ACTOR_403100_DEFEAT_PUFF_BURST_MASK) {
        return;
    }
    ACTOR_403100_SPAWN_FLOOR_PUFF(spawnedPuff, puffCoord, 3);
    if (D_actor_403100_80155808->stateFrames & ACTOR_403100_DEFEAT_PUFF_BURST_MASK) {
        return;
    }
    ACTOR_403100_SPAWN_FLOOR_PUFF(spawnedPuff, puffCoord, 4);
}
#undef ACTOR_403100_SPAWN_FLOOR_PUFF

/// Retains selected collision-body flags and sets additional bits.
///
/// Requires a writable body. Computes (flags & keepMask) | setBits and narrows
/// the store to the 16-bit flags field. Callers preserve shape bits while
/// enabling or disabling grid and body-pair passes independently.
static __inline__ void _actor403100MaskCollisionFlags(WorldCollisionBody* body, s32 keepMask, s32 setBits)
{
    body->flags = (body->flags & keepMask) | setBits;
}

/// Draws the live flame prefix and advances its contacts, movement and lifetime.
///
/// `sizeBias` adds game-coordinate units to each puff's age-dependent drawn size;
/// callers use 150. Requires the singleton work, initialized flame pool, composed
/// view and loaded balcony renderer. `flameLifetime` is initially 0, then 20
/// or 28 after emission setup, bounded by the 28-slot pool. It selects both the
/// walked prefix and release age; live puffs must lie in that prefix. `unusedTask`
/// is ignored. Screen positions are packed signed pixels; projected scale is Q12.
/// Running updates consume contacts, move or lift puffs, resize their attack
/// spheres and unlink expired bodies. Paused updates draw and disable collision;
/// the shared hit cooldown still decrements. Entering player region 2 damages
/// balcony section 2 independently of the control state.
static void _actor403100UpdateFlames(Task* unusedTask, s32 sizeBias)
{
    enum {
        ACTOR_403100_FLAME_BREAK_SECTION      = 2,
        ACTOR_403100_FLAME_FAST_GROWTH_STATE  = 5,
        ACTOR_403100_FLAME_INITIAL_SIZE       = 144,
        ACTOR_403100_FLAME_NORMAL_GROWTH      = 60,
        ACTOR_403100_FLAME_FAST_GROWTH        = 240,
        ACTOR_403100_FLAME_HIT_COOLDOWN_TICKS = 10,
    };
    SVECTOR             worldPoint;
    WorldCollisionDelta unusedDelta;
    s32                 packedScreen;
    s32                 gteFlags;
    s32                 projectedDepth;
    s16                 flameSize;
    s32                 ageGrowth;
    s32                 baseSize = sizeBias;
    s32                 pushbackResult;
    s32                 flameIndex;
    s32                 contactIndex;
    _Actor403100Flame*  flame;

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    for (flameIndex = 0; flameIndex < D_actor_403100_80155808->flameLifetime; flameIndex++) {
        flame = &D_actor_403100_80155814[flameIndex];
        if (flame->active == 0) {
            continue;
        }
        if (D_actor_403100_80155808->sectionDamaged[ACTOR_403100_FLAME_BREAK_SECTION] == 0 &&
            _actor403100FindPlayerRegion(flame->position.vx, flame->position.vz) == ACTOR_403100_PLAYER_REGION_2) {
            dryfieldNightMotelBalconySetSectionState(ACTOR_403100_FLAME_BREAK_SECTION, ACTOR_403100_BALCONY_SECTION_DAMAGED);
            D_actor_403100_80155808->sectionDamaged[ACTOR_403100_FLAME_BREAK_SECTION] = 1;
        }
        worldPoint.vx = flame->position.vx;
        worldPoint.vy = flame->position.vy;
        worldPoint.vz = flame->position.vz;
        gte_ldv0(&worldPoint);
        gte_rtps();
        gte_stsxy(&packedScreen);
        gte_stflg(&gteFlags);
        gte_stszotz(&projectedDepth);
        if ((s16)D_actor_403100_80155808->state == ACTOR_403100_FLAME_FAST_GROWTH_STATE) {
            ageGrowth = flame->age * ACTOR_403100_FLAME_FAST_GROWTH + ACTOR_403100_FLAME_INITIAL_SIZE;
            flameSize = baseSize + ageGrowth;
        } else {
            ageGrowth = flame->age * ACTOR_403100_FLAME_NORMAL_GROWTH + ACTOR_403100_FLAME_INITIAL_SIZE;
            flameSize = baseSize + ageGrowth;
        }
        if (gteFlags >= 0) {
            dryfieldNightMotelBalconyDrawBreathFlame(packedScreen, (projectedDepth << 0xC) >> 0x10, (s16)((flameSize << 0x10 >> 1) / (projectedDepth * 4)), flame->spriteStep);
        } else {
            _actor403100MaskCollisionFlags(&D_actor_403100_80155814[flameIndex].body, (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED), WORLD_COLLISION_BODY_GRID_ENABLED);
        }
        // Drawing remains active while control is paused; motion and contacts advance only while running.
        if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
            if (worldCollisionFindContactIndex(flame->body.context.contacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
                for (contactIndex = 0; contactIndex < ARRAY_SIZE(flame->contacts); contactIndex++) {
                    if ((D_actor_403100_80155814[flameIndex].contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY && D_actor_403100_80155810 == 0) {
                        D_actor_403100_80155810 = ACTOR_403100_FLAME_HIT_COOLDOWN_TICKS;
                    }
                }
            }
            pushbackResult = worldCollisionResolvePushback(D_actor_403100_80155814[flameIndex].contacts, &unusedDelta, ARRAY_SIZE(flame->contacts), 0);
            if (pushbackResult == WORLD_COLLISION_PUSHBACK_NO_GRID_HIT) {
                flame->position.vx += flame->velocity.vx;
                flame->position.vy += flame->velocity.vy;
                flame->position.vz += flame->velocity.vz;
            } else if (pushbackResult >= 0) {
                if (pushbackResult < WORLD_COLLISION_PUSHBACK_OPPOSED + 1) {
                    D_actor_403100_80155814[flameIndex].position.vy -= 0x100 + D_actor_403100_80155814[flameIndex].velocity.vy;
                }
            }
            actorRenderComposeCoord(&D_actor_403100_80155814[flameIndex].coord);
            flame->coord.coord.t[0]   = flame->position.vx;
            flame->coord.composeStamp = GRAPHICS_COORD_DIRTY;
            flame->coord.coord.t[1]   = flame->position.vy;
            flame->coord.coord.t[2]   = flame->position.vz;
            flame->body.radius        = flameSize / 3;
            if (D_actor_403100_80155810 == 0) {
                _actor403100MaskCollisionFlags(&D_actor_403100_80155814[flameIndex].body, WORLD_COLLISION_BODY_FLAGS_MASK, WORLD_COLLISION_BODY_PAIR_ENABLED);
            } else {
                _actor403100MaskCollisionFlags(&D_actor_403100_80155814[flameIndex].body, (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED), 0);
            }
            _actor403100MaskCollisionFlags(&D_actor_403100_80155814[flameIndex].body, WORLD_COLLISION_BODY_FLAGS_MASK, WORLD_COLLISION_BODY_GRID_ENABLED);
            worldCollisionClearContacts(D_actor_403100_80155814[flameIndex].contacts);
            flame->age++;
            flame->spriteStep++;
            if (flame->age == D_actor_403100_80155808->flameLifetime) {
                flame->active = 0;
                worldCollisionUnlinkBody(&D_actor_403100_80155814[flameIndex].body);
            }
        } else {
            _actor403100MaskCollisionFlags(&D_actor_403100_80155814[flameIndex].body, (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)), 0);
        }
    }
    // This pool-wide cooldown loses a tick even when motion and age are frozen.
    if (D_actor_403100_80155810 != 0) {
        D_actor_403100_80155810--;
    }
}

/// States of the task `_actor403100FloorPuffEmitterTask` runs, by `Task::state`.
static const TaskFuncTable3 D_actor_403100_80131E70 = {
    {
        _actor403100BeginFloorPuffEmission,
        _actor403100EmitFloorPuffs,
        _actor403100WaitAfterFloorPuffEmission,
    },
};

/// States of the task `_actor403100DriftPuffEmitterTask` runs, by `Task::state`.
static const TaskFuncTable3 D_actor_403100_80131E7C = {
    {
        _actor403100BeginDriftPuffEmission,
        _actor403100EmitDriftPuffs,
        _actor403100WaitAfterDriftPuffEmission,
    },
};

/// Runs the selected scripted balcony scene and refreshes flames, pose and shake.
///
/// Borrows work and enemy from the live task and publishes them for singleton
/// helpers. `state` must be 0..9; dispatch is unchecked. Scene callbacks own
/// animation stepping and substate changes. Requires model coordinates 0..8 and
/// loaded balcony resources. Nonnegative buffer-release delays lose one update;
/// zero frees the primitive buffer and becomes -1, disabling further release.
static void _actor403100StepScene(Task* task)
{
    TmdObject* model      = task->extra.tmd;
    TaskFunc   handlers[] = {
        _actor403100SceneIdleState0,
        _actor403100DispatchSceneEntrance,
        _actor403100DispatchSceneAimAndImpact,
        _actor403100DispatchSceneAdvance,
        _actor403100DispatchSceneFlameBreath,
        _actor403100DispatchSceneBalconyImpact,
        _actor403100DispatchSceneTurn,
        _actor403100DispatchSceneFlameRetreat,
        _actor403100DispatchSceneDeparture,
        _actor403100SceneIdleState9
    };
    GfxCoord* actorCoords;
    GfxCoord* jawCoord;
    GfxCoord* headCoord;
    s32       releaseDelay;

    D_actor_403100_80155808 = task->work;
    D_actor_403100_8015580C = task->spawnArg2.pointer;
    handlers[(s16)D_actor_403100_80155808->state](task);
    _actor403100SetRootYaw(task);
    _actor403100ScaleRoot(task, D_actor_403100_80155808->sceneScale);
    _actor403100UpdateFlames(task, ACTOR_403100_FLAME_BASE_SIZE);
    actorCoords                 = task->extra.tmd->coords;
    actorCoords[8].composeStamp = GRAPHICS_COORD_DIRTY;
    actorCoords[7].composeStamp = GRAPHICS_COORD_DIRTY;
    actorCoords[6].composeStamp = GRAPHICS_COORD_DIRTY;
    actorCoords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    actorCoords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    actorCoords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    actorCoords[2].composeStamp = GRAPHICS_COORD_DIRTY;
    actorCoords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorCoords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    jawCoord                    = &actorCoords[4];
    headCoord                   = &actorCoords[3];
    actorRenderComposeCoord(&actorCoords[8]);
    actorRenderComposeCoord(jawCoord);
    _actor403100UpdateColor(task, headCoord);
    _actor403100StepScreenShake();
    // The countdown releases only the model primitive buffer, once at zero.
    releaseDelay = D_actor_403100_80155808->bufferReleaseDelay;
    if (releaseDelay >= 0) {
        if (releaseDelay == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        D_actor_403100_80155808->bufferReleaseDelay = (u16)D_actor_403100_80155808->bufferReleaseDelay - 1;
    }
}
/// Moves the entrance along +X with a cosine bob and plays its arrival impact.
///
/// Requires live singleton work, Burner model and enemy sound instance. Each
/// call increments the wrapping halfword timer and adds 64 root-parent units
/// along X. Y is a 512-unit Q12 cosine bob offset by -768. At tick 64, resets
/// the timer, changes scene sprites, plays lower-trunk/jaw cues with rumble and
/// shake, and advances to the entrance wait; animation is stepped by dispatch.
static void _actor403100StepSceneEntrance(Task* task)
{
    enum {
        ACTOR_403100_ENTRANCE_BOB_ANGLE_STEP = 32,
        ACTOR_403100_ENTRANCE_IMPACT_TICK    = 64,
    };
    GfxCoord* rootCoord;
    u16       nextFrame;
    s32       footfallSound;
    s32       footfallPan;
    s32       impactSound;
    s32       impactPan;

    rootCoord                            = task->extra.tmd->coords;
    nextFrame                            = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = nextFrame;
    rootCoord->coord.t[1]                = (s16)(rcos((s16)nextFrame * ACTOR_403100_ENTRANCE_BOB_ANGLE_STEP) << 0xD >> 0x10) - 0x300;
    rootCoord->coord.t[0]               += 0x40;
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_ENTRANCE_IMPACT_TICK) {
        D_actor_403100_80155808->stateFrames = 0;
        padScriptSpawnVariableMotorRamp(ACTOR_403100_FOOTFALL_FEEDBACK_FRAMES, ACTOR_403100_FEEDBACK_START_INTENSITY, ACTOR_403100_FEEDBACK_END_INTENSITY);
        D_actor_403100_80155808->shakeFrames = ACTOR_403100_FOOTFALL_FEEDBACK_FRAMES;
        dryfieldNightMotelBalconyUpdateSceneSprites(ACTOR_403100_SCENE_SPRITE_CUE_0);
        footfallSound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_FOOTFALL_SOUND;
        footfallPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]);
        sndEvtRequestScriptStart(footfallSound, footfallPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]) / 2));
        impactSound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_JAW_IMPACT_SOUND;
        impactPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]);
        sndEvtRequestScriptStart(impactSound, impactPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]) / 2));
        D_actor_403100_80155808->subState += 1;
    }
}
/// Aims the scripted impact toward a fixed room point and plays its timed cues.
///
/// Requires live singleton work/model and the enemy sound instance. Increments
/// the halfword timer first: tick 40 selects fast tracking and subsequent calls
/// supply target (-1000, 0, -2400). Tick 62 changes scene sprites, plays trunk
/// and jaw sounds with rumble/shake, and advances the scene substate.
static void _actor403100StepSceneAimAndImpact(Task* task)
{
    enum {
        ACTOR_403100_SCENE_AIM_START_TICK  = 40,
        ACTOR_403100_SCENE_AIM_IMPACT_TICK = 62,
    };
    u16 nextFrame;
    s32 footfallSound;
    s32 footfallPan;
    s32 impactSound;
    s32 impactPan;

    nextFrame                            = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = nextFrame;
    if ((s16)nextFrame == ACTOR_403100_SCENE_AIM_IMPACT_TICK) {
        dryfieldNightMotelBalconyUpdateSceneSprites(ACTOR_403100_SCENE_SPRITE_CUE_0);
        padScriptSpawnVariableMotorRamp(ACTOR_403100_FOOTFALL_FEEDBACK_FRAMES, ACTOR_403100_FEEDBACK_START_INTENSITY, ACTOR_403100_FEEDBACK_END_INTENSITY);
        D_actor_403100_80155808->shakeFrames = ACTOR_403100_FOOTFALL_FEEDBACK_FRAMES;
        footfallSound                        = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_FOOTFALL_SOUND;
        footfallPan                          = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]);
        sndEvtRequestScriptStart(footfallSound, footfallPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]) / 2));
        impactSound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_JAW_IMPACT_SOUND;
        impactPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]);
        sndEvtRequestScriptStart(impactSound, impactPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]) / 2));
        D_actor_403100_80155808->subState += 1;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_SCENE_AIM_START_TICK) {
        D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_TRACK_FAST;
    }
    if ((s16)D_actor_403100_80155808->stateFrames >= ACTOR_403100_SCENE_AIM_START_TICK) {
        D_actor_403100_80155808->aimTarget.vx = -0x3E8;
        D_actor_403100_80155808->aimTarget.vy = 0;
        D_actor_403100_80155808->aimTarget.vz = -0x960;
    }
}

/// Starts the scripted advance toward the balcony, with the old breath cleared.
///
/// Requires the singleton work, enemy and live model. Places the root in the
/// room frame, starts clip 2 immediately at normal rate and advances the scene
/// substate. The following step moves the root along +Z before its impact cues.
static void _actor403100BeginSceneAdvance(Task* task)
{
    enum {
        ACTOR_403100_ANIMATION_SCENE_ADVANCE = 2,
        ACTOR_403100_SCENE_ADVANCE_SCALE_Q12 = 0x1910,
    };
    GfxCoord* rootCoord;

    D_actor_403100_80155810 = 0;
    rootCoord               = task->extra.tmd->coords;
    _actor403100ReleaseFlames();
    sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    rootCoord->coord.t[0]                     = -0x74E;
    rootCoord->coord.t[2]                     = -0x1C51;
    rootCoord->coord.t[1]                     = 0;
    D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
    D_actor_403100_80155808->animationId      = ACTOR_403100_ANIMATION_SCENE_ADVANCE;
    D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
    D_actor_403100_80155808->rotation.vy      = 0;
    D_actor_403100_80155808->stateFrames      = 0;
    D_actor_403100_80155808->sceneScale       = ACTOR_403100_SCENE_ADVANCE_SCALE_Q12;
    _actor403100UpdateAnimation(task);
    D_actor_403100_80155808->subState += 1;
}
/// Plays the scripted advance impact and its two room-sprite cues.
///
/// Requires live singleton work/model and enemy sound instance. Increments the
/// halfword timer before testing: tick 12 plays trunk audio then rumble/shake
/// and selects scene-sprite step 1; tick 24 selects step 0; tick 40 advances
/// the substate. Advances animation itself; dispatch advances it again.
static void _actor403100StepSceneAdvanceImpact(Task* task)
{
    enum {
        ACTOR_403100_ADVANCE_IMPACT_TICK       = 12,
        ACTOR_403100_ADVANCE_SPRITE_RESET_TICK = 24,
        ACTOR_403100_ADVANCE_IMPACT_END_TICK   = 40,
    };
    s32 footfallSound;
    s32 footfallPan;
    u16 nextFrame;

    nextFrame                            = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = nextFrame;
    if ((s16)nextFrame == ACTOR_403100_ADVANCE_IMPACT_TICK) {
        footfallSound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_FOOTFALL_SOUND;
        footfallPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]);
        sndEvtRequestScriptStart(footfallSound, footfallPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]) / 2));
        padScriptSpawnVariableMotorRamp(ACTOR_403100_FOOTFALL_FEEDBACK_FRAMES, ACTOR_403100_FEEDBACK_START_INTENSITY, ACTOR_403100_FEEDBACK_END_INTENSITY);
        D_actor_403100_80155808->shakeFrames = ACTOR_403100_FOOTFALL_FEEDBACK_FRAMES;
        dryfieldNightMotelBalconyUpdateSceneSprites(ACTOR_403100_SCENE_SPRITE_CUE_1);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_ADVANCE_SPRITE_RESET_TICK) {
        dryfieldNightMotelBalconyUpdateSceneSprites(ACTOR_403100_SCENE_SPRITE_CUE_0);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_ADVANCE_IMPACT_END_TICK) {
        D_actor_403100_80155808->subState += 1;
    }
    _actor403100UpdateAnimation(task);
}
/// Starts the scripted flame breath at its fixed room placement.
///
/// Clears live flames and their hit cooldown, selects fast head tracking and
/// starts the breath clip immediately. Puffs live for the pool's 28-slot extent;
/// the following step emits at most one per update with collision disabled.
static void _actor403100BeginSceneFlameBreath(Task* task)
{
    enum {
        ACTOR_403100_ANIMATION_FLAME_BREATH = 7,
        ACTOR_403100_SCENE_BREATH_SCALE_Q12 = ONE * 5 / 4,
    };
    GfxCoord* rootCoord;

    D_actor_403100_80155810                = 0;
    rootCoord                              = task->extra.tmd->coords;
    D_actor_403100_80155808->aimMode       = ACTOR_403100_AIM_TRACK_FAST;
    D_actor_403100_80155808->flameLifetime = ARRAY_SIZE(D_actor_403100_80155814);
    _actor403100ReleaseFlames();
    sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    rootCoord->coord.t[0]                     = -0x74E;
    rootCoord->coord.t[2]                     = -0x1770;
    rootCoord->coord.t[1]                     = 0;
    D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
    D_actor_403100_80155808->animationId      = ACTOR_403100_ANIMATION_FLAME_BREATH;
    D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
    D_actor_403100_80155808->rotation.vy      = 0;
    D_actor_403100_80155808->stateFrames      = 0;
    D_actor_403100_80155808->sceneScale       = ACTOR_403100_SCENE_BREATH_SCALE_Q12;
    _actor403100UpdateAnimation(task);
    D_actor_403100_80155808->subState += 1;
}

/// Emits the scripted breath toward a fixed room-space target until clip completion.
///
/// Requires the live model, enemy sound instance and singleton work. Emits
/// collision-disabled puffs on updates 1..50, fades the breath sound at 50,
/// and advances on a slot-1 animation boundary, jump or settled pose.
static void _actor403100StepSceneFlameBreath(Task* task)
{
    enum {
        ACTOR_403100_SCENE_BREATH_FRAMES       = 50,
        ACTOR_403100_SCENE_BREATH_FADE_UPDATES = 10,
        ACTOR_403100_PART_JAW                  = 4,
    };
    SVECTOR headOffset;
    SVECTOR headVelocity;
    s32     breathSound;
    s32     audioPan;

    D_actor_403100_80155808->stateFrames += 1;
    D_actor_403100_80155808->aimTarget.vx = -0xFA0;
    D_actor_403100_80155808->aimTarget.vy = 0;
    D_actor_403100_80155808->aimTarget.vz = 0x7B2;
    if ((s16)D_actor_403100_80155808->stateFrames == 1) {
        breathSound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_BURNER, 4);
        audioPan    = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]);
        sndEvtRequestScriptStart(breathSound, audioPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]) / 2));
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_SCENE_BREATH_FRAMES) {
        sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), ACTOR_403100_SCENE_BREATH_FADE_UPDATES);
    }
    if (D_actor_403100_80155808->stateFrames < (u32)(ACTOR_403100_SCENE_BREATH_FRAMES + 1)) {
        // Both vectors start in the head frame; emission converts only the offset.
        headOffset.vy   = -0x1F0;
        headOffset.vz   = 0x620;
        headVelocity.vy = -0x20;
        headOffset.vx   = 0;
        headVelocity.vx = 0;
        headVelocity.vz = 0xF0;
        _actor403100EmitFlame(task, &headOffset, &headVelocity, ACTOR_403100_FLAME_COLLISION_DISABLED);
    }
    if (_actor403100AnimationAtBoundaryOrJump()) {
        D_actor_403100_80155808->subState += 1;
    }
}
/// Plays the balcony impact and walks its seven-entry scene-sprite cue sequence.
///
/// Requires live singleton work/model and enemy sound instance, with stateFrames
/// and auxFrames initially zero. Tick 14 plays trunk audio then rumble/shake.
/// Ticks 14..20 consume sequence entries 0..6; tick 21 selects the idle wait.
/// Animation and head pose are stepped by the enclosing scene dispatcher.
static void _actor403100StepSceneBalconyImpact(Task* task)
{
    enum {
        ACTOR_403100_BALCONY_IMPACT_TICK         = 14,
        ACTOR_403100_BALCONY_IMPACT_SPRITE_TICKS = 7U,
        ACTOR_403100_BALCONY_IMPACT_END_TICK     = 21,
    };
    s32 footfallSound;
    s32 footfallPan;
    u16 nextFrame;

    nextFrame                            = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = nextFrame;
    if ((s16)nextFrame == ACTOR_403100_BALCONY_IMPACT_TICK) {
        footfallSound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_FOOTFALL_SOUND;
        footfallPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]);
        sndEvtRequestScriptStart(footfallSound, footfallPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]) / 2));
        padScriptSpawnVariableMotorRamp(ACTOR_403100_FOOTFALL_FEEDBACK_FRAMES, ACTOR_403100_FEEDBACK_START_INTENSITY, ACTOR_403100_FEEDBACK_END_INTENSITY);
        D_actor_403100_80155808->shakeFrames = ACTOR_403100_FOOTFALL_FEEDBACK_FRAMES;
    }
    if ((u32)(D_actor_403100_80155808->stateFrames - ACTOR_403100_BALCONY_IMPACT_TICK) < ACTOR_403100_BALCONY_IMPACT_SPRITE_TICKS) {
        dryfieldNightMotelBalconyUpdateSceneSprites(D_actor_403100_801557A8[D_actor_403100_80155808->auxFrames]);
        D_actor_403100_80155808->auxFrames += 1;
    }
    if ((s16)D_actor_403100_80155808->stateFrames >= ACTOR_403100_BALCONY_IMPACT_END_TICK) {
        D_actor_403100_80155808->subState += 1;
    }
}
/// Places the Burner and restarts the scripted turn after releasing the breath.
///
/// Requires live singleton work/model, initialized flames and loaded balcony
/// updates. Sets root-parent position (-900, 0, 4400), forearm X -2055 and
/// Q12 scale 1.25; restarts clip 19, clears the timer and advances the substate.
/// The enclosing scene dispatcher advances animation and applies scale.
static void _actor403100BeginSceneTurn(Task* task)
{
    enum {
        ACTOR_403100_SCENE_TURN_CLIP      = 19,
        ACTOR_403100_SCENE_TURN_SCALE_Q12 = ONE * 5 / 4,
    };
    GfxCoord*        rootCoord;
    GfxCoord*        forearmCoord;
    Actor403100Work* work;
    s32              initialAnimationRate;

    initialAnimationRate    = ANIMATION_RATE_ONE;
    D_actor_403100_80155810 = 0;
    rootCoord               = task->extra.tmd->coords;
    forearmCoord            = &rootCoord[6];
    _actor403100ReleaseFlames();
    sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    forearmCoord->coord.t[0] = -0x807;
    rootCoord->coord.t[0]    = -0x384;
    work                     = D_actor_403100_80155808;
    rootCoord->coord.t[2]    = 0x1130;
    rootCoord->coord.t[1]    = 0;
    work->animationRate      = initialAnimationRate;
    work->animationId        = ACTOR_403100_SCENE_TURN_CLIP;
    work->animationRequest   = ACTOR_403100_ANIMATION_REQUEST_RESET;
    work->stateFrames        = 0;
    work->sceneScale         = ACTOR_403100_SCENE_TURN_SCALE_Q12;
    work->subState          += 1;
    areaApplySavedUpdates(D_dryfield_night_motel_balcony_8018F2CC);
}
/// Eases the scene heading toward a negative quarter-turn and plays its timed cue.
///
/// Scene-turn substate 1 requires live singleton work, model and an Enemy in
/// `task->spawnArg2.pointer`. Yaw uses 4096 units per turn. The prior wrapping
/// halfword timer selects sound/rumble/shake at tick 190 and the walk blend at
/// tick 240, then increments; animation advances in the enclosing dispatcher.
static void _actor403100StepSceneTurn(Task* task)
{
    enum {
        ACTOR_403100_SCENE_TURN_TARGET_YAW  = -ACTOR_TRANSFORM_ANGLE_TURN / 4,
        ACTOR_403100_SCENE_TURN_YAW_SCALE   = 16,
        ACTOR_403100_SCENE_TURN_STEP_SHIFT  = 9,
        ACTOR_403100_SCENE_TURN_SOUND_TICK  = 190,
        ACTOR_403100_SCENE_TURN_END_TICK    = 240,
        ACTOR_403100_SCENE_TURN_SHAKE_TICKS = 30,
        ACTOR_403100_SCENE_TURN_MOTOR_START = 255,
        ACTOR_403100_SCENE_TURN_MOTOR_END   = 8,
    };
    s32 soundId;
    s32 audioPan;
    u16 rootYawBits;

    // Narrow the scaled yaw error to twelve angle bits before easing by 1/32.
    rootYawBits                          = (u16)D_actor_403100_80155808->rotation.vy;
    D_actor_403100_80155808->rotation.vy = rootYawBits + ((s16)(ACTOR_403100_SCENE_TURN_TARGET_YAW * ACTOR_403100_SCENE_TURN_YAW_SCALE - rootYawBits * ACTOR_403100_SCENE_TURN_YAW_SCALE) >> ACTOR_403100_SCENE_TURN_STEP_SHIFT);
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_SCENE_TURN_SOUND_TICK) {
        soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_BURNER, 1);
        audioPan = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]) / 2));
        padScriptSpawnVariableMotorRamp(ACTOR_403100_SCENE_TURN_SHAKE_TICKS, ACTOR_403100_SCENE_TURN_MOTOR_START, ACTOR_403100_SCENE_TURN_MOTOR_END);
        D_actor_403100_80155808->shakeFrames = ACTOR_403100_SCENE_TURN_SHAKE_TICKS;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_SCENE_TURN_END_TICK) {
        D_actor_403100_80155808->subState += 1;
    }
    D_actor_403100_80155808->stateFrames += 1;
}

/// Emits one scripted balcony flame burst and starts its jaw-positioned breath audio.
///
/// Requires the loaded room effects and live Burner enemy/model. The placement
/// nibble selects the audio instance; jaw pan and half depth narrow to signed
/// bytes. The caller owns cue timing and the later ten-update sound fade.
static inline void _actor403100SpawnSceneFlameBurst(Task* task)
{
    s32 breathSound;
    s32 audioPan;
    s32 audioDepth;

    dryfieldNightMotelBalconySpawnFlameBurst();
    breathSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_BREATH_SOUND;
    audioPan    = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]);
    audioDepth  = worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]);
    sndEvtRequestScriptStart(breathSound, (s32)audioPan, (s8)(audioDepth / 2));
}

/// Plays the four timed room flame bursts before the scene retreat.
///
/// Requires live singleton work, enemy/model and balcony effects, with the timer
/// cleared. Updates 30, 40, 60 and 90 start a room burst and jaw breath audio;
/// 34, 48, 63 and 94 fade that audio over ten sound updates. A settled slot-2
/// clip restarts clip 21, clears the timer and selects retreat recovery.
static void _actor403100StepSceneFlameRetreatBursts(Task* task)
{
    enum {
        ACTOR_403100_RETREAT_BURST_FIRST_TICK  = 30,
        ACTOR_403100_RETREAT_FADE_FIRST_TICK   = 34,
        ACTOR_403100_RETREAT_BURST_SECOND_TICK = 40,
        ACTOR_403100_RETREAT_FADE_SECOND_TICK  = 48,
        ACTOR_403100_RETREAT_BURST_THIRD_TICK  = 60,
        ACTOR_403100_RETREAT_FADE_THIRD_TICK   = 63,
        ACTOR_403100_RETREAT_BURST_FOURTH_TICK = 90,
        ACTOR_403100_RETREAT_FADE_FOURTH_TICK  = 94,
        ACTOR_403100_RETREAT_RECOVERY_CLIP     = 21,
    };
    u16 elapsedFrames;

    elapsedFrames                        = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames == ACTOR_403100_RETREAT_BURST_FIRST_TICK) {
        _actor403100SpawnSceneFlameBurst(task);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_RETREAT_FADE_FIRST_TICK) {
        sndEvtRequestScriptStop(ACTOR_403100_BREATH_SOUND, ACTOR_403100_FLAME_CONTACT_STOP_FADE_UPDATES);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_RETREAT_BURST_SECOND_TICK) {
        _actor403100SpawnSceneFlameBurst(task);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_RETREAT_FADE_SECOND_TICK) {
        sndEvtRequestScriptStop(ACTOR_403100_BREATH_SOUND, ACTOR_403100_FLAME_CONTACT_STOP_FADE_UPDATES);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_RETREAT_BURST_THIRD_TICK) {
        _actor403100SpawnSceneFlameBurst(task);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_RETREAT_FADE_THIRD_TICK) {
        sndEvtRequestScriptStop(ACTOR_403100_BREATH_SOUND, ACTOR_403100_FLAME_CONTACT_STOP_FADE_UPDATES);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_RETREAT_BURST_FOURTH_TICK) {
        _actor403100SpawnSceneFlameBurst(task);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_RETREAT_FADE_FOURTH_TICK) {
        sndEvtRequestScriptStop(ACTOR_403100_BREATH_SOUND, ACTOR_403100_FLAME_CONTACT_STOP_FADE_UPDATES);
    }
    if (_actor403100Slot2AnimationSettled()) {
        _actor403100RequestAnimationReset(ACTOR_403100_RETREAT_RECOVERY_CLIP);
        D_actor_403100_80155808->stateFrames = 0;
        D_actor_403100_80155808->subState   += 1;
    }
}
/// Finishes the scene flame cues and blends into the retreat walk.
///
/// Requires live singleton work, enemy/model and balcony effects, with the timer
/// cleared. Bursts at update 10, fades breath at 14 and sounds the jaw at 38.
/// A settled slot-2 clip blends to clip 22 over 24 animation frames at 1.5x
/// rate, resets the stride/timer and clears the model ordering-depth offset.
static void _actor403100StepSceneFlameRetreatRecovery(Task* task)
{
    enum {
        ACTOR_403100_RETREAT_LAST_BURST_TICK   = 10,
        ACTOR_403100_RETREAT_LAST_FADE_TICK    = 14,
        ACTOR_403100_RETREAT_JAW_CUE_TICK      = 38,
        ACTOR_403100_RETREAT_WALK_BLEND_FRAMES = 24,
        ACTOR_403100_RETREAT_WALK_CLIP         = 22,
    };
    s32        jawSound;
    s32        jawPan;
    u16        elapsedFrames;
    s32        jawDepth;
    TmdObject* model;

    model                                = task->extra.tmd;
    elapsedFrames                        = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames == ACTOR_403100_RETREAT_LAST_BURST_TICK) {
        _actor403100SpawnSceneFlameBurst(task);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_RETREAT_LAST_FADE_TICK) {
        sndEvtRequestScriptStop(ACTOR_403100_BREATH_SOUND, ACTOR_403100_FLAME_CONTACT_STOP_FADE_UPDATES);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_RETREAT_JAW_CUE_TICK) {
        jawSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_JAW_IMPACT_SOUND;
        jawPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]);
        jawDepth = worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]);
        sndEvtRequestScriptStart(jawSound, jawPan, (s8)(jawDepth / 2));
    }
    if (_actor403100Slot2AnimationSettled()) {
        D_actor_403100_80155808->animationBlendFrames = ACTOR_403100_RETREAT_WALK_BLEND_FRAMES;
        D_actor_403100_80155808->animationRate        = ANIMATION_RATE_ONE * 3 / 2;
        D_actor_403100_80155808->animationId          = ACTOR_403100_RETREAT_WALK_CLIP;
        D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
        D_actor_403100_80155808->stateFrames          = 0;
        D_actor_403100_80155808->stridePhase          = 0;
        D_actor_403100_80155808->subState            += 1;
        model->otOffset                               = 0;
    }
}
/// Walks the scene's flame retreat toward negative room Z while turning and bobbing.
///
/// Requires live singleton work/model with cleared stateFrames and stridePhase.
/// Translation uses root-parent game units, yaw uses 4096 units per turn, and the
/// half-turn stride produces a 512-unit upward bob. Timed ten-update bands alter
/// turning and forward speed; the first sixteen updates also offset X and Z.
/// This terminal substate continues until a scene command changes the behaviour.
static void _actor403100StepSceneRetreatWalk(Task* task)
{
    enum {
        ACTOR_403100_RETREAT_TURN_FIRST_FRAME  = 17,
        ACTOR_403100_RETREAT_TURN_SECOND_FRAME = 27,
        ACTOR_403100_RETREAT_TURN_THIRD_FRAME  = 37,
        ACTOR_403100_RETREAT_TURN_FOURTH_FRAME = 47,
        ACTOR_403100_RETREAT_TURN_FIFTH_FRAME  = 57,
        ACTOR_403100_RETREAT_TURN_BAND_FRAMES  = 10,
    };
    s16       stridePhase;
    s32       footfallSound;
    s32       rootHeight;
    s32       rootZ;
    s32       audioPan;
    u16       elapsedFrames;
    s32       audioDepth;
    GfxCoord* rootCoord;

    rootCoord                             = task->extra.tmd->coords;
    stridePhase                           = ((u16)D_actor_403100_80155808->stridePhase + ACTOR_403100_STRIDE_STEP) & ACTOR_403100_STRIDE_MASK;
    D_actor_403100_80155808->stateFrames += 1;
    D_actor_403100_80155808->stridePhase  = stridePhase;
    rootHeight                            = -((s32)(rsin((s32)stridePhase) << 0xD) >> 0x10);
    rootCoord->coord.t[1]                 = rootHeight;
    if (rootHeight == 0) {
        footfallSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_FOOTFALL_SOUND;
        audioPan      = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]);
        audioDepth    = worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]);
        sndEvtRequestScriptStart(footfallSound, (s32)audioPan, (s8)(audioDepth / 2));
        padScriptSpawnVariableMotorRamp(ACTOR_403100_FOOTFALL_FEEDBACK_FRAMES, ACTOR_403100_FEEDBACK_START_INTENSITY, ACTOR_403100_FEEDBACK_END_INTENSITY);
        D_actor_403100_80155808->shakeFrames = ACTOR_403100_FOOTFALL_FEEDBACK_FRAMES;
    }
    // The initial diagonal offset precedes the independent speed/turn bands.
    if ((s16)D_actor_403100_80155808->stateFrames < ACTOR_403100_RETREAT_TURN_FIRST_FRAME) {
        D_actor_403100_80155808->rotation.vy = (u16)D_actor_403100_80155808->rotation.vy - 0x34;
        rootCoord->coord.t[2]                = (s32)(rootCoord->coord.t[2] - 0x10);
        rootCoord->coord.t[0]                = (s32)(rootCoord->coord.t[0] - 8);
    }
    elapsedFrames = D_actor_403100_80155808->stateFrames;
    if ((u32)(elapsedFrames - ACTOR_403100_RETREAT_TURN_FIRST_FRAME) < (u32)ACTOR_403100_RETREAT_TURN_BAND_FRAMES) {
        D_actor_403100_80155808->rotation.vy = (u16)D_actor_403100_80155808->rotation.vy - 0x10;
        rootZ                                = rootCoord->coord.t[2] - 0x14;
    } else if ((u16)(elapsedFrames - ACTOR_403100_RETREAT_TURN_SECOND_FRAME) < (u32)ACTOR_403100_RETREAT_TURN_BAND_FRAMES) {
        D_actor_403100_80155808->rotation.vy = (u16)D_actor_403100_80155808->rotation.vy - 0x10;
        rootZ                                = rootCoord->coord.t[2] - 0x20;
    } else {
        if ((u16)(elapsedFrames - ACTOR_403100_RETREAT_TURN_THIRD_FRAME) < (u32)ACTOR_403100_RETREAT_TURN_BAND_FRAMES) {
            D_actor_403100_80155808->rotation.vy -= 0x10;
        } else if ((u16)(elapsedFrames - ACTOR_403100_RETREAT_TURN_FOURTH_FRAME) < (u32)ACTOR_403100_RETREAT_TURN_BAND_FRAMES) {
            D_actor_403100_80155808->rotation.vy -= 0xC;
        } else if ((u16)(elapsedFrames - ACTOR_403100_RETREAT_TURN_FIFTH_FRAME) < (u32)ACTOR_403100_RETREAT_TURN_BAND_FRAMES) {
            D_actor_403100_80155808->rotation.vy -= 8;
        }
        rootZ = rootCoord->coord.t[2] - 0x40;
    }
    rootCoord->coord.t[2] = rootZ;
}

/// Moves the scripted departure through its clip's timed Z steps and footfall cues.
///
/// Requires live singleton work/model, with both counters cleared at entry.
/// Root-parent Z displacement is in game units. auxFrames counts within each
/// clip restart; stateFrames counts the whole departure, with halfword wrapping.
/// A settled slot-2 clip restarts clip 24 and resets only auxFrames. At update
/// 280 the substate advances to the reward-and-teardown callback.
static void _actor403100StepSceneDeparture(Task* task)
{
    enum { ACTOR_403100_DEPARTURE_CLIP                  = 24,
           ACTOR_403100_DEPARTURE_FRAMES                = 280,
           ACTOR_403100_DEPARTURE_FOOTFALL_FIRST_FRAME  = 45,
           ACTOR_403100_DEPARTURE_FOOTFALL_SECOND_FRAME = 87,
           ACTOR_403100_DEPARTURE_FOOTFALL_THIRD_FRAME  = 180,
           ACTOR_403100_DEPARTURE_FOOTFALL_FOURTH_FRAME = 230 };
    s16       elapsedFrames;
    s16       clipFrames;
    s32       footfallSound;
    s32       audioPan;
    s32       audioDepth;
    GfxCoord* rootCoord;

    elapsedFrames                        = D_actor_403100_80155808->stateFrames + 1;
    rootCoord                            = task->extra.tmd->coords;
    D_actor_403100_80155808->stateFrames = (u16)elapsedFrames;
    D_actor_403100_80155808->auxFrames  += 1;
    D_actor_403100_80155808->stridePhase = (u16)((D_actor_403100_80155808->stridePhase + ACTOR_403100_STRIDE_STEP) & ACTOR_403100_STRIDE_MASK);
    // Movement and sound cues follow the clip timer, not the overall departure timer.
    if (D_actor_403100_80155808->auxFrames < 0xA) {
        rootCoord->coord.t[2] -= 0x6;
    } else if (D_actor_403100_80155808->auxFrames < 0x14) {
        rootCoord->coord.t[2] -= 0xA;
    } else if (D_actor_403100_80155808->auxFrames < 0x1E) {
        rootCoord->coord.t[2] -= 0x12;
    } else if (D_actor_403100_80155808->auxFrames < 0x28) {
        rootCoord->coord.t[2] -= 0x6;
    } else if (D_actor_403100_80155808->auxFrames < 0x32) {
        rootCoord->coord.t[2] -= 0x6;
    } else if (D_actor_403100_80155808->auxFrames < 0x3C) {
        rootCoord->coord.t[2] -= 0xA;
    } else if (D_actor_403100_80155808->auxFrames < 0x46) {
        rootCoord->coord.t[2] -= 0x12;
    } else if (D_actor_403100_80155808->auxFrames < 0x5D) {
        rootCoord->coord.t[2] -= 0x4;
    } else if (D_actor_403100_80155808->auxFrames < 0x69) {
        rootCoord->coord.t[2] -= 0x8;
    } else if (D_actor_403100_80155808->auxFrames < 0x75) {
        rootCoord->coord.t[2] -= 0x10;
    } else if (D_actor_403100_80155808->auxFrames < 0x81) {
        rootCoord->coord.t[2] -= 0x28;
    } else if (D_actor_403100_80155808->auxFrames < 0x92) {
        rootCoord->coord.t[2] -= 0x18;
    } else if (D_actor_403100_80155808->auxFrames < 0x9C) {
        rootCoord->coord.t[2] -= 0x6;
    } else if (D_actor_403100_80155808->auxFrames < 0xA6) {
        rootCoord->coord.t[2] -= 0xA;
    } else if (D_actor_403100_80155808->auxFrames < 0xB0) {
        rootCoord->coord.t[2] -= 0x12;
    } else if (D_actor_403100_80155808->auxFrames < 0xBA) {
        rootCoord->coord.t[2] -= 0x6;
    } else if (D_actor_403100_80155808->auxFrames < 0xC4) {
        rootCoord->coord.t[2] -= 0x6;
    }
    clipFrames = (s16)D_actor_403100_80155808->auxFrames;
    if ((clipFrames == ACTOR_403100_DEPARTURE_FOOTFALL_FIRST_FRAME) || (clipFrames == ACTOR_403100_DEPARTURE_FOOTFALL_FOURTH_FRAME) || (clipFrames == ACTOR_403100_DEPARTURE_FOOTFALL_THIRD_FRAME) || (clipFrames == ACTOR_403100_DEPARTURE_FOOTFALL_SECOND_FRAME)) {
        footfallSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_FOOTFALL_SOUND;
        audioPan      = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]);
        audioDepth    = worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]);
        sndEvtRequestScriptStart(footfallSound, audioPan, (s8)(audioDepth / 2));
        padScriptSpawnVariableMotorRamp(ACTOR_403100_FOOTFALL_FEEDBACK_FRAMES, ACTOR_403100_FEEDBACK_START_INTENSITY, ACTOR_403100_FEEDBACK_END_INTENSITY);
        D_actor_403100_80155808->shakeFrames = ACTOR_403100_FOOTFALL_FEEDBACK_FRAMES;
    }
    if (_actor403100Slot2AnimationSettled()) {
        _actor403100RequestAnimationReset(ACTOR_403100_DEPARTURE_CLIP);
        D_actor_403100_80155808->auxFrames = 0U;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_DEPARTURE_FRAMES) {
        D_actor_403100_80155808->subState = (u16)(D_actor_403100_80155808->subState + 1);
    }
}
/// Allocates and initializes the Burner for the night motel balcony encounter.
///
/// Task state 0 requires a live model and enemy spawn argument, the loaded room
/// and room resources. Only stage 3, area 29, room variant 2 is accepted.
/// Failure destroys through the previously published singleton enemy; the
/// validity of that binding before successful setup is unproven. Owns a zeroed
/// Actor403100Work until task teardown; the model borrows
/// its light/colour matrices and rig. Links enemy targeting, binds messages and
/// combat bodies, resets the flame pool and room sprites, restores full HP, and
/// selects top-level scene state 1 with scene/substate zero.
static void _actor403100Initialize(Task* task)
{
    enum {
        ACTOR_403100_BALCONY_ROOM_VARIANT = 2,
        ACTOR_403100_INITIAL_SCENE_STATE  = 1,
    };
    Actor403100Work* work;
    TmdObject*       model;
    Enemy*           spawnedEnemy;
    s32              flameIndex;
    s32              savedViewOrTargetFlags;
    Enemy*           linkedEnemy;
    GfxCoord*        rootCoord;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_KEY(0xFF, 0xFF, 0xFF, 0)) != GAME_LOCATION_KEY(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY, ACTOR_403100_BALCONY_ROOM_VARIANT, 0) ||
        (task->work = memCalloc(sizeof(Actor403100Work), false)) == NULL) {
        enemyDestroy(D_actor_403100_8015580C, task);
        return;
    }
    spawnedEnemy            = task->spawnArg2.pointer;
    D_actor_403100_8015580C = spawnedEnemy;
    work                    = task->work;
    model->lightMtx         = &work->light;
    D_actor_403100_80155808 = work;
    model->colorMtx         = &work->color;
    task->msgTable          = D_actor_403100_801556EC;
    // Save the view before binding the hidden, non-lockable target.
    savedViewOrTargetFlags  = gGameSession->location.loc.view;
    work->savedView         = savedViewOrTargetFlags;
    spawnedEnemy->field_48  = 0;
    spawnedEnemy->field_4   = &rootCoord->coord;
    linkedEnemy             = D_actor_403100_8015580C;
    linkedEnemy->bodyPos.vx = 0;
    linkedEnemy->bodyPos.vy = 0;
    linkedEnemy->bodyPos.vz = 0x300;
    linkedEnemy->coord      = &task->extra.tmd->coords[3];
    worldTargetLinkNode(&linkedEnemy->node);
    savedViewOrTargetFlags                      = WORLD_TARGET_HIDE_HP | WORLD_TARGET_NOT_LOCKABLE;
    linkedEnemy                                 = D_actor_403100_8015580C;
    linkedEnemy->node.state.parts.flags         = savedViewOrTargetFlags;
    linkedEnemy                                 = D_actor_403100_8015580C;
    linkedEnemy->param                          = &D_actor_403100_8014762C;
    linkedEnemy->recs                           = D_actor_403100_80155808->hitContacts;
    D_actor_403100_80155630.coord               = task->extra.tmd->coords;
    model->flags                                = 0;
    D_actor_403100_80155808->bufferReleaseDelay = ACTOR_403100_BUFFER_RELEASE_DISABLED;
    animationInitContext(&D_actor_403100_80155808->rig.anim, D_actor_403100_8015572C, model, D_actor_403100_80155808->rig.poses, D_actor_403100_80155808->rig.slots);
    _actor403100RequestAnimationReset(ACTOR_403100_ANIMATION_WALK);
    _actor403100UpdateAnimation(task);
    rootCoord->parent = &gGfxViewCoord;
    _actor403100InitCombatBodies(task);
    for (flameIndex = ARRAY_SIZE(D_actor_403100_80155814) - 1; flameIndex >= 0; flameIndex--) {
        D_actor_403100_80155814[flameIndex].active = 0;
    }
    dryfieldNightMotelBalconyResetSceneSprites();
    dryfieldNightMotelBalconyRestoreSectionStates();
    D_actor_403100_80155810     = 0;
    D_actor_403100_8015580C->hp = D_actor_403100_8015580C->hpMax = D_actor_403100_8014762C.hpMax;
    task->state                                                  = ACTOR_403100_INITIAL_SCENE_STATE;
    D_actor_403100_80155808->state                               = 0;
    D_actor_403100_80155808->subState                            = 0;
}
/// Steps of the behaviour mode `_actor403100DispatchSceneEntrance`, indexed by `subState`.
static const TaskFuncTable3 D_actor_403100_80131EB0 = {
    {
        _actor403100BeginSceneEntrance,
        _actor403100StepSceneEntrance,
        _actor403100WaitAfterSceneEntrance,
    },
};

/// Steps of the behaviour mode `_actor403100DispatchSceneAimAndImpact`, indexed by `subState`.
static const TaskFuncTable3 D_actor_403100_80131EBC = {
    {
        _actor403100BeginSceneAimAndImpact,
        _actor403100StepSceneAimAndImpact,
        _actor403100WaitAfterSceneAimAndImpact,
    },
};

/// Steps of the behaviour mode `_actor403100DispatchSceneAdvance`, indexed by `subState`.
static const TaskFuncTable4 D_actor_403100_80131EC8 = {
    {
        _actor403100BeginSceneAdvance,
        _actor403100StepSceneAdvance,
        _actor403100StepSceneAdvanceImpact,
        _actor403100WaitAfterSceneAdvance,
    },
};

/// Steps of the behaviour mode `_actor403100DispatchSceneFlameBreath`, indexed by `subState`.
static const TaskFuncTable3 D_actor_403100_80131ED8 = {
    {
        _actor403100BeginSceneFlameBreath,
        _actor403100StepSceneFlameBreath,
        _actor403100WaitAfterSceneFlameBreath,
    },
};

/// Steps of the behaviour mode `_actor403100DispatchSceneBalconyImpact`, indexed by `subState`.
static const TaskFuncTable3 D_actor_403100_80131EE4 = {
    {
        _actor403100BeginSceneBalconyImpact,
        _actor403100StepSceneBalconyImpact,
        _actor403100WaitAfterSceneBalconyImpact,
    },
};

/// Steps of the behaviour mode `_actor403100DispatchSceneTurn`, indexed by `subState`.
static const TaskFuncTable4 D_actor_403100_80131EF0 = {
    {
        _actor403100BeginSceneTurn,
        _actor403100StepSceneTurn,
        _actor403100BlendSceneWalk,
        _actor403100WaitAfterSceneTurn,
    },
};

/// Steps of the behaviour mode `_actor403100DispatchSceneFlameRetreat`, indexed by `subState`.
static const TaskFuncTable4 D_actor_403100_80131F00 = {
    {
        _actor403100BeginSceneFlameRetreat,
        _actor403100StepSceneFlameRetreatBursts,
        _actor403100StepSceneFlameRetreatRecovery,
        _actor403100StepSceneRetreatWalk,
    },
};

/// Steps of the behaviour mode `_actor403100DispatchSceneDeparture`, indexed by `subState`.
static const TaskFuncTable3 D_actor_403100_80131F10 = {
    {
        _actor403100BeginSceneDeparture,
        _actor403100StepSceneDeparture,
        _actor403100FinishSceneDeparture,
    },
};

/// The actor's six top-level states, by `Task::state`; run by
/// `_actor403100Task`.
static const TaskFuncTable6 D_actor_403100_80131F1C = {
    {
        _actor403100Initialize,
        _actor403100StepScene,
        _actor403100StepFight,
        _actor403100StepDefeat,
        _actor403100BeginTimeoutEvent,
        _actor403100Destroy,
    },
};

/// Behaviour modes of the actor's main state, indexed by `state`. Each
/// mode steps through its own table by `subState`.
static const TaskFuncTable11 D_actor_403100_80131F34 = {
    {
        _actor403100StepFightInitialization,
        _actor403100StepAttackApproach,
        _actor403100StepArmSwingCombo,
        _actor403100StepAimedFlameAttack,
        _actor403100StepJumpSlam,
        _actor403100StepGrabAndSqueeze,
        _actor403100StepSideFlameAttack,
        _actor403100StepGrabAndDrag,
        _actor403100StepStagger,
        _actor403100DispatchLowHealthScene,
        _actor403100StepBuildupStun,
    },
};

/// Runs the current stage of the player's reaction to a Burner arm hit.
///
/// Requires live singleton work and a playerReactionStage in
/// ACTOR_403100_PLAYER_REACTION_NONE..RECOVERING. Dispatch is unchecked and
/// precedes the Burner's behaviour; callbacks use the singleton and take no arguments.
static inline void _actor403100StepPlayerReaction(void)
{
    _Actor403100PlayerReactionTable reactionHandlers = D_actor_403100_80131E24;

    reactionHandlers.handlers[D_actor_403100_80155808->playerReactionStage]();
}

/// Adds the forearm offsets to its animated local Euler rotation and refreshes the transform.
///
/// The task owns the live Burner model; coordinate 6 is its forearm. Angles
/// use 4096 units per turn and narrow to signed halfwords before `RotMatrix`.
/// Translation and matrix alignment bytes are retained.
static inline void _actor403100TurnForearmInline(Task* task)
{
    SVECTOR   angles;
    MATRIX    rotation;
    MATRIX*   localRotation;
    GfxCoord* forearm;

    forearm               = task->extra.tmd->coords + 6;
    localRotation         = &forearm->coord;
    forearm->composeStamp = GRAPHICS_COORD_DIRTY;
    gfxSetRotIdentity(&rotation);
    gfxExtractEulerAngles(localRotation, &angles);
    angles.vz += D_actor_403100_80155808->forearmTurn.vz;
    angles.vy += D_actor_403100_80155808->forearmTurn.vy;
    angles.vx += D_actor_403100_80155808->forearmTurn.vx;
    RotMatrix(&angles, &rotation);
    _actor403100CopyRotation(localRotation, &rotation);
    actorRenderComposeCoord(forearm);
}

/// Adds the jaw and head pitch kicks to their animated local rotations.
///
/// Requires the live Burner model and work block. Offsets use 4096 units per
/// turn and addition wraps to a signed halfword. Only the nine rotation
/// coefficients change; translation, alignment bytes and cache stamps remain.
/// The fight update marks the affected coordinate chain dirty after posing it.
static inline void _actor403100ApplyJawAndHeadPitch(Task* task)
{
    enum { ACTOR_403100_PITCH_JAW_PART = 4 };
    SVECTOR   angles;
    MATRIX    rotation;
    MATRIX*   targetRotation;
    GfxCoord* coords;

    coords = task->extra.tmd->coords;
    gfxSetRotIdentity(&rotation);
    targetRotation = &coords[ACTOR_403100_PITCH_JAW_PART].coord;
    gfxExtractEulerAngles(targetRotation, &angles);
    angles.vx += D_actor_403100_80155808->jawPitchOffset;
    RotMatrix(&angles, &rotation);
    _actor403100CopyRotation(targetRotation, &rotation);

    coords = task->extra.tmd->coords;
    gfxSetRotIdentity(&rotation);
    targetRotation = &coords[ACTOR_403100_PART_HEAD].coord;
    gfxExtractEulerAngles(targetRotation, &angles);
    angles.vx += D_actor_403100_80155808->headPitchOffset;
    RotMatrix(&angles, &rotation);
    _actor403100CopyRotation(targetRotation, &rotation);
}

/// Rebuilds the posed head/arm coordinate caches and samples head-origin lighting.
///
/// Requires live Burner model coordinates 0..8 and enemy lighting state.
/// Posing has finished for this update; compose the claw and jaw parent chains
/// after dirtying their shared prefix, then sample the composed head origin.
static inline void _actor403100RefreshPosedCoords(Task* task)
{
    GfxCoord* actorCoords;
    GfxCoord* jawCoord;
    GfxCoord* headCoord;

    actorCoords                 = task->extra.tmd->coords;
    actorCoords[8].composeStamp = GRAPHICS_COORD_DIRTY;
    actorCoords[7].composeStamp = GRAPHICS_COORD_DIRTY;
    actorCoords[6].composeStamp = GRAPHICS_COORD_DIRTY;
    actorCoords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    actorCoords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    actorCoords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    actorCoords[2].composeStamp = GRAPHICS_COORD_DIRTY;
    actorCoords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorCoords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    jawCoord                    = actorCoords + 4;
    headCoord                   = actorCoords + 3;
    actorRenderComposeCoord(actorCoords + 8);
    actorRenderComposeCoord(jawCoord);
    _actor403100UpdateColor(task, headCoord);
}

/// Runs Burner combat, player hold reactions and the posed-model refresh.
///
/// Requires live singleton work/enemy, a player task/model and the loaded balcony.
/// state indexes eleven combat handlers (0..10); each owns its substate bounds.
/// The engagement timer advances even when hidden or paused. Running updates
/// count down the fight, dispatch behavior and apply animation, contacts and
/// Q12 scale 1.25. Pause keeps the flame/draw refresh; hidden marks drawing skipped.
/// Death hands off to scene state 9 unless the player also died, restoring 1000 HP.
static void _actor403100StepFight(Task* task)
{
    enum {
        ACTOR_403100_LOW_HEALTH_PERCENT     = 35,
        ACTOR_403100_FIGHT_LOW_HEALTH_STATE = 9,
        ACTOR_403100_SCENE_IDLE_STATE       = 9,
        ACTOR_403100_FIGHT_SCALE_Q12        = ONE * 5 / 4,
        ACTOR_403100_SIMULTANEOUS_DEATH_HP  = 1000,
        ACTOR_403100_TASK_SCENE_DISPATCH    = 1,
    };
    TaskFuncTable11 stateHandlers;
    PlayerStatus*   playerStatus = &gPlayerStatus;
    s32             rootScaleQ12;
    s16             hitColorFramesLeft;
    s16             engageFramesLeft;
    s32             fightFramesLeft;
    Task*           playerTask;
    TmdObject*      model;
    GfxCoord*       playerRootCoord;

    model         = task->extra.tmd;
    playerTask    = *gPlayerActorTasks;
    stateHandlers = D_actor_403100_80131F34;
    // Engagement is independent of the scene actor pause/hide gate.
    engageFramesLeft = D_actor_403100_80155808->engageDelay;
    if (engageFramesLeft != 0) {
        if ((engageFramesLeft == 1) && (D_actor_403100_8015580C->hp > 0)) {
            sceneEngageBattle(1);
        }
        D_actor_403100_80155808->engageDelay = (s16)((u16)D_actor_403100_80155808->engageDelay - 1);
    }
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (playerTask == NULL) {
                D_actor_403100_80155808->playerRegion = 0;
            }
            _actor403100SamplePlayerTarget(task);
            fightFramesLeft = D_actor_403100_80155808->fightFramesLeft;
            if (fightFramesLeft >= 0) {
                D_actor_403100_80155808->fightFramesLeft = (s32)(fightFramesLeft - 1);
            }
            if ((playerStatus->hp > 0) && (D_actor_403100_80155808->playerReactionStage == ACTOR_403100_PLAYER_REACTION_NONE)) {
                if (D_actor_403100_8015580C->hp < (s16)(((s16)D_actor_403100_8015580C->hpMax * ACTOR_403100_LOW_HEALTH_PERCENT) / 100)) {
                    D_actor_403100_80155808->lowHealth = 1U;
                } else {
                    D_actor_403100_80155808->lowHealth = 0U;
                }
            }
            playerRootCoord                       = playerTask->extra.tmd->coords;
            D_actor_403100_80155808->playerRegion = _actor403100FindZoneId((s16)playerRootCoord->coord.t[0], (s16)playerRootCoord->coord.t[2], D_actor_403100_80155638);
            // Advance the held-player reaction before the selected combat behavior.
            _actor403100StepPlayerReaction();
            stateHandlers.funcs[(s16)D_actor_403100_80155808->state](task);
            if ((D_actor_403100_80155808->lowHealth != 0) && (D_actor_403100_80155808->phaseChangeDone == 0) && (D_actor_403100_80155808->holdingPlayer == 0)) {
                D_actor_403100_80155808->state    = ACTOR_403100_FIGHT_LOW_HEALTH_STATE;
                D_actor_403100_80155808->subState = 0;
            }
            _actor403100StepJawPitchKick(task);
            _actor403100StepHeadPitchKick();
            _actor403100UpdateWalk(task);
            _actor403100UpdateAnimation(task);
            D_actor_403100_80155808->previousAnimationFlags = D_actor_403100_80155808->rig.slots[1].status.fields.flags;
            _actor403100StepScreenShake();
            _actor403100AimHead(task, D_actor_403100_80155808->aimMode);
            _actor403100SetRootYaw(task);
            rootScaleQ12 = ACTOR_403100_FIGHT_SCALE_Q12;
            _actor403100ScaleRoot(task, rootScaleQ12);
            _actor403100TurnUpperArm(task);
            _actor403100TurnForearmInline(task);
            _actor403100ApplyJawAndHeadPitch(task);
            _actor403100ProcessCombatContacts(task);
            if (D_actor_403100_80155808->hitColorFrames != 0) {
                hitColorFramesLeft = --D_actor_403100_80155808->hitColorFrames;
                if ((s16)hitColorFramesLeft == 0) {
                    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
                }
            }
            // Defer defeat during vulnerability/held-hit phases; simultaneous player death restores HP.
            if ((D_actor_403100_8015580C->hp <= 0) && (D_actor_403100_80155808->vulnerable == 0) && (D_actor_403100_80155808->playerReactionStage == ACTOR_403100_PLAYER_REACTION_NONE)) {
                if (playerStatus->hp <= 0) {
                    D_actor_403100_8015580C->hp = ACTOR_403100_SIMULTANEOUS_DEATH_HP;
                } else {
                    D_actor_403100_80155808->bufferReleaseDelay = ACTOR_403100_BUFFER_RELEASE_DISABLED;
                    D_actor_403100_80155808->sceneScale         = ACTOR_403100_FIGHT_SCALE_Q12;
                    gGameSession->suppressDeathChecks           = 0;
                    Gp_StateC08.flags                           = (u8)(Gp_StateC08.flags | ATTACHMENT_FLAG_EVENT_LOCK);
                    gGameSession->suppressViewTriggers          = 0;
                    D_actor_403100_8015580C->reactionFlags      = 0;
                    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
                    D_actor_403100_8015580C->node.state.parts.flags = (WORLD_TARGET_HIDE_HP | WORLD_TARGET_NOT_LOCKABLE);
                    evsStartScript(D_actor_335800_80166098, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                    task->state                       = ACTOR_403100_TASK_SCENE_DISPATCH;
                    D_actor_403100_80155808->state    = 0;
                    D_actor_403100_80155808->subState = 0;
                    D_actor_403100_80155808->state    = ACTOR_403100_SCENE_IDLE_STATE;
                    D_actor_403100_80155808->subState = 0;
                }
            }
        // Paused combat still refreshes flame drawing and the composed pose.
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor403100UpdateFlames(task, ACTOR_403100_FLAME_BASE_SIZE);
            _actor403100RefreshPosedCoords(task);
            model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
}
/// Places the Burner for combat and acquires the fight's scene battle hold.
///
/// Requires the live singleton work, enemy and model. Starts the 5400-update
/// fight countdown and a 30-update engagement delay, resets the walk clip and
/// clears the preceding scene's flames before advancing the substate.
static void _actor403100BeginFight(Task* task)
{
    enum {
        ACTOR_403100_FIGHT_FRAMES        = 5400,
        ACTOR_403100_ENGAGE_DELAY_FRAMES = 30,
    };
    GfxCoord*        rootCoord;
    Actor403100Work* work;

    rootCoord                                       = task->extra.tmd->coords;
    D_actor_403100_8015580C->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
    D_actor_403100_80155808->engageDelay            = ACTOR_403100_ENGAGE_DELAY_FRAMES;
    D_actor_403100_80155808->walkSpeed              = 0x20;
    D_actor_403100_80155808->stridePhase            = 0;
    sceneAcquireBattleRef(0);
    // Preserve the work read before the cooldown clear and root placement stores.
    work                                     = *(Actor403100Work* volatile*)&D_actor_403100_80155808;
    rootCoord->coord.t[0]                    = -0x44C;
    rootCoord->coord.t[2]                    = 0x980;
    *(volatile s16*)&D_actor_403100_80155810 = 0;
    rootCoord->coord.t[1]                    = 0;
    work->rotation.vy                        = 0xC00;
    work->fightFramesLeft                    = ACTOR_403100_FIGHT_FRAMES;
    work->animationRate                      = ANIMATION_RATE_ONE;
    work->animationId                        = ACTOR_403100_ANIMATION_WALK;
    work->rotation.vx                        = 0;
    work->rotation.vz                        = 0;
    work->animationRequest                   = ACTOR_403100_ANIMATION_REQUEST_RESET;
    work->stateFrames                        = 0;
    _actor403100ReleaseFlames();
    sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    D_actor_403100_80155808->subState += 1;
}
/// Steps of the behaviour mode `_actor403100StepAttackApproach`, indexed by `subState`.
static const TaskFuncTable3 D_actor_403100_80131F60 = {
    {
        _actor403100BeginAttackApproach,
        _actor403100WaitForPlayerInAttackRange,
        _actor403100ChooseAttack,
    },
};

/// Advances the idle substate when the player's region permits an attack.
///
/// Region 1 requires distance below 8000 game units; regions 3..5 below 6100.
/// Regions 2 and 6 advance immediately, and an unlisted region leaves it waiting.
/// The cached player distance/region must describe this frame. `task` is unused
/// but retained for the task-handler table's signature.
static void _actor403100WaitForPlayerInAttackRange(Task* task)
{
    enum { ACTOR_403100_REGION_1_ATTACK_RANGE       = 8000,
           ACTOR_403100_REGIONS_3_TO_5_ATTACK_RANGE = 6100 };
    s16 regionIndex;

    regionIndex = D_actor_403100_80155808->playerRegion - ACTOR_403100_PLAYER_REGION_1;
    switch (regionIndex) {
        case ACTOR_403100_PLAYER_REGION_1 - 1:
            if (D_actor_403100_80155808->playerDistance < ACTOR_403100_REGION_1_ATTACK_RANGE) {
                D_actor_403100_80155808->subState += 1;
            }
            break;
        case ACTOR_403100_PLAYER_REGION_2 - 1:
        case ACTOR_403100_PLAYER_REGION_6 - 1:
            D_actor_403100_80155808->subState += 1;
            break;
        case ACTOR_403100_PLAYER_REGION_3 - 1:
        case ACTOR_403100_PLAYER_REGION_4 - 1:
        case ACTOR_403100_PLAYER_REGION_5 - 1:
            if (D_actor_403100_80155808->playerDistance < ACTOR_403100_REGIONS_3_TO_5_ATTACK_RANGE) {
                D_actor_403100_80155808->subState += 1;
            }
            break;
    }
}
/// Chooses an attack from health-dependent odds, region eligibility and history.
///
/// Resets subState for the chosen state. Regions 2/6 select the side flame
/// without a draw or history update. Other regions draw from sixteen entries,
/// substitute ineligible attacks, then avoid a third consecutive repeat.
/// recentStateCursor must index the three-entry history. `task` is unused but
/// retained for the task-handler table's signature.
static void _actor403100ChooseAttack(Task* task)
{
    enum {
        ACTOR_403100_ATTACK_ARM_SWING        = 2,
        ACTOR_403100_ATTACK_AIMED_FLAME      = 3,
        ACTOR_403100_ATTACK_JUMP_SLAM        = 4,
        ACTOR_403100_ATTACK_GRAB_AND_SQUEEZE = 5,
        ACTOR_403100_ATTACK_SIDE_FLAME       = 6,
        ACTOR_403100_ATTACK_GRAB_AND_DRAG    = 7,
    };
    s32 halfPlayerHealth;
    s16 playerRegion;
    s16 firstHistoryAttack;
    s16 selectedAttack;
    u32 weakenedDraw;
    u32 normalDraw;

    halfPlayerHealth = gPlayerStatus.hpMax / 2;
    playerRegion     = D_actor_403100_80155808->playerRegion;
    // Apply the health-dependent odds, then restrict attacks to eligible regions.
    if (playerRegion != ACTOR_403100_PLAYER_REGION_2 && playerRegion != ACTOR_403100_PLAYER_REGION_6) {
        if ((gPlayerStatus.hp < halfPlayerHealth) || (D_actor_403100_80155808->lowHealth != 0)) {
            weakenedDraw                      = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState                   = weakenedDraw;
            D_actor_403100_80155808->state    = D_actor_403100_801557B0.states[ACTOR_403100_ATTACK_ODDS_WEAKENED][(weakenedDraw >> 16) & (ACTOR_403100_ATTACK_DRAWS - 1)];
            D_actor_403100_80155808->subState = 0;
        } else {
            normalDraw                        = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState                   = normalDraw;
            D_actor_403100_80155808->state    = D_actor_403100_801557B0.states[ACTOR_403100_ATTACK_ODDS_NORMAL][(normalDraw >> 16) & (ACTOR_403100_ATTACK_DRAWS - 1)];
            D_actor_403100_80155808->subState = 0;
        }
        if ((s16)D_actor_403100_80155808->state == ACTOR_403100_ATTACK_JUMP_SLAM) {
            if (D_actor_403100_80155808->lowHealth != 0) {
                D_actor_403100_80155808->state    = ACTOR_403100_ATTACK_ARM_SWING;
                D_actor_403100_80155808->subState = 0;
            }
        }
        if ((s16)D_actor_403100_80155808->state == ACTOR_403100_ATTACK_GRAB_AND_SQUEEZE) {
            if (D_actor_403100_80155808->playerRegion != ACTOR_403100_PLAYER_REGION_1) {
                D_actor_403100_80155808->state    = ACTOR_403100_ATTACK_GRAB_AND_DRAG;
                D_actor_403100_80155808->subState = 0;
            }
        }
        if (((s16)D_actor_403100_80155808->state == ACTOR_403100_ATTACK_GRAB_AND_DRAG) && ((u32)((u16)D_actor_403100_80155808->playerRegion - ACTOR_403100_PLAYER_REGION_3) >= 2U)) {
            D_actor_403100_80155808->state    = ACTOR_403100_ATTACK_JUMP_SLAM;
            D_actor_403100_80155808->subState = 0;
        }
        // Replace a third consecutive repeat and record the final choice.
        D_actor_403100_80155808->recentStates[D_actor_403100_80155808->recentStateCursor] = (u16)D_actor_403100_80155808->state;
        firstHistoryAttack                                                                = D_actor_403100_80155808->recentStates[0];
        if ((firstHistoryAttack == D_actor_403100_80155808->recentStates[1]) && (firstHistoryAttack == D_actor_403100_80155808->recentStates[2])) {
            selectedAttack = (s16)D_actor_403100_80155808->state;
            if (selectedAttack == ACTOR_403100_ATTACK_ARM_SWING || selectedAttack == ACTOR_403100_ATTACK_JUMP_SLAM || selectedAttack == ACTOR_403100_ATTACK_GRAB_AND_DRAG) {
                D_actor_403100_80155808->state    = ACTOR_403100_ATTACK_AIMED_FLAME;
                D_actor_403100_80155808->subState = 0;
            } else if (selectedAttack == ACTOR_403100_ATTACK_AIMED_FLAME) {
                if (D_actor_403100_80155808->lowHealth != 0) {
                    D_actor_403100_80155808->state    = ACTOR_403100_ATTACK_ARM_SWING;
                    D_actor_403100_80155808->subState = 0;
                } else {
                    D_actor_403100_80155808->state    = ACTOR_403100_ATTACK_JUMP_SLAM;
                    D_actor_403100_80155808->subState = 0;
                }
            }
            D_actor_403100_80155808->recentStates[D_actor_403100_80155808->recentStateCursor] = (u16)D_actor_403100_80155808->state;
        }
        D_actor_403100_80155808->recentStateCursor = D_actor_403100_80155808->recentStateCursor + 1;
        if (D_actor_403100_80155808->recentStateCursor >= ARRAY_SIZE(D_actor_403100_80155808->recentStates)) {
            D_actor_403100_80155808->recentStateCursor = 0;
        }
    } else {
        D_actor_403100_80155808->state    = ACTOR_403100_ATTACK_SIDE_FLAME;
        D_actor_403100_80155808->subState = 0;
    }
}
/// Clamps the saved shoulder yaw target to the arm attack's reach.
///
/// Requires live singleton work. Target and inclusive limits use signed
/// angles in 4096 units per turn; the accepted range is -352..208.
static inline void _actor403100ClampArmYawTarget(void)
{
    if (D_actor_403100_80155808->armYawTarget >= ACTOR_403100_ARM_YAW_MAX + 1) {
        D_actor_403100_80155808->armYawTarget = ACTOR_403100_ARM_YAW_MAX;
    }
    if (D_actor_403100_80155808->armYawTarget < ACTOR_403100_ARM_YAW_MIN) {
        D_actor_403100_80155808->armYawTarget = ACTOR_403100_ARM_YAW_MIN;
    }
}

/// Initializes the arm-swing combo and its one-to-three repeat count.
///
/// Requires live singleton work. Captures head yaw as the shoulder target in
/// 4096 units per turn, clears the prior contacts and pitch kicks, and starts
/// the arm-attack clip at three-quarter speed while approaching at 32 units
/// per update. The enclosing attack dispatcher consumes hit interruptions;
/// `task` is retained for the `TaskFunc` signature.
static void _actor403100BeginArmSwingCombo(Task* task)
{
    enum { ACTOR_403100_ARM_SWING_RATE = ANIMATION_RATE_ONE * 3 / 4 };
    u16 headYawBits;
    u32 secondSwingDraw;
    u32 firstSwingDraw;

    // Two independent bits choose one, two or three repeats with weights 1:2:1.
    firstSwingDraw                        = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    secondSwingDraw                       = (firstSwingDraw * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    gRandomLcgState                       = secondSwingDraw;
    D_actor_403100_80155808->stateCounter = ((firstSwingDraw >> 0x10) & 1) + 1 + ((secondSwingDraw >> 0x10) & 1);
    D_actor_403100_80155808->walkSpeed    = 0x20;
    headYawBits                           = (u16)D_actor_403100_80155808->headAim.vy;
    D_actor_403100_80155808->walkStage    = ACTOR_403100_WALK_APPROACH;
    D_actor_403100_80155808->aimMode      = ACTOR_403100_AIM_YAW_ONLY_FAST;
    D_actor_403100_80155808->armPitch     = 0;
    D_actor_403100_80155808->armYaw       = 0;
    D_actor_403100_80155808->armYawTarget = (s16)headYawBits;
    _actor403100ClampArmYawTarget();
    D_actor_403100_80155808->animationId      = ACTOR_403100_ANIMATION_ARM_ATTACK;
    D_actor_403100_80155808->animationRate    = ACTOR_403100_ARM_SWING_RATE;
    D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
    D_actor_403100_80155808->stateFrames      = 0;
    // Contacts are sampled from the animated arm; disable the two body-pair tests.
    D_actor_403100_80155808->handAttack.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    D_actor_403100_80155808->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    D_actor_403100_80155808->handTouchedPlayer    = 0;
    D_actor_403100_80155808->forearmTouchedPlayer = 0;
    D_actor_403100_80155808->subState            += 1;
    D_actor_403100_80155808->jawPitchPhase        = ACTOR_403100_PITCH_PHASE_REST;
    D_actor_403100_80155808->headPitchPhase       = ACTOR_403100_PITCH_PHASE_REST;
    D_actor_403100_80155808->swingConnected       = 0;
}
/// Starts the player's held arm-hit reaction and applies the Burner's contact attack.
///
/// Requires live singleton work, player and damage parameters. Acquires the
/// scripted-action lock, installs the package's hit clip and holds the reaction
/// for 23 updates. Attack entry zero supplies damage; a lethal result selects
/// player state ten. This sequence retains no additional player task reference.
static inline void _actor403100StartPlayerArmHitReaction(void)
{
    enum { ACTOR_403100_PLAYER_ARM_HIT_CLIP     = 5,
           ACTOR_403100_PLAYER_HIT_HOLD_FRAMES  = 23,
           ACTOR_403100_PLAYER_LETHAL_HIT_STATE = 10 };
    Task* playerTask;

    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
    _actor403100PlayPlayerAnimation(ACTOR_403100_PLAYER_ARM_HIT_CLIP, ANIMATION_MESSAGE_INSTALL_AND_PLAY);
    D_actor_403100_80155808->playerReactionFrames = ACTOR_403100_PLAYER_HIT_HOLD_FRAMES;
    D_actor_403100_80155808->playerReactionStage  = ACTOR_403100_PLAYER_REACTION_HIT_HELD;
    playerTask                                    = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(D_actor_403100_80147614, 0), 0) == 1) {
        ((GameActor*)(*gPlayerActorTasks)->work)->state = ACTOR_403100_PLAYER_LETHAL_HIT_STATE;
    }
}

/// Checks the first arm swing and chooses repeat or connected-hit recovery.
///
/// Requires live singleton work, player and Burner model/enemy; stateFrames
/// starts at zero. Samples contacts at prior timer values 54..57, with the
/// balcony impact cue at 57. A touch stops the stride and starts a player hit
/// reaction only when idle. A connected swing skips repeats; a miss starts
/// clip 2 at 5/8 rate, caching head yaw within -352..208 angle units.
static void _actor403100StepFirstArmSwing(Task* task)
{
    enum {
        ACTOR_403100_REPEAT_SWING_CLIP       = 2,
        ACTOR_403100_SWING_ANGLE_SCALE_SHIFT = 4,
        ACTOR_403100_SWING_EASE_SHIFT        = 7,
    };
    enum {
        ACTOR_403100_ARM_IMPACT_PART = 7,
    };
    enum {
        ACTOR_403100_REPEAT_SWING_RATE    = ANIMATION_RATE_ONE * 5 / 8,
        ACTOR_403100_SWING_CONTACT_START  = 54,
        ACTOR_403100_SWING_CONTACT_FRAMES = 4U,
        ACTOR_403100_SWING_IMPACT_TICK    = 57,
    };
    s32 armImpactSound;
    s32 armImpactPan;
    u16 previousSubState;
    u16 headYawBits;

    // Sample the contact window before incrementing the step timer.
    if ((u32)(D_actor_403100_80155808->stateFrames - ACTOR_403100_SWING_CONTACT_START) < ACTOR_403100_SWING_CONTACT_FRAMES) {
        _actor403100ProbeArmPlayerContact(task);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_SWING_IMPACT_TICK) {
        _actor403100BreakBalconyAtArm(task);
        armImpactSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_BURNER, 3);
        armImpactPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_ARM_IMPACT_PART]);
        sndEvtRequestScriptStart(armImpactSound, armImpactPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_ARM_IMPACT_PART]) / 2));
        padScriptSpawnVariableMotorRamp(ACTOR_403100_IMPACT_FEEDBACK_FRAMES, ACTOR_403100_FEEDBACK_START_INTENSITY, ACTOR_403100_FEEDBACK_END_INTENSITY);
        D_actor_403100_80155808->shakeFrames = ACTOR_403100_IMPACT_FEEDBACK_FRAMES;
    }
    D_actor_403100_80155808->stateFrames += 1;
    if (D_actor_403100_80155808->handTouchedPlayer != 0 || D_actor_403100_80155808->forearmTouchedPlayer != 0) {
        D_actor_403100_80155808->walkStage = ACTOR_403100_WALK_FINISH_STRIDE;
        if (D_actor_403100_80155808->playerReactionStage == ACTOR_403100_PLAYER_REACTION_NONE) {
            _actor403100StartPlayerArmHitReaction();
        }
        D_actor_403100_80155808->handTouchedPlayer    = 0;
        D_actor_403100_80155808->forearmTouchedPlayer = 0;
        D_actor_403100_80155808->swingConnected       = 1;
        _actor403100RequestPitchKick(ACTOR_403100_PITCH_KICK_SELECT_ATTACK_SOUND);
    }
    D_actor_403100_80155808->armYaw += (s16)((D_actor_403100_80155808->armYawTarget - D_actor_403100_80155808->armYaw) << ACTOR_403100_SWING_ANGLE_SCALE_SHIFT) >> ACTOR_403100_SWING_EASE_SHIFT;
    // Choose recovery or cache a new shoulder target for the next swing.
    if (_actor403100AnimationAtBoundaryOrJump()) {
        if (D_actor_403100_80155808->swingConnected != 0) {
            _actor403100RequestAnimationReset(ACTOR_403100_ANIMATION_ARM_RECOVERY);
            D_actor_403100_80155808->subState            += 2;
            D_actor_403100_80155808->handAttack.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            D_actor_403100_80155808->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            return;
        }
        // Qualified accesses preserve the clip-boundary store/read order.
        previousSubState                                           = D_actor_403100_80155808->subState;
        *(volatile s16*)&D_actor_403100_80155808->animationRate    = ACTOR_403100_REPEAT_SWING_RATE;
        *(volatile s16*)&D_actor_403100_80155808->animationId      = ACTOR_403100_REPEAT_SWING_CLIP;
        *(volatile s16*)&D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
        headYawBits                                                = *(volatile s16*)&D_actor_403100_80155808->headAim.vy;
        D_actor_403100_80155808->stateFrames                       = 0;
        D_actor_403100_80155808->armYawTarget                      = (s16)headYawBits;
        D_actor_403100_80155808->subState                          = previousSubState + 1;
        if ((s16)headYawBits >= ACTOR_403100_ARM_YAW_MAX + 1) {
            D_actor_403100_80155808->armYawTarget = ACTOR_403100_ARM_YAW_MAX;
        }
        if (D_actor_403100_80155808->armYawTarget < ACTOR_403100_ARM_YAW_MIN) {
            D_actor_403100_80155808->armYawTarget = ACTOR_403100_ARM_YAW_MIN;
        }
    }
}
/// Repeats arm swings until the byte counter expires or a touch ends the combo.
///
/// Requires live singleton work, player and Burner model/enemy. Samples contacts
/// at prior timer values 60..68, with the balcony impact cue at 68. A touch
/// forces one remaining swing and starts a player hit reaction only when idle.
/// At a clip boundary the unsigned-byte counter decrements with byte wrapping;
/// zero starts recovery, otherwise clip 2 restarts with yaw limited to -320..208.
static void _actor403100StepRepeatedArmSwing(Task* task)
{
    enum {
        ACTOR_403100_REPEAT_SWING_CLIP       = 2,
        ACTOR_403100_SWING_ANGLE_SCALE_SHIFT = 4,
        ACTOR_403100_SWING_EASE_SHIFT        = 7,
    };
    enum {
        ACTOR_403100_ARM_IMPACT_PART = 7,
    };
    enum {
        ACTOR_403100_REPEAT_SWING_RATE    = ANIMATION_RATE_ONE * 5 / 8,
        ACTOR_403100_REPEAT_SWING_YAW_MIN = -320,
        ACTOR_403100_SWING_CONTACT_START  = 60,
        ACTOR_403100_SWING_CONTACT_FRAMES = 9U,
        ACTOR_403100_SWING_IMPACT_TICK    = 68,
    };
    s32 armImpactSound;
    s32 armImpactPan;
    u8  swingsLeft;
    u16 headYawBits;

    // Sample the contact window before incrementing the step timer.
    if ((u32)(D_actor_403100_80155808->stateFrames - ACTOR_403100_SWING_CONTACT_START) < ACTOR_403100_SWING_CONTACT_FRAMES) {
        _actor403100ProbeArmPlayerContact(task);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_SWING_IMPACT_TICK) {
        _actor403100BreakBalconyAtArm(task);
        armImpactSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_BURNER, 3);
        armImpactPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_ARM_IMPACT_PART]);
        sndEvtRequestScriptStart(armImpactSound, armImpactPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_ARM_IMPACT_PART]) / 2));
        padScriptSpawnVariableMotorRamp(ACTOR_403100_IMPACT_FEEDBACK_FRAMES, ACTOR_403100_FEEDBACK_START_INTENSITY, ACTOR_403100_FEEDBACK_END_INTENSITY);
        D_actor_403100_80155808->shakeFrames = ACTOR_403100_IMPACT_FEEDBACK_FRAMES;
    }
    D_actor_403100_80155808->stateFrames += 1;
    if (D_actor_403100_80155808->handTouchedPlayer != 0 || D_actor_403100_80155808->forearmTouchedPlayer != 0) {
        D_actor_403100_80155808->walkStage = ACTOR_403100_WALK_FINISH_STRIDE;
        if (D_actor_403100_80155808->playerReactionStage == ACTOR_403100_PLAYER_REACTION_NONE) {
            _actor403100StartPlayerArmHitReaction();
        }
        D_actor_403100_80155808->handTouchedPlayer    = 0;
        D_actor_403100_80155808->forearmTouchedPlayer = 0;
        _actor403100RequestPitchKick(ACTOR_403100_PITCH_KICK_SELECT_ATTACK_SOUND);
        D_actor_403100_80155808->swingConnected = 1;
        D_actor_403100_80155808->stateCounter   = 1;
    }
    D_actor_403100_80155808->armYaw += (s16)((D_actor_403100_80155808->armYawTarget - D_actor_403100_80155808->armYaw) << ACTOR_403100_SWING_ANGLE_SCALE_SHIFT) >> ACTOR_403100_SWING_EASE_SHIFT;
    // Choose recovery or cache a new shoulder target for the next swing.
    if (_actor403100AnimationAtBoundaryOrJump()) {
        swingsLeft                            = D_actor_403100_80155808->stateCounter - 1;
        D_actor_403100_80155808->stateCounter = swingsLeft;
        if (swingsLeft == 0) {
            _actor403100RequestAnimationReset(ACTOR_403100_ANIMATION_ARM_RECOVERY);
            D_actor_403100_80155808->subState            += 1;
            D_actor_403100_80155808->handAttack.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            D_actor_403100_80155808->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            return;
        }
        D_actor_403100_80155808->animationRate    = ACTOR_403100_REPEAT_SWING_RATE;
        headYawBits                               = (u16)D_actor_403100_80155808->headAim.vy;
        D_actor_403100_80155808->stateFrames      = 0;
        D_actor_403100_80155808->animationId      = ACTOR_403100_REPEAT_SWING_CLIP;
        D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
        D_actor_403100_80155808->armYawTarget     = (s16)headYawBits;
        if ((s16)headYawBits >= ACTOR_403100_ARM_YAW_MAX + 1) {
            D_actor_403100_80155808->armYawTarget = ACTOR_403100_ARM_YAW_MAX;
        }
        if (D_actor_403100_80155808->armYawTarget < ACTOR_403100_REPEAT_SWING_YAW_MIN) {
            D_actor_403100_80155808->armYawTarget = ACTOR_403100_REPEAT_SWING_YAW_MIN;
        }
    }
}
/// Eases the shoulder pitch/yaw offsets toward zero by one eighth.
///
/// Requires live singleton work. Subtraction wraps as a signed 12-bit turn
/// difference and each result is stored back in its signed halfword.
static inline void _actor403100RelaxArmPose(void)
{
    u16 armYawBits;
    u16 armPitchBits;
    armPitchBits                      = (u16)D_actor_403100_80155808->armPitch;
    armYawBits                        = (u16)D_actor_403100_80155808->armYaw;
    D_actor_403100_80155808->armPitch = armPitchBits + ((s32) - (armPitchBits << ACTOR_403100_ARM_SIGNED_ANGLE_SHIFT) >> (ACTOR_403100_ARM_SIGNED_ANGLE_SHIFT + ACTOR_403100_ARM_EASE_SHIFT));
    D_actor_403100_80155808->armYaw   = armYawBits + ((s32) - (armYawBits << ACTOR_403100_ARM_SIGNED_ANGLE_SHIFT) >> (ACTOR_403100_ARM_SIGNED_ANGLE_SHIFT + ACTOR_403100_ARM_EASE_SHIFT));
}

/// Relaxes the shoulder after the combo and selects its recovery continuation.
///
/// Requires live work and animation slots. Pitch and yaw ease by one eighth
/// of the signed 12-bit turn difference each update. At a clip boundary a
/// connected swing blends into impact or a fast walk; a miss resumes attack
/// approach immediately. `task` supplies the dispatch signature only.
static void _actor403100FinishArmSwingCombo(Task* task)
{
    enum {
        ACTOR_403100_COMBO_IMPACT_BLEND_FRAMES = 20,
        ACTOR_403100_COMBO_IMPACT_RATE         = ANIMATION_RATE_ONE * 7 / 4,
        ACTOR_403100_COMBO_WALK_BLEND_FRAMES   = 8,
    };
    u32 followupDraw;

    _actor403100RelaxArmPose();
    if (_actor403100AnimationAtBoundaryOrJump()) {
        // A connected combo gets an impact follow-up one quarter of the time.
        if (D_actor_403100_80155808->swingConnected != 0) {
            D_actor_403100_80155808->stateFrames = 0;
            followupDraw                         = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState                      = followupDraw;
            if (!((followupDraw >> 0x10) & 3)) {
                D_actor_403100_80155808->animationBlendFrames = ACTOR_403100_COMBO_IMPACT_BLEND_FRAMES;
                D_actor_403100_80155808->animationRate        = ACTOR_403100_COMBO_IMPACT_RATE;
                D_actor_403100_80155808->animationId          = ACTOR_403100_ANIMATION_SCENE_IMPACT;
                D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
                D_actor_403100_80155808->subState            += 1;
                return;
            }
            D_actor_403100_80155808->animationBlendFrames = ACTOR_403100_COMBO_WALK_BLEND_FRAMES;
            D_actor_403100_80155808->animationRate        = ANIMATION_RATE_ONE * 2;
            D_actor_403100_80155808->animationId          = ACTOR_403100_ANIMATION_WALK;
            D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
            D_actor_403100_80155808->subState            += 2;
            return;
        }
        D_actor_403100_80155808->state    = ACTOR_403100_BEHAVIOUR_ATTACK_APPROACH;
        D_actor_403100_80155808->subState = 0;
    }
}
/// Runs the impact follow-up selected after a connected arm combo.
///
/// Requires the live model and singleton work with stateFrames cleared. Update
/// 49 plays jaw and trunk cues and starts eighteen updates of rumble/shake.
/// A clip boundary or jump resumes attack approach. Sound ids include the
/// enemy placement index; audio pan/depth are narrowed to signed bytes.
static void _actor403100StepArmComboImpact(Task* task)
{
    s32 jawSound;
    s32 footfallSound;
    s32 jawPan;
    s32 footfallPan;
    u16 elapsedFrames;

    elapsedFrames                        = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames == ACTOR_403100_IMPACT_CUE_FRAME) {
        jawSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_JAW_IMPACT_SOUND;
        jawPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]);
        sndEvtRequestScriptStart(jawSound, jawPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]) / 2));
        footfallSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_FOOTFALL_SOUND;
        footfallPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]);
        sndEvtRequestScriptStart(footfallSound, footfallPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]) / 2));
        padScriptSpawnVariableMotorRamp(ACTOR_403100_IMPACT_FEEDBACK_FRAMES, ACTOR_403100_FEEDBACK_START_INTENSITY, ACTOR_403100_FEEDBACK_END_INTENSITY);
        D_actor_403100_80155808->shakeFrames = ACTOR_403100_IMPACT_FEEDBACK_FRAMES;
    }
    if (_actor403100AnimationAtBoundaryOrJump()) {
        D_actor_403100_80155808->state    = ACTOR_403100_BEHAVIOUR_ATTACK_APPROACH;
        D_actor_403100_80155808->subState = 0;
    }
}

/// Begins the aimed flame attack with a fresh twenty-update flame lifetime.
///
/// Requires initialized singleton work and flame pool. Clears the previous
/// puffs and breath audio, restarts the aimed-flame clip at normal speed and
/// approaches at 28 units per update, or 48 at low health. The enclosing
/// dispatcher handles hit interruptions; `task` is unused here.
static void _actor403100BeginAimedFlameAttack(Task* task)
{
    enum { ACTOR_403100_AIMED_FLAME_LIFETIME_FRAMES = 20 };

    D_actor_403100_80155808->walkStage = ACTOR_403100_WALK_APPROACH;
    if (D_actor_403100_80155808->lowHealth == 0) {
        D_actor_403100_80155808->walkSpeed = 0x1C;
    } else {
        D_actor_403100_80155808->walkSpeed = 0x30;
    }
    D_actor_403100_80155810                = 0;
    D_actor_403100_80155808->flameLifetime = ACTOR_403100_AIMED_FLAME_LIFETIME_FRAMES;
    // Clear the previous breath before restarting this attack's clip.
    _actor403100ReleaseFlames();
    sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    _actor403100RequestAnimationReset(ACTOR_403100_ANIMATION_AIMED_FLAME);
    D_actor_403100_80155808->aimMode     = ACTOR_403100_AIM_YAW_ONLY;
    D_actor_403100_80155808->stateFrames = 0;
    D_actor_403100_80155808->subState   += 1;
}
/// Tracks the player with flame breath and selects recovery when a puff touches.
///
/// Requires live singleton work/flames and Burner model/enemy, timer cleared.
/// Before update 51 regions 4/5 choose fixed world targets; stepped yaw begins
/// at 51 and normal-health tracking resumes at 61. Updates 52..123 emit
/// damaging head-local flame and refresh breath light; 125..135 emit smoke.
/// The final flame-hit cooldown update selects contact recovery; otherwise
/// a clip boundary advances to the normal recovery wait.
static void _actor403100StepAimedFlameBreath(Task* task)
{
    enum {
        ACTOR_403100_FLAME_CONTACT_CUE_FRAMES_LEFT   = 1,
        ACTOR_403100_AIMED_FLAME_CONTACT_SUBSTATE    = 3,
        ACTOR_403100_AIMED_BREATH_TRACK_FRAME        = 51,
        ACTOR_403100_AIMED_BREATH_NORMAL_TRACK_FRAME = 61,
        ACTOR_403100_AIMED_BREATH_STOP_FRAME         = 124,
        ACTOR_403100_AIMED_BREATH_EMIT_START         = 52,
        ACTOR_403100_AIMED_BREATH_EMIT_FRAMES        = 72,
        ACTOR_403100_AIMED_BREATH_JAW_CUE_FRAME      = 126,
        ACTOR_403100_AIMED_BREATH_SMOKE_START        = 125,
    };
    SVECTOR   emissionOffset;
    SVECTOR   flameVelocity;
    GfxCoord* smokeCoord;
    s32       breathSound;
    s32       jawSound;
    s32       hitTaken;
    s32       breathPan;
    s32       jawPan;

    // A puff touched the target; recover on its final cooldown update.
    if (D_actor_403100_80155810 == ACTOR_403100_FLAME_CONTACT_CUE_FRAMES_LEFT) {
        D_actor_403100_80155808->subState = ACTOR_403100_AIMED_FLAME_CONTACT_SUBSTATE;
        return;
    }
    hitTaken                              = D_actor_403100_80155808->hitTaken;
    D_actor_403100_80155808->stateFrames += 1;
    if (hitTaken == ACTOR_403100_HIT_TAKEN) {
        if (D_actor_403100_80155808->headPitchPhase == ACTOR_403100_PITCH_PHASE_REST) {
            D_actor_403100_80155808->headPitchPhase = hitTaken;
        }
    }
    if ((s16)D_actor_403100_80155808->stateFrames < ACTOR_403100_AIMED_BREATH_TRACK_FRAME) {
        if (D_actor_403100_80155808->playerRegion == ACTOR_403100_PLAYER_REGION_4) {
            D_actor_403100_80155808->aimTarget.vx = -0x1770;
            D_actor_403100_80155808->aimTarget.vy = -0xC80;
            D_actor_403100_80155808->aimTarget.vz = -0x1B58;
            D_actor_403100_80155808->aimMode      = ACTOR_403100_AIM_TRACK_FAST;
        }
        if (D_actor_403100_80155808->playerRegion == ACTOR_403100_PLAYER_REGION_5) {
            D_actor_403100_80155808->aimTarget.vx = 0x500;
            D_actor_403100_80155808->aimTarget.vy = -0xC80;
            D_actor_403100_80155808->aimTarget.vz = 0x2710;
            D_actor_403100_80155808->aimMode      = ACTOR_403100_AIM_TRACK_FAST;
        }
        if ((s16)D_actor_403100_80155808->stateFrames < ACTOR_403100_AIMED_BREATH_TRACK_FRAME) {
            dryfieldNightMotelBalconyTrySpawnFlame();
        }
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_AIMED_BREATH_TRACK_FRAME) {
        D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_TRACK_STEPPED;
        if (D_actor_403100_80155808->lowHealth) {
            D_actor_403100_80155808->aimYawStep = 0x10;
        } else {
            D_actor_403100_80155808->aimYawStep = 8;
        }
    }
    if (((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_AIMED_BREATH_NORMAL_TRACK_FRAME) && (D_actor_403100_80155808->lowHealth == 0)) {
        D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_TRACK;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_AIMED_BREATH_STOP_FRAME) {
        D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_YAW_ONLY;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_AIMED_BREATH_TRACK_FRAME) {
        breathSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_BREATH_SOUND;
        breathPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]);
        sndEvtRequestScriptStart(breathSound, breathPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]) / 2));
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_AIMED_BREATH_STOP_FRAME) {
        sndEvtRequestScriptStop(ACTOR_403100_BREATH_SOUND, ACTOR_403100_FLAME_CONTACT_STOP_FADE_UPDATES);
    }
    // Emit from the head frame while tracking, then replace breath with recovery smoke.
    if ((u32)(D_actor_403100_80155808->stateFrames - ACTOR_403100_AIMED_BREATH_EMIT_START) < (u32)ACTOR_403100_AIMED_BREATH_EMIT_FRAMES) {
        emissionOffset.vy = -0x1F0;
        emissionOffset.vz = 0x620;
        flameVelocity.vy  = -0x20;
        emissionOffset.vx = 0;
        flameVelocity.vx  = 0;
        flameVelocity.vz  = 0xF0;
        _actor403100EmitFlame(task, &emissionOffset, &flameVelocity, ACTOR_403100_FLAME_COLLISION_ENABLED);
        _actor403100RefreshBreathLight(task);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_AIMED_BREATH_JAW_CUE_FRAME) {
        jawSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_JAW_IMPACT_SOUND;
        jawPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]);
        sndEvtRequestScriptStart(jawSound, jawPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]) / 2));
    }
    if ((u32)(D_actor_403100_80155808->stateFrames - ACTOR_403100_AIMED_BREATH_SMOKE_START) < (u32)ACTOR_403100_RECOVERY_SMOKE_FRAMES) {
        smokeCoord        = &task->extra.tmd->coords[ACTOR_403100_PART_HEAD];
        emissionOffset.vy = -0x140;
        emissionOffset.vx = 0;
        emissionOffset.vz = 0x400;
        effectSpawn(EFFECT_SMOKE_PUFF, smokeCoord, ACTOR_403100_RECOVERY_SMOKE_ARG, &emissionOffset);
    }
    if (_actor403100AnimationAtBoundaryOrJump()) {
        D_actor_403100_80155808->aimMode     = ACTOR_403100_AIM_YAW_ONLY;
        D_actor_403100_80155808->stateFrames = 0;
        D_actor_403100_80155808->subState   += 1;
    }
}
/// Runs the impact and smoke recovery after an aimed flame touched the player.
///
/// Requires the live model and singleton work with stateFrames cleared. Update
/// 49 plays jaw and trunk cues and starts eighteen updates of rumble/shake.
/// Recovery also kicks the head pitch and emits head smoke at updates 50..60.
/// A clip boundary or jump resumes attack approach. Sound ids include the
/// enemy placement index; audio pan/depth are narrowed to signed bytes.
static void _actor403100StepAimedFlameContactRecovery(Task* task)
{
    SVECTOR   smokeOffset;
    s32       jawSound;
    s32       footfallSound;
    s32       jawPan;
    GfxCoord* smokeCoord;
    s32       footfallPan;
    u16       elapsedFrames;

    elapsedFrames                        = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames == ACTOR_403100_IMPACT_CUE_FRAME) {
        jawSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_JAW_IMPACT_SOUND;
        jawPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]);
        sndEvtRequestScriptStart(jawSound, jawPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]) / 2));
        footfallSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_FOOTFALL_SOUND;
        footfallPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]);
        sndEvtRequestScriptStart(footfallSound, footfallPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]) / 2));
        padScriptSpawnVariableMotorRamp(ACTOR_403100_IMPACT_FEEDBACK_FRAMES, ACTOR_403100_FEEDBACK_START_INTENSITY, ACTOR_403100_FEEDBACK_END_INTENSITY);
        D_actor_403100_80155808->shakeFrames    = ACTOR_403100_IMPACT_FEEDBACK_FRAMES;
        D_actor_403100_80155808->headPitchPhase = ACTOR_403100_PITCH_PHASE_START;
    }
    if ((u32)(D_actor_403100_80155808->stateFrames - ACTOR_403100_RECOVERY_SMOKE_START_FRAME) < (u32)ACTOR_403100_RECOVERY_SMOKE_FRAMES) {
        smokeCoord     = &task->extra.tmd->coords[ACTOR_403100_PART_HEAD];
        smokeOffset.vy = -0x140;
        smokeOffset.vx = 0;
        smokeOffset.vz = 0x400;
        effectSpawn(EFFECT_SMOKE_PUFF, smokeCoord, ACTOR_403100_RECOVERY_SMOKE_ARG, &smokeOffset);
    }
    if (_actor403100AnimationAtBoundaryOrJump()) {
        D_actor_403100_80155808->state    = ACTOR_403100_BEHAVIOUR_ATTACK_APPROACH;
        D_actor_403100_80155808->subState = 0;
    }
}
/// Lowers the root for eighteen updates before launching the jump slam.
///
/// Requires live task/model and singleton work with the crouch timer cleared.
/// A stagger or build-up stun interrupts it. Positive room Y points down;
/// the root drops 48 units per update. Completion resets vertical speed and
/// captures the head yaw as the clamped shoulder target.
static void _actor403100CrouchForJumpSlam(Task* task)
{
    enum { ACTOR_403100_JUMP_CROUCH_FRAMES        = 18,
           ACTOR_403100_JUMP_INITIAL_ACCELERATION = 2 };
    u16       subStateBits;
    u16       elapsedFrames;
    u16       headYawBits;
    GfxCoord* rootCoord;

    rootCoord = task->extra.tmd->coords;
    if (_actor403100HandleHitReaction() == 0) {
        rootCoord->coord.t[1]               += 0x30;
        elapsedFrames                        = D_actor_403100_80155808->stateFrames;
        D_actor_403100_80155808->stateFrames = elapsedFrames + 1;
        if ((s16)elapsedFrames >= ACTOR_403100_JUMP_CROUCH_FRAMES - 1) {
            // Keep the launch writes ordered while capturing the next shoulder target.
            subStateBits                                               = D_actor_403100_80155808->subState;
            *(volatile s16*)&D_actor_403100_80155808->jumpAcceleration = 0;
            *(volatile s16*)&D_actor_403100_80155808->jumpAcceleration = ACTOR_403100_JUMP_INITIAL_ACCELERATION;
            headYawBits                                                = (u16) * (volatile s16*)&D_actor_403100_80155808->headAim.vy;
            D_actor_403100_80155808->stateFrames                       = 0;
            D_actor_403100_80155808->jumpSpeed                         = 0;
            D_actor_403100_80155808->armYawTarget                      = (s16)headYawBits;
            D_actor_403100_80155808->subState                          = subStateBits + 1;
            if ((s16)headYawBits >= ACTOR_403100_ARM_YAW_MAX + 1) {
                D_actor_403100_80155808->armYawTarget = ACTOR_403100_ARM_YAW_MAX;
            }
            if (D_actor_403100_80155808->armYawTarget < ACTOR_403100_ARM_YAW_MIN) {
                D_actor_403100_80155808->armYawTarget = ACTOR_403100_ARM_YAW_MIN;
            }
        }
    }
}
/// Integrates one update of the jump's vertical motion in the root-parent frame.
///
/// Borrows the live root and singleton jump fields. Adds `accelerationStep`
/// to acceleration, then acceleration to speed, wrapping each to 16 bits.
/// Signed speed is subtracted from Y because negative Y points up.
static inline void _actor403100IntegrateJumpHeight(GfxCoord* rootCoord, s16 accelerationStep)
{
    u16 jumpSpeedBits;
    u16 jumpAccelerationBits;

    jumpAccelerationBits                      = D_actor_403100_80155808->jumpAcceleration + accelerationStep;
    jumpSpeedBits                             = D_actor_403100_80155808->jumpSpeed + jumpAccelerationBits;
    D_actor_403100_80155808->jumpSpeed        = jumpSpeedBits;
    D_actor_403100_80155808->jumpAcceleration = jumpAccelerationBits;
    rootCoord->coord.t[1]                    -= (s16)jumpSpeedBits;
}

/// Accelerates the jump slam's ascent for six updates while turning the shoulder.
///
/// Requires live task/model and singleton work. Acceleration changes by +4
/// game-coordinate units per update squared; speed and acceleration retain
/// their low halfwords, and signed speed is subtracted from root Y. Shoulder
/// yaw eases by one eighth of the signed 12-bit turn difference. The timer
/// is reset on completion for the next ascent phase.
static void _actor403100AccelerateJumpSlam(Task* task)
{
    enum { ACTOR_403100_JUMP_ACCELERATION_STEP = 4,
           ACTOR_403100_JUMP_PHASE_FRAMES      = 6 };
    GfxCoord* rootCoord;

    rootCoord = task->extra.tmd->coords;
    _actor403100RequestHitPitchKick();
    D_actor_403100_80155808->armYaw =
        (u16)D_actor_403100_80155808->armYaw +
        ((s32)(((u16)D_actor_403100_80155808->armYawTarget - (u16)D_actor_403100_80155808->armYaw) << ACTOR_403100_ARM_SIGNED_ANGLE_SHIFT) >> (ACTOR_403100_ARM_SIGNED_ANGLE_SHIFT + ACTOR_403100_ARM_EASE_SHIFT));
    // Integrate as wrapping halfwords; subtract signed speed because negative Y is up.
    D_actor_403100_80155808->stateFrames += 1;
    _actor403100IntegrateJumpHeight(rootCoord, ACTOR_403100_JUMP_ACCELERATION_STEP);
    if ((s16)D_actor_403100_80155808->stateFrames >= ACTOR_403100_JUMP_PHASE_FRAMES) {
        D_actor_403100_80155808->stateFrames = 0;
        D_actor_403100_80155808->subState   += 1;
    }
}
/// Decelerates the jump slam's ascent for six updates while turning the shoulder.
///
/// Requires live task/model and singleton work. Acceleration changes by -4
/// game-coordinate units per update squared; speed and acceleration retain
/// their low halfwords, and signed speed is subtracted from root Y. Shoulder
/// yaw eases by one eighth of the signed 12-bit turn difference. The timer
/// stays at six for the following descent/contact phase.
static void _actor403100DecelerateJumpSlam(Task* task)
{
    enum { ACTOR_403100_JUMP_ACCELERATION_STEP = 4,
           ACTOR_403100_JUMP_PHASE_FRAMES      = 6 };
    GfxCoord* rootCoord;

    rootCoord = task->extra.tmd->coords;
    _actor403100RequestHitPitchKick();
    D_actor_403100_80155808->armYaw =
        (u16)D_actor_403100_80155808->armYaw +
        ((s32)(((u16)D_actor_403100_80155808->armYawTarget - (u16)D_actor_403100_80155808->armYaw) << ACTOR_403100_ARM_SIGNED_ANGLE_SHIFT) >> (ACTOR_403100_ARM_SIGNED_ANGLE_SHIFT + ACTOR_403100_ARM_EASE_SHIFT));
    // Integrate as wrapping halfwords; subtract signed speed because negative Y is up.
    D_actor_403100_80155808->stateFrames += 1;
    _actor403100IntegrateJumpHeight(rootCoord, -ACTOR_403100_JUMP_ACCELERATION_STEP);
    if ((s16)D_actor_403100_80155808->stateFrames >= ACTOR_403100_JUMP_PHASE_FRAMES) {
        D_actor_403100_80155808->subState += 1;
    }
}
/// Requests the combat footfall sound, controller ramp and vertical screen shake.
///
/// Requires live singleton work/model and an Enemy in task->spawnArg2.pointer.
/// The placement-index nibble selects the sound instance. Lower-trunk pan and
/// half depth truncate to signed bytes. Rumble counts thirty eligible pad-script
/// updates; shake counts thirty actor updates. All pointers remain borrowed.
static inline void _actor403100PlayWalkFootfall(Task* task)
{
    s32 footfallSound;
    s32 audioPan;
    s32 audioDepth;

    padScriptSpawnVariableMotorRamp(ACTOR_403100_FOOTFALL_FEEDBACK_FRAMES, ACTOR_403100_FEEDBACK_START_INTENSITY, ACTOR_403100_FEEDBACK_END_INTENSITY);
    D_actor_403100_80155808->shakeFrames = ACTOR_403100_FOOTFALL_FEEDBACK_FRAMES;
    footfallSound                        = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_FOOTFALL_SOUND;
    audioPan                             = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]);
    audioDepth                           = worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]);
    sndEvtRequestScriptStart(footfallSound, audioPan, (s8)(audioDepth / 2));
}

/// Descends the jump slam, samples arm contacts and signals its landing.
///
/// Requires live singleton work, player and Burner model/enemy. Uses contacts
/// latched by the preceding update before sampling new ones. Acceleration
/// drops by four game units per update squared; signed halfword speed is
/// subtracted from root-parent Y. At Y >= 0, breaks the balcony, plays trunk
/// and arm impact cues with thirty-update rumble/shake, clamps Y to zero and
/// clears the timer before selecting the landed-hit phase.
static void _actor403100DescendJumpSlam(Task* task)
{
    enum {
        ACTOR_403100_ARM_IMPACT_PART = 7,
    };
    enum { ACTOR_403100_JUMP_ACCELERATION_STEP = 4 };
    s32       armImpactSound;
    s32       rootY;
    s32       armImpactPan;
    GfxCoord* rootCoord;

    rootCoord = task->extra.tmd->coords;
    _actor403100RequestHitPitchKick();
    // Consume the preceding contact latches before sampling this falling pose.
    if ((D_actor_403100_80155808->handTouchedPlayer != 0 || D_actor_403100_80155808->forearmTouchedPlayer != 0) && (D_actor_403100_80155808->playerReactionStage == ACTOR_403100_PLAYER_REACTION_NONE)) {
        _actor403100StartPlayerArmHitReaction();
    }
    _actor403100ProbeArmPlayerContact(task);
    D_actor_403100_80155808->stateFrames += 1;
    D_actor_403100_80155808->armYaw =
        (u16)D_actor_403100_80155808->armYaw +
        ((s32)(((u16)D_actor_403100_80155808->armYawTarget - (u16)D_actor_403100_80155808->armYaw) << ACTOR_403100_ARM_SIGNED_ANGLE_SHIFT) >> (ACTOR_403100_ARM_SIGNED_ANGLE_SHIFT + ACTOR_403100_ARM_EASE_SHIFT));
    _actor403100IntegrateJumpHeight(rootCoord, -ACTOR_403100_JUMP_ACCELERATION_STEP);
    rootY = rootCoord->coord.t[1];
    // Clamp to the floor only after playing the separate trunk and arm landing cues.
    if (rootY >= 0) {
        D_actor_403100_80155808->handTouchedPlayer    = 0;
        D_actor_403100_80155808->forearmTouchedPlayer = 0;
        _actor403100BreakBalconyAtArm(task);
        _actor403100PlayWalkFootfall(task);
        armImpactSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_BURNER, 3);
        armImpactPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_ARM_IMPACT_PART]);
        sndEvtRequestScriptStart(armImpactSound, armImpactPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_ARM_IMPACT_PART]) / 2));
        rootCoord->coord.t[1]                = 0;
        D_actor_403100_80155808->stateFrames = 0;
        D_actor_403100_80155808->subState   += 1;
    }
}

/// Handles the jump slam's landed hit window and starts arm recovery at the clip boundary.
///
/// Requires live singleton work and the live player. A latched hand contact
/// through frame 31 starts the player's 23-update held hit reaction and
/// applies attack entry zero once while that reaction is idle. This callback
/// does not sample new contacts; `task` is unused for its dispatch signature.
static void _actor403100StepJumpSlamImpact(Task* task)
{
    enum { ACTOR_403100_SLAM_CONTACT_END_FRAME = 32 };
    u16 elapsedFrames;

    _actor403100RequestHitPitchKick();
    elapsedFrames                        = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = elapsedFrames;
    // A latched hand contact can still damage the player during the early impact pose.
    if (((s16)elapsedFrames < ACTOR_403100_SLAM_CONTACT_END_FRAME) && (D_actor_403100_80155808->handTouchedPlayer != 0) && (D_actor_403100_80155808->playerReactionStage == ACTOR_403100_PLAYER_REACTION_NONE)) {
        _actor403100StartPlayerArmHitReaction();
    }
    if (_actor403100AnimationAtBoundaryOrJump()) {
        D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
        D_actor_403100_80155808->animationId      = ACTOR_403100_ANIMATION_ARM_RECOVERY;
        D_actor_403100_80155808->walkStage        = ACTOR_403100_WALK_APPROACH;
        D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
        D_actor_403100_80155808->subState        += 1;
    }
}
/// Relaxes the shoulder during jump-slam recovery and blends back to the walk clip.
///
/// Requires live singleton work and animation slots. Pitch/yaw ease by one
/// eighth of their signed 12-bit turn offset. At the recovery boundary a
/// single random bit optionally requests a voiced pitch kick, then an eight-
/// frame normal-rate walk blend starts the timed recovery substate. `task`
/// is retained for dispatch only.
static void _actor403100FinishJumpSlamRecovery(Task* task)
{
    enum { ACTOR_403100_JUMP_RECOVERY_BLEND_FRAMES = 8 };
    u32 pitchKickDraw;

    _actor403100RequestHitPitchKick();
    _actor403100RelaxArmPose();
    if (_actor403100AnimationAtBoundaryOrJump()) {
        pitchKickDraw   = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState = pitchKickDraw;
        if ((pitchKickDraw >> 0x10) & 1) {
            _actor403100RequestPitchKick(ACTOR_403100_PITCH_KICK_SELECT_ATTACK_SOUND);
        }
        D_actor_403100_80155808->animationBlendFrames = ACTOR_403100_JUMP_RECOVERY_BLEND_FRAMES;
        D_actor_403100_80155808->animationRate        = ANIMATION_RATE_ONE;
        D_actor_403100_80155808->stateFrames          = 0;
        D_actor_403100_80155808->animationId          = ACTOR_403100_ANIMATION_WALK;
        D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
        D_actor_403100_80155808->subState            += 1;
    }
}
/// Starts the grab-and-squeeze reach with clean arm/contact state.
///
/// Requires live task/model and singleton work. A stagger or build-up stun
/// interrupts initialization. Otherwise restores normal ordering depth,
/// selects the normal-rate reach clip, approaches at 64 units per update and
/// gives head aim back to the animation. The next step samples the grab.
static void _actor403100BeginGrabAndSqueeze(Task* task)
{
    TmdObject*       model;
    Actor403100Work* work;

    model = task->extra.tmd;
    if (_actor403100HandleHitReaction() == 0) {
        model->otOffset                               = 0;
        work                                          = D_actor_403100_80155808;
        D_actor_403100_80155808->handTouchedPlayer    = 0;
        work->walkSpeed                               = 0x40;
        work->animationRate                           = ANIMATION_RATE_ONE;
        work->animationId                             = ACTOR_403100_ANIMATION_GRAB_REACH;
        work->walkStage                               = ACTOR_403100_WALK_APPROACH;
        work->animationRequest                        = ACTOR_403100_ANIMATION_REQUEST_RESET;
        work->aimMode                                 = ACTOR_403100_AIM_ANIMATED;
        work->stateFrames                             = 0;
        work->armPitch                                = 0;
        work->armYaw                                  = 0;
        work->field_632                               = 0;
        D_actor_403100_80155808->forearmTouchedPlayer = 0;
        D_actor_403100_80155808->vulnerable           = 0;
        D_actor_403100_80155808->subState            += 1;
    }
}
/// Samples the grab reach and chooses the held-player or reverse-playback continuation.
///
/// Requires live task/model, singleton work and player. Increments the wrapping
/// halfword timer before processing hits. Through frame 256, hand contact
/// acquires the scripted player hold, enables vulnerability and arms the
/// button prompt; forearm-only contact reverses the clip into substate ten.
/// Animation completion returns to approach even after a contact that update.
static void _actor403100StepGrabAndSqueezeReach(Task* task)
{
    enum { ACTOR_403100_GRAB_AIM_END_FRAME   = 257,
           ACTOR_403100_GRAB_REWIND_SUBSTATE = 10,
           ACTOR_403100_PLAYER_HELD_CLIP     = 1,
           ACTOR_403100_PLAYER_GRABBED_SOUND = SOUND_COMMON(7) };
    s32              grabSoundId;
    s32              audioPan;
    s32              audioDepth;
    Task*            playerTask;
    Actor403100Work* work;

    playerTask                           = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    D_actor_403100_80155808->stateFrames = (u16)(D_actor_403100_80155808->stateFrames + 1);
    if (_actor403100HandleHitReaction() == 0) {
        if ((s16)D_actor_403100_80155808->stateFrames < ACTOR_403100_GRAB_AIM_END_FRAME) {
            _actor403100ProbeArmPlayerContact(task);
            work = D_actor_403100_80155808;
            // Hand contact enters the hold; forearm-only contact rewinds the reach.
            if (work->handTouchedPlayer != 0) {
                work->vulnerable                       = 1;
                D_actor_403100_80155808->holdingPlayer = 1;
                Gp_StateC08.flags                      = (u8)(Gp_StateC08.flags | ATTACHMENT_FLAG_EVENT_LOCK);
                grabSoundId                            = (((u16)((Enemy*)playerTask->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_PLAYER_GRABBED_SOUND;
                audioPan                               = (s8)worldCoordGetOriginAudioPan(&playerTask->extra.tmd->coords[1]);
                audioDepth                             = worldCoordGetOriginAudioDepth(&playerTask->extra.tmd->coords[1]);
                sndEvtRequestScriptStart(grabSoundId, audioPan, (s8)(audioDepth / 2));
                D_actor_403100_80155808->releaseRequested = 0;
                D_actor_403100_80155808->promptPending    = 1;
                _actor403100PlayPlayerAnimation(ACTOR_403100_PLAYER_HELD_CLIP, ANIMATION_MESSAGE_INSTALL_AND_PLAY);
                D_actor_403100_80155808->stateFrames  = 0U;
                D_actor_403100_80155808->stateCounter = 0;
                D_actor_403100_80155808->subState    += 1;
            } else if (work->forearmTouchedPlayer != 0) {
                work->animationRate = -ANIMATION_RATE_ONE;
                work->subState      = ACTOR_403100_GRAB_REWIND_SUBSTATE;
            }
            D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_TRACK;
        } else {
            D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_ANIMATED;
        }
        if (_actor403100AnimationAtBoundaryOrJump()) {
            D_actor_403100_80155808->state    = ACTOR_403100_BEHAVIOUR_ATTACK_APPROACH;
            D_actor_403100_80155808->subState = 0U;
        }
    }
}
/// Finishes lifting the grabbed player and starts the squeeze/button-press cycle.
///
/// Requires live singleton work, enemy, player and the fifteen-part model.
/// Eases root Y to zero by one sixty-fourth per update while retaining
/// the held pose. At the clip boundary, selects view eleven, resets the squeeze
/// pose and timers, and synchronously asks the player for forty button presses.
/// Only the press count of the borrowed request is read by the player.
static void _actor403100BeginHeldPlayerSqueeze(Task* task)
{
    enum { ACTOR_403100_SQUEEZE_VIEW           = 11,
           ACTOR_403100_PLAYER_HELD_CLIP       = 1,
           ACTOR_403100_SQUEEZE_BUTTON_PRESSES = 40 };
    GameActorButtonPressHold pressHold;
    s32                      soundId;
    s32                      audioPan;
    s32                      audioDepth;
    GfxCoord*                rootCoord;
    GfxCoord*                forearmCoord;

    rootCoord    = task->extra.tmd->coords;
    forearmCoord = rootCoord + 6;
    _actor403100DrawHoldForeground(D_actor_403100_80155808->overlayX, D_actor_403100_80155808->overlayY);
    D_actor_403100_80155808->overlayX    = (u16)D_actor_403100_80155808->overlayX - 1;
    D_actor_403100_80155808->overlayY    = (u16)D_actor_403100_80155808->overlayY + 6;
    rootCoord->coord.t[1]               += -rootCoord->coord.t[1] >> 6;
    D_actor_403100_80155808->rotation.vx = 0;
    D_actor_403100_80155808->rotation.vy = ACTOR_TRANSFORM_ANGLE_TURN * 5 / 8;
    D_actor_403100_80155808->rotation.vz = 0;
    _actor403100PlaceHeldPlayer(task);
    // Hand control to the player's struggle counter once the lift clip finishes.
    if (_actor403100AnimationAtBoundaryOrJump()) {
        soundId    = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_BURNER, 12);
        audioPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[4]);
        audioDepth = worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[4]);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)(audioDepth / 2));
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTOR_403100_SQUEEZE_VIEW;
        rootCoord->coord.t[0]                                      = -0x44C;
        rootCoord->coord.t[1]                                      = 0;
        rootCoord->coord.t[2]                                      = 0x1770;
        D_actor_403100_80155808->rotation.vx                       = 0;
        D_actor_403100_80155808->rotation.vy                       = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4;
        D_actor_403100_80155808->rotation.vz                       = 0;
        _actor403100RequestAnimationReset(ACTOR_403100_ANIMATION_SQUEEZE);
        D_actor_403100_80155808->armPitch        = 0xD0;
        D_actor_403100_80155808->armYaw          = -0x350;
        D_actor_403100_80155808->forearmTurn.vx  = -0x110;
        D_actor_403100_80155808->forearmTurn.vy  = 0x290;
        D_actor_403100_80155808->stateFrames     = 0;
        D_actor_403100_80155808->forearmTurn.vz  = 0x60;
        D_actor_403100_80155808->subState       += 1;
        forearmCoord->coord.t[0]                 = -0xBD0;
        D_actor_403100_80155808->armPitch        = 0x30;
        D_actor_403100_80155808->armYaw          = -0xD0;
        D_actor_403100_80155808->forearmTurn.vx  = -0x150;
        D_actor_403100_80155808->forearmTurn.vy  = 0x270;
        D_actor_403100_80155808->forearmTurn.vz  = -0xA0;
        forearmCoord->coord.t[0]                 = -0x1120;
        D_actor_403100_80155808->headBody.radius = 0x500;
        _actor403100ResetHoldCycle(task);
        D_actor_403100_80155808->forearmStrokeDone = 0;
        D_actor_403100_80155808->releaseRequested  = 0;
        D_actor_403100_80155808->promptPending     = 0;
        D_actor_403100_80155808->jawPitchOffset    = 0;
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        _actor403100PlayPlayerAnimation(ACTOR_403100_PLAYER_HELD_CLIP, ANIMATION_MESSAGE_REPLACE_AND_PLAY);
        pressHold.pressCount = ACTOR_403100_SQUEEZE_BUTTON_PRESSES;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &pressHold, 0);
        D_actor_403100_80155808->holdStartHp = D_actor_403100_8015580C->hp;
    }
}
/// Restores the arm's unmodified angles and forearm X for held-player playback.
///
/// Requires live singleton work and model coordinate six. Clears the shoulder
/// and forearm angle offsets (4096 units per turn) and restores forearm X in
/// parent-coordinate units. Retains Y/Z, rotation and the composition stamp;
/// the caller's pose update applies the cleared offsets.
static inline void _actor403100ResetHeldArmPose(GfxCoord* forearmCoord)
{
    enum { ACTOR_403100_HELD_FOREARM_REST_X = -2167 };
    D_actor_403100_80155808->armPitch       = 0;
    D_actor_403100_80155808->armYaw         = 0;
    D_actor_403100_80155808->forearmTurn.vx = 0;
    D_actor_403100_80155808->forearmTurn.vy = 0;
    D_actor_403100_80155808->forearmTurn.vz = 0;
    forearmCoord->coord.t[0]                = ACTOR_403100_HELD_FOREARM_REST_X;
}

/// Steps the held-player squeeze and chooses flame retaliation or a throw.
///
/// Requires live singleton work, player and Burner model/enemy, holdingPlayer set.
/// Probes two squeeze ticks ahead to place the player, restores the arm pose,
/// then advances one visible tick; damage and prompt timers count all three.
/// A completed forty-press prompt delays the next prompt by 180..273 calls.
/// Completion, a 450-frame finished stroke or Burner death ends the hold: at
/// least 60 HP lost with HP remaining selects flame breath, otherwise throw.
/// The prompt payload is borrowed only during synchronous message dispatch.
static void _actor403100StepHeldPlayerSqueeze(Task* task)
{
    enum {
        ACTOR_403100_SQUEEZE_PLAYER_CLIP = 1,
    };
    enum {
        ACTOR_403100_HOLD_BUTTON_PRESSES            = 40,
        ACTOR_403100_SQUEEZE_HIT_CLIP               = 13,
        ACTOR_403100_PLAYER_SCRIPTED_ATTACK_MESSAGE = 1024,
        ACTOR_403100_HOLD_TIMEOUT_FRAMES            = 450,
        ACTOR_403100_HOLD_FLAME_DAMAGE_THRESHOLD    = 60,
        ACTOR_403100_HELD_DEATH_KICK_FRAME          = 60,
        ACTOR_403100_HELD_DEATH_RELEASE_FRAME       = 121,
        ACTOR_403100_JAW_HIT_SOUND                  = 1,
        ACTOR_403100_HOLD_THROW_SUBSTATE            = 8,
    };
    GameActorButtonPressHold pressHold;
    s16                      burnerHp;
    s32                      damageTakenDuringHold;
    u16                      deathFrames;
    u32                      repromptDraw;
    u8                       releaseRequested;
    GfxCoord*                rootCoord;
    GfxCoord*                forearmCoord;

    rootCoord                             = task->extra.tmd->coords;
    forearmCoord                          = rootCoord + 6;
    D_actor_403100_80155808->stateFrames += 1;
    // Sample the grip two ticks ahead, without rolling back damage or prompt timers.
    _actor403100SaveHoldPose(task);
    _actor403100StepPlayerSqueeze(task);
    _actor403100StepPlayerSqueeze(task);
    D_actor_403100_80155808->rotation.vy = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4;
    D_actor_403100_80155808->rotation.vx = 0;
    D_actor_403100_80155808->rotation.vz = 0;
    if (D_actor_403100_80155808->playerKilled == 0) {
        releaseRequested = D_actor_403100_80155808->releaseRequested;
        if ((releaseRequested == 1) && (D_actor_403100_80155808->forearmStrokeDone == releaseRequested)) {
            repromptDraw                           = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState                        = repromptDraw;
            D_actor_403100_80155808->repromptDelay = (((repromptDraw >> 0x10) & 0x1F) + 0x3C) * 3;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ACTOR_403100_PLAYER_SCRIPTED_ATTACK_MESSAGE, 0, 0);
            D_actor_403100_80155808->releaseRequested = 0;
        }
    } else {
        deathFrames                                = D_actor_403100_80155808->playerDeathFrames + 1;
        D_actor_403100_80155808->playerDeathFrames = deathFrames;
        if ((s16)deathFrames == ACTOR_403100_HELD_DEATH_KICK_FRAME) {
            _actor403100RequestPitchKick(ACTOR_403100_PITCH_KICK_SELECT_ATTACK_SOUND);
        }
        if ((s16)D_actor_403100_80155808->playerDeathFrames >= ACTOR_403100_HELD_DEATH_RELEASE_FRAME) {
            gGameSession->suppressDeathChecks = 0;
        }
    }
    _actor403100PlaceHeldPlayer(task);
    _actor403100RestoreHoldPose(task);
    _actor403100StepPlayerSqueeze(task);
    if (D_actor_403100_80155808->hitTaken != 0) {
        if (D_actor_403100_80155808->animationId != ACTOR_403100_SQUEEZE_HIT_CLIP) {
            D_actor_403100_80155808->animationId      = ACTOR_403100_SQUEEZE_HIT_CLIP;
            D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
            D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
            if (D_actor_403100_80155808->jawPitchPhase == ACTOR_403100_PITCH_PHASE_REST) {
                D_actor_403100_80155808->jawKickSound  = ACTOR_403100_JAW_HIT_SOUND;
                D_actor_403100_80155808->jawPitchPhase = ACTOR_403100_PITCH_PHASE_START;
            }
        }
    } else {
        if (_actor403100PreviousAnimationAtBoundaryOrJump()) {
            _actor403100RequestAnimationReset(ACTOR_403100_ANIMATION_SQUEEZE);
        }
    }
    // Reissue the press count synchronously; the player ignores the animation payload.
    if (D_actor_403100_80155808->promptPending != 0) {
        D_actor_403100_80155808->promptPending = 0;
        _actor403100PlayPlayerAnimation(ACTOR_403100_SQUEEZE_PLAYER_CLIP, ANIMATION_MESSAGE_REPLACE_AND_PLAY);
        pressHold.pressCount = ACTOR_403100_HOLD_BUTTON_PRESSES;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &pressHold, 0);
        D_actor_403100_80155808->stateCounter = D_actor_403100_80155808->stateCounter + 1;
    }
    // A finished hold retaliates with flame only after enough Burner HP was lost.
    if ((D_actor_403100_80155808->stateCounter != 0) ||
        ((D_actor_403100_80155808->forearmStrokeDone == 1) &&
         ((s16)D_actor_403100_80155808->stateFrames >= ACTOR_403100_HOLD_TIMEOUT_FRAMES) &&
         (D_actor_403100_80155808->playerKilled == 0)) ||
        (D_actor_403100_8015580C->hp <= 0)) {
        damageTakenDuringHold = D_actor_403100_80155808->holdStartHp - D_actor_403100_8015580C->hp;
        burnerHp              = D_actor_403100_8015580C->hp;
        if ((damageTakenDuringHold >= ACTOR_403100_HOLD_FLAME_DAMAGE_THRESHOLD) && (burnerHp > 0)) {
            D_actor_403100_80155808->stateFrames = 0;
            D_actor_403100_80155808->subState   += 1;
            return;
        }
        _actor403100PlayPlayerAnimation(ACTOR_403100_SQUEEZE_PLAYER_CLIP, ANIMATION_MESSAGE_INSTALL_AND_PLAY);
        D_actor_403100_80155808->stateFrames                       = 0;
        D_actor_403100_80155808->animationRate                     = ANIMATION_RATE_ONE / 2;
        D_actor_403100_80155808->animationId                       = ACTOR_403100_ANIMATION_THROW_HELD_PLAYER;
        D_actor_403100_80155808->animationRequest                  = ACTOR_403100_ANIMATION_REQUEST_RESET;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTOR_403100_HELD_PLAYER_THROW_VIEW;
        rootCoord->coord.t[0]                                      = -0x44C;
        rootCoord->coord.t[2]                                      = 0x1770;
        rootCoord->coord.t[1]                                      = 0;
        D_actor_403100_80155808->rotation.vy                       = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4;
        D_actor_403100_80155808->rotation.vx                       = 0;
        D_actor_403100_80155808->rotation.vz                       = 0;
        _actor403100ResetHeldArmPose(forearmCoord);
        D_actor_403100_80155808->subState = ACTOR_403100_HOLD_THROW_SUBSTATE;
    }
}

/// Poses the held player for thirty-one updates before starting flame breath.
///
/// Requires live singleton work, player, model and initialized flame pool.
/// Eases shoulder/forearm angles and forearm X by one eighth while maintaining
/// the grip. Then switches to view twenty-four and the raised breath placement,
/// resets the arm, and starts the aimed-flame clip with a twenty-update puff
/// lifetime. All old puffs and breath audio are released at that transition.
static void _actor403100PrepareHeldPlayerFlameBreath(Task* task)
{
    enum { ACTOR_403100_HELD_FLAME_PREPARE_FRAMES = 31,
           ACTOR_403100_HELD_FLAME_VIEW           = 24,
           ACTOR_403100_HELD_FLAME_LIFETIME       = 20 };
    s32        forearmX;
    u16        elapsedFrames;
    TmdObject* model;
    GfxCoord*  forearmCoord;
    GfxCoord*  rootCoord;

    model                                   = task->extra.tmd;
    rootCoord                               = model->coords;
    forearmCoord                            = rootCoord + 6;
    D_actor_403100_80155808->armPitch       = (u16)D_actor_403100_80155808->armPitch + ((s32)(-0x2E0 - D_actor_403100_80155808->armPitch) >> 3);
    D_actor_403100_80155808->armYaw         = (u16)D_actor_403100_80155808->armYaw + ((s32)(0x10 - D_actor_403100_80155808->armYaw) >> 3);
    D_actor_403100_80155808->forearmTurn.vx = (u16)D_actor_403100_80155808->forearmTurn.vx + ((s32)(-0x150 - D_actor_403100_80155808->forearmTurn.vx) >> 3);
    D_actor_403100_80155808->forearmTurn.vy = (u16)D_actor_403100_80155808->forearmTurn.vy + ((s32)(0x280 - D_actor_403100_80155808->forearmTurn.vy) >> 3);
    D_actor_403100_80155808->forearmTurn.vz = (u16)D_actor_403100_80155808->forearmTurn.vz + ((s32)(0x140 - D_actor_403100_80155808->forearmTurn.vz) >> 3);
    forearmX                                = forearmCoord->coord.t[0];
    forearmCoord->coord.t[0]                = forearmX + ((-0xBE0 - forearmX) >> 3);
    D_actor_403100_80155808->rotation.vx    = 0;
    D_actor_403100_80155808->rotation.vy    = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4;
    D_actor_403100_80155808->rotation.vz    = 0;
    _actor403100PlaceHeldPlayer(task);
    elapsedFrames                        = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = elapsedFrames;
    // Start the breath from its fixed placement with an empty flame pool.
    if ((s16)elapsedFrames >= ACTOR_403100_HELD_FLAME_PREPARE_FRAMES) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTOR_403100_HELD_FLAME_VIEW;
        rootCoord->coord.t[1]                                      = -0x1388;
        D_actor_403100_80155808->jawPitchOffset                    = 0;
        _actor403100ResetHeldArmPose(forearmCoord);
        D_actor_403100_80155808->animationId      = ACTOR_403100_ANIMATION_AIMED_FLAME;
        D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
        D_actor_403100_80155808->flameLifetime    = ACTOR_403100_HELD_FLAME_LIFETIME;
        D_actor_403100_80155808->headAim.vy       = 0x200;
        D_actor_403100_80155808->stateFrames      = 0;
        D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
        D_actor_403100_80155808->aimMode          = ACTOR_403100_AIM_TRACK;
        D_actor_403100_80155808->headAim.vx       = 0;
        D_actor_403100_80155808->headAim.vz       = 0;
        D_actor_403100_80155810                   = 0;
        D_actor_403100_80155808->subState        += 1;
        rootCoord->coord.t[0]                     = -0x44C;
        rootCoord->coord.t[2]                     = 0x2710;
        rootCoord->coord.t[1]                     = -0x1388;
        D_actor_403100_80155808->rotation.vx      = 0;
        D_actor_403100_80155808->rotation.vy      = ACTOR_TRANSFORM_ANGLE_TURN * 5 / 8;
        D_actor_403100_80155808->rotation.vz      = 0;
        _actor403100ReleaseFlames();
        sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }
}
/// Breathes flame over the held player and applies the retaliation hit.
///
/// Requires live singleton work, player/model/enemy and Burner model/enemy.
/// Emits head-local flame at updates 52..123 and smoke at 125..135; update 100
/// applies attack row four once. Keeps the player bound to the fixed raised
/// grip and draws its foreground. A surviving player advances to recovery at
/// a previous-animation boundary; lethal damage selects clip 6, holds the
/// death sound and releases normal death handling when the death timer reaches 121.
static void _actor403100StepHeldPlayerFlameBreath(Task* task)
{
    enum {
        ACTOR_403100_HELD_BREATH_TRACK_FRAME = 51,
        ACTOR_403100_HELD_BREATH_STOP_FRAME  = 124,
        ACTOR_403100_HELD_BREATH_EMIT_START  = 52,
        ACTOR_403100_HELD_BREATH_EMIT_FRAMES = 72,
        ACTOR_403100_HELD_BREATH_SMOKE_START = 125,
        ACTOR_403100_HELD_FLAME_HIT_VARIANT  = 3,
        ACTOR_403100_HELD_PLAYER_CLIP        = 1,
        ACTOR_403100_HELD_PLAYER_DEATH_CLIP  = 6,
        ACTOR_403100_HELD_FOREGROUND_Y       = 60,
    };
    enum {
        ACTOR_403100_PLAYER_SCRIPTED_ATTACK_MESSAGE = 1024,
        ACTOR_403100_HELD_FLAME_DAMAGE_FRAME        = 100,
        ACTOR_403100_HELD_FLAME_ATTACK_ROW          = 4,
        ACTOR_403100_HELD_FLAME_JAW_CUE_FRAME       = 128,
        ACTOR_403100_HELD_FLAME_TRUNK_CUE_FRAME     = 201,
        ACTOR_403100_HELD_FLAME_DEATH_KICK_FRAME    = 60,
        ACTOR_403100_HELD_FLAME_DEATH_RELEASE_FRAME = 121,
    };
    SVECTOR          emissionOffset;
    SVECTOR          flameVelocity;
    Task*            playerTask;
    Task*            damageTarget;
    s16              deathFrames;
    s32              breathSound;
    s32              fatalPlayerSound;
    s32              hurtPlayerSound;
    s32              jawCueSound;
    s32              trunkCueSound;
    s32              breathPan;
    s32              fatalPlayerPan;
    s32              hurtPlayerPan;
    s32              jawCuePan;
    s32              trunkCuePan;
    u16              elapsedFrames;
    s32              breathDepth;
    s32              fatalPlayerDepth;
    s32              hurtPlayerDepth;
    s32              jawCueDepth;
    s32              trunkCueDepth;
    GfxCoord*        smokeCoords;
    Actor403100Work* work;
    PlayerStatus*    playerStatus;
    GfxCoord*        forearmCoord;
    GfxCoord*        rootCoord;

    playerTask   = *gPlayerActorTasks;
    rootCoord    = task->extra.tmd->coords;
    forearmCoord = rootCoord + 6;
    playerStatus = &gPlayerStatus;
    if (D_actor_403100_80155808->playerKilled == 0) {
        if (D_actor_403100_80155808->releaseRequested == 1) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ACTOR_403100_PLAYER_SCRIPTED_ATTACK_MESSAGE, 0, 0);
            D_actor_403100_80155808->releaseRequested = 0;
        }
    }
    D_actor_403100_80155808->overlayX = 0;
    D_actor_403100_80155808->overlayY = ACTOR_403100_HELD_FOREGROUND_Y;
    _actor403100DrawHoldForeground(D_actor_403100_80155808->overlayX, ACTOR_403100_HELD_FOREGROUND_Y);
    elapsedFrames                        = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames == ACTOR_403100_HELD_BREATH_TRACK_FRAME) {
        D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_TRACK;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_HELD_BREATH_STOP_FRAME) {
        D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_YAW_ONLY;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_HELD_BREATH_TRACK_FRAME) {
        breathSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_BREATH_SOUND;
        breathPan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords + 4);
        breathDepth = worldCoordGetOriginAudioDepth(task->extra.tmd->coords + 4);
        sndEvtRequestScriptStart(breathSound, breathPan, (s8)(breathDepth / 2));
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_HELD_BREATH_STOP_FRAME) {
        sndEvtRequestScriptStop(ACTOR_403100_BREATH_SOUND, ACTOR_403100_FLAME_CONTACT_STOP_FADE_UPDATES);
    }
    if ((u32)(D_actor_403100_80155808->stateFrames - ACTOR_403100_HELD_BREATH_EMIT_START) < (u32)ACTOR_403100_HELD_BREATH_EMIT_FRAMES) {
        emissionOffset.vy = -0x1F0;
        emissionOffset.vz = 0x620;
        flameVelocity.vy  = -0x20;
        emissionOffset.vx = 0;
        flameVelocity.vx  = 0;
        flameVelocity.vz  = 0xE0;
        _actor403100EmitFlame(task, &emissionOffset, &flameVelocity, ACTOR_403100_FLAME_COLLISION_ENABLED);
    }
    if ((u32)(D_actor_403100_80155808->stateFrames - ACTOR_403100_HELD_BREATH_SMOKE_START) < (u32)ACTOR_403100_RECOVERY_SMOKE_FRAMES) {
        smokeCoords       = task->extra.tmd->coords;
        emissionOffset.vy = -0x140;
        emissionOffset.vx = 0;
        emissionOffset.vz = 0x400;
        effectSpawn(EFFECT_SMOKE_PUFF, smokeCoords + 3, ACTOR_403100_RECOVERY_SMOKE_ARG, &emissionOffset);
    }
    // Apply the direct held-player hit once; lethal damage hands playback to the death clip.
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_HELD_FLAME_DAMAGE_FRAME) {
        effectSpawnPlayerBodyHit(EFFECT_PLAYER_BODY_HIT_BLAST, ACTOR_403100_HELD_FLAME_HIT_VARIANT);
        _actor403100PlayPlayerAnimation(ACTOR_403100_HELD_PLAYER_CLIP, ANIMATION_MESSAGE_INSTALL_AND_PLAY);
        damageTarget = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        taskMessageDispatch(damageTarget, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(D_actor_403100_80147614, ACTOR_403100_HELD_FLAME_ATTACK_ROW), 0);
        if (playerStatus->hp <= 0) {
            fatalPlayerSound = (((u16)((Enemy*)playerTask->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY, 11);
            fatalPlayerPan   = (s8)worldCoordGetOriginAudioPan(playerTask->extra.tmd->coords + 1);
            fatalPlayerDepth = worldCoordGetOriginAudioDepth(playerTask->extra.tmd->coords + 1);
            sndEvtRequestScriptStart(fatalPlayerSound, fatalPlayerPan, (s8)(fatalPlayerDepth / 2));
            gGameSession->deathSoundCountdown = GAME_SESSION_DEATH_SOUND_HOLD;
            work                              = D_actor_403100_80155808;
            work->playerKilled                = 1;
            work->playerDeathFrames           = 0;
            gGameSession->suppressDeathChecks = 1;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ACTOR_403100_PLAYER_SCRIPTED_ATTACK_MESSAGE, 0, 0);
            _actor403100PlayPlayerAnimation(ACTOR_403100_HELD_PLAYER_DEATH_CLIP, ANIMATION_MESSAGE_REPLACE_AND_PLAY);
        } else {
            hurtPlayerSound = (((u16)((Enemy*)playerTask->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_COMMON(7);
            hurtPlayerPan   = (s8)worldCoordGetOriginAudioPan(playerTask->extra.tmd->coords + 1);
            hurtPlayerDepth = worldCoordGetOriginAudioDepth(playerTask->extra.tmd->coords + 1);
            sndEvtRequestScriptStart(hurtPlayerSound, hurtPlayerPan, (s8)(hurtPlayerDepth / 2));
        }
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_HELD_FLAME_JAW_CUE_FRAME) {
        jawCueSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_BURNER, 13);
        jawCuePan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords + 4);
        jawCueDepth = worldCoordGetOriginAudioDepth(task->extra.tmd->coords + 4);
        sndEvtRequestScriptStart(jawCueSound, jawCuePan, (s8)(jawCueDepth / 2));
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_HELD_FLAME_TRUNK_CUE_FRAME) {
        trunkCueSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_BURNER, 14);
        trunkCuePan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords + 1);
        trunkCueDepth = worldCoordGetOriginAudioDepth(task->extra.tmd->coords + 1);
        sndEvtRequestScriptStart(trunkCueSound, trunkCuePan, (s8)(trunkCueDepth / 2));
        padScriptSpawnVariableMotorRamp(ACTOR_403100_IMPACT_FEEDBACK_FRAMES, ACTOR_403100_FEEDBACK_START_INTENSITY, ACTOR_403100_FEEDBACK_END_INTENSITY);
        D_actor_403100_80155808->shakeFrames = ACTOR_403100_IMPACT_FEEDBACK_FRAMES;
    }
    // Keep the player in the raised grip until recovery or normal death handling resumes.
    D_actor_403100_80155808->armPitch       = -0x20;
    D_actor_403100_80155808->armYaw         = 0x450;
    D_actor_403100_80155808->forearmTurn.vx = 0x290;
    D_actor_403100_80155808->forearmTurn.vy = 0x210;
    D_actor_403100_80155808->forearmTurn.vz = -0x160;
    forearmCoord->coord.t[0]                = -0xE27;
    rootCoord->coord.t[0]                   = -0x44C;
    rootCoord->coord.t[1]                   = -0x1388;
    rootCoord->coord.t[2]                   = 0x2710;
    D_actor_403100_80155808->rotation.vx    = 0;
    D_actor_403100_80155808->rotation.vy    = 0xA00;
    D_actor_403100_80155808->rotation.vz    = 0;
    _actor403100PlaceHeldPlayer(task);
    if (D_actor_403100_80155808->playerKilled == 0) {
        if (_actor403100PreviousAnimationAtBoundaryOrJump()) {
            _actor403100PlayPlayerAnimation(ACTOR_403100_HELD_PLAYER_CLIP, ANIMATION_MESSAGE_INSTALL_AND_PLAY);
            D_actor_403100_80155808->stateFrames = 0;
            D_actor_403100_80155808->aimMode     = ACTOR_403100_AIM_TRACK;
            D_actor_403100_80155808->overlayY    = ACTOR_403100_HELD_FOREGROUND_Y;
            D_actor_403100_80155808->subState   += 1;
        }
    } else {
        deathFrames                                = (u16)D_actor_403100_80155808->playerDeathFrames + 1;
        D_actor_403100_80155808->playerDeathFrames = deathFrames;
        if (deathFrames == ACTOR_403100_HELD_FLAME_DEATH_KICK_FRAME) {
            _actor403100RequestPitchKick(ACTOR_403100_PITCH_KICK_KEEP_SOUND);
        }
        if (D_actor_403100_80155808->playerDeathFrames >= ACTOR_403100_HELD_FLAME_DEATH_RELEASE_FRAME) {
            gGameSession->suppressDeathChecks = 0;
        }
    }
}
/// Returns the held player from flame breath to the half-speed throw clip.
///
/// Requires live singleton work, player and the fifteen-part model.
/// For thirty-one updates, moves the foreground and eases root Y, shoulder
/// angles and forearm pose. Each angle is sign-extended from twelve turn bits
/// into a four-fractional-bit value before taking one eighth of the target
/// difference; stores retain the low halfword. The transition selects view
/// twelve and resets the arm at the ground-level throw placement.
static void _actor403100RecoverHeldPlayerFromFlameBreath(Task* task)
{
    enum { ACTOR_403100_HELD_FLAME_RECOVERY_FRAMES = 31,
           ACTOR_403100_HELD_POSE_ANGLE_SCALE      = 16,
           ACTOR_403100_HELD_POSE_STEP_SHIFT       = 7 };
    s16       foregroundY;
    s32       rootY;
    s32       forearmX;
    u16       armPitchBits;
    u16       forearmPitchBits;
    u16       forearmRollBits;
    u16       armYawBits;
    u16       forearmYawBits;
    u16       elapsedFrames;
    GfxCoord* forearmCoord;
    GfxCoord* rootCoord;

    rootCoord                         = task->extra.tmd->coords;
    D_actor_403100_80155808->overlayX = (u16)D_actor_403100_80155808->overlayX - 1;
    foregroundY                       = (u16)D_actor_403100_80155808->overlayY + 6;
    D_actor_403100_80155808->overlayY = foregroundY;
    _actor403100DrawHoldForeground(D_actor_403100_80155808->overlayX, foregroundY);
    forearmCoord          = rootCoord + 6;
    rootY                 = rootCoord->coord.t[1];
    rootCoord->coord.t[1] = rootY + (-rootY >> 6);
    // Sign-extend each twelve-bit angle before easing toward the recovery pose.
    armPitchBits                            = (u16)D_actor_403100_80155808->armPitch;
    armYawBits                              = (u16)D_actor_403100_80155808->armYaw;
    D_actor_403100_80155808->armPitch       = armPitchBits + ((s32)(-0x1100 - (s16)(armPitchBits * ACTOR_403100_HELD_POSE_ANGLE_SCALE)) >> ACTOR_403100_HELD_POSE_STEP_SHIFT);
    forearmPitchBits                        = (u16)D_actor_403100_80155808->forearmTurn.vx;
    D_actor_403100_80155808->armYaw         = armYawBits + ((s32)(0x4E00 - (s16)(armYawBits * ACTOR_403100_HELD_POSE_ANGLE_SCALE)) >> ACTOR_403100_HELD_POSE_STEP_SHIFT);
    forearmYawBits                          = (u16)D_actor_403100_80155808->forearmTurn.vy;
    D_actor_403100_80155808->forearmTurn.vx = forearmPitchBits + ((s32)(0x2400 - (s16)(forearmPitchBits * ACTOR_403100_HELD_POSE_ANGLE_SCALE)) >> ACTOR_403100_HELD_POSE_STEP_SHIFT);
    forearmRollBits                         = (u16)D_actor_403100_80155808->forearmTurn.vz;
    D_actor_403100_80155808->forearmTurn.vy = forearmYawBits + ((s32)(-0x1D00 - (s16)(forearmYawBits * ACTOR_403100_HELD_POSE_ANGLE_SCALE)) >> ACTOR_403100_HELD_POSE_STEP_SHIFT);
    D_actor_403100_80155808->forearmTurn.vz = forearmRollBits + ((s32)(0x2E00 - (s16)(forearmRollBits * ACTOR_403100_HELD_POSE_ANGLE_SCALE)) >> ACTOR_403100_HELD_POSE_STEP_SHIFT);
    forearmX                                = forearmCoord->coord.t[0];
    forearmCoord->coord.t[0]                = forearmX + ((-0x807 - forearmX) >> 3);
    _actor403100PlaceHeldPlayer(task);
    elapsedFrames                        = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames >= ACTOR_403100_HELD_FLAME_RECOVERY_FRAMES) {
        D_actor_403100_80155808->animationRate                     = ANIMATION_RATE_ONE / 2;
        D_actor_403100_80155808->animationId                       = ACTOR_403100_ANIMATION_THROW_HELD_PLAYER;
        D_actor_403100_80155808->animationRequest                  = ACTOR_403100_ANIMATION_REQUEST_RESET;
        D_actor_403100_80155808->stateFrames                       = 0;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTOR_403100_HELD_PLAYER_THROW_VIEW;
        rootCoord->coord.t[0]                                      = -0x44C;
        rootCoord->coord.t[2]                                      = 0x1770;
        rootCoord->coord.t[1]                                      = 0;
        D_actor_403100_80155808->rotation.vy                       = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4;
        D_actor_403100_80155808->rotation.vx                       = 0;
        D_actor_403100_80155808->rotation.vz                       = 0;
        _actor403100ResetHeldArmPose(forearmCoord);
        D_actor_403100_80155808->subState += 1;
    }
}
/// Releases the squeezed player into the balcony throw and applies its damage.
///
/// Requires live singleton work, enemy, player and the fifteen-part model.
/// Sounds at update sixty; update sixty-eight places the player at
/// the impact, applies attack row five, and blends the Burner into recovery.
/// A lethal player result selects the death clip and suppresses death checks;
/// if the Burner also has no HP, its HP is restored to 1000 for this sequence.
static void _actor403100ThrowHeldPlayer(Task* task)
{
    enum { ACTOR_403100_HELD_THROW_SOUND_FRAME   = 60,
           ACTOR_403100_HELD_THROW_RELEASE_FRAME = 68,
           ACTOR_403100_HELD_THROW_BLEND_FRAMES  = 20,
           ACTOR_403100_HELD_THROW_IMPACT_VIEW   = 23,
           ACTOR_403100_HELD_THROW_ATTACK_ROW    = 5,
           ACTOR_403100_PLAYER_THROWN_CLIP       = 2,
           ACTOR_403100_PLAYER_DEATH_CLIP        = 7,
           ACTOR_403100_PLAYER_LETHAL_HIT_STATE  = 10 };
    GameActor* playerActor;
    Task*      playerTask;
    s16        playerClipId;
    s16        animationMessageId;
    s32        soundId;
    s32        audioPan;
    u16        elapsedFrames;
    s32        audioDepth;

    playerActor                          = (*gPlayerActorTasks)->work;
    elapsedFrames                        = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames == ACTOR_403100_HELD_THROW_SOUND_FRAME) {
        soundId    = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_BURNER, 15);
        audioPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[8]);
        audioDepth = worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[8]);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)(audioDepth / 2));
    }
    D_actor_403100_80155808->rotation.vx = 0;
    D_actor_403100_80155808->rotation.vy = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4;
    D_actor_403100_80155808->rotation.vz = 0;
    _actor403100PlaceHeldPlayer(task);
    // Move the player out of the grip before choosing the damaged or death clip.
    if ((s16)D_actor_403100_80155808->stateFrames >= ACTOR_403100_HELD_THROW_RELEASE_FRAME) {
        D_actor_403100_80155808->animationBlendFrames              = ACTOR_403100_HELD_THROW_BLEND_FRAMES;
        D_actor_403100_80155808->animationRate                     = ANIMATION_RATE_ONE;
        D_actor_403100_80155808->animationId                       = ACTOR_403100_ANIMATION_ARM_RECOVERY;
        D_actor_403100_80155808->animationRequest                  = ACTOR_403100_ANIMATION_REQUEST_BLEND;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTOR_403100_HELD_THROW_IMPACT_VIEW;
        D_actor_403100_80155808->subState                         += 1;
        _actor403100PlacePlayer(-0x1BBC, -0xC80, -0x4B0, ACTOR_TRANSFORM_ANGLE_TURN / 4);
        playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        if (taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(D_actor_403100_80147614, ACTOR_403100_HELD_THROW_ATTACK_ROW), 0) != 0) {
            gGameSession->suppressDeathChecks = 1;
            playerClipId                      = ACTOR_403100_PLAYER_DEATH_CLIP;
            if (D_actor_403100_8015580C->hp <= 0) {
                D_actor_403100_8015580C->hp = 0x3E8;
            }
            animationMessageId                    = ANIMATION_MESSAGE_REPLACE_AND_PLAY;
            D_actor_403100_80155808->playerKilled = 1;
            playerActor->state                    = ACTOR_403100_PLAYER_LETHAL_HIT_STATE;
        } else {
            playerClipId       = ACTOR_403100_PLAYER_THROWN_CLIP;
            animationMessageId = ANIMATION_MESSAGE_INSTALL_AND_PLAY;
        }
        _actor403100PlayPlayerAnimation(playerClipId, animationMessageId);
        D_actor_403100_80155808->stateFrames = 0;
    }
}
/// Finishes the held-player throw with balcony damage and control restoration.
///
/// Requires live singleton work, player model/enemy and balcony section sprites;
/// task is unused for its callback signature. Update 11 damages section 1
/// (or selects state 2 on repeats), plays impact audio and eight-update feedback;
/// 28 plays recovery audio. When surviving player playback ends, releases
/// scripted control, restores the normal head radius and resumes approach.
/// For a killed player, normal death checks resume from update 30.
static void _actor403100FinishHeldPlayerThrow(Task* task)
{
    enum {
        ACTOR_403100_THROW_IMPACT_FRAME           = 11,
        ACTOR_403100_THROW_RECOVERY_SOUND_FRAME   = 28,
        ACTOR_403100_THROW_DEATH_RELEASE_FRAME    = 30,
        ACTOR_403100_THROW_RETURN_VIEW            = 6,
        ACTOR_403100_NORMAL_HEAD_RADIUS           = 1024,
        ACTOR_403100_THROW_SECTION                = 1,
        ACTOR_403100_THROW_SECTION_REPEATED_STATE = 2,
        ACTOR_403100_THROW_FEEDBACK_FRAMES        = 8,
    };
    Task*            playerTask;
    Actor403100Work* work;
    s32              impactSound;
    s32              recoverySound;
    s32              impactPan;
    s32              recoveryPan;
    u16              elapsedFrames;
    s32              impactDepth;
    s32              recoveryDepth;

    playerTask                           = *gPlayerActorTasks;
    elapsedFrames                        = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames == ACTOR_403100_THROW_IMPACT_FRAME) {
        if (D_actor_403100_80155808->sectionDamaged[ACTOR_403100_THROW_SECTION] == 0) {
            dryfieldNightMotelBalconySetSectionState(ACTOR_403100_THROW_SECTION, ACTOR_403100_BALCONY_SECTION_DAMAGED);
            D_actor_403100_80155808->sectionDamaged[ACTOR_403100_THROW_SECTION] = 1;
        } else {
            dryfieldNightMotelBalconySetSectionState(ACTOR_403100_THROW_SECTION, ACTOR_403100_THROW_SECTION_REPEATED_STATE);
        }
        padScriptSpawnVariableMotorRamp(ACTOR_403100_THROW_FEEDBACK_FRAMES, ACTOR_403100_FEEDBACK_START_INTENSITY, ACTOR_403100_FEEDBACK_END_INTENSITY);
        D_actor_403100_80155808->shakeFrames = ACTOR_403100_THROW_FEEDBACK_FRAMES;
        impactSound                          = (((u16)((Enemy*)(playerTask)->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_BURNER, 6);
        impactPan                            = (s8)worldCoordGetOriginAudioPan(playerTask->extra.tmd->coords + 1);
        impactDepth                          = worldCoordGetOriginAudioDepth(playerTask->extra.tmd->coords + 1);
        sndEvtRequestScriptStart(impactSound, impactPan, (s8)(impactDepth / 2));
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_THROW_RECOVERY_SOUND_FRAME) {
        recoverySound = (((u16)((Enemy*)(playerTask)->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_BURNER, 7);
        recoveryPan   = (s8)worldCoordGetOriginAudioPan(playerTask->extra.tmd->coords + 1);
        recoveryDepth = worldCoordGetOriginAudioDepth(playerTask->extra.tmd->coords + 1);
        sndEvtRequestScriptStart(recoverySound, recoveryPan, (s8)(recoveryDepth / 2));
    }
    D_actor_403100_80155808->releaseRequested = 0;
    if (D_actor_403100_80155808->playerKilled == 0) {
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
            if (D_actor_403100_8015580C->hp > 0) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTOR_403100_THROW_RETURN_VIEW;
            }
            D_actor_403100_80155808->field_632         = 0;
            D_actor_403100_80155808->handTouchedPlayer = 0;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            work                                   = D_actor_403100_80155808;
            work->vulnerable                       = 0;
            work->headBody.radius                  = ACTOR_403100_NORMAL_HEAD_RADIUS;
            D_actor_403100_80155808->holdingPlayer = 0;
            D_actor_403100_80155808->state         = ACTOR_403100_BEHAVIOUR_ATTACK_APPROACH;
            D_actor_403100_80155808->subState      = 0;
        }
    } else if ((s16)D_actor_403100_80155808->stateFrames >= ACTOR_403100_THROW_DEATH_RELEASE_FRAME) {
        gGameSession->suppressDeathChecks = 0;
    }
}
/// Begins the side flame sweep with a fresh full-pool puff lifetime.
///
/// Requires live singleton work, model and initialized flame pool. Sets ordering
/// depth to 31, clears the breath interrupt latch, approaches at 48 units per
/// update and gives puffs a 28-update lifetime. Releases old puffs and breath
/// audio before restarting the normal-rate flame clip and advancing the step.
static void _actor403100BeginSideFlameAttack(Task* task)
{
    Actor403100Work* work;

    task->extra.tmd->otOffset                  = 0x1F;
    work                                       = *(Actor403100Work* volatile*)&D_actor_403100_80155808;
    (*(volatile s16*)&D_actor_403100_80155810) = 0;
    work->walkSpeed                            = 0x30;
    work->walkStage                            = ACTOR_403100_WALK_APPROACH;
    work->flameLifetime                        = ARRAY_SIZE(D_actor_403100_80155814);
    _actor403100ReleaseFlames();
    sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    _actor403100RequestAnimationReset(ACTOR_403100_ANIMATION_AIMED_FLAME);
    D_actor_403100_80155808->stateFrames = 0;
    D_actor_403100_80155808->subState   += 1;
}
/// Sweeps a damaging flame breath along the side of the balcony.
///
/// Requires live singleton work/flame pool and the aimed-flame clip, with the
/// step timer cleared. The world-space aim oscillates along Z at fixed X/Y;
/// emission offsets and velocity are head-local game units and units per update.
/// Updates 52..123 emit flame, and 125..135 emit smoke. A detected flame touch
/// selects contact recovery when its cooldown reaches 1; otherwise a clip boundary
/// advances to the normal recovery wait. The frame counter wraps as a halfword.
static void _actor403100StepSideFlameBreath(Task* task)
{
    enum {
        ACTOR_403100_SIDE_BREATH_AIM_FRAME   = 51,
        ACTOR_403100_SIDE_BREATH_STOP_FRAME  = 124,
        ACTOR_403100_SIDE_BREATH_EMIT_START  = 52,
        ACTOR_403100_SIDE_BREATH_EMIT_FRAMES = 72,
        ACTOR_403100_SIDE_BREATH_SMOKE_START = 125,
    };
    SVECTOR   emissionOffset;
    SVECTOR   flameVelocity;
    s32       breathSound;
    s32       audioPan;
    s32       audioDepth;
    GfxCoord* smokeCoord;

    // A puff touched the player; wait until the final cooldown update to recover.
    if (D_actor_403100_80155810 == 1) {
        D_actor_403100_80155808->subState = 3;
        return;
    }
    D_actor_403100_80155808->aimTarget.vx = -0x3110;
    D_actor_403100_80155808->stateFrames += 1;
    D_actor_403100_80155808->aimTarget.vy = -0xC80;
    D_actor_403100_80155808->aimTarget.vz = ((s32)(rsin((s16)D_actor_403100_80155808->stateFrames << 5) * 0x10) >> 6) + 0x6DB;
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_SIDE_BREATH_AIM_FRAME) {
        D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_TRACK;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_SIDE_BREATH_STOP_FRAME) {
        D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_YAW_ONLY;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_SIDE_BREATH_AIM_FRAME) {
        breathSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_BREATH_SOUND;
        audioPan    = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]);
        audioDepth  = worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]);
        sndEvtRequestScriptStart(breathSound, audioPan, (s8)(audioDepth / 2));
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_SIDE_BREATH_STOP_FRAME) {
        sndEvtRequestScriptStop(ACTOR_403100_BREATH_SOUND, ACTOR_403100_FLAME_CONTACT_STOP_FADE_UPDATES);
    }
    if ((u32)(D_actor_403100_80155808->stateFrames - ACTOR_403100_SIDE_BREATH_EMIT_START) < (u32)ACTOR_403100_SIDE_BREATH_EMIT_FRAMES) {
        emissionOffset.vy = -0x1F0;
        emissionOffset.vz = 0x620;
        flameVelocity.vy  = -0x20;
        emissionOffset.vx = 0;
        flameVelocity.vx  = 0;
        flameVelocity.vz  = 0xE0;
        _actor403100EmitFlame(task, &emissionOffset, &flameVelocity, ACTOR_403100_FLAME_COLLISION_ENABLED);
    }
    if ((u32)(D_actor_403100_80155808->stateFrames - ACTOR_403100_SIDE_BREATH_SMOKE_START) < (u32)ACTOR_403100_RECOVERY_SMOKE_FRAMES) {
        smokeCoord        = &task->extra.tmd->coords[ACTOR_403100_PART_HEAD];
        emissionOffset.vy = -0x140;
        emissionOffset.vx = 0;
        emissionOffset.vz = 0x400;
        effectSpawn(EFFECT_SMOKE_PUFF, smokeCoord, ACTOR_403100_RECOVERY_SMOKE_ARG, &emissionOffset);
    }
    if (_actor403100AnimationAtBoundaryOrJump()) {
        D_actor_403100_80155808->stateFrames = 0;
        D_actor_403100_80155808->subState   += 1;
    }
}
/// Runs the impact and smoke recovery after the side flame touched the player.
///
/// Requires the live model and singleton work with stateFrames cleared. Update
/// 49 plays jaw and trunk cues and starts eighteen updates of rumble/shake.
/// Recovery also kicks the head pitch and emits head smoke at updates 50..60.
/// A clip boundary or jump resumes attack approach. Sound ids include the
/// enemy placement index; audio pan/depth are narrowed to signed bytes.
static void _actor403100StepSideFlameContactRecovery(Task* task)
{
    SVECTOR   smokeOffset;
    s32       jawSound;
    s32       footfallSound;
    s32       jawPan;
    GfxCoord* smokeCoord;
    s32       footfallPan;
    u16       elapsedFrames;

    elapsedFrames                        = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames == ACTOR_403100_IMPACT_CUE_FRAME) {
        jawSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_JAW_IMPACT_SOUND;
        jawPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]);
        sndEvtRequestScriptStart(jawSound, jawPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]) / 2));
        footfallSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_FOOTFALL_SOUND;
        footfallPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]);
        sndEvtRequestScriptStart(footfallSound, footfallPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_LOWER_TRUNK]) / 2));
        D_actor_403100_80155808->headPitchPhase = ACTOR_403100_PITCH_PHASE_START;
        padScriptSpawnVariableMotorRamp(ACTOR_403100_IMPACT_FEEDBACK_FRAMES, ACTOR_403100_FEEDBACK_START_INTENSITY, ACTOR_403100_FEEDBACK_END_INTENSITY);
        D_actor_403100_80155808->shakeFrames = ACTOR_403100_IMPACT_FEEDBACK_FRAMES;
    }
    if ((u32)(D_actor_403100_80155808->stateFrames - ACTOR_403100_RECOVERY_SMOKE_START_FRAME) < (u32)ACTOR_403100_RECOVERY_SMOKE_FRAMES) {
        smokeCoord     = &task->extra.tmd->coords[ACTOR_403100_PART_HEAD];
        smokeOffset.vy = -0x140;
        smokeOffset.vx = 0;
        smokeOffset.vz = 0x400;
        effectSpawn(EFFECT_SMOKE_PUFF, smokeCoord, ACTOR_403100_RECOVERY_SMOKE_ARG, &smokeOffset);
    }
    if (_actor403100AnimationAtBoundaryOrJump()) {
        D_actor_403100_80155808->state    = ACTOR_403100_BEHAVIOUR_ATTACK_APPROACH;
        D_actor_403100_80155808->subState = 0;
    }
}
/// Samples the grab-and-drag reach while turning toward negative quarter-turn yaw.
///
/// Requires live singleton work, player and model coordinates through part
/// seven. Eases the signed twelve-bit yaw error by one quarter. The previous
/// wrapping halfword timer selects contacts at 51..72; outside that window,
/// a hit reaction interrupts the step. Either latched arm contact locks the
/// player into the drag clip, offsets it 3000 units along Z, suppresses view
/// triggers and advances. A completed miss returns to attack approach.
static void _actor403100StepGrabAndDragReach(Task* task)
{
    enum { ACTOR_403100_DRAG_YAW_SCALE           = 16,
           ACTOR_403100_DRAG_SIGNED_YAW_SHIFT    = 16,
           ACTOR_403100_DRAG_YAW_STEP_SHIFT      = 22,
           ACTOR_403100_FRAME_COUNTER_MASK       = 0xFFFF,
           ACTOR_403100_DRAG_CONTACT_START_FRAME = 51,
           ACTOR_403100_DRAG_CONTACT_FRAMES      = 22,
           ACTOR_403100_DRAG_CONTACT_SOUND_FRAME = 68,
           ACTOR_403100_PLAYER_DRAG_CLIP         = 3 };
    s32 soundId;
    s32 audioPan;
    u16 rootYawBits;
    u16 previousFrames;
    s32 audioDepth;

    rootYawBits                          = (u16)D_actor_403100_80155808->rotation.vy;
    previousFrames                       = D_actor_403100_80155808->stateFrames;
    D_actor_403100_80155808->rotation.vy = rootYawBits + ((s32)(((-ACTOR_TRANSFORM_ANGLE_TURN / 4 * ACTOR_403100_DRAG_YAW_SCALE) - (rootYawBits * ACTOR_403100_DRAG_YAW_SCALE)) << ACTOR_403100_DRAG_SIGNED_YAW_SHIFT) >> ACTOR_403100_DRAG_YAW_STEP_SHIFT);
    D_actor_403100_80155808->stateFrames = previousFrames + 1;
    // Contact frames use the pre-increment timer; hit interruptions use the rest.
    if ((u32)((previousFrames - ACTOR_403100_DRAG_CONTACT_START_FRAME) & ACTOR_403100_FRAME_COUNTER_MASK) < (u32)ACTOR_403100_DRAG_CONTACT_FRAMES) {
        _actor403100ProbeArmPlayerContact(task);
    } else if (_actor403100HandleHitReaction() != 0) {
        return;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_DRAG_CONTACT_SOUND_FRAME) {
        soundId    = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_BURNER, 8);
        audioPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[7]);
        audioDepth = worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[7]);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)(audioDepth / 2));
    }
    if (D_actor_403100_80155808->handTouchedPlayer != 0 || D_actor_403100_80155808->forearmTouchedPlayer != 0) {
        D_actor_403100_80155808->vulnerable    = 1;
        D_actor_403100_80155808->holdingPlayer = 1;
        Gp_StateC08.flags                     |= ATTACHMENT_FLAG_EVENT_LOCK;
        _actor403100PlayPlayerAnimation(ACTOR_403100_PLAYER_DRAG_CLIP, ANIMATION_MESSAGE_INSTALL_AND_PLAY);
        _actor403100PlacePlayer(D_actor_403100_80155808->playerPosition.vx, D_actor_403100_80155808->playerPosition.vy, (s16)((u16)D_actor_403100_80155808->playerPosition.vz + 0xBB8), ACTOR_TRANSFORM_ANGLE_HALF_TURN);
        D_actor_403100_80155808->handTouchedPlayer = 0;
        D_actor_403100_80155808->stateFrames       = 0;
        gGameSession->suppressViewTriggers         = 1;
        D_actor_403100_80155808->subState         += 1;
        return;
    }
    if (_actor403100AnimationAtBoundaryOrJump()) {
        D_actor_403100_80155808->handTouchedPlayer = 0;
        D_actor_403100_80155808->state             = ACTOR_403100_BEHAVIOUR_ATTACK_APPROACH;
        D_actor_403100_80155808->subState          = 0;
    }
}
/// Drags the held player forward and hands off to the impact reaction.
///
/// Requires a live player task/GameActor, singleton Burner work, and a cleared
/// step timer. The first five updates add 1000 world Z units to the sampled player
/// placement. At the seventh callback, or once sampled Z reaches 7000, applies
/// the actor's drag attack and places the player at the fixed impact position.
/// A lethal hit suppresses normal death handling and selects scripted death
/// playback; a survivor gets the recovery clip. The callback task is unused.
static void _actor403100StepPlayerDrag(Task* unusedTask)
{
    enum {
        ACTOR_403100_PLAYER_SCRIPTED_ATTACK_STATE = 10,
        ACTOR_403100_DRAG_RESULT_VIEW             = 20,
        ACTOR_403100_PLAYER_DRAG_DEATH_CLIP       = 7,
        ACTOR_403100_PLAYER_DRAG_RECOVERY_CLIP    = 2,
        ACTOR_403100_DRAG_RUMBLE_FRAMES           = 12,
        ACTOR_403100_DRAG_RUMBLE_END_INTENSITY    = 128,
        ACTOR_403100_DRAG_PLACEMENT_FRAMES        = 5,
        ACTOR_403100_DRAG_LAST_UPDATE             = 6,
        ACTOR_403100_DRAG_FINISH_Z                = 7000,
        ACTOR_403100_DRAG_STEP_DISTANCE           = 1000,
        ACTOR_403100_PLAYER_DRAG_SOUND            = 7,
        ACTOR_403100_DRAG_ATTACK_ROW              = 2,
    };
    s32              playerClip;
    s32              animationMessage;
    s32              dragSound;
    Task*            damageRecipientTask;
    s32              dragFinished;
    s32              audioPan;
    s32              audioDepth;
    Task*            playerTask;
    GameActor*       playerWork;
    u8               finishedByte;
    Actor403100Work* burnerWork;

    playerTask   = *gPlayerActorTasks;
    playerWork   = playerTask->work;
    dragFinished = 0;
    if ((s16)D_actor_403100_80155808->stateFrames == 0) {
        padScriptSpawnVariableMotorRamp(ACTOR_403100_DRAG_RUMBLE_FRAMES, ACTOR_403100_FEEDBACK_START_INTENSITY, ACTOR_403100_DRAG_RUMBLE_END_INTENSITY);
        dragSound  = (((u16)((Enemy*)playerTask->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_403100_PLAYER_DRAG_SOUND;
        audioPan   = (s8)worldCoordGetOriginAudioPan(&playerTask->extra.tmd->coords[1]);
        audioDepth = worldCoordGetOriginAudioDepth(&playerTask->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(dragSound, audioPan, (s8)(audioDepth / 2));
    }
    burnerWork = D_actor_403100_80155808;
    if ((s16)burnerWork->stateFrames < ACTOR_403100_DRAG_PLACEMENT_FRAMES) {
        _actor403100PlacePlayer(burnerWork->playerPosition.vx, burnerWork->playerPosition.vy, (s16)(burnerWork->playerPosition.vz + ACTOR_403100_DRAG_STEP_DISTANCE), ACTOR_TRANSFORM_ANGLE_HALF_TURN);
    }
    if (((s16)D_actor_403100_80155808->stateFrames >= ACTOR_403100_DRAG_LAST_UPDATE) || (D_actor_403100_80155808->playerPosition.vz >= ACTOR_403100_DRAG_FINISH_Z)) {
        dragFinished = 1;
    }
    D_actor_403100_80155808->stateFrames = (s16)((u16)D_actor_403100_80155808->stateFrames + 1);
    // Keep the byte-valued completion copy used across the damage/animation handoff.
    if ((finishedByte = dragFinished != 0)) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTOR_403100_DRAG_RESULT_VIEW;
        damageRecipientTask                                        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        if (taskMessageDispatch(damageRecipientTask, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(D_actor_403100_80147614, ACTOR_403100_DRAG_ATTACK_ROW), 0) != 0) {
            gGameSession->suppressDeathChecks     = 1;
            gGameSession->deathSoundCountdown     = GAME_SESSION_DEATH_SOUND_HOLD;
            D_actor_403100_80155808->playerKilled = 1U;
        }
        _actor403100PlacePlayer(-0x1928, -0xC7C, 0x29D6, ACTOR_TRANSFORM_ANGLE_HALF_TURN);
        if (D_actor_403100_80155808->playerKilled != 0) {
            playerWork->state = ACTOR_403100_PLAYER_SCRIPTED_ATTACK_STATE;
            playerClip        = ACTOR_403100_PLAYER_DRAG_DEATH_CLIP;
            animationMessage  = ANIMATION_MESSAGE_REPLACE_AND_PLAY;
        } else {
            playerClip       = ACTOR_403100_PLAYER_DRAG_RECOVERY_CLIP;
            animationMessage = ANIMATION_MESSAGE_INSTALL_AND_PLAY;
        }
        _actor403100PlayPlayerAnimation(playerClip, animationMessage);
        D_actor_403100_80155808->stateFrames = 0;
        D_actor_403100_80155808->subState    = (u16)(D_actor_403100_80155808->subState + 1);
    }
}
/// Finishes the player drag with balcony damage, reaction audio and control release.
///
/// Requires live singleton work, player model/enemy and balcony section sprites;
/// task is unused for its callback signature. Update 10 damages section 0
/// (or selects state 2 on repeats); 11 plays impact audio and eighteen-update
/// feedback, 28 plays recovery audio and 32 sounds a fatal hit if needed.
/// Surviving player playback completion restores view 4 if Burner still lives,
/// re-enables view triggers and resumes approach; fatal death checks resume at 50.
static void _actor403100FinishPlayerDrag(Task* task)
{
    enum {
        ACTOR_403100_DRAG_SECTION_FRAME          = 10,
        ACTOR_403100_DRAG_IMPACT_FRAME           = 11,
        ACTOR_403100_DRAG_RECOVERY_SOUND_FRAME   = 28,
        ACTOR_403100_DRAG_FATAL_SOUND_FRAME      = 32,
        ACTOR_403100_DRAG_DEATH_RELEASE_FRAME    = 50,
        ACTOR_403100_DRAG_RETURN_VIEW            = 4,
        ACTOR_403100_DRAG_SECTION                = 0,
        ACTOR_403100_DRAG_SECTION_REPEATED_STATE = 2,
    };
    Task* playerTask;
    s32   impactSound;
    s32   recoverySound;
    s32   fatalSound;
    s32   impactPan;
    s32   recoveryPan;
    s32   fatalPan;
    s32   audioDepth;
    u16   elapsedFrames;

    playerTask                           = *gPlayerActorTasks;
    elapsedFrames                        = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames == ACTOR_403100_DRAG_SECTION_FRAME) {
        if (D_actor_403100_80155808->sectionDamaged[ACTOR_403100_DRAG_SECTION] == 0) {
            dryfieldNightMotelBalconySetSectionState(ACTOR_403100_DRAG_SECTION, ACTOR_403100_BALCONY_SECTION_DAMAGED);
            D_actor_403100_80155808->sectionDamaged[ACTOR_403100_DRAG_SECTION] = 1;
        } else {
            dryfieldNightMotelBalconySetSectionState(ACTOR_403100_DRAG_SECTION, ACTOR_403100_DRAG_SECTION_REPEATED_STATE);
        }
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_DRAG_IMPACT_FRAME) {
        impactSound = (((u16)((Enemy*)playerTask->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_BURNER, 6);
        impactPan   = (s8)worldCoordGetOriginAudioPan(&playerTask->extra.tmd->coords[1]);
        audioDepth  = worldCoordGetOriginAudioDepth(&playerTask->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(impactSound, impactPan, (s8)(audioDepth / 2));
        padScriptSpawnVariableMotorRamp(ACTOR_403100_IMPACT_FEEDBACK_FRAMES, ACTOR_403100_FEEDBACK_START_INTENSITY, ACTOR_403100_FEEDBACK_END_INTENSITY);
        D_actor_403100_80155808->shakeFrames = ACTOR_403100_IMPACT_FEEDBACK_FRAMES;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_DRAG_RECOVERY_SOUND_FRAME) {
        recoverySound = (((u16)((Enemy*)playerTask->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_BURNER, 7);
        recoveryPan   = (s8)worldCoordGetOriginAudioPan(&playerTask->extra.tmd->coords[1]);
        audioDepth    = worldCoordGetOriginAudioDepth(&playerTask->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(recoverySound, recoveryPan, (s8)(audioDepth / 2));
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_DRAG_FATAL_SOUND_FRAME) {
        if (D_actor_403100_80155808->playerKilled != 0) {
            fatalSound = (((u16)((Enemy*)playerTask->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY, 11);
            fatalPan   = (s8)worldCoordGetOriginAudioPan(&playerTask->extra.tmd->coords[1]);
            audioDepth = worldCoordGetOriginAudioDepth(&playerTask->extra.tmd->coords[1]);
            sndEvtRequestScriptStart(fatalSound, fatalPan, (s8)(audioDepth / 2));
        }
    }
    if (D_actor_403100_80155808->playerKilled == 0) {
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
            D_actor_403100_80155808->handTouchedPlayer = 0;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            if (D_actor_403100_8015580C->hp > 0) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTOR_403100_DRAG_RETURN_VIEW;
            }
            gGameSession->suppressViewTriggers     = 0;
            D_actor_403100_80155808->vulnerable    = 0;
            D_actor_403100_80155808->holdingPlayer = 0;
            D_actor_403100_80155808->state         = ACTOR_403100_BEHAVIOUR_ATTACK_APPROACH;
            D_actor_403100_80155808->subState      = 0;
        }
    } else if ((s16)D_actor_403100_80155808->stateFrames >= ACTOR_403100_DRAG_DEATH_RELEASE_FRAME) {
        gGameSession->suppressDeathChecks = 0;
    }
}
/// Suspends combat for the one-time low-health scene, or hands an expired fight to events.
///
/// Requires the singleton work, linked enemy target and live model. The scene
/// clears flames, saves the fight pose and camera, hides the HUD and holds the
/// player under scripted control. Hits remain enabled with doubled damage and
/// defeat deferred until the scene step restores combat.
static void _actor403100BeginLowHealthScene(Task* task)
{
    enum {
        ACTOR_403100_TASK_EVENT_HANDOFF         = 4,
        ACTOR_403100_ANIMATION_LOW_HEALTH_SCENE = 16,
        ACTOR_403100_PART_JAW                   = 4,
    };
    Actor403100Work* work;
    Actor403100Work* completionWork;
    s32              sceneSound;
    s32              audioPan;
    s32              audioDepth;
    GfxCoord*        rootCoord;
    TmdObject*       model;

    model                                  = task->extra.tmd;
    rootCoord                              = model->coords;
    gGameSession->suppressViewTriggers     = 0;
    model->otOffset                        = 0;
    D_actor_403100_8015580C->reactionFlags = 0;
    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
    D_actor_403100_8015580C->node.state.parts.flags = (WORLD_TARGET_HIDE_HP | WORLD_TARGET_NOT_LOCKABLE);
    if (D_actor_403100_80155808->fightFramesLeft < 0) {
        task->state                       = ACTOR_403100_TASK_EVENT_HANDOFF;
        D_actor_403100_80155808->state    = 0;
        D_actor_403100_80155808->subState = 0U;
        return;
    }
    worldTargetUnlinkNode(&D_actor_403100_8015580C->node);
    D_actor_403100_80155810 = 0;
    _actor403100ReleaseFlames();
    sndEvtRequestScriptStop(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), SOUND_SCRIPT_STOP_KEEP_RELEASE);
    // Preserve the fight pose before moving to the scene's fixed placement.
    D_actor_403100_80155808->savedView                         = gGameSession->location.loc.view;
    D_actor_403100_80155808->savedRootMatrix                   = rootCoord->coord;
    D_actor_403100_80155808->savedRotation                     = D_actor_403100_80155808->rotation;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTOR_403100_LOW_HEALTH_SCENE_VIEW;
    work                                                       = D_actor_403100_80155808;
    work->phaseChangeDone                                      = 1;
    work->walkStage                                            = ACTOR_403100_WALK_STOPPED;
    work->animationRate                                        = ANIMATION_RATE_ONE;
    work->animationId                                          = ACTOR_403100_ANIMATION_LOW_HEALTH_SCENE;
    work->animationRequest                                     = ACTOR_403100_ANIMATION_REQUEST_RESET;
    work->aimMode                                              = ACTOR_403100_AIM_ANIMATED;
    D_actor_403100_80155808->armPitch                          = 0;
    D_actor_403100_80155808->armYaw                            = 0;
    D_actor_403100_80155808->forearmTurn.vx                    = 0;
    D_actor_403100_80155808->forearmTurn.vy                    = 0;
    D_actor_403100_80155808->forearmTurn.vz                    = 0;
    rootCoord->coord.t[0]                                      = -0x44C;
    rootCoord->coord.t[1]                                      = -0x1388;
    rootCoord->coord.t[2]                                      = 0x2710;
    D_actor_403100_80155808->rotation.vx                       = 0;
    D_actor_403100_80155808->rotation.vy                       = 0xA00;
    D_actor_403100_80155808->rotation.vz                       = 0;
    gGameSession->hideHud                                      = 1;
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    sceneSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_BURNER, 11);
    audioPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]);
    audioDepth = worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]);
    sndEvtRequestScriptStart(sceneSound, audioPan, (s8)(audioDepth / 2));
    completionWork              = D_actor_403100_80155808;
    completionWork->stateFrames = 0;
    completionWork->vulnerable  = 1;
    completionWork->subState    = (u16)(completionWork->subState + 1);
}
/// Runs the low-health scene's timed cues and restores the saved combat pose.
///
/// Requires the scene state saved by `_actor403100BeginLowHealthScene`.
/// Uploads the image list at update 100 and plays the jaw cue at 270. A
/// boundary, jump or settled pose from the previous animation update restores
/// targeting, the HUD, player control, root transform and camera, then resumes approach.
static void _actor403100StepLowHealthScene(Task* task)
{
    enum {
        ACTOR_403100_LOW_HEALTH_IMAGE_FRAME = 100,
        ACTOR_403100_LOW_HEALTH_SOUND_FRAME = 270,
        ACTOR_403100_PART_JAW               = 4,
    };
    s32       sceneSound;
    s32       audioPan;
    s32       audioDepth;
    GfxCoord* rootCoord;

    rootCoord                                                  = task->extra.tmd->coords;
    D_actor_403100_80155808->stateFrames                      += 1;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTOR_403100_LOW_HEALTH_SCENE_VIEW;
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_LOW_HEALTH_IMAGE_FRAME) {
        gpuUploadImages(&D_actor_403100_801555EC[0]);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_LOW_HEALTH_SOUND_FRAME) {
        sceneSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_BURNER, 5);
        audioPan   = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]);
        audioDepth = worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_PART_JAW]);
        sndEvtRequestScriptStart(sceneSound, audioPan, (s8)(audioDepth / 2));
    }
    rootCoord->coord.t[0]                = -0x44C;
    rootCoord->coord.t[1]                = -0x1388;
    rootCoord->coord.t[2]                = 0x2710;
    D_actor_403100_80155808->rotation.vx = 0;
    D_actor_403100_80155808->rotation.vy = 0xA00;
    D_actor_403100_80155808->rotation.vz = 0;
    if (_actor403100PreviousAnimationAtBoundaryOrJump()) {
        // Return the player and the saved camera/pose to the running fight.
        D_actor_403100_80155808->vulnerable = 0;
        worldTargetLinkNode(&D_actor_403100_8015580C->node);
        D_actor_403100_8015580C->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        gGameSession->hideHud                           = 0;
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
        rootCoord->coord                                           = D_actor_403100_80155808->savedRootMatrix;
        D_actor_403100_80155808->rotation                          = D_actor_403100_80155808->savedRotation;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_actor_403100_80155808->savedView;
        D_actor_403100_80155808->state                             = ACTOR_403100_BEHAVIOUR_ATTACK_APPROACH;
        D_actor_403100_80155808->subState                          = 0;
    }
}

/// Updates the head's aim and rebuilds its rotation in the trunk's local frame.
///
/// `aimMode` selects ACTOR_403100_AIM_*; target and model positions are game
/// coordinates and angles use 4096 units per turn. Tracking steps root-frame
/// pitch/yaw, while animated modes ease toward the current pose. Requires live
/// parts 0..3 and current composed root/head/view matrices. Borrows one scratch
/// block during the call, retains translation and marks the trunk/head dirty.
static void _actor403100AimHead(Task* task, s16 aimMode)
{
    enum {
        ACTOR_403100_SIGNED_ANGLE_SHIFT  = 20, // Sign-extend a 12-bit turn angle in a 32-bit value
        ACTOR_403100_AIM_EASE_SHIFT      = 3,  // Approach animated angles by one eighth per update
        ACTOR_403100_AIM_ORIGIN_Y_OFFSET = 1536,
    };
    SVECTOR                     animatedHeadAngles, animatedMiddleAngles, animatedLowerAngles;
    MATRIX                      headWorldTransform;
    VECTOR                      worldTargetOffset, rootTargetOffset;
    _Actor403100HeadAimScratch* aimScratch;
    SVECTOR*                    targetAngles;
    GfxCoord*                   actorCoords;
    GfxCoord*                   headCoord;
    GfxCoord*                   middleTrunkCoord;
    GfxCoord*                   lowerTrunkCoord;
    GfxCoord*                   rootCoord;
    MATRIX*                     headLocalRotation;
    MATRIX*                     lowerInverseRotation;
    MATRIX*                     headRotation;
    s32                         animatedPitchSum;
    s32                         headAimOriginY;
    s32                         headPartIndex;

    actorCoords      = task->extra.tmd->coords;
    headCoord        = &actorCoords[ACTOR_403100_PART_HEAD];
    middleTrunkCoord = &actorCoords[ACTOR_403100_PART_MIDDLE_TRUNK];
    lowerTrunkCoord  = &actorCoords[ACTOR_403100_PART_LOWER_TRUNK];
    headPartIndex    = ACTOR_403100_PART_HEAD;
    aimScratch       = SCRATCH_STACK_RESERVE_BLOCK(_Actor403100HeadAimScratch);
    targetAngles     = &aimScratch->targetAngles;
    gfxSetRotIdentity(&aimScratch->aim);
    rootCoord = task->extra.tmd->coords;
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &rootCoord[headPartIndex].workm, &headWorldTransform);
    // Measure target pitch/yaw in the root frame from the offset head origin.
    worldTargetOffset.vx = D_actor_403100_80155808->aimTarget.vx - headWorldTransform.t[0];
    headAimOriginY       = headWorldTransform.t[1] + ACTOR_403100_AIM_ORIGIN_Y_OFFSET;
    worldTargetOffset.vy = D_actor_403100_80155808->aimTarget.vy - headAimOriginY;
    worldTargetOffset.vz = D_actor_403100_80155808->aimTarget.vz - headWorldTransform.t[2];
    ApplyTransposeMatrixLV(&rootCoord->coord, &worldTargetOffset, &rootTargetOffset);
    targetAngles->vx = (ratan2(-rootTargetOffset.vy, rootTargetOffset.vz) << ACTOR_403100_SIGNED_ANGLE_SHIFT) >> ACTOR_403100_SIGNED_ANGLE_SHIFT;
    targetAngles->vy = (ratan2(rootTargetOffset.vx, rootTargetOffset.vz) << ACTOR_403100_SIGNED_ANGLE_SHIFT) >> ACTOR_403100_SIGNED_ANGLE_SHIFT;
    targetAngles->vz = 0;
    // Tracking modes step the aim; animated modes ease toward the current pose.
    if (aimMode == ACTOR_403100_AIM_TRACK) {
        _actor403100StepHeadAimPitch(targetAngles, 8, 0x280, -0x2C0);
        _actor403100StepHeadAimYaw(targetAngles, 8, 2, 1, 4);
        _actor403100RelaxHeadAimRoll();
        RotMatrixZXY(&D_actor_403100_80155808->headAim, &aimScratch->aim);
    } else if (aimMode == ACTOR_403100_AIM_YAW_ONLY) {
        gfxExtractEulerAngles(&actorCoords[headPartIndex].coord, &animatedHeadAngles);
        D_actor_403100_80155808->headAim.vx += ((s32)(((u16)animatedHeadAngles.vx - (u16)D_actor_403100_80155808->headAim.vx) << ACTOR_403100_SIGNED_ANGLE_SHIFT) >> (ACTOR_403100_SIGNED_ANGLE_SHIFT + ACTOR_403100_AIM_EASE_SHIFT));
        D_actor_403100_80155808->headAim.vz += ((s32)(((u16)animatedHeadAngles.vz - (u16)D_actor_403100_80155808->headAim.vz) << ACTOR_403100_SIGNED_ANGLE_SHIFT) >> (ACTOR_403100_SIGNED_ANGLE_SHIFT + ACTOR_403100_AIM_EASE_SHIFT));
        _actor403100StepHeadAimYaw(targetAngles, 8, 4, 1, 4);
        RotMatrixZXY(&D_actor_403100_80155808->headAim, &aimScratch->aim);
    } else if (aimMode == ACTOR_403100_AIM_ANIMATED) {
        gfxExtractEulerAngles(&actorCoords[headPartIndex].coord, &animatedHeadAngles);
        gfxExtractEulerAngles(&actorCoords[ACTOR_403100_PART_MIDDLE_TRUNK].coord, &animatedMiddleAngles);
        gfxExtractEulerAngles(&actorCoords[ACTOR_403100_PART_LOWER_TRUNK].coord, &animatedLowerAngles);
        animatedPitchSum                     = (u16)animatedHeadAngles.vx + ((u16)animatedMiddleAngles.vx + (u16)animatedLowerAngles.vx);
        animatedHeadAngles.vx                = animatedPitchSum;
        animatedHeadAngles.vy                = (u16)animatedHeadAngles.vy + ((u16)animatedMiddleAngles.vy + (u16)animatedLowerAngles.vy);
        animatedHeadAngles.vz                = (u16)animatedHeadAngles.vz + ((u16)animatedMiddleAngles.vz + (u16)animatedLowerAngles.vz);
        D_actor_403100_80155808->headAim.vx += ((s32)((animatedPitchSum - (u16)D_actor_403100_80155808->headAim.vx) << ACTOR_403100_SIGNED_ANGLE_SHIFT) >> (ACTOR_403100_SIGNED_ANGLE_SHIFT + ACTOR_403100_AIM_EASE_SHIFT));
        D_actor_403100_80155808->headAim.vy += ((s32)(((u16)animatedHeadAngles.vy - (u16)D_actor_403100_80155808->headAim.vy) << ACTOR_403100_SIGNED_ANGLE_SHIFT) >> (ACTOR_403100_SIGNED_ANGLE_SHIFT + ACTOR_403100_AIM_EASE_SHIFT));
        D_actor_403100_80155808->headAim.vz += ((s32)(((u16)animatedHeadAngles.vz - (u16)D_actor_403100_80155808->headAim.vz) << ACTOR_403100_SIGNED_ANGLE_SHIFT) >> (ACTOR_403100_SIGNED_ANGLE_SHIFT + ACTOR_403100_AIM_EASE_SHIFT));
        RotMatrixZXY(&D_actor_403100_80155808->headAim, &aimScratch->aim);
    } else if (aimMode == ACTOR_403100_AIM_TRACK_FAST) {
        _actor403100StepHeadAimPitch(targetAngles, 0x10, 0x280, -0x280);
        _actor403100StepHeadAimYaw(targetAngles, 0x10, 4, 2, 8);
        _actor403100RelaxHeadAimRoll();
        RotMatrixZXY(&D_actor_403100_80155808->headAim, &aimScratch->aim);
    } else if (aimMode == ACTOR_403100_AIM_YAW_ONLY_FAST) {
        gfxExtractEulerAngles(&actorCoords[headPartIndex].coord, &animatedHeadAngles);
        D_actor_403100_80155808->headAim.vx += ((s32)(((u16)animatedHeadAngles.vx - (u16)D_actor_403100_80155808->headAim.vx) << ACTOR_403100_SIGNED_ANGLE_SHIFT) >> (ACTOR_403100_SIGNED_ANGLE_SHIFT + ACTOR_403100_AIM_EASE_SHIFT));
        D_actor_403100_80155808->headAim.vz += ((s32)(((u16)animatedHeadAngles.vz - (u16)D_actor_403100_80155808->headAim.vz) << ACTOR_403100_SIGNED_ANGLE_SHIFT) >> (ACTOR_403100_SIGNED_ANGLE_SHIFT + ACTOR_403100_AIM_EASE_SHIFT));
        _actor403100StepHeadAimYaw(targetAngles, 0x10, 4, 2, 8);
        RotMatrixZXY(&D_actor_403100_80155808->headAim, &aimScratch->aim);
    } else if (aimMode == ACTOR_403100_AIM_TRACK_STEPPED) {
        _actor403100StepHeadAimPitch(targetAngles, 0x10, 0x280, -0x280);
        _actor403100StepHeadAimYaw(targetAngles, D_actor_403100_80155808->aimYawStep, 4, 2, 8);
        _actor403100RelaxHeadAimRoll();
        RotMatrixZXY(&D_actor_403100_80155808->headAim, &aimScratch->aim);
    }
    // The headCoord's own rotation is the aim brought into the frame of the two
    // trunk parts it hangs from: (lowerTrunkCoord * middleTrunkCoord)^-1 * aim.
    headLocalRotation = &aimScratch->local;
    TransposeMatrix(&middleTrunkCoord->coord, headLocalRotation);
    lowerInverseRotation = &aimScratch->lowerInverse;
    TransposeMatrix(&lowerTrunkCoord->coord, lowerInverseRotation);
    MulMatrix(headLocalRotation, lowerInverseRotation);
    MulMatrix(headLocalRotation, &aimScratch->aim);
    headRotation = &headCoord->coord;
    _actor403100CopyRotation(headRotation, headLocalRotation);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403100HeadAimScratch);
    lowerTrunkCoord->composeStamp  = GRAPHICS_COORD_DIRTY;
    middleTrunkCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    headCoord->composeStamp        = GRAPHICS_COORD_DIRTY;
}
/// Steps of the behaviour mode `_actor403100StepArmSwingCombo`, indexed by `subState`.
static const TaskFuncTable6 D_actor_403100_80131F84 = {
    {
        _actor403100BeginArmSwingCombo,
        _actor403100StepFirstArmSwing,
        _actor403100StepRepeatedArmSwing,
        _actor403100FinishArmSwingCombo,
        _actor403100StepArmComboImpact,
        _actor403100FinishArmSwingRecovery,
    },
};

/// Steps of the behaviour mode `_actor403100StepAimedFlameAttack`, indexed by `subState`.
static const TaskFuncTable5 D_actor_403100_80131F9C = {
    {
        _actor403100BeginAimedFlameAttack,
        _actor403100StepAimedFlameBreath,
        _actor403100WaitAfterAimedFlameAttack,
        _actor403100BeginAimedFlameContactRecovery,
        _actor403100StepAimedFlameContactRecovery,
    },
};

/// Steps of the behaviour mode `_actor403100StepJumpSlam`, indexed by `subState`.
static const TaskFuncTable9 D_actor_403100_80131FB0 = {
    {
        _actor403100PrepareJumpSlam,
        _actor403100BeginJumpSlam,
        _actor403100CrouchForJumpSlam,
        _actor403100AccelerateJumpSlam,
        _actor403100DecelerateJumpSlam,
        _actor403100DescendJumpSlam,
        _actor403100StepJumpSlamImpact,
        _actor403100FinishJumpSlamRecovery,
        _actor403100WaitAfterJumpSlam,
    },
};

/// Steps of the behaviour mode `_actor403100StepGrabAndSqueeze`, indexed by `subState`.
static const TaskFuncTable11 D_actor_403100_80131FD4 = {
    {
        _actor403100BeginGrabAndSqueeze,
        _actor403100StepGrabAndSqueezeReach,
        _actor403100BeginHeldPlayerLift,
        _actor403100BeginHeldPlayerSqueeze,
        _actor403100StepHeldPlayerSqueeze,
        _actor403100PrepareHeldPlayerFlameBreath,
        _actor403100StepHeldPlayerFlameBreath,
        _actor403100RecoverHeldPlayerFromFlameBreath,
        _actor403100ThrowHeldPlayer,
        _actor403100FinishHeldPlayerThrow,
        _actor403100FinishGrabRewind,
    },
};

/// Steps of the behaviour mode `_actor403100StepSideFlameAttack`, indexed by `subState`.
static const TaskFuncTable5 D_actor_403100_80132000 = {
    {
        _actor403100BeginSideFlameAttack,
        _actor403100StepSideFlameBreath,
        _actor403100WaitAfterSideFlameAttack,
        _actor403100BeginSideFlameContactRecovery,
        _actor403100StepSideFlameContactRecovery,
    },
};

/// Steps of the behaviour mode `_actor403100StepGrabAndDrag`, indexed by `subState`.
static const TaskFuncTable4 D_actor_403100_80132014 = {
    {
        _actor403100BeginGrabAndDrag,
        _actor403100StepGrabAndDragReach,
        _actor403100StepPlayerDrag,
        _actor403100FinishPlayerDrag,
    },
};

/// Steps of the behaviour mode `_actor403100StepBuildupStun`, indexed by `subState`.
static const TaskFuncTable3 D_actor_403100_80132024 = {
    {
        _actor403100BeginBuildupStun,
        _actor403100FinishBuildupStunAnimation,
        _actor403100WaitAfterBuildupStun,
    },
};

/// Handlers `_actor403100UpdateWalk` dispatches on `walkStage`.
static const TaskFuncTable6 D_actor_403100_80132030 = {
    {
        _actor403100WalkApproach,
        _actor403100FinishApproachStride,
        _actor403100WalkIdleState2,
        _actor403100WalkIdleState3,
        _actor403100FinishStoppingStride,
        _actor403100WalkStopped,
    },
};

/// Dispatches the current walk phase, then eases the root's lateral balcony position.
///
/// Requires live singleton work/model and walkStage in 0..5; dispatch is unchecked.
/// Captures the previous half-turn stride phase for footfall detection. In mapped
/// views 7/8 the grab-and-drag behaviour seeks one of two region-dependent X lanes;
/// other views ease toward X=-1100. Coordinates are root-parent game units.
static void _actor403100UpdateWalk(Task* task)
{
    enum {
        ACTOR_403100_BEHAVIOUR_GRAB_AND_DRAG = 6,
        ACTOR_403100_MAPPED_VIEW_BYTE_MASK   = 0xFF,
        ACTOR_403100_DRAG_LANE_FIRST_VIEW    = 7,
        ACTOR_403100_DRAG_LANE_SECOND_VIEW   = 8,
    };
    GfxCoord*      rootCoord    = task->extra.tmd->coords;
    TaskFuncTable6 walkHandlers = D_actor_403100_80132030;

    D_actor_403100_80155808->previousStridePhase = (u16)D_actor_403100_80155808->stridePhase;
    walkHandlers.funcs[D_actor_403100_80155808->walkStage](task);
    if (((viewGetMappedIndex() & ACTOR_403100_MAPPED_VIEW_BYTE_MASK) == ACTOR_403100_DRAG_LANE_FIRST_VIEW) || ((viewGetMappedIndex() & ACTOR_403100_MAPPED_VIEW_BYTE_MASK) == ACTOR_403100_DRAG_LANE_SECOND_VIEW)) {
        if ((s16)D_actor_403100_80155808->state == ACTOR_403100_BEHAVIOUR_GRAB_AND_DRAG) {
            if (D_actor_403100_80155808->playerRegion == ACTOR_403100_PLAYER_REGION_2) {
                rootCoord->coord.t[0] += (-4000 - rootCoord->coord.t[0]) >> 4;
            } else {
                rootCoord->coord.t[0] += (-2700 - rootCoord->coord.t[0]) >> 4;
            }
        }
    } else {
        rootCoord->coord.t[0] += (-1100 - rootCoord->coord.t[0]) >> 3;
    }
}
/// Moves one approach update along parent-frame Z and advances the stride phase.
///
/// Requires live singleton work and a writable root. `signedDistance` is the
/// signed translation in parent-coordinate game units for this update. The
/// phase advances by 32 of 4096 angle units per turn and wraps to 0..2047,
/// independently of distance or direction. The caller manages cache dirtiness
/// and vertical bobbing; this step has no pause or collision gate.
static inline void _actor403100MoveApproachStride(GfxCoord* rootCoord, s32 signedDistance)
{
    enum { ACTOR_403100_APPROACH_STRIDE_STEP = 0x20,
           ACTOR_403100_APPROACH_STRIDE_MASK = 0x7FF };
    rootCoord->coord.t[2]               += signedDistance;
    D_actor_403100_80155808->stridePhase = (D_actor_403100_80155808->stridePhase + ACTOR_403100_APPROACH_STRIDE_STEP) & ACTOR_403100_APPROACH_STRIDE_MASK;
}

/// Steps the root along balcony Z until the current player region's target is reached.
///
/// Requires live singleton work, its model and that model's root coordinate.
/// Regions 1/5 keep a 4864-unit player standoff, 3/4 keep 640, and 2/6 seek
/// Z=1300 within 48 game units. Outside regions 1..6 nothing changes. Movement
/// uses walkSpeed per update and advances the half-turn stride by 32 angle units;
/// reaching the inclusive target band increments walkStage without moving.
/// Regions 1/3/4/5 also clear the model's ordering-table bias. Cache dirtiness
/// and the vertical stride bob are managed by the caller.
static inline void _actor403100StepApproachRoot(TmdObject* model, GfxCoord* rootCoord)
{
    enum { ACTOR_403100_APPROACH_FAR_DISTANCE    = 4864,
           ACTOR_403100_APPROACH_NEAR_DISTANCE   = 640,
           ACTOR_403100_APPROACH_FIXED_Z         = 1300,
           ACTOR_403100_APPROACH_FIXED_TOLERANCE = 48 };
    s16 regionIndex;
    s32 farPlayerDeltaZ;
    s32 fixedTargetDeltaZ;
    s32 nearPlayerDeltaZ;

    // The two cross-balcony regions approach a fixed Z; other regions keep player standoff.
    regionIndex = D_actor_403100_80155808->playerRegion - 1;
    switch (regionIndex) {
        case ACTOR_403100_PLAYER_REGION_1 - 1:
        case ACTOR_403100_PLAYER_REGION_5 - 1:
            model->otOffset = 0;
            farPlayerDeltaZ = D_actor_403100_80155808->playerPosition.vz - rootCoord->coord.t[2];
            if (farPlayerDeltaZ > ACTOR_403100_APPROACH_FAR_DISTANCE) {
                _actor403100MoveApproachStride(rootCoord, D_actor_403100_80155808->walkSpeed);
            } else if (farPlayerDeltaZ < -ACTOR_403100_APPROACH_FAR_DISTANCE) {
                _actor403100MoveApproachStride(rootCoord, -D_actor_403100_80155808->walkSpeed);
            } else {
                D_actor_403100_80155808->walkStage++;
            }
            break;
        case ACTOR_403100_PLAYER_REGION_2 - 1:
        case ACTOR_403100_PLAYER_REGION_6 - 1:
            fixedTargetDeltaZ = ACTOR_403100_APPROACH_FIXED_Z - rootCoord->coord.t[2];
            if (fixedTargetDeltaZ > ACTOR_403100_APPROACH_FIXED_TOLERANCE) {
                _actor403100MoveApproachStride(rootCoord, D_actor_403100_80155808->walkSpeed);
            } else if (fixedTargetDeltaZ < -ACTOR_403100_APPROACH_FIXED_TOLERANCE) {
                _actor403100MoveApproachStride(rootCoord, -D_actor_403100_80155808->walkSpeed);
            } else {
                D_actor_403100_80155808->walkStage++;
            }
            break;
        case ACTOR_403100_PLAYER_REGION_3 - 1:
        case ACTOR_403100_PLAYER_REGION_4 - 1:
            model->otOffset  = 0;
            nearPlayerDeltaZ = D_actor_403100_80155808->playerPosition.vz - rootCoord->coord.t[2];
            if (nearPlayerDeltaZ > ACTOR_403100_APPROACH_NEAR_DISTANCE) {
                _actor403100MoveApproachStride(rootCoord, D_actor_403100_80155808->walkSpeed);
            } else if (nearPlayerDeltaZ < -ACTOR_403100_APPROACH_NEAR_DISTANCE) {
                _actor403100MoveApproachStride(rootCoord, -D_actor_403100_80155808->walkSpeed);
            } else {
                D_actor_403100_80155808->walkStage++;
            }
            break;
    }
}

/// Moves toward the current player-region target and bobs with the stride.
///
/// Requires live singleton work/model. Stride phase is 0..2047 in 4096 units
/// per turn; its Q12 sine produces a 512-game-unit upward root-parent Y bob.
/// Reaching the target selects the finish-stride phase. A newly wrapped phase
/// plays a footfall cue; an already stationary phase does not.
static void _actor403100WalkApproach(Task* task)
{
    GfxCoord* rootCoord;

    rootCoord = task->extra.tmd->coords;
    _actor403100StepApproachRoot(task->extra.tmd, rootCoord);
    if (D_actor_403100_80155808->stridePhase == 0) {
        if (D_actor_403100_80155808->previousStridePhase != 0) {
            _actor403100PlayWalkFootfall(task);
        }
    }
    rootCoord->coord.t[1] = -((rsin(D_actor_403100_80155808->stridePhase) << 13) >> 16);
}
/// Finishes the current bobbing stride before sampling the approach target again.
///
/// Requires live singleton work/model. Stride phase is 0..2047 in 4096 units
/// per turn; its Q12 sine produces a 512-game-unit upward root-parent Y bob.
/// A nonzero phase advances by 32 and plays feedback on wrapping. The following
/// callback returns to walk approach; root Z does not change in this phase.
static void _actor403100FinishApproachStride(Task* task)
{
    s16       stridePhase;
    GfxCoord* rootCoord;

    rootCoord = task->extra.tmd->coords;
    if (D_actor_403100_80155808->stridePhase != 0) {
        stridePhase                          = ((u16)D_actor_403100_80155808->stridePhase + ACTOR_403100_STRIDE_STEP) & ACTOR_403100_STRIDE_MASK;
        D_actor_403100_80155808->stridePhase = stridePhase;
        if (stridePhase == 0) {
            _actor403100PlayWalkFootfall(task);
        }
    } else {
        D_actor_403100_80155808->walkStage = ACTOR_403100_WALK_APPROACH;
    }
    rootCoord->coord.t[1] = -((rsin(D_actor_403100_80155808->stridePhase) << 13) >> 16);
}
/// Finishes the current bobbing stride before entering the stopped walk phase.
///
/// Requires live singleton work/model. Stride phase is 0..2047 in 4096 units
/// per turn; its Q12 sine produces a 512-game-unit upward root-parent Y bob.
/// A nonzero phase advances by 32 and plays feedback on wrapping. The following
/// callback advances from walk phase 4 to stopped phase 5; root Z is retained.
static void _actor403100FinishStoppingStride(Task* task)
{
    s16       stridePhase;
    GfxCoord* rootCoord;

    rootCoord = task->extra.tmd->coords;
    if (D_actor_403100_80155808->stridePhase != 0) {
        stridePhase                          = ((u16)D_actor_403100_80155808->stridePhase + ACTOR_403100_STRIDE_STEP) & ACTOR_403100_STRIDE_MASK;
        D_actor_403100_80155808->stridePhase = stridePhase;
        if (stridePhase == 0) {
            _actor403100PlayWalkFootfall(task);
        }
    } else {
        D_actor_403100_80155808->walkStage++;
    }
    rootCoord->coord.t[1] = -((rsin(D_actor_403100_80155808->stridePhase) << 13) >> 16);
}
/// Fills the textured quad for one piece of the held player's foreground.
///
/// Borrows a read-only sprite source and requires a separate writable packet.
/// Offsets and dimensions are signed screen pixels; UV endpoints wrap to bytes.
/// Initializes a four-vertex raw-texture packet using the source's page and CLUT.
/// The caller owns the packet allocation and ordering-table link; colour bytes
/// are untouched because raw-texture drawing does not use them.
static inline void _actor403100WriteHoldForegroundQuad(POLY_FT4* quad, const SpriteSource* spriteSource, s16 offsetX, s16 offsetY)
{
    u16 clut;

    setPolyFT4(quad);
    quad->tpage = spriteSource->tpage;
    clut        = spriteSource->clut;
    setShadeTex(quad, 1);
    quad->clut = clut;
    quad->u0   = spriteSource->uv.fields.u0;
    quad->v0   = spriteSource->uv.fields.v0;
    quad->u1   = spriteSource->uv.fields.u0 + (u8)spriteSource->size.fields.w;
    quad->v1   = spriteSource->uv.fields.v0;
    quad->u2   = spriteSource->uv.fields.u0;
    quad->v2   = spriteSource->uv.fields.v0 + (u8)spriteSource->size.fields.h;
    quad->u3   = spriteSource->uv.fields.u0 + (u8)spriteSource->size.fields.w;
    quad->v3   = spriteSource->uv.fields.v0 + (u8)spriteSource->size.fields.h;
    quad->x0   = spriteSource->x0 + offsetX;
    quad->y0   = spriteSource->y0 + offsetY;
    quad->x1   = offsetX + (spriteSource->x0 + spriteSource->size.fields.w);
    quad->y1   = spriteSource->y0 + offsetY;
    quad->x2   = spriteSource->x0 + offsetX;
    quad->y2   = offsetY + (spriteSource->y0 + spriteSource->size.fields.h);
    quad->x3   = offsetX + (spriteSource->x0 + spriteSource->size.fields.w);
    quad->y3   = offsetY + (spriteSource->y0 + spriteSource->size.fields.h);
}

/// Draws the two textured foreground pieces used while the player is held.
///
/// Offsets are signed screen pixels relative to each source's position. Emits
/// raw-texture POLY_FT4s at the sources' ordering depths; the primitive cursor
/// must provide two packets and the current ordering table must be live.
static void _actor403100DrawHoldForeground(s16 offsetX, s16 offsetY)
{
    POLY_FT4*           quad;
    const SpriteSource* spriteSource;
    s32                 spriteIndex;

    for (spriteIndex = 0; spriteIndex < ARRAY_SIZE(D_actor_403100_801557E0); spriteIndex++) {
        spriteSource   = &D_actor_403100_801557E0[spriteIndex];
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        _actor403100WriteHoldForegroundQuad(quad, spriteSource, offsetX, offsetY);
        addPrim((&gGpuCurrentOt[((((u32)(spriteSource->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), quad);
    }
}
/// Eases the five squeeze angular offsets toward one stroke pose.
///
/// Requires live singleton work. Targets and stored offsets use 4096 angle
/// units per turn. `easeShift` is 1 or 2: signed 32-bit target-minus-current
/// error is shifted by that count before the sum retains its low halfword.
/// The error is not reduced to a shortest-turn difference.
static inline void _actor403100EaseSqueezePose(s32 targetArmPitch, s32 targetArmYaw, s32 targetForearmPitch, s32 targetForearmYaw, s32 targetForearmRoll, s32 easeShift)
{
    D_actor_403100_80155808->armPitch       = (u16)D_actor_403100_80155808->armPitch + ((targetArmPitch - D_actor_403100_80155808->armPitch) >> easeShift);
    D_actor_403100_80155808->armYaw         = (u16)D_actor_403100_80155808->armYaw + ((targetArmYaw - D_actor_403100_80155808->armYaw) >> easeShift);
    D_actor_403100_80155808->forearmTurn.vx = (u16)D_actor_403100_80155808->forearmTurn.vx + ((targetForearmPitch - D_actor_403100_80155808->forearmTurn.vx) >> easeShift);
    D_actor_403100_80155808->forearmTurn.vy = (u16)D_actor_403100_80155808->forearmTurn.vy + ((targetForearmYaw - D_actor_403100_80155808->forearmTurn.vy) >> easeShift);
    D_actor_403100_80155808->forearmTurn.vz = (u16)D_actor_403100_80155808->forearmTurn.vz + ((targetForearmRoll - D_actor_403100_80155808->forearmTurn.vz) >> easeShift);
}

/// Advances one held-player squeeze tick, including periodic damage and the arm stroke.
///
/// Requires live singleton work, Burner forearm coordinate 6 and player model/
/// enemy binding. The hold caller probes two ticks ahead, restores the pose,
/// then advances one display tick. Damage and re-prompt timers are not restored
/// and count all three calls; the restored stroke timer advances once.
/// A nonzero re-prompt delay freezes squeezing and raises promptPending at 1.
/// Otherwise every 180 calls applies attack entry 3; fatal damage selects player
/// clip 6 and defers normal death checks, while surviving hits may play a voice.
/// The four-phase stroke uses 4096-unit angles and parent-frame X translations,
/// with halfword wrapping and signed easing; a killed player stops all updates.
static void _actor403100StepPlayerSqueeze(Task* task)
{
    enum {
        ACTOR_403100_SQUEEZE_DAMAGE_TICKS           = 180,
        ACTOR_403100_SQUEEZE_ATTACK_INDEX           = 3,
        ACTOR_403100_SQUEEZE_FATAL_SOUND_ENTRY      = 11,
        ACTOR_403100_PLAYER_SCRIPTED_ATTACK_MESSAGE = 1024,
        ACTOR_403100_PLAYER_SQUEEZE_DEATH_CLIP      = 6,
        ACTOR_403100_SQUEEZE_RUMBLE_TICKS           = 10,
        ACTOR_403100_SQUEEZE_RUMBLE_START           = 192U,
        ACTOR_403100_SQUEEZE_RUMBLE_END             = 32U,
        ACTOR_403100_SQUEEZE_PHASE_SHIFT            = 5,
        ACTOR_403100_SQUEEZE_PHASE_MASK             = 3,
        ACTOR_403100_SQUEEZE_OUTWARD                = 0,
        ACTOR_403100_SQUEEZE_INWARD                 = 1,
        ACTOR_403100_SQUEEZE_OUTWARD_RETURN         = 2,
        ACTOR_403100_SQUEEZE_INWARD_RETURN          = 3,
    };
    Task*            playerTask;
    GfxCoord*        soundCoord;
    Task*            damageTarget;
    s16              repromptTicksLeft;
    s16              squeezeTicks;
    s32              fatalSound;
    s32              voiceChoice;
    s32              strokePhase;
    s32              outwardStartX;
    s32              outwardX;
    s32              inwardStartX;
    s32              inwardX;
    s32              outwardReturnStartX;
    s32              outwardReturnX;
    s32              inwardReturnStartX;
    s32              inwardReturnX;
    s32              hurtSound6;
    s32              fatalPan;
    s32              hurtPan6;
    s32              fatalDepth;
    u32              voiceDraw;
    s32              hurtDepth6;
    GfxCoord*        forearmCoord;
    Actor403100Work* work;
    s32              hurtSound7, hurtPan7;
    s32              hurtDepth7;

    playerTask   = *gPlayerActorTasks;
    forearmCoord = task->extra.tmd->coords + 6;
    // Delay re-prompting without spending squeeze ticks; fatal damage hands animation control to the player.
    if (D_actor_403100_80155808->playerKilled == 0) {
        if (D_actor_403100_80155808->repromptDelay != 0) {
            repromptTicksLeft                      = (u16)D_actor_403100_80155808->repromptDelay - 1;
            D_actor_403100_80155808->repromptDelay = repromptTicksLeft;
            if (repromptTicksLeft == 1) {
                D_actor_403100_80155808->promptPending = 1;
            }
        } else {
            D_actor_403100_80155808->auxFrames     = (u16)D_actor_403100_80155808->auxFrames + 1;
            squeezeTicks                           = (u16)D_actor_403100_80155808->squeezeFrames + 1;
            D_actor_403100_80155808->squeezeFrames = squeezeTicks;
            if (squeezeTicks >= ACTOR_403100_SQUEEZE_DAMAGE_TICKS) {
                D_actor_403100_80155808->squeezeFrames = 0;
                damageTarget                           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                if (taskMessageDispatch(damageTarget, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(D_actor_403100_80147614, ACTOR_403100_SQUEEZE_ATTACK_INDEX), 0) != 0) {
                    fatalSound = (((u16)((Enemy*)(playerTask)->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MOTEL_BALCONY, ACTOR_403100_SQUEEZE_FATAL_SOUND_ENTRY);
                    fatalPan   = (s8)worldCoordGetOriginAudioPan(playerTask->extra.tmd->coords + 1);
                    fatalDepth = worldCoordGetOriginAudioDepth(playerTask->extra.tmd->coords + 1);
                    sndEvtRequestScriptStart(fatalSound, fatalPan, (s8)(fatalDepth / 2));
                    gGameSession->deathSoundCountdown = GAME_SESSION_DEATH_SOUND_HOLD;
                    work                              = D_actor_403100_80155808;
                    work->playerKilled                = 1;
                    work->playerDeathFrames           = 0;
                    gGameSession->suppressDeathChecks = 1;
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ACTOR_403100_PLAYER_SCRIPTED_ATTACK_MESSAGE, 0, 0);
                    _actor403100PlayPlayerAnimation(ACTOR_403100_PLAYER_SQUEEZE_DEATH_CLIP, ANIMATION_MESSAGE_REPLACE_AND_PLAY);
                }
                if (D_actor_403100_80155808->playerKilled == 0) {
                    padScriptSpawnVariableMotorRamp(ACTOR_403100_SQUEEZE_RUMBLE_TICKS, ACTOR_403100_SQUEEZE_RUMBLE_START, ACTOR_403100_SQUEEZE_RUMBLE_END);
                    voiceDraw       = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = voiceDraw;
                    voiceChoice     = (voiceDraw >> 0x10) & 3;
                    if (voiceChoice == 0) {
                        soundCoord = playerTask->extra.tmd->coords + 1;
                        hurtSound6 = (((u16)((Enemy*)(playerTask)->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_COMMON(6);
                        hurtPan6   = (s8)worldCoordGetOriginAudioPan(soundCoord);
                        hurtDepth6 = worldCoordGetOriginAudioDepth(playerTask->extra.tmd->coords + 1);
                        sndEvtRequestScriptStart(hurtSound6, hurtPan6, (s8)(hurtDepth6 / 2));
                    } else if (voiceChoice == 1) {
                        soundCoord = playerTask->extra.tmd->coords + 1;
                        hurtSound7 = (((u16)((Enemy*)(playerTask)->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_COMMON(7);
                        hurtPan7   = (s8)worldCoordGetOriginAudioPan(soundCoord);
                        hurtDepth7 = worldCoordGetOriginAudioDepth(playerTask->extra.tmd->coords + 1);
                        sndEvtRequestScriptStart(hurtSound7, hurtPan7, (s8)(hurtDepth7 / 2));
                    }
                }
            }
            // Four 32-call phases ease the shoulder/forearm pose and slide the forearm along local X.
            strokePhase = ((u16)D_actor_403100_80155808->auxFrames >> ACTOR_403100_SQUEEZE_PHASE_SHIFT) & ACTOR_403100_SQUEEZE_PHASE_MASK;
            if (strokePhase == ACTOR_403100_SQUEEZE_OUTWARD) {
                D_actor_403100_80155808->forearmStrokeDone = 0;
                _actor403100EaseSqueezePose(0xD0, -0x350, -0x110, 0x290, 0x60, 1);
                outwardStartX            = forearmCoord->coord.t[0];
                outwardX                 = outwardStartX + ((s32)(-0xB80 - outwardStartX) >> 3);
                forearmCoord->coord.t[0] = outwardX;
                if (outwardX >= -0xBD0) {
                    D_actor_403100_80155808->forearmStrokeDone = 1;
                    return;
                }
            } else if (strokePhase == ACTOR_403100_SQUEEZE_INWARD) {
                D_actor_403100_80155808->forearmStrokeDone = 0;
                _actor403100EaseSqueezePose(0x30, -0xD0, -0x150, 0x270, -0xA0, 2);
                inwardStartX             = forearmCoord->coord.t[0];
                inwardX                  = inwardStartX + ((s32)(-0x1180 - inwardStartX) >> 2);
                forearmCoord->coord.t[0] = inwardX;
                if (inwardX < -0x111F) {
                    D_actor_403100_80155808->forearmStrokeDone = 1;
                }
            } else if (strokePhase == ACTOR_403100_SQUEEZE_OUTWARD_RETURN) {
                D_actor_403100_80155808->forearmStrokeDone = 0;
                _actor403100EaseSqueezePose(0xD0, -0x350, -0x110, 0x290, 0x60, 2);
                outwardReturnStartX      = forearmCoord->coord.t[0];
                outwardReturnX           = outwardReturnStartX + ((s32)(-0xB80 - outwardReturnStartX) >> 2);
                forearmCoord->coord.t[0] = outwardReturnX;
                if (outwardReturnX >= -0xBD0) {
                    D_actor_403100_80155808->forearmStrokeDone = 1;
                    return;
                }
            } else if (strokePhase == ACTOR_403100_SQUEEZE_INWARD_RETURN) {
                D_actor_403100_80155808->forearmStrokeDone = 0;
                _actor403100EaseSqueezePose(0x30, -0xD0, -0x150, 0x270, -0xA0, 1);
                inwardReturnStartX       = forearmCoord->coord.t[0];
                inwardReturnX            = inwardReturnStartX + ((s32)(-0x1180 - inwardReturnStartX) >> 2);
                forearmCoord->coord.t[0] = inwardReturnX;
                if (inwardReturnX < -0x111F) {
                    D_actor_403100_80155808->forearmStrokeDone = 1;
                }
            }
        }
    }
}
/// Carries a local point through the parent chain into world coordinates.
///
/// Stops before applying the view node. Each GTE rotation/translation result
/// narrows to signed halfwords before the next parent. Returns 1 and writes
/// `point` only on reaching a view node with a non-NULL parent; returns 0
/// unchanged at a parentless node. The input node and its acyclic chain must
/// stay live; `point` is writable and separate from them. GTE registers change.
static inline s32 _actor403100PointToWorld(const GfxCoord* coord, SVECTOR* point)
{
    SVECTOR accumulatedPoint;
    VECTOR  transformedPoint;
    s32     gteFlags;

    accumulatedPoint.vx = point->vx;
    accumulatedPoint.vy = point->vy;
    accumulatedPoint.vz = point->vz;
    while (1) {
        if (coord->parent == NULL) {
            return 0;
        }
        if (coord == &gGfxViewCoord) {
            point->vx = accumulatedPoint.vx;
            point->vy = accumulatedPoint.vy;
            point->vz = accumulatedPoint.vz;
            return 1;
        }
        gte_SetTransMatrix(&coord->coord);
        gte_SetRotMatrix(&coord->coord);
        gte_ldv0(&accumulatedPoint);
        gte_rtv0tr();
        gte_stlvnl(&transformedPoint);
        gte_stflg(&gteFlags);
        accumulatedPoint.vx = transformedPoint.vx;
        accumulatedPoint.vy = transformedPoint.vy;
        accumulatedPoint.vz = transformedPoint.vz;
        coord               = coord->parent;
    }
}

/// Latches a blended clip request for the next animation update.
///
/// `animationId` selects a loaded Burner clip. `rate` counts sixteenths of a
/// normal frame per tick and narrows to a signed byte when applied; negative
/// plays backwards. `blendFrames` is a duration in whole normal-rate frames
/// (0 no transition time, 0..2047 fits playback's signed timer). A repeated
/// clip changes only the rate and leaves the duration pending. No pose is ticked here.
static inline void _actor403100RequestAnimationBlendInline(s16 animationId, s16 rate, s16 blendFrames)
{
    D_actor_403100_80155808->animationBlendFrames = blendFrames;
    D_actor_403100_80155808->animationRate        = rate;
    D_actor_403100_80155808->animationId          = animationId;
    D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
}

/// Transforms an arm sample and latches contact with the player.
///
/// Coordinate pointers must be stable and side-effect-free; vector lvalues are
/// writable and separate. deltaX/deltaZ/contactFlag are writable scalar lvalues.
/// Arguments may be evaluated repeatedly; a miss leaves contactFlag unchanged.
/// Requires the three ACTOR_403100_ARM_CONTACT_* constants in the caller's scope.
/// Expands to a braced block; use the invocation as a standalone statement.
#define ACTOR_403100_LATCH_ARM_CONTACT(limbCoord, playerCoord, point, playerOffset, deltaX, deltaZ, contactFlag)                                                                                                                                           \
    {                                                                                                                                                                                                                                                      \
        _actor403100PointToWorld((limbCoord), &(point));                                                                                                                                                                                                   \
        (deltaX)          = (u16)(playerCoord)->coord.t[0] - (u16)(point).vx;                                                                                                                                                                              \
        (playerOffset).vx = (deltaX);                                                                                                                                                                                                                      \
        (playerOffset).vy = (u16)(playerCoord)->coord.t[1] - ((u16)(point).vy + ACTOR_403100_ARM_CONTACT_CENTER_Y);                                                                                                                                        \
        (deltaZ)          = (u16)(playerCoord)->coord.t[2] - (u16)(point).vz;                                                                                                                                                                              \
        (playerOffset).vz = (deltaZ);                                                                                                                                                                                                                      \
        if ((SquareRoot0(((deltaX) * (deltaX)) + ((deltaZ) * (deltaZ))) < (ACTOR_403100_ARM_CONTACT_RADIUS + 1)) && ((u32)(((u16)(playerOffset).vy + ACTOR_403100_ARM_CONTACT_HALF_HEIGHT) & 0xFFFF) < (2U * ACTOR_403100_ARM_CONTACT_HALF_HEIGHT + 1))) { \
            (contactFlag) = 1;                                                                                                                                                                                                                             \
        }                                                                                                                                                                                                                                                  \
    }

/// Latches hand/forearm contact with the player at a one-tick-ahead arm pose.
///
/// Tests the two world-space sample points against the player root: integer
/// X/Z distance at most 1024 game units and Y within 849 of point Y + 850.
/// Differences narrow to signed halfwords. Requires both live models and the
/// actor's animation rig. Reverses the sampled advance and restores the requested
/// rate; contact latches, animation bookkeeping and pose/composition updates remain.
static void _actor403100ProbeArmPlayerContact(Task* task)
{
    enum {
        ACTOR_403100_ARM_CONTACT_RADIUS      = 1024,
        ACTOR_403100_ARM_CONTACT_CENTER_Y    = 850,
        ACTOR_403100_ARM_CONTACT_HALF_HEIGHT = 849,
    };
    SVECTOR   handPoint, forearmPoint, playerOffset;
    GfxCoord* playerCoord;
    GfxCoord* handCoord;
    s16       handDeltaX;
    s16       forearmDeltaX;
    s16       handDeltaZ;
    s16       forearmDeltaZ;
    u16       savedRateBits;
    GfxCoord* actorCoords;
    GfxCoord* forearmCoord;

    playerCoord   = (*gPlayerActorTasks)->extra.tmd->coords;
    savedRateBits = (u16)D_actor_403100_80155808->animationRate;
    actorCoords   = task->extra.tmd->coords;
    forearmCoord  = actorCoords + ACTOR_403100_PART_FOREARM;
    // Sample one tick ahead; contact flags latch rather than clearing on a miss.
    _actor403100RequestAnimationBlendInline(D_actor_403100_80155808->animationId, D_actor_403100_80155808->animationRate, 0);
    _actor403100UpdateAnimation();
    _actor403100TurnUpperArm(task);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    handCoord                                        = actorCoords + ACTOR_403100_PART_HAND;
    actorCoords[ACTOR_403100_PART_HAND].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(handCoord);
    handPoint.vx = 0x160;
    handPoint.vy = 0x148;
    handPoint.vz = 0x2C0;
    ACTOR_403100_LATCH_ARM_CONTACT(handCoord, playerCoord, handPoint, playerOffset, handDeltaX, handDeltaZ, D_actor_403100_80155808->handTouchedPlayer);
    forearmPoint.vx = 0x160;
    forearmPoint.vy = 0x148;
    forearmPoint.vz = 0x180;
    ACTOR_403100_LATCH_ARM_CONTACT(forearmCoord, playerCoord, forearmPoint, playerOffset, forearmDeltaX, forearmDeltaZ, D_actor_403100_80155808->forearmTouchedPlayer);
    // Rewind the pose sample and restore the forward playback rate.
    D_actor_403100_80155808->animationRate = -(s16)savedRateBits;
    _actor403100RequestAnimationBlendInline(D_actor_403100_80155808->animationId, D_actor_403100_80155808->animationRate, 0);
    _actor403100UpdateAnimation(task);
    D_actor_403100_80155808->animationRate = (s16)savedRateBits;
    _actor403100RequestAnimationBlendInline(D_actor_403100_80155808->animationId, D_actor_403100_80155808->animationRate, 0);
}

#undef ACTOR_403100_LATCH_ARM_CONTACT

/// Queues a sound positioned at the Burner's jaw with this enemy's instance id.
///
/// Requires a live task/model, its borrowed `Enemy` spawn argument and an
/// already composed jaw coordinate. Pan projection borrows 24 scratch bytes.
/// `soundId` is a packed sound-script bank/entry id with instance bits 8..15
/// clear; callers use Burner entries 9, 2 and 5. The place-key high nibble
/// supplies the instance (0..15). Uses jaw coordinate 4's pan
/// narrowed to s8 and half its audio depth, truncated toward zero then to s8.
/// Queueing copies the scalar request and retains no task/model pointer.
static inline void _actor403100PlayJawSound(Task* task, s32 soundId)
{
    enum { ACTOR_403100_JAW_PART_INDEX       = 4,
           ACTOR_403100_SOUND_INSTANCE_SHIFT = 8 };
    s32 instanceSoundId;
    s32 audioPan;

    instanceSoundId = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_403100_SOUND_INSTANCE_SHIFT) | soundId;
    audioPan        = (s8)worldCoordGetOriginAudioPan(&task->extra.tmd->coords[ACTOR_403100_JAW_PART_INDEX]);
    sndEvtRequestScriptStart(instanceSoundId, audioPan, (s8)(worldCoordGetOriginAudioDepth(&task->extra.tmd->coords[ACTOR_403100_JAW_PART_INDEX]) / 2));
}

/// Advances the jaw's pitch kick and plays its requested onset sound.
///
/// Requires live singleton work; a sounding start also requires the live task,
/// Enemy spawn argument and composed jaw coordinate. Angles use 4096 per turn.
/// Rest eases toward zero by one eighth; swing eases toward -544 until at most
/// -512, then return adds 12 per update until nonnegative. Halfword stores wrap.
/// Start consumes jawKickSound (0 silent, 1 hit, 2 random attack sound) and
/// advances phase without changing pitch; drawing applies the offset later.
static void _actor403100StepJawPitchKick(Task* task)
{
    enum { ACTOR_403100_JAW_KICK_FRACTION_SCALE = 16,
           ACTOR_403100_JAW_KICK_EASE_SHIFT     = 7,
           ACTOR_403100_JAW_KICK_TARGET_SCALED  = -0x2200,
           ACTOR_403100_JAW_KICK_RETURN_STEP    = 12,
           ACTOR_403100_JAW_KICK_SWING_LIMIT    = -0x200,
           ACTOR_403100_JAW_KICK_SOUND_NONE     = 0,
           ACTOR_403100_JAW_KICK_SOUND_HIT      = 1,
           ACTOR_403100_JAW_KICK_SOUND_ATTACK   = 2 };
    s16 swingPitch;
    s16 returnPitch;
    u32 soundDraw;
    u8  soundRequest;

    switch (D_actor_403100_80155808->jawPitchPhase) {
        case ACTOR_403100_PITCH_PHASE_REST:
            D_actor_403100_80155808->jawPitchOffset = (s16)((u16)D_actor_403100_80155808->jawPitchOffset + ((s32) - (D_actor_403100_80155808->jawPitchOffset * ACTOR_403100_JAW_KICK_FRACTION_SCALE) >> ACTOR_403100_JAW_KICK_EASE_SHIFT));
            return;
        case ACTOR_403100_PITCH_PHASE_START:
            soundRequest = D_actor_403100_80155808->jawKickSound;
            if (soundRequest == ACTOR_403100_JAW_KICK_SOUND_HIT) {
                _actor403100PlayJawSound(task, SOUND_CHARACTER(SOUND_BANK_BURNER, 9));
            } else if (soundRequest == ACTOR_403100_JAW_KICK_SOUND_ATTACK) {
                soundDraw       = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = soundDraw;
                if ((soundDraw >> 16) & 1) {
                    _actor403100PlayJawSound(task, SOUND_CHARACTER(SOUND_BANK_BURNER, 2));
                } else {
                    _actor403100PlayJawSound(task, SOUND_CHARACTER(SOUND_BANK_BURNER, 5));
                }
            }
            D_actor_403100_80155808->jawKickSound  = ACTOR_403100_JAW_KICK_SOUND_NONE;
            D_actor_403100_80155808->jawPitchPhase = D_actor_403100_80155808->jawPitchPhase + 1;
            return;
        case ACTOR_403100_PITCH_PHASE_SWING:
            swingPitch                              = (u16)D_actor_403100_80155808->jawPitchOffset + ((s32)(ACTOR_403100_JAW_KICK_TARGET_SCALED - (D_actor_403100_80155808->jawPitchOffset * ACTOR_403100_JAW_KICK_FRACTION_SCALE)) >> ACTOR_403100_JAW_KICK_EASE_SHIFT);
            D_actor_403100_80155808->jawPitchOffset = swingPitch;
            if (swingPitch < ACTOR_403100_JAW_KICK_SWING_LIMIT + 1) {
                D_actor_403100_80155808->jawPitchPhase = D_actor_403100_80155808->jawPitchPhase + 1;
                return;
            }
            return;
        case ACTOR_403100_PITCH_PHASE_RETURN:
            returnPitch                             = (u16)D_actor_403100_80155808->jawPitchOffset + ACTOR_403100_JAW_KICK_RETURN_STEP;
            D_actor_403100_80155808->jawPitchOffset = returnPitch;
            if (returnPitch >= 0) {
                D_actor_403100_80155808->jawPitchPhase = ACTOR_403100_PITCH_PHASE_REST;
            }
            break;
    }
}
/// Advances the head's pitch kick without an onset sound.
///
/// Requires live singleton work. Angles use 4096 per turn. Rest eases toward
/// zero by one eighth; start enters swing without moving; swing eases toward
/// 480 until at least 448, then return subtracts 12 until nonpositive.
/// Intermediate stores wrap to signed halfwords; drawing applies the offset later.
static void _actor403100StepHeadPitchKick(void)
{
    enum { ACTOR_403100_HEAD_KICK_FRACTION_SCALE = 16,
           ACTOR_403100_HEAD_KICK_EASE_SHIFT     = 7,
           ACTOR_403100_HEAD_KICK_TARGET_SCALED  = 0x1E00,
           ACTOR_403100_HEAD_KICK_RETURN_STEP    = 12,
           ACTOR_403100_HEAD_KICK_SWING_LIMIT    = 0x1C0 };
    s16 swingPitch;
    s16 returnPitch;
    u8  phase;

    phase = D_actor_403100_80155808->headPitchPhase;
    switch (phase) {
        case ACTOR_403100_PITCH_PHASE_REST:
            D_actor_403100_80155808->headPitchOffset =
                (u16)D_actor_403100_80155808->headPitchOffset +
                ((s32) - (D_actor_403100_80155808->headPitchOffset * ACTOR_403100_HEAD_KICK_FRACTION_SCALE) >> ACTOR_403100_HEAD_KICK_EASE_SHIFT);
            return;
        case ACTOR_403100_PITCH_PHASE_START:
            D_actor_403100_80155808->headPitchPhase = ACTOR_403100_PITCH_PHASE_SWING;
            return;
        case ACTOR_403100_PITCH_PHASE_SWING:
            swingPitch = (u16)D_actor_403100_80155808->headPitchOffset +
                         ((s32)(ACTOR_403100_HEAD_KICK_TARGET_SCALED - (D_actor_403100_80155808->headPitchOffset * ACTOR_403100_HEAD_KICK_FRACTION_SCALE)) >> ACTOR_403100_HEAD_KICK_EASE_SHIFT);
            D_actor_403100_80155808->headPitchOffset = swingPitch;
            if (swingPitch >= ACTOR_403100_HEAD_KICK_SWING_LIMIT) {
                D_actor_403100_80155808->headPitchPhase =
                    D_actor_403100_80155808->headPitchPhase + 1;
                return;
            }
            return;
        case ACTOR_403100_PITCH_PHASE_RETURN:
            returnPitch                              = (u16)D_actor_403100_80155808->headPitchOffset - ACTOR_403100_HEAD_KICK_RETURN_STEP;
            D_actor_403100_80155808->headPitchOffset = returnPitch;
            if (returnPitch <= 0) {
                D_actor_403100_80155808->headPitchPhase = ACTOR_403100_PITCH_PHASE_REST;
            }
            break;
    }
}

/// Steps head pitch toward a biased target inside its permitted pitch interval.
///
/// Mutates targetAngles->vx by subtracting 0x140, with halfword wrapping.
/// Angles/steps use 4096 units per turn; limits compare as signed halfwords and
/// are exclusive. Outside them pitch returns toward zero at 24 per update.
/// Inside them a difference in -32..32 holds the current pitch; otherwise the
/// supplied pitchStep is added/subtracted with halfword wrapping, without a clamp.
static void _actor403100StepHeadAimPitch(SVECTOR* targetAngles, s32 pitchStep, s32 upperPitchLimit, s16 lowerPitchLimit)
{
    enum {
        ACTOR_403100_AIM_PITCH_BIAS        = 0x140,
        ACTOR_403100_AIM_PITCH_RETURN_STEP = 0x18,
    };
    s16 restPitch;
    s16 currentPitch;
    s16 biasedPitch;
    u16 currentPitchBits;
    u16 restPitchBits;

    biasedPitch      = (u16)targetAngles->vx - ACTOR_403100_AIM_PITCH_BIAS;
    targetAngles->vx = biasedPitch;
    if ((biasedPitch < (s16)upperPitchLimit) && (lowerPitchLimit < (s16)biasedPitch)) {
        currentPitch     = D_actor_403100_80155808->headAim.vx;
        currentPitchBits = (u16)D_actor_403100_80155808->headAim.vx;
        if ((u32)(((s16)biasedPitch - currentPitch) + ACTOR_403100_AIM_DEAD_ZONE) >= (2U * ACTOR_403100_AIM_DEAD_ZONE + 1)) {
            if (currentPitch < (s16)biasedPitch) {
                D_actor_403100_80155808->headAim.vx = (s16)(currentPitchBits + pitchStep);
                return;
            }
            D_actor_403100_80155808->headAim.vx = (s16)(currentPitchBits - pitchStep);
        }
    } else {
        restPitch     = D_actor_403100_80155808->headAim.vx;
        restPitchBits = (u16)D_actor_403100_80155808->headAim.vx;
        if (restPitch >= ACTOR_403100_AIM_DEAD_ZONE + 1) {
            D_actor_403100_80155808->headAim.vx = (s16)(restPitchBits - ACTOR_403100_AIM_PITCH_RETURN_STEP);
            return;
        }
        if (restPitch < -ACTOR_403100_AIM_DEAD_ZONE) {
            D_actor_403100_80155808->headAim.vx = (s16)(restPitchBits + ACTOR_403100_AIM_PITCH_RETURN_STEP);
        }
    }
}
/// Tracks head yaw and turns the body toward the target or the held head yaw.
///
/// Angles and steps use 4096 units per turn and stores wrap to halfwords.
/// Target yaw in -639..639 tracks the head by headYawStep, adding bodyYawStep
/// while the head error exceeds 32. Otherwise the body approaches held head yaw
/// by bodyCatchupStep. Beyond +/-640 it turns by bodyTurnStep; exactly +/-640
/// does not turn in that branch. The target vector is borrowed read-only.
static void _actor403100StepHeadAimYaw(const SVECTOR* targetAngles, s32 headYawStep, s32 bodyYawStep, s32 bodyCatchupStep, s32 bodyTurnStep)
{
    enum { ACTOR_403100_AIM_YAW_LIMIT = 0x280 };
    s16 bodyYaw;
    s16 currentHeadYaw;
    u16 targetYawBits;
    u16 currentHeadYawBits;
    u16 bodyYawBits;

    targetYawBits = (u16)targetAngles->vy;
    if ((u32)((targetYawBits + (ACTOR_403100_AIM_YAW_LIMIT - 1)) & 0xFFFF) < (2U * ACTOR_403100_AIM_YAW_LIMIT - 1)) {
        currentHeadYaw     = D_actor_403100_80155808->headAim.vy;
        currentHeadYawBits = (u16)D_actor_403100_80155808->headAim.vy;
        if ((u32)(((s16)targetYawBits - currentHeadYaw) + ACTOR_403100_AIM_DEAD_ZONE) >= (2U * ACTOR_403100_AIM_DEAD_ZONE + 1)) {
            if (currentHeadYaw < (s16)targetYawBits) {
                D_actor_403100_80155808->headAim.vy  = currentHeadYawBits + headYawStep;
                D_actor_403100_80155808->rotation.vy = D_actor_403100_80155808->rotation.vy + bodyYawStep;
                return;
            }
            D_actor_403100_80155808->headAim.vy  = currentHeadYawBits - headYawStep;
            D_actor_403100_80155808->rotation.vy = D_actor_403100_80155808->rotation.vy - bodyYawStep;
            return;
        }
        bodyYaw     = (s16)D_actor_403100_80155808->rotation.vy;
        bodyYawBits = D_actor_403100_80155808->rotation.vy;
        if (bodyYaw < currentHeadYaw) {
            D_actor_403100_80155808->rotation.vy = bodyYawBits + bodyCatchupStep;
            return;
        }
        if (currentHeadYaw < bodyYaw) {
            D_actor_403100_80155808->rotation.vy = bodyYawBits - bodyCatchupStep;
        }
    } else {
        if ((s16)targetYawBits >= ACTOR_403100_AIM_YAW_LIMIT + 1) {
            D_actor_403100_80155808->rotation.vy = D_actor_403100_80155808->rotation.vy + bodyTurnStep;
        }
        if (targetAngles->vy < -ACTOR_403100_AIM_YAW_LIMIT) {
            D_actor_403100_80155808->rotation.vy = D_actor_403100_80155808->rotation.vy - bodyTurnStep;
        }
    }
}

/// Returns head roll toward zero by eight angle units outside -16..16.
///
/// Angles use 4096 units per turn; the two tests see any result of the first
/// store and narrow stores to a halfword.
static void _actor403100RelaxHeadAimRoll(void)
{
    enum { ACTOR_403100_AIM_ROLL_DEAD_ZONE   = 16,
           ACTOR_403100_AIM_ROLL_RETURN_STEP = 8 };
    if (D_actor_403100_80155808->headAim.vz >= ACTOR_403100_AIM_ROLL_DEAD_ZONE + 1) {
        D_actor_403100_80155808->headAim.vz = (u16)D_actor_403100_80155808->headAim.vz - ACTOR_403100_AIM_ROLL_RETURN_STEP;
    }
    if (D_actor_403100_80155808->headAim.vz < -ACTOR_403100_AIM_ROLL_DEAD_ZONE) {
        D_actor_403100_80155808->headAim.vz = (u16)D_actor_403100_80155808->headAim.vz + ACTOR_403100_AIM_ROLL_RETURN_STEP;
    }
}

/// Places the player at a world position and yaw with zero pitch and roll.
///
/// Position components are signed halfword game coordinates; yaw uses 4096
/// units per turn. The placement message synchronously consumes the stack
/// transform, updates stored angles and composes the live player's root.
static void _actor403100PlacePlayer(s16 x, s16 y, s16 z, s16 yaw)
{
    ActorTransform transform;

    transform.pos.vx = x;
    transform.pos.vy = y;
    transform.pos.vz = z;
    transform.rot.vx = 0;
    transform.rot.vy = yaw;
    transform.rot.vz = 0;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &transform, 0);
}
/// Refreshes transient light slot 2 with the Burner's flickering breath colour.
///
/// Requires the live Burner model and initialized gameplay light pool. Refreshes
/// an eight-frame lifetime with inner/outer radii 768/12288 game units, advancing
/// the shared random state once. Red is 0x800..0xF00 in Q12 steps of 0x100;
/// green and blue are one eighth and one sixteenth of red. Copies the head's
/// local translation directly into the light's view-parented coordinate and
/// marks it dirty; this does not compose the head or convert its position.
static void _actor403100RefreshBreathLight(Task* task)
{
    enum { ACTOR_403100_BREATH_LIGHT_SLOT   = 2,
           ACTOR_403100_BREATH_LIGHT_FRAMES = 8 };
    const GfxCoord*                headCoord;
    WorldCoordTransientPointLight* transientLight;
    WorldCoordPointLight*          pointLight;
    s16                            redIntensityQ12;
    u32                            flickerDraw;

    headCoord                                               = task->extra.tmd->coords;
    transientLight                                          = &gWorldCoordTransientPointLights[ACTOR_403100_BREATH_LIGHT_SLOT];
    transientLight->framesLeft                              = ACTOR_403100_BREATH_LIGHT_FRAMES;
    pointLight                                              = &transientLight->light;
    pointLight->inner                                       = 0x300;
    flickerDraw                                             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    pointLight->outer                                       = 0x3000;
    redIntensityQ12                                         = ((flickerDraw >> 16) & 0x700) + 0x800;
    pointLight->head.color.r                                = redIntensityQ12;
    pointLight->head.color.g                                = redIntensityQ12 >> 3;
    pointLight->head.color.b                                = redIntensityQ12 >> 4;
    headCoord                                              += ACTOR_403100_PART_HEAD;
    pointLight->head.transform.lighting.local.t[0]          = headCoord->coord.t[0];
    pointLight->head.transform.lighting.local.t[1]          = headCoord->coord.t[1];
    pointLight->head.transform.lighting.local.t[2]          = headCoord->coord.t[2];
    gRandomLcgState                                         = flickerDraw;
    transientLight->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
}
/// Plays a Burner-supplied animation on the player with grid participation disabled.
///
/// `animationId` must select a loaded entry in the package's eight-entry player
/// table. `messageId` chooses `ANIMATION_MESSAGE_REPLACE_AND_PLAY` or
/// `ANIMATION_MESSAGE_INSTALL_AND_PLAY`; other values only update the stored
/// signed-byte id. Dispatch consumes the stack request synchronously, while
/// the table and clip data remain borrowed for playback. The pose is reset.
static void _actor403100PlayPlayerAnimation(s16 animationId, s16 messageId)
{
    AnimationPlayRequest request;

    request.source.sets                        = _gActor403100PlayerAnimationSets;
    request.animationId                        = animationId;
    request.blend                              = ANIMATION_BLEND_RESET;
    request.blendFrames                        = 0;
    request.enableWorldCollision               = ANIMATION_WORLD_COLLISION_DISABLE;
    D_actor_403100_80155808->playerAnimationId = (s8)animationId;
    if (messageId == ANIMATION_MESSAGE_REPLACE_AND_PLAY) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &request, 0);
    } else if (messageId == ANIMATION_MESSAGE_INSTALL_AND_PLAY) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &request, 0);
    }
}
/// Requests a jaw/head pitch kick for a damaged frame while both parts are at rest.
///
/// Sets both phases to start and selects the jaw's hit sound. The damage latch
/// is not consumed, so another kick can be requested when both parts return to rest.
static void _actor403100RequestHitPitchKick(void)
{
    s32 hitTaken;

    hitTaken = D_actor_403100_80155808->hitTaken;
    if ((hitTaken == ACTOR_403100_HIT_TAKEN) && (D_actor_403100_80155808->jawPitchPhase == ACTOR_403100_PITCH_PHASE_REST) && (D_actor_403100_80155808->headPitchPhase == ACTOR_403100_PITCH_PHASE_REST)) {
        D_actor_403100_80155808->jawPitchPhase  = hitTaken;
        D_actor_403100_80155808->headPitchPhase = hitTaken;
        D_actor_403100_80155808->jawKickSound   = hitTaken;
    }
}
/// Requests a jaw/head pitch kick when both parts are at rest.
///
/// Only ACTOR_403100_PITCH_KICK_SELECT_ATTACK_SOUND (1) selects the random
/// attack sound; all other values retain the current sound request. The next
/// per-frame pitch updates take up the two start phases.
static void _actor403100RequestPitchKick(s16 selectAttackSound)
{
    enum { ACTOR_403100_JAW_KICK_ATTACK_SOUND = 2 };
    if ((D_actor_403100_80155808->jawPitchPhase == ACTOR_403100_PITCH_PHASE_REST) && (D_actor_403100_80155808->headPitchPhase == ACTOR_403100_PITCH_PHASE_REST)) {
        if (selectAttackSound == ACTOR_403100_PITCH_KICK_SELECT_ATTACK_SOUND) {
            D_actor_403100_80155808->jawKickSound = ACTOR_403100_JAW_KICK_ATTACK_SOUND;
        }
        D_actor_403100_80155808->jawPitchPhase  = ACTOR_403100_PITCH_PHASE_START;
        D_actor_403100_80155808->headPitchPhase = ACTOR_403100_PITCH_PHASE_START;
    }
}

/// Composes a joint's rotation into world space, excluding the view transform.
///
/// Normalizes each parent's basis before multiplication and each product
/// afterwards. Returns 1 when the view is reached or 0 at NULL, retaining
/// the accumulated basis on either exit. Only the output's 3x3 is valid after
/// a product; coefficients have 12 fractional bits. The live acyclic chain
/// is borrowed. The writable output is word-aligned and separate from its nodes.
static s32 _actor403100GetWorldRotation(const GfxCoord* joint, MATRIX* rotation)
{
    MATRIX          normalizedRotation;
    MATRIX          parentRotation;
    const GfxCoord* ancestor;

    ancestor  = joint->parent;
    *rotation = joint->coord;
    while (1) {
        if (ancestor == NULL) {
            return 0;
        }
        if (ancestor == &gGfxViewCoord) {
            return 1;
        }
        // Remove each ancestor's scale before accumulating the world orientation.
        parentRotation = ancestor->coord;
        MatrixNormal(&parentRotation, &parentRotation);
        ACTOR_403100_PREMULTIPLY_NORMALIZED_ROTATION(&parentRotation, rotation, &normalizedRotation);
        ancestor = ancestor->parent;
    }
}
#undef ACTOR_403100_PREMULTIPLY_NORMALIZED_ROTATION

#include "../../shared/coord_math_local_to_world.inc.c"

/// Applies a Burner behaviour, scripted-scene or defeat command.
///
/// Borrows a readable ActorCommand through synchronous dispatch; its context
/// is ignored. Commands 0..8 enter the fight at that behaviour, 10 enters
/// defeat, and 65535 enters the scripted scene. Every command, including an
/// unsupported value, resets the substate. Requires live singleton work.
/// This callback supplies no message result; senders must ignore the return word.
static void _actor403100ApplyCommand(Task* task, s32 unusedMessageId, const ActorCommand* command, s32 unusedSecondArg)
{
    enum { ACTOR_403100_COMMAND_DEFEAT          = 10,
           ACTOR_403100_COMMAND_SCRIPTED_SCENE  = 0xFFFF,
           ACTOR_403100_COMMAND_BEHAVIOUR_LIMIT = 9,
           ACTOR_403100_TASK_FIGHT              = 1,
           ACTOR_403100_TASK_SCENE              = 2,
           ACTOR_403100_TASK_DEFEAT             = 3 };
    u16 behaviourId;

    switch (command->command) {
        case ACTOR_403100_COMMAND_DEFEAT:
            task->state                       = ACTOR_403100_TASK_DEFEAT;
            D_actor_403100_80155808->state    = 0;
            D_actor_403100_80155808->subState = 0;
            break;
        case ACTOR_403100_COMMAND_SCRIPTED_SCENE:
            D_actor_403100_80155808->state    = 0;
            D_actor_403100_80155808->subState = 0;
            task->state                       = ACTOR_403100_TASK_SCENE;
            break;
        default:
            behaviourId = command->command;
            if (behaviourId < (u32)ACTOR_403100_COMMAND_BEHAVIOUR_LIMIT) {
                D_actor_403100_80155808->state    = behaviourId;
                D_actor_403100_80155808->subState = 0;
                task->state                       = ACTOR_403100_TASK_FIGHT;
            }
            break;
    }
    D_actor_403100_80155808->subState = 0;
}
/// Latches the player's request to end the current hold.
///
/// Requires live singleton work. Takes no payload; hold steps consume the
/// latch when their pose is ready. This callback supplies no message result;
/// senders must ignore the dispatch result.
static void _actor403100RequestPlayerRelease(Task* unusedTask, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    D_actor_403100_80155808->releaseRequested = 1;
}

/// Sets the Burner model's active drawing and automatic-buffer mode.
///
/// Requires a live receiver model and singleton work. Modes 0/1 hide/show
/// with automatic buffering; 2 hides and disables buffering while scheduling
/// release with a countdown of two; 3 shows while disabling automatic buffering.
/// Other values leave state unchanged. The second payload and message ID are
/// ignored, and this void callback supplies no result to the sender.
static void _actor403100SetModelDrawMode(Task* task, s32 unusedMessageId, s32 drawMode, s32 unusedSecondArg)
{
    enum { ACTOR_403100_MODEL_HIDE_AUTO_BUFFER    = 0,
           ACTOR_403100_MODEL_SHOW_AUTO_BUFFER    = 1,
           ACTOR_403100_MODEL_HIDE_RELEASE_BUFFER = 2,
           ACTOR_403100_MODEL_SHOW_NO_AUTO_BUFFER = 3 };
    u16        modelFlags;
    TmdObject* model;

    model = task->extra.tmd;
    switch (drawMode) {
        case ACTOR_403100_MODEL_HIDE_AUTO_BUFFER:
            model->flags = (model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW) & (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            return;
        case ACTOR_403100_MODEL_SHOW_AUTO_BUFFER:
            model->flags = model->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
        case ACTOR_403100_MODEL_HIDE_RELEASE_BUFFER:
            model->flags                                = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
            D_actor_403100_80155808->bufferReleaseDelay = drawMode;
            modelFlags                                  = model->flags | TMD_OBJECT_SKIP_AUTO_BUFFER;
            model->flags                                = modelFlags;
            return;
        case ACTOR_403100_MODEL_SHOW_NO_AUTO_BUFFER:
            modelFlags   = (model->flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW) | TMD_OBJECT_SKIP_AUTO_BUFFER;
            model->flags = modelFlags;
            return;
    }
}
/// Saves the arm pose and stroke timer before sampling the held-player pose ahead.
///
/// Requires live singleton work and the Burner model. Copies shoulder pitch/yaw,
/// forearm Euler offsets, coordinate 6's local X translation and auxFrames into
/// the singleton's one saved pose. A second save replaces it; no storage is allocated.
static void _actor403100SaveHoldPose(Task* task)
{
    enum { ACTOR_403100_HOLD_FOREARM_PART = 6 };
    const GfxCoord* actorCoords;

    actorCoords                                  = task->extra.tmd->coords;
    D_actor_403100_80155808->savedArmPitch       = D_actor_403100_80155808->armPitch;
    D_actor_403100_80155808->savedArmYaw         = D_actor_403100_80155808->armYaw;
    D_actor_403100_80155808->savedForearmTurn.vx = D_actor_403100_80155808->forearmTurn.vx;
    D_actor_403100_80155808->savedForearmTurn.vy = D_actor_403100_80155808->forearmTurn.vy;
    D_actor_403100_80155808->savedForearmTurn.vz = D_actor_403100_80155808->forearmTurn.vz;
    D_actor_403100_80155808->savedForearmX       = actorCoords[ACTOR_403100_HOLD_FOREARM_PART].coord.t[0];
    D_actor_403100_80155808->savedAuxFrames      = D_actor_403100_80155808->auxFrames;
}
/// Restores the saved arm pose and stroke timer after the held-player pose sample.
///
/// Requires the same live singleton/model as the preceding save. Restores only
/// shoulder angles, forearm offsets, coordinate 6's local X and auxFrames;
/// animation bookkeeping and other hold state retain the sample's effects.
/// The caller subsequently steps and poses the arm; this does not dirty caches.
static void _actor403100RestoreHoldPose(Task* task)
{
    enum { ACTOR_403100_HOLD_FOREARM_PART = 6 };
    GfxCoord* actorCoords;

    actorCoords                                            = task->extra.tmd->coords;
    D_actor_403100_80155808->armPitch                      = D_actor_403100_80155808->savedArmPitch;
    D_actor_403100_80155808->armYaw                        = D_actor_403100_80155808->savedArmYaw;
    D_actor_403100_80155808->forearmTurn.vx                = D_actor_403100_80155808->savedForearmTurn.vx;
    D_actor_403100_80155808->forearmTurn.vy                = D_actor_403100_80155808->savedForearmTurn.vy;
    D_actor_403100_80155808->forearmTurn.vz                = D_actor_403100_80155808->savedForearmTurn.vz;
    actorCoords[ACTOR_403100_HOLD_FOREARM_PART].coord.t[0] = D_actor_403100_80155808->savedForearmX;
    D_actor_403100_80155808->auxFrames                     = D_actor_403100_80155808->savedAuxFrames;
}
/// Restarts the held player's forearm-stroke, prompt and squeeze-damage timers.
///
/// Requires the live work block. Clears only the cycle's progress; pose,
/// animation, release request and prompt flags are initialized by the grab step.
/// `task` is unused but its argument is present at the binary's call site.
static void _actor403100ResetHoldCycle(Task* task)
{
    D_actor_403100_80155808->auxFrames         = 0;
    D_actor_403100_80155808->forearmStrokeDone = 0;
    D_actor_403100_80155808->repromptDelay     = 0;
    D_actor_403100_80155808->squeezeFrames     = 0;
}
/// Applies the forearm turn while sampling the player's held pose ahead of playback.
static void _actor403100TurnForearm(Task* task)
{
    _actor403100TurnForearmInline(task);
}
/// Unlinks the four combat bodies before destroying the Burner task and enemy.
///
/// Requires live singleton work and the task's Enemy spawn argument. The task
/// teardown releases its owned model/work storage; the published singleton
/// pointers are not cleared and must not be used after destruction.
static void _actor403100Destroy(Task* task)
{
    worldCollisionUnlinkBody(&D_actor_403100_80155808->headBody);
    worldCollisionUnlinkBody(&D_actor_403100_80155808->trunkBody);
    worldCollisionUnlinkBody(&D_actor_403100_80155808->handAttack);
    worldCollisionUnlinkBody(&D_actor_403100_80155808->forearmAttack);
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// Hands an expired fight to the balcony event script and returns to scene dispatch.
///
/// Requires live singleton enemy/work and task/model. Restores Q12 scale 1.25,
/// disables pending draw-buffer release, locks player attachment control and
/// clears enemy reactions/colour. Unlinks the target and fades breath audio before
/// starting the HUD-managed event; scene dispatch remains idle until commands arrive.
static void _actor403100BeginTimeoutEvent(Task* task)
{
    enum {
        ACTOR_403100_TIMEOUT_SCALE_Q12   = ONE * 5 / 4,
        ACTOR_403100_BUFFER_RELEASE_IDLE = -1,
        ACTOR_403100_TASK_SCENE          = 1,
        ACTOR_403100_SCENE_IDLE          = 9,
    };
    D_actor_403100_80155808->bufferReleaseDelay = ACTOR_403100_BUFFER_RELEASE_IDLE;
    D_actor_403100_80155808->sceneScale         = ACTOR_403100_TIMEOUT_SCALE_Q12;
    Gp_StateC08.flags                           = Gp_StateC08.flags | ATTACHMENT_FLAG_EVENT_LOCK;
    gGameSession->suppressViewTriggers          = 0;
    D_actor_403100_8015580C->reactionFlags      = 0;
    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
    sndEvtRequestScriptStop(ACTOR_403100_BREATH_SOUND, ACTOR_403100_FLAME_CONTACT_STOP_FADE_UPDATES);
    worldTargetUnlinkNode(&D_actor_403100_8015580C->node);
    evsStartScript(D_actor_335800_80165FC0, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    task->state                       = ACTOR_403100_TASK_SCENE;
    D_actor_403100_80155808->state    = 0;
    D_actor_403100_80155808->subState = 0;
    D_actor_403100_80155808->state    = ACTOR_403100_SCENE_IDLE;
    D_actor_403100_80155808->subState = 0;
}
/// Finds the first rectangle in a supplied zone table containing world X/Z.
///
/// Coordinates are signed-halfword game units. Borrows a readable table ending
/// in `ACTOR_403100_ZONE_END`; both far edges are included and table order
/// decides overlapping edges. Returns the entry's 32-bit id, or 0 outside all
/// rectangles. The query neither changes the table nor retains its pointer.
static s32 _actor403100FindZoneId(s16 x, s16 z, const _Actor403100Zone* zone)
{
    enum { ACTOR_403100_ZONE_NONE = 0 };

    while (zone->id != ACTOR_403100_ZONE_END) {
        if (x >= zone->x && zone->x + zone->width >= x &&
            z >= zone->z && zone->z + zone->depth >= z) {
            return zone->id;
        }
        zone++;
    }
    return ACTOR_403100_ZONE_NONE;
}
/// Dispatches the fight setup or wait-for-start substate.
///
/// Requires live singleton work and subState 0..1. Setup advances to the wait,
/// which changes behaviour to attack approach after thirty-one uninterrupted
/// updates. A hit reaction can divert the wait without advancing its timer.
static void _actor403100StepFightInitialization(Task* task)
{
    TaskFunc handlers[2] = { _actor403100BeginFight, _actor403100WaitForFightStart };

    handlers[(s16)D_actor_403100_80155808->subState](task);
}

/// Dispatches attack approach, range gating or attack selection unless a hit interrupts.
///
/// Requires live singleton work with subState 0..2. A stagger or build-up
/// stun changes behaviour through the hit handler and skips the approach
/// callback for this update. The selected callback owns substate advancement.
static void _actor403100StepAttackApproach(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_actor_403100_80131F60;
    if (_actor403100HandleHitReaction() == 0) {
        handlers.funcs[(s16)D_actor_403100_80155808->subState](task);
    }
}
/// Draws two floor-shadow strips beneath the Burner's long arm.
///
/// Requires the live fifteen-part model and the arm-shadow drawer's rendering
/// resources. Samples segments 6->7 then 7->8, with half-width 1024 game units
/// at world Y -3200. Drawing follows the attack step and its current pose.
static inline void _actor403100DrawLongArmShadow(Task* task)
{
    _actor403100DrawArmShadow(task, ACTOR_403100_ARM_SHADOW_BASE_PART, ACTOR_403100_ARM_SHADOW_MIDDLE_PART, ACTOR_403100_ARM_SHADOW_HALF_WIDTH, ACTOR_403100_ARM_SHADOW_FLOOR_Y);
    _actor403100DrawArmShadow(task, ACTOR_403100_ARM_SHADOW_MIDDLE_PART, ACTOR_403100_ARM_SHADOW_TIP_PART, ACTOR_403100_ARM_SHADOW_HALF_WIDTH, ACTOR_403100_ARM_SHADOW_FLOOR_Y);
}

/// Dispatches the arm swing combo and draws its shadow unless a hit interrupts.
///
/// Requires live singleton work and the Burner task/model with `subState`
/// 0..5. A stagger or build-up stun skips both the selected step and its
/// shadow. The combo steps own substate changes; the fight update advances
/// animation after this dispatch.
static void _actor403100StepArmSwingCombo(Task* task)
{
    TaskFuncTable6 handlers;

    handlers = D_actor_403100_80131F84;
    if (_actor403100HandleHitReaction() == 0) {
        handlers.funcs[(s16)D_actor_403100_80155808->subState](task);
        _actor403100DrawLongArmShadow(task);
    }
}
/// Dispatches the aimed flame attack or its contact recovery unless a hit interrupts.
///
/// Requires the live Burner task/model, singleton work and `subState` 0..4:
/// begin, emit, wait, begin contact recovery, finish recovery. A hit reaction
/// skips the selected step. The fight update advances animation afterwards.
static void _actor403100StepAimedFlameAttack(Task* task)
{
    TaskFuncTable5 handlers;

    handlers = D_actor_403100_80131F9C;
    if (_actor403100HandleHitReaction() == 0) {
        handlers.funcs[(s16)D_actor_403100_80155808->subState](task);
    }
}
/// Dispatches the jump slam and draws the long arm's shadow after every step.
///
/// Requires the live Burner task/model, singleton work and `subState` 0..8:
/// prepare, begin, crouch, accelerate, decelerate, descend, impact, recover,
/// wait. Individual steps handle hits; shadow drawing remains unconditional.
/// The fight update advances animation afterwards.
static void _actor403100StepJumpSlam(Task* task)
{
    TaskFuncTable9 handlers;

    handlers = D_actor_403100_80131FB0;
    handlers.funcs[(s16)D_actor_403100_80155808->subState](task);
    _actor403100DrawLongArmShadow(task);
}
/// Dispatches the grab, held-player squeeze and flame breath, throw or missed-grab rewind.
///
/// Requires the live Burner task/model, player and singleton work with
/// `subState` 0..10. Individual steps gate hits and manage player control;
/// dispatch itself has no hit gate. The fight update advances animation afterwards.
static void _actor403100StepGrabAndSqueeze(Task* task)
{
    TaskFuncTable11 handlers;

    handlers = D_actor_403100_80131FD4;
    handlers.funcs[(s16)D_actor_403100_80155808->subState](task);
}
/// Dispatches the side flame sweep or its contact recovery unless a hit interrupts.
///
/// Requires the live Burner task/model, singleton work and `subState` 0..4:
/// begin, emit, wait, begin contact recovery, finish recovery. A hit reaction
/// skips the selected step. The fight update advances animation afterwards.
static void _actor403100StepSideFlameAttack(Task* task)
{
    TaskFuncTable5 handlers;

    handlers = D_actor_403100_80132000;
    if (_actor403100HandleHitReaction() == 0) {
        handlers.funcs[(s16)D_actor_403100_80155808->subState](task);
    }
}
/// Dispatches the grab-and-drag reach, player displacement or release.
///
/// Requires the live Burner task/model, player and singleton work with
/// `subState` 0..3: begin, reach, drag, release. Hit gates belong to the selected
/// step; the fight update advances animation after dispatch.
static void _actor403100StepGrabAndDrag(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = D_actor_403100_80132014;
    handlers.funcs[(s16)D_actor_403100_80155808->subState](task);
}
/// Dispatches the start or completion of a hit-induced stagger.
///
/// Requires live singleton work with subState 0..1. Both handlers take no
/// argument and use that work directly; `unusedTask` supplies the enclosing
/// TaskFunc signature. The fight update advances animation after dispatch.
static void _actor403100StepStagger(Task* unusedTask)
{
    void (*handlers[])(void) = { _actor403100BeginStagger, _actor403100FinishStagger };

    handlers[(s16)D_actor_403100_80155808->subState]();
}

/// Dispatches low-health scene setup or its timed continuation.
///
/// Requires the live Burner task/model and singleton work with subState 0..1.
/// Setup saves the combat pose or hands off an expired fight to the event
/// script; continuation restores combat when the previous clip update finishes.
/// Animation advancement remains with the fight update.
static void _actor403100DispatchLowHealthScene(Task* task)
{
    TaskFunc handlers[] = { _actor403100BeginLowHealthScene, _actor403100StepLowHealthScene };

    handlers[(s16)D_actor_403100_80155808->subState](task);
}

/// Steps the build-up stun while accepting the current frame's pitch kick.
///
/// Requires live singleton work and subState 0..2: start the half-speed
/// stagger, blend into walking, then wait before resuming attack approach.
/// The damage-latch pitch request precedes dispatch; animation advances later
/// in the fight update. `task` is forwarded through the TaskFunc table.
static void _actor403100StepBuildupStun(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_actor_403100_80132024;
    _actor403100RequestHitPitchKick();
    handlers.funcs[(s16)D_actor_403100_80155808->subState](task);
}
/// Queues the blended clip request used when sampling the player's held pose.
///
/// Units and loaded-clip requirements are those of `_actor403100RequestAnimationBlendInline`.
static void _actor403100RequestAnimationBlend(s16 animationId, s16 rate, s16 blendFrames)
{
    _actor403100RequestAnimationBlendInline(animationId, rate, blendFrames);
}

/// Dispatches a floor-puff emitter's initialization, emission or terminal wait.
///
/// Requires an owned `TASK_BODY_COORD` body and state 0..2. Normal expiration
/// advances to state 2 and immediately releases the task, so its retained wait
/// slot is not reached by that path. The task owns its emission coordinate.
static void _actor403100FloorPuffEmitterTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_actor_403100_80131E70;
    states.funcs[task->state](task);
}

/// Dispatches a drift-puff emitter's initialization, emission or terminal wait.
///
/// Requires an owned `TASK_BODY_COORD` body and state 0..2. The first update
/// uses rising puff arguments; subsequent updates select horizontal drift.
/// Expiration advances to state 2 and immediately releases the task and coordinate.
static void _actor403100DriftPuffEmitterTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_actor_403100_80131E7C;
    states.funcs[task->state](task);
}

/// Dispatches the Burner's setup, scripted scene, fight, defeat, post-fight scene or teardown.
///
/// Requires the live `TASK_BODY_TMD` Burner task and state 0..5. Setup creates
/// singleton work; subsequent handlers require that work and its Enemy spawn
/// record to remain live. Teardown releases the model, work and enemy.
static void _actor403100Task(Task* task)
{
    TaskFuncTable6 states;

    states = D_actor_403100_80131F1C;
    states.funcs[task->state](task);
}

/// Leaves the player alone when no Burner arm-hit reaction is running.
static void _actor403100PlayerReactionNone(void)
{
}

/// Restarts the player's arm-hit clip while its hold countdown runs down.
///
/// Requires the live player and singleton work. The signed countdown decrements
/// through zero to -1; that last update restarts the clip once more and advances
/// to HIT_ENDING so it can play out. A dead player leaves this stage unchanged.
static void _actor403100HoldPlayerHitPose(void)
{
    enum { ACTOR_403100_PLAYER_ANIMATION_ARM_HIT = 5 };
    s16 framesLeft;

    if (gPlayerStatus.hp > 0) {
        framesLeft                                    = (u16)D_actor_403100_80155808->playerReactionFrames - 1;
        D_actor_403100_80155808->playerReactionFrames = framesLeft;
        if (framesLeft < 0) {
            _actor403100PlayPlayerAnimation(ACTOR_403100_PLAYER_ANIMATION_ARM_HIT, ANIMATION_MESSAGE_INSTALL_AND_PLAY);
            D_actor_403100_80155808->playerReactionStage = ACTOR_403100_PLAYER_REACTION_HIT_ENDING;
            return;
        }
        _actor403100PlayPlayerAnimation(ACTOR_403100_PLAYER_ANIMATION_ARM_HIT, ANIMATION_MESSAGE_INSTALL_AND_PLAY);
    }
}
/// Blends the player from the completed arm-hit clip into their weapon's aim pose.
///
/// Runs the HIT_ENDING stage. Requires live player/singleton work, character id
/// 1..2 and a loaded equipped-weapon animation bank. Its aim-entry set becomes
/// entry 4 of the package's player table and must remain live through recovery.
/// The stack request is consumed synchronously; the table stays borrowed.
static void _actor403100BeginPlayerHitRecovery(void)
{
    enum {
        ACTOR_403100_PLAYER_ANIMATION_HIT_RECOVERY    = 4,
        ACTOR_403100_NATIVE_PLAYER_AIM_ENTRY          = 7,
        ACTOR_403100_PLAYER_HIT_RECOVERY_BLEND_FRAMES = 3,
    };
    AnimationPlayRequest request;

    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        // Reuse the equipped weapon's aim-entry pose while retaining scripted control.
        _gActor403100PlayerAnimationSets[ACTOR_403100_PLAYER_ANIMATION_HIT_RECOVERY] = Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon]->table.sets[ACTOR_403100_NATIVE_PLAYER_AIM_ENTRY];
        request.source.sets                                                          = _gActor403100PlayerAnimationSets;
        request.blend                                                                = ANIMATION_BLEND_INTERPOLATE;
        request.blendFrames                                                          = ACTOR_403100_PLAYER_HIT_RECOVERY_BLEND_FRAMES;
        request.enableWorldCollision                                                 = ANIMATION_WORLD_COLLISION_DISABLE;
        request.animationId                                                          = ACTOR_403100_PLAYER_ANIMATION_HIT_RECOVERY;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &request, 0);
        D_actor_403100_80155808->playerReactionStage = ACTOR_403100_PLAYER_REACTION_RECOVERING;
    }
}

/// Returns the player to normal control when the arm-hit recovery pose settles.
///
/// Runs the RECOVERING stage with the live player and singleton work. Restores
/// the native weapon bank and collision while preserving part 1's horizontal offset,
/// then clears the reaction countdown, recorded clip and hand-contact latch.
static void _actor403100FinishPlayerHitRecovery(void)
{
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET, 0);
        D_actor_403100_80155808->playerReactionFrames = 0;
        D_actor_403100_80155808->playerAnimationId    = 0;
        D_actor_403100_80155808->handTouchedPlayer    = 0;
        D_actor_403100_80155808->playerReactionStage  = ACTOR_403100_PLAYER_REACTION_NONE;
    }
}
/// Composes a joint's rotation into the frame of an excluded ancestor.
///
/// Borrows a live acyclic joint chain and separate word-aligned output matrix.
/// Uses 12-fractional-bit coefficients, including the joint and normalizing
/// each raw parent product. Returns 1 at the non-NULL excluded ancestor or 0
/// at NULL, leaving the partial result. Only the output's 3x3 is valid after
/// a parent product; translation/alignment bytes are overwritten. Changes GTE state.
static s32 _actorRenderAccumulateJointRotation(const GfxCoord* joint, MATRIX* rotation, const GfxCoord* excludedAncestor)
{
    const GfxCoord* ancestor;

    ancestor  = joint->parent;
    *rotation = joint->coord;
    while (1) {
        if (ancestor == NULL) {
            return 0;
        }
        if (ancestor == excludedAncestor) {
            return 1;
        }
        _actorRenderPreMultiplyNormalizedRotation(&ancestor->coord, rotation);
        ancestor = ancestor->parent;
    }
}
/// Converts an excluded-ancestor-frame rotation into a joint's parent frame.
///
/// Requires a non-NULL parent and a live acyclic chain; `excludedAncestor`
/// must lie above that parent for success. Borrows the chain and a separate
/// writable word-aligned matrix with initialized 12-fractional-bit rotation.
/// Normalizes parent products, then applies their transpose without final
/// normalization. Returns 1 on conversion or 0 unchanged at NULL or when the
/// immediate parent is the view. Translation is retained; alignment bytes and
/// GTE working registers can change.
static s32 _actorRenderLocalizeRotationToAncestor(const GfxCoord* joint, MATRIX* rotation, const GfxCoord* excludedAncestor)
{
    return _actor403100LocalizeRotation(joint, rotation, excludedAncestor);
}
/// Detaches the defeated enemy's hit records and starts the reward wait.
///
/// Requires live singleton enemy/work. Clears the elapsed timer and advances
/// defeat state 3 to 4; task/model lifetime continues through the wait.
/// `unusedTask` retains the enclosing TaskFunc signature.
static void _actor403100BeginDefeatRewardWait(Task* unusedTask)
{
    D_actor_403100_8015580C->recs        = NULL;
    D_actor_403100_80155808->stateFrames = 0;
    D_actor_403100_80155808->state      += 1;
}
/// Credits defeat rewards at tick 18 and selects actor teardown at tick 360.
///
/// Runs defeat state 4 with a live enemy in `task->spawnArg2.pointer` and a
/// timer cleared by the preceding step. The counter wraps as u16 and is tested
/// as s16 at each exact cue. Releasing the battle hold leaves the actor alive;
/// tick 360 resets its behaviour selectors and teardown runs on the next update.
static void _actor403100StepDefeatRewardWait(Task* task)
{
    enum {
        ACTOR_403100_DEFEAT_REWARD_TICK = 18,
        ACTOR_403100_DEFEAT_EXIT_TICK   = 360,
        ACTOR_403100_TASK_EXIT          = 5,
    };
    u16 elapsedTicks;

    elapsedTicks                         = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = elapsedTicks;
    if ((s16)elapsedTicks == ACTOR_403100_DEFEAT_REWARD_TICK) {
        sceneReleaseBattleRefWithRewards(task, 0);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_DEFEAT_EXIT_TICK) {
        task->state                       = ACTOR_403100_TASK_EXIT;
        D_actor_403100_80155808->state    = 0;
        D_actor_403100_80155808->subState = 0;
    }
}
/// Starts the placed puff emitter's ninety-update lifetime in the world/view frame.
///
/// Requires task state 0 and an owned coordinate body with its local placement
/// initialized. Parents it to the view, marks composition dirty and advances to
/// state 1 even while actors are paused. No puff is emitted here.
static inline void _actor403100InitializePuffEmitter(Task* task)
{
    GfxCoord* emissionCoord;

    emissionCoord               = task->extra.coordBody->coord;
    task->killCountdown         = ACTOR_403100_PUFF_EMISSION_TICKS;
    emissionCoord->parent       = &gGfxViewCoord;
    emissionCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    task->state                += 1;
}

/// Emits room puffs on the countdown's eight-update cadence and releases an expired emitter.
///
/// Requires a live coordinate-body task in emission state 1. puffArg is the room
/// puff's packed size/palette/drift word, passed unchanged. Running actors dirty
/// the placement and emit when the pre-decrement halfword countdown is divisible
/// by eight. A fresh ninety-update lifetime emits first on update 3; pause freezes
/// it. Signed expiry clamps the countdown to zero, advances state and immediately
/// kills the task; spawned effects have captured the coordinate placement.
static inline void _actor403100StepPuffEmission(Task* task, u32 puffArg)
{
    GfxCoord* emissionCoord;
    u16       remainingTicks;

    emissionCoord = task->extra.coordBody->coord;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        emissionCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        if (!(task->killCountdown & ACTOR_403100_PUFF_INTERVAL_MASK)) {
            effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, emissionCoord, puffArg, NULL);
        }
        remainingTicks      = task->killCountdown - 1;
        task->killCountdown = remainingTicks;
        if ((s16)remainingTicks <= 0) {
            task->killCountdown = 0;
            task->state        += 1;
            // The terminal wait slot remains unused by normal expiration.
            taskKill(task);
        }
    }
}

/// Starts a ninety-running-update floor-puff emitter and performs its first step.
///
/// Requires an owned coordinate body with its floor placement already set.
/// Initialization runs even while actors pause; emission honors their run gate.
/// Subsequent puffs use rising drift with palette row 2 and size 1024.
static void _actor403100BeginFloorPuffEmission(Task* task)
{
    _actor403100InitializePuffEmitter(task);
    _actor403100EmitFloorPuffs(task);
}

/// Emits rising floor puffs every eight running updates until the emitter expires.
///
/// Requires an owned coordinate body and remaining lifetime in killCountdown.
/// A fresh lifetime is ninety updates; its first puff is on the third running
/// update. Puffs use size 1024 and palette row 2. Pause freezes the timer;
/// expiration advances state and immediately releases the task.
static void _actor403100EmitFloorPuffs(Task* task)
{
    _actor403100StepPuffEmission(task, ACTOR_403100_PUFF_RISING_ARG);
}

/// Counts terminal floor-puff updates and releases the coordinate task at thirty.
///
/// Uses killCountdown as elapsed ticks, incrementing with halfword wrapping
/// and testing it as signed. Entry with zero takes thirty callbacks; this
/// handler has no actor-pause gate. Releasing the task ends its coordinate's lifetime.
static void _actor403100WaitAfterFloorPuffEmission(Task* task)
{
    enum { ACTOR_403100_FLOOR_PUFF_CLEANUP_TICKS = 30 };
    u16 elapsedTicks;

    elapsedTicks        = task->killCountdown + 1;
    task->killCountdown = elapsedTicks;
    if ((s16)elapsedTicks >= ACTOR_403100_FLOOR_PUFF_CLEANUP_TICKS) {
        taskKill(task);
    }
}

/// Starts a ninety-running-update drift emitter with a rising-puff first step.
///
/// Requires an owned, placed coordinate body. Initialization runs while actors
/// pause, then emission honors their run gate. Later callbacks use horizontal
/// drift; a fresh countdown of ninety emits nothing on this first step.
static void _actor403100BeginDriftPuffEmission(Task* task)
{
    _actor403100InitializePuffEmitter(task);
    _actor403100StepPuffEmission(task, ACTOR_403100_PUFF_RISING_ARG);
}

/// Emits horizontally drifting puffs every eight running updates until expiration.
///
/// Requires an owned coordinate body and remaining lifetime in killCountdown.
/// Puffs use size 1024 and palette row 2. Paused actors freeze emission and
/// the timer; expiration advances state and immediately releases the task.
static void _actor403100EmitDriftPuffs(Task* task)
{
    _actor403100StepPuffEmission(task, ACTOR_403100_PUFF_DRIFT_ARG);
}

/// Counts terminal drift-puff updates and releases the coordinate task at thirty.
///
/// `killCountdown` counts elapsed updates here, wrapping as a halfword and
/// tested as signed; zero on entry takes thirty callbacks. This state has no
/// actor-pause gate. Killing the task ends its owned coordinate's lifetime.
static void _actor403100WaitAfterDriftPuffEmission(Task* task)
{
    enum { ACTOR_403100_DRIFT_PUFF_CLEANUP_TICKS = 30 };
    u16 elapsedTicks;

    elapsedTicks        = task->killCountdown + 1;
    task->killCountdown = elapsedTicks;
    if ((s16)elapsedTicks >= ACTOR_403100_DRIFT_PUFF_CLEANUP_TICKS) {
        taskKill(task);
    }
}
/// Leaves scene state 0 idle until an actor command selects a sequence.
///
/// Used after initial model setup and entry into the scene's idle state.
/// Performs no animation, selector or model update; `unusedTask` is unused.
static void _actor403100SceneIdleState0(Task* unusedTask)
{
}

/// Dispatches entrance placement, movement or the final balcony cue, then ticks animation.
///
/// Scene command 1 requires live singleton work/rig and the Burner task/model
/// with `subState` 0..2. Exactly one step runs per dispatch; its final cue waits
/// for another command. Placement also starts playback before this common tick.
static void _actor403100DispatchSceneEntrance(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_actor_403100_80131EB0;
    handlers.funcs[(s16)D_actor_403100_80155808->subState](task);
    _actor403100UpdateAnimation(task);
}
/// Dispatches the aimed scene impact, advances animation, then applies head aiming.
///
/// Scene command 2 requires live singleton work/rig and the Burner task/model
/// with `subState` 0..2. Placement starts playback itself; later steps set the
/// aim mode/target before the common animation and aim updates, even while waiting.
static void _actor403100DispatchSceneAimAndImpact(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_actor_403100_80131EBC;
    handlers.funcs[(s16)D_actor_403100_80155808->subState](task);
    _actor403100UpdateAnimation(task);
    _actor403100AimHead(task, D_actor_403100_80155808->aimMode);
}
/// Dispatches the scripted advance and balcony impact cues with model flags reset.
///
/// Scene command 3 requires live singleton work/rig and the Burner task/model
/// with `subState` 0..3. Placement, movement and impact steps advance animation
/// themselves; the final wait keeps the pose without a common playback tick.
static void _actor403100DispatchSceneAdvance(Task* task)
{
    TaskFuncTable4 handlers;
    TmdObject*     model;

    model        = task->extra.tmd;
    handlers     = D_actor_403100_80131EC8;
    model->flags = 0;
    handlers.funcs[(s16)D_actor_403100_80155808->subState](task);
}
/// Dispatches the scripted flame breath, ticks animation, then applies head aiming.
///
/// Scene command 4 requires live singleton work/rig, initialized flame pool
/// and the Burner task/model with `subState` 0..2. Resets model flags before
/// the step. Its fixed-target breath runs before playback and head aiming;
/// the common updates continue in the final wait.
static void _actor403100DispatchSceneFlameBreath(Task* task)
{
    TaskFuncTable3 handlers;
    TmdObject*     model;

    model        = task->extra.tmd;
    handlers     = D_actor_403100_80131ED8;
    model->flags = 0;
    handlers.funcs[(s16)D_actor_403100_80155808->subState](task);
    _actor403100UpdateAnimation(task);
    _actor403100AimHead(task, D_actor_403100_80155808->aimMode);
}
/// Dispatches the balcony sprite impact and its timed sound and shake cues.
///
/// Scene command 5 requires live singleton work and the Burner task/model
/// with `subState` 0..2. Steps reset or advance the cue timers, then wait for
/// another command. The dispatcher performs no animation or head-aim update.
static void _actor403100DispatchSceneBalconyImpact(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_actor_403100_80131EE4;
    handlers.funcs[(s16)D_actor_403100_80155808->subState](task);
}
/// Dispatches the scene turn and return to walking, then advances animation.
///
/// Scene command 6 requires live singleton work/rig, initialized flame pool
/// and the Burner task/model with `subState` 0..3. Resets model flags before
/// the step; playback continues through the final wait and its walk blend.
static void _actor403100DispatchSceneTurn(Task* task)
{
    TaskFuncTable4 handlers;
    TmdObject*     model;

    model        = task->extra.tmd;
    handlers     = D_actor_403100_80131EF0;
    model->flags = 0;
    handlers.funcs[(s16)D_actor_403100_80155808->subState](task);
    _actor403100UpdateAnimation(task);
}
/// Dispatches the scene's flame bursts and walking retreat, then advances animation.
///
/// Scene command 7 requires live singleton work/rig and the Burner task/model
/// with `subState` 0..3. Resets model flags before each step. Burst clips advance
/// to the retreat, which keeps moving until another command replaces it.
static void _actor403100DispatchSceneFlameRetreat(Task* task)
{
    TaskFuncTable4 handlers;
    TmdObject*     model;

    model        = task->extra.tmd;
    handlers     = D_actor_403100_80131F00;
    model->flags = 0;
    handlers.funcs[(s16)D_actor_403100_80155808->subState](task);
    _actor403100UpdateAnimation(task);
}
/// Dispatches the final scene departure and reward release, then advances animation.
///
/// Scene command 8 requires live singleton work/rig and the Burner task/model
/// with `subState` 0..2. Clear model flags before the step so completion can
/// hide the model for this frame. Completion selects teardown, but the rig
/// remains live for the final playback update; destruction runs next frame.
static void _actor403100DispatchSceneDeparture(Task* task)
{
    TaskFuncTable3 handlers;
    TmdObject*     model;

    model        = task->extra.tmd;
    handlers     = D_actor_403100_80131F10;
    model->flags = 0;
    handlers.funcs[(s16)D_actor_403100_80155808->subState](task);
    _actor403100UpdateAnimation(task);
}
/// Leaves scene state 9 idle while the event script controls the encounter.
///
/// Event handoff selects this slot after ending combat targeting. Performs
/// no animation, selector or model update; `unusedTask` is unused.
static void _actor403100SceneIdleState9(Task* unusedTask)
{
}

/// Places the Burner for the entrance across the balcony's +X axis.
///
/// Scene command 1 starts at (-10000, -600, -9000) in root-parent game
/// coordinates, yaw 0 and Q12 scale 1.25. Requires the live task model and
/// singleton rig. Starts the impact clip immediately, clears the stride/frame
/// counters and advances to the entrance movement; the dispatcher ticks again.
static void _actor403100BeginSceneEntrance(Task* task)
{
    enum { ACTOR_403100_SCENE_ENTRANCE_SCALE_Q12 = ONE * 5 / 4 };
    GfxCoord* rootCoord;

    rootCoord                           = task->extra.tmd->coords;
    D_actor_403100_80155808->sceneScale = ACTOR_403100_SCENE_ENTRANCE_SCALE_Q12;
    _actor403100RequestAnimationReset(ACTOR_403100_ANIMATION_SCENE_IMPACT);
    _actor403100UpdateAnimation();
    D_actor_403100_80155808->rotation.vy = 0;
    rootCoord->coord.t[0]                = -0x2710;
    rootCoord->coord.t[1]                = -0x258;
    rootCoord->coord.t[2]                = -0x2328;
    D_actor_403100_80155808->stridePhase = 0;
    D_actor_403100_80155808->stateFrames = 0;
    D_actor_403100_80155808->subState    = D_actor_403100_80155808->subState + 1;
}

/// Holds the final entrance substate and changes scene sprites after thirty calls.
///
/// Requires live singleton work with the timer reset by entrance movement.
/// Increments its wrapping halfword timer and selects scene-sprite step 1 at
/// signed tick 30. Does not advance the substate; `unusedTask` is ignored.
static void _actor403100WaitAfterSceneEntrance(Task* unusedTask)
{
    enum {
        ACTOR_403100_ENTRANCE_WAIT_SPRITE_TICK = 30,
    };
    u16 nextFrame;

    nextFrame                            = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = nextFrame;
    if ((s16)nextFrame == ACTOR_403100_ENTRANCE_WAIT_SPRITE_TICK) {
        dryfieldNightMotelBalconyUpdateSceneSprites(ACTOR_403100_SCENE_SPRITE_CUE_1);
    }
}

/// Places the Burner for the balcony scene's aimed impact.
///
/// Scene command 2 restarts the impact clip, initially lets animation aim the
/// head and advances to the timed aim/impact cues. The root is (-5904, 0,
/// -10310) in parent game coordinates, yaw 0 and Q12 scale 0x1910. Requires
/// live task/model and singleton rig; the dispatcher applies yaw and scale.
static void _actor403100BeginSceneAimAndImpact(Task* task)
{
    enum { ACTOR_403100_SCENE_AIM_AND_IMPACT_SCALE_Q12 = 0x1910 };
    GfxCoord* rootCoord;

    rootCoord                                 = task->extra.tmd->coords;
    D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
    D_actor_403100_80155808->aimMode          = ACTOR_403100_AIM_ANIMATED;
    D_actor_403100_80155808->animationId      = ACTOR_403100_ANIMATION_SCENE_IMPACT;
    D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
    _actor403100UpdateAnimation(task);
    D_actor_403100_80155808->rotation.vy = 0;
    rootCoord->coord.t[0]                = -0x1710;
    rootCoord->coord.t[1]                = 0;
    rootCoord->coord.t[2]                = -0x2846;
    D_actor_403100_80155808->sceneScale  = ACTOR_403100_SCENE_AIM_AND_IMPACT_SCALE_Q12;
    D_actor_403100_80155808->stateFrames = 0;
    D_actor_403100_80155808->subState   += 1;
}
/// Leaves scene command 2 at its final substate until another command arrives.
///
/// Animation and head aiming continue in the dispatcher. `task` is unused.
static void _actor403100WaitAfterSceneAimAndImpact(Task* task)
{
}

/// Advances the Burner 100 parent-coordinate units along +Z for 32 scene ticks.
///
/// Runs scene command 3 after placement, with a live task/model and singleton
/// work. The frame counter wraps as a halfword; exactly tick 32 clears it and
/// advances to the impact cues. Each call also ticks the animation.
static void _actor403100StepSceneAdvance(Task* task)
{
    enum { ACTOR_403100_SCENE_ADVANCE_FRAMES = 32 };
    GfxCoord* rootCoord;

    rootCoord                             = task->extra.tmd->coords;
    D_actor_403100_80155808->stateFrames += 1;
    rootCoord->coord.t[2]                += 100;
    if ((s16)D_actor_403100_80155808->stateFrames == ACTOR_403100_SCENE_ADVANCE_FRAMES) {
        D_actor_403100_80155808->stateFrames = 0;
        D_actor_403100_80155808->subState   += 1;
    }
    _actor403100UpdateAnimation();
}
/// Leaves scene command 3 at its final substate until another command arrives.
///
/// The dispatcher continues drawing the placed model. `task` is unused.
static void _actor403100WaitAfterSceneAdvance(Task* task)
{
}

/// Leaves scene command 4 at its final substate after the flame-breath clip ends.
///
/// Animation and head aiming continue in the dispatcher. `task` is unused.
static void _actor403100WaitAfterSceneFlameBreath(Task* task)
{
}

/// Starts the timed sprite impact on the balcony during scene command 5.
///
/// Clears the singleton's cue timer and sprite-sequence cursor, then advances
/// to the timed sound, shake and sprite changes. `task` is unused.
static void _actor403100BeginSceneBalconyImpact(Task* task)
{
    D_actor_403100_80155808->stateFrames = 0;
    D_actor_403100_80155808->auxFrames   = 0;
    D_actor_403100_80155808->subState    = D_actor_403100_80155808->subState + 1;
}

/// Leaves scene command 5 at its final substate after the balcony sprite cues.
///
/// Waits for another actor command without advancing counters. `task` is unused.
static void _actor403100WaitAfterSceneBalconyImpact(Task* task)
{
}

/// Blends into the walk pose after scene command 6's turn clip settles.
///
/// Requires the live singleton rig. Waits specifically for slot 2's held
/// boundary pose, then requests a 180-frame blend at normal rate and advances
/// to the final substate. The dispatcher applies the request. `task` is unused.
static void _actor403100BlendSceneWalk(Task* task)
{
    enum { ACTOR_403100_SCENE_WALK_BLEND_FRAMES = 180 };
    if (_actor403100Slot2AnimationSettled()) {
        _actor403100RequestAnimationBlendInline(ACTOR_403100_ANIMATION_WALK, ANIMATION_RATE_ONE, ACTOR_403100_SCENE_WALK_BLEND_FRAMES);
        D_actor_403100_80155808->subState += 1;
    }
}

/// Leaves scene command 6 at its final substate with the walk clip playing.
///
/// The dispatcher continues animation until another command arrives. `task` is unused.
static void _actor403100WaitAfterSceneTurn(Task* task)
{
}

/// Places the Burner for the flame bursts and retreat of scene command 7.
///
/// Requires live task/model and singleton rig. Starts clip 20 on the next
/// dispatcher update, with the root at (-1180, 0, 4400) in parent game units,
/// yaw three quarters of a turn and Q12 scale 1.25. The 16-entry ordering bias
/// lasts through the flame-burst clips and is cleared when retreat starts.
static void _actor403100BeginSceneFlameRetreat(Task* task)
{
    enum {
        ACTOR_403100_ANIMATION_SCENE_FLAME_RETREAT = 20,
        ACTOR_403100_SCENE_FLAME_RETREAT_SCALE_Q12 = ONE * 5 / 4,
        ACTOR_403100_SCENE_FLAME_RETREAT_OT_OFFSET = 16,
    };
    TmdObject* model;
    GfxCoord*  rootCoord;

    model                 = task->extra.tmd;
    model->otOffset       = ACTOR_403100_SCENE_FLAME_RETREAT_OT_OFFSET;
    rootCoord             = model->coords;
    rootCoord->coord.t[0] = -0x49C;
    rootCoord->coord.t[2] = 0x1130;
    rootCoord->coord.t[1] = 0;

    D_actor_403100_80155808->rotation.vy = ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4;
    _actor403100RequestAnimationReset(ACTOR_403100_ANIMATION_SCENE_FLAME_RETREAT);
    D_actor_403100_80155808->sceneScale  = ACTOR_403100_SCENE_FLAME_RETREAT_SCALE_Q12;
    D_actor_403100_80155808->stateFrames = 0;
    D_actor_403100_80155808->subState    = D_actor_403100_80155808->subState + 1;
}

/// Places the Burner for the final departure along the room's -Z axis.
///
/// Scene command 8 starts clip 24 on the next dispatcher update and clears
/// both frame counters and the stride phase. Requires live task/model and
/// singleton rig. Root position is (-2800, 768, -3700) in parent game units,
/// with half-turn yaw and Q12 scale 1.25; the timed departure lasts 280 ticks.
static void _actor403100BeginSceneDeparture(Task* task)
{
    enum {
        ACTOR_403100_ANIMATION_SCENE_DEPARTURE = 24,
        ACTOR_403100_SCENE_DEPARTURE_SCALE_Q12 = ONE * 5 / 4,
    };
    GfxCoord* rootCoord;

    rootCoord             = task->extra.tmd->coords;
    rootCoord->coord.t[0] = -0xAF0;
    rootCoord->coord.t[1] = 0x300;
    rootCoord->coord.t[2] = -0xE74;

    D_actor_403100_80155808->rotation.vy = ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    _actor403100RequestAnimationReset(ACTOR_403100_ANIMATION_SCENE_DEPARTURE);
    D_actor_403100_80155808->sceneScale  = ACTOR_403100_SCENE_DEPARTURE_SCALE_Q12;
    D_actor_403100_80155808->stridePhase = 0;
    D_actor_403100_80155808->stateFrames = 0;
    D_actor_403100_80155808->auxFrames   = 0;
    D_actor_403100_80155808->subState    = D_actor_403100_80155808->subState + 1;
}

/// Finishes departure with no BP/MP and half EXP, then selects actor teardown.
///
/// Requires live task/model, singleton work, session and enemy parameters.
/// Hides the model and selects balcony variant 4 before releasing the battle
/// hold. Reward changes persist in the package parameter record, so repeating
/// this callback would halve EXP again; the task advances to exit after one call.
static void _actor403100FinishSceneDeparture(Task* task)
{
    enum {
        ACTOR_403100_DEPARTURE_ROOM_VARIANT = 4,
        ACTOR_403100_TASK_EXIT              = 5,
    };
    TmdObject* model;

    model                              = task->extra.tmd;
    model->flags                      |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    D_actor_403100_8014762C.bp         = 0;
    D_actor_403100_8014762C.mp         = 0;
    D_actor_403100_8014762C.exp      >>= 1;
    gGameSession->location.loc.variant = ACTOR_403100_DEPARTURE_ROOM_VARIANT;
    sceneReleaseBattleRefWithRewards(task, 0);
    task->state                       = ACTOR_403100_TASK_EXIT;
    D_actor_403100_80155808->state    = 0;
    D_actor_403100_80155808->subState = 0;
}
/// Waits thirty-one uninterrupted updates after fight setup before starting attack approach.
///
/// Requires live singleton work with a cleared `stateFrames` counter. Stagger
/// and build-up stun divert control without advancing this timer. The timer
/// wraps as u16 and is compared as s16; `task` is retained for dispatch only.
static void _actor403100WaitForFightStart(Task* task)
{
    enum { ACTOR_403100_FIGHT_START_FRAMES = 31 };
    u16 elapsedFrames;

    if (_actor403100HandleHitReaction() == 0) {
        elapsedFrames                        = D_actor_403100_80155808->stateFrames + 1;
        D_actor_403100_80155808->stateFrames = elapsedFrames;
        if ((s16)elapsedFrames >= ACTOR_403100_FIGHT_START_FRAMES) {
            D_actor_403100_80155808->state    = ACTOR_403100_BEHAVIOUR_ATTACK_APPROACH;
            D_actor_403100_80155808->subState = 0;
        }
    }
}

/// Starts the walking approach that waits for the player to enter attack range.
///
/// Requires live singleton work. Blends into the normal-rate walk over four
/// frames, selects normal head tracking and a 32-unit movement step, then
/// advances to the range gate. `task` is retained for the dispatch signature.
static void _actor403100BeginAttackApproach(Task* task)
{
    enum { ACTOR_403100_APPROACH_BLEND_FRAMES = 4 };
    _actor403100RequestAnimationBlendInline(ACTOR_403100_ANIMATION_WALK, ANIMATION_RATE_ONE, ACTOR_403100_APPROACH_BLEND_FRAMES);
    D_actor_403100_80155808->walkSpeed   = 0x20;
    D_actor_403100_80155808->walkStage   = ACTOR_403100_WALK_APPROACH;
    D_actor_403100_80155808->aimMode     = ACTOR_403100_AIM_TRACK;
    D_actor_403100_80155808->stateFrames = 0;
    D_actor_403100_80155808->subState    = D_actor_403100_80155808->subState + 1;
}

/// Resumes attack approach when the combo's fast walking recovery reaches a transition.
///
/// Requires live singleton work and slot 1. A boundary, control jump or
/// settled pose resets behaviour/substate to approach step zero.
/// `unusedTask` is retained for dispatch.
static void _actor403100FinishArmSwingRecovery(Task* unusedTask)
{
    if (_actor403100AnimationAtBoundaryOrJump()) {
        D_actor_403100_80155808->state    = ACTOR_403100_BEHAVIOUR_ATTACK_APPROACH;
        D_actor_403100_80155808->subState = 0;
    }
}
/// Waits fourteen updates after an uninterrupted aimed breath before resuming approach.
///
/// Requires live singleton work with stateFrames initially zero. Tests the
/// saved pre-increment value as signed; the unsigned halfword still wraps.
/// The enclosing dispatcher handles hit interruptions. `unusedTask` is retained for dispatch.
static void _actor403100WaitAfterAimedFlameAttack(Task* unusedTask)
{
    u16 previousFrames;

    previousFrames                       = D_actor_403100_80155808->stateFrames;
    D_actor_403100_80155808->stateFrames = previousFrames + 1;
    if ((s16)previousFrames >= ACTOR_403100_FLAME_RECOVERY_FRAMES - 1) {
        D_actor_403100_80155808->state    = ACTOR_403100_BEHAVIOUR_ATTACK_APPROACH;
        D_actor_403100_80155808->subState = 0;
    }
}

/// Starts the impact continuation after an aimed flame touches a player body.
///
/// Requires live singleton work. Resets its cue timer, fades the breath sound
/// over ten audio updates and blends to the impact clip over twenty normal
/// frames at 7/4 normal speed. The next substate supplies its timed cues;
/// `unusedTask` is retained for dispatch.
static void _actor403100BeginAimedFlameContactRecovery(Task* unusedTask)
{
    D_actor_403100_80155808->stateFrames = 0;
    sndEvtRequestScriptStop(ACTOR_403100_BREATH_SOUND, ACTOR_403100_FLAME_CONTACT_STOP_FADE_UPDATES);
    _actor403100RequestAnimationBlendInline(ACTOR_403100_ANIMATION_SCENE_IMPACT, ACTOR_403100_FLAME_CONTACT_RECOVERY_RATE, ACTOR_403100_FLAME_CONTACT_RECOVERY_BLEND_FRAMES);
    D_actor_403100_80155808->subState = D_actor_403100_80155808->subState + 1;
}

/// Clears arm contact and pose offsets while asking the walk to finish before a jump slam.
///
/// Requires live singleton work. A hit reaction can divert the behaviour;
/// otherwise fast yaw-only head tracking remains active while the stride
/// finishes. The next substate waits for walking to stop. `unusedTask` is retained for dispatch.
static void _actor403100PrepareJumpSlam(Task* unusedTask)
{
    if (_actor403100HandleHitReaction() == 0) {
        D_actor_403100_80155808->handTouchedPlayer    = 0;
        D_actor_403100_80155808->forearmTouchedPlayer = 0;
        D_actor_403100_80155808->aimMode              = ACTOR_403100_AIM_YAW_ONLY_FAST;
        D_actor_403100_80155808->walkStage            = ACTOR_403100_WALK_FINISH_STRIDE;
        D_actor_403100_80155808->armPitch             = 0;
        D_actor_403100_80155808->armYaw               = 0;
        D_actor_403100_80155808->subState             = D_actor_403100_80155808->subState + 1;
    }
}

/// Starts the jump-slam clip once walking has stopped and no hit diverts the attack.
///
/// Requires live singleton work. Restarts the arm-attack clip at normal speed,
/// clears the crouch timer and advances to the root-lowering step.
/// `unusedTask` is retained for dispatch.
static void _actor403100BeginJumpSlam(Task* unusedTask)
{
    if ((_actor403100HandleHitReaction() == 0) &&
        (D_actor_403100_80155808->walkStage == ACTOR_403100_WALK_STOPPED)) {
        D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
        D_actor_403100_80155808->animationId      = ACTOR_403100_ANIMATION_ARM_ATTACK;
        D_actor_403100_80155808->stateFrames      = 0;
        D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
        D_actor_403100_80155808->subState         = D_actor_403100_80155808->subState + 1;
    }
}

/// Waits ninety updates after jump-slam recovery before resuming attack approach.
///
/// Requires live singleton work with stateFrames initially zero. Increments
/// with unsigned halfword wrapping, then compares the new value as signed.
/// `unusedTask` is retained for dispatch.
static void _actor403100WaitAfterJumpSlam(Task* unusedTask)
{
    enum { ACTOR_403100_JUMP_RECOVERY_FRAMES = 90 };
    u16 elapsedFrames;

    elapsedFrames                        = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames >= ACTOR_403100_JUMP_RECOVERY_FRAMES) {
        D_actor_403100_80155808->state    = ACTOR_403100_BEHAVIOUR_ATTACK_APPROACH;
        D_actor_403100_80155808->subState = 0;
    }
}

/// Moves the Burner to the held-player lift placement and starts its lift clip.
///
/// Requires the live Burner model, singleton work and a player already under
/// scripted hold. Stops walking, places the root at (-1100, -5000, 10000)
/// in parent coordinates with yaw of 5/8 turn, and samples the new grip
/// before selecting view twenty-four and clearing foreground offsets.
static void _actor403100BeginHeldPlayerLift(Task* task)
{
    enum { ACTOR_403100_ANIMATION_HELD_LIFT = 11,
           ACTOR_403100_HELD_LIFT_VIEW      = 24 };
    GfxCoord* rootCoord;

    rootCoord                                 = task->extra.tmd->coords;
    D_actor_403100_80155808->walkStage        = ACTOR_403100_WALK_STOPPED;
    rootCoord->coord.t[0]                     = -1100;
    rootCoord->coord.t[1]                     = -5000;
    rootCoord->coord.t[2]                     = 10000;
    D_actor_403100_80155808->rotation.vy      = ACTOR_TRANSFORM_ANGLE_TURN * 5 / 8;
    D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
    D_actor_403100_80155808->animationId      = ACTOR_403100_ANIMATION_HELD_LIFT;
    D_actor_403100_80155808->rotation.vx      = 0;
    D_actor_403100_80155808->rotation.vz      = 0;
    D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
    // The grip must be sampled from the lift clip before changing the camera.
    _actor403100PlaceHeldPlayer(task);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTOR_403100_HELD_LIFT_VIEW;
    D_actor_403100_80155808->overlayX                          = 0;
    D_actor_403100_80155808->overlayY                          = 0;
    D_actor_403100_80155808->subState                         += 1;
}
/// Resumes attack approach when the reversed grab clip reaches a transition.
///
/// Runs after a forearm-only grab contact. Requires live singleton work and
/// tests slot 1's results saved by the previous animation update, including
/// boundary, control jump and settled pose. `unusedTask` is retained for dispatch.
static void _actor403100FinishGrabRewind(Task* unusedTask)
{
    if (_actor403100PreviousAnimationAtBoundaryOrJump()) {
        D_actor_403100_80155808->state    = ACTOR_403100_BEHAVIOUR_ATTACK_APPROACH;
        D_actor_403100_80155808->subState = 0;
    }
}
/// Waits fourteen updates after an uninterrupted side breath before resuming approach.
///
/// Requires live singleton work with stateFrames initially zero. Tests the
/// saved pre-increment value as signed; the unsigned halfword still wraps.
/// The enclosing dispatcher handles hit interruptions. `unusedTask` is retained for dispatch.
static void _actor403100WaitAfterSideFlameAttack(Task* unusedTask)
{
    u16 previousFrames;

    previousFrames                       = D_actor_403100_80155808->stateFrames;
    D_actor_403100_80155808->stateFrames = previousFrames + 1;
    if ((s16)previousFrames >= ACTOR_403100_FLAME_RECOVERY_FRAMES - 1) {
        D_actor_403100_80155808->state    = ACTOR_403100_BEHAVIOUR_ATTACK_APPROACH;
        D_actor_403100_80155808->subState = 0;
    }
}
/// Starts the impact continuation after a side flame touches a player body.
///
/// Requires live singleton work. Resets its cue timer, fades the breath sound
/// over ten audio updates and blends to the impact clip over twenty normal
/// frames at 7/4 normal speed. The next substate supplies its timed cues;
/// `unusedTask` is retained for dispatch.
static void _actor403100BeginSideFlameContactRecovery(Task* unusedTask)
{
    D_actor_403100_80155808->stateFrames = 0;
    sndEvtRequestScriptStop(ACTOR_403100_BREATH_SOUND, ACTOR_403100_FLAME_CONTACT_STOP_FADE_UPDATES);
    _actor403100RequestAnimationBlendInline(ACTOR_403100_ANIMATION_SCENE_IMPACT, ACTOR_403100_FLAME_CONTACT_RECOVERY_RATE, ACTOR_403100_FLAME_CONTACT_RECOVERY_BLEND_FRAMES);
    D_actor_403100_80155808->subState += 1;
}
/// Starts the grab-and-drag reach with fresh arm contact and normalized heading.
///
/// Requires live singleton work. A hit reaction can divert initialization;
/// otherwise finishes the current stride, restarts the drag-reach clip at
/// three-quarter speed and returns head aim to the animation. Heading is
/// masked to 0..4095 before the reach step eases it. `unusedTask` is retained for dispatch.
static void _actor403100BeginGrabAndDrag(Task* unusedTask)
{
    enum { ACTOR_403100_ANIMATION_DRAG_REACH = 8 };
    if (_actor403100HandleHitReaction() == 0) {
        D_actor_403100_80155808->walkStage            = ACTOR_403100_WALK_FINISH_STRIDE;
        D_actor_403100_80155808->animationRate        = ANIMATION_RATE_ONE * 3 / 4;
        D_actor_403100_80155808->aimMode              = ACTOR_403100_AIM_ANIMATED;
        D_actor_403100_80155808->animationId          = ACTOR_403100_ANIMATION_DRAG_REACH;
        D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_RESET;
        D_actor_403100_80155808->stateFrames          = 0;
        D_actor_403100_80155808->rotation.vy          = (u16)D_actor_403100_80155808->rotation.vy & ACTOR_TRANSFORM_ANGLE_MASK;
        D_actor_403100_80155808->handTouchedPlayer    = 0;
        D_actor_403100_80155808->forearmTouchedPlayer = 0;
        D_actor_403100_80155808->subState            += 1;
    }
}
/// Begins the normal-speed stagger selected by hit reaction two.
///
/// Requires live singleton work. Returns head aim to the animated pose, finishes
/// the walking stride and blends into the stagger clip over eight normal-rate
/// frames. The behaviour dispatcher advances animation after this callback.
static void _actor403100BeginStagger(void)
{
    enum { ACTOR_403100_STAGGER_BLEND_FRAMES = 8 };
    D_actor_403100_80155808->aimMode   = ACTOR_403100_AIM_ANIMATED;
    D_actor_403100_80155808->walkStage = ACTOR_403100_WALK_FINISH_STRIDE;
    _actor403100RequestAnimationBlendInline(ACTOR_403100_ANIMATION_STAGGER, ANIMATION_RATE_ONE, ACTOR_403100_STAGGER_BLEND_FRAMES);
    D_actor_403100_80155808->subState = D_actor_403100_80155808->subState + 1;
}

/// Returns from stagger to attack approach when the clip reaches a transition.
///
/// Requires live singleton work and animation slots. A boundary, jump or
/// settled pose resets behaviour/substate to attack approach at step zero.
static void _actor403100FinishStagger(void)
{
    if (_actor403100AnimationAtBoundaryOrJump()) {
        D_actor_403100_80155808->state    = ACTOR_403100_BEHAVIOUR_ATTACK_APPROACH;
        D_actor_403100_80155808->subState = 0;
    }
}
/// Starts the half-speed stagger used when a hit interrupts an attack build-up.
///
/// Requires live singleton work. Returns head aim to the animated pose and
/// asks walking to finish its stride before stopping. `task` is unused and
/// retained for the behaviour dispatch signature.
static void _actor403100BeginBuildupStun(Task* task)
{
    enum { ACTOR_403100_BUILDUP_STUN_BLEND_FRAMES = 10 };
    D_actor_403100_80155808->aimMode   = ACTOR_403100_AIM_ANIMATED;
    D_actor_403100_80155808->walkStage = ACTOR_403100_WALK_FINISH_STRIDE;
    _actor403100RequestAnimationBlendInline(ACTOR_403100_ANIMATION_STAGGER, ANIMATION_RATE_ONE / 2, ACTOR_403100_BUILDUP_STUN_BLEND_FRAMES);
    D_actor_403100_80155808->subState += 1;
}
/// Blends back to the walk clip when the build-up stun animation reaches a transition.
///
/// Requires the live animation slots and work block. A boundary, jump or settled
/// pose starts a four-frame normal-rate walk blend and resets the recovery timer.
/// `task` is retained for the dispatch signature.
static void _actor403100FinishBuildupStunAnimation(Task* task)
{
    enum { ACTOR_403100_STUN_RECOVERY_BLEND_FRAMES = 4 };
    if (_actor403100AnimationAtBoundaryOrJump()) {
        D_actor_403100_80155808->animationBlendFrames = ACTOR_403100_STUN_RECOVERY_BLEND_FRAMES;
        D_actor_403100_80155808->animationRate        = ANIMATION_RATE_ONE;
        D_actor_403100_80155808->stateFrames          = 0;
        D_actor_403100_80155808->animationId          = ACTOR_403100_ANIMATION_WALK;
        D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
        D_actor_403100_80155808->subState            += 1;
    }
}
/// Waits 62 updates after the stagger before resuming the attack approach.
///
/// Requires live singleton work with `stateFrames` initially zero. Tests the
/// saved pre-increment value as signed; the unsigned halfword still wraps.
/// `task` is retained for the dispatch signature.
static void _actor403100WaitAfterBuildupStun(Task* task)
{
    enum { ACTOR_403100_STUN_RECOVERY_FRAMES = 62 };
    u16 previousFrames;

    previousFrames                       = D_actor_403100_80155808->stateFrames;
    D_actor_403100_80155808->stateFrames = previousFrames + 1;
    if ((s16)previousFrames >= ACTOR_403100_STUN_RECOVERY_FRAMES - 1) {
        D_actor_403100_80155808->state    = ACTOR_403100_BEHAVIOUR_ATTACK_APPROACH;
        D_actor_403100_80155808->subState = 0;
    }
}

/// Leaves the walking pose unchanged in walkStage 2.
///
/// Empty dispatch callback; it neither reads the task nor advances the stride or stage.
static void _actor403100WalkIdleState2(Task* task)
{
}

/// Leaves the walking pose unchanged in walkStage 3.
///
/// Empty dispatch callback; it neither reads the task nor advances the stride or stage.
static void _actor403100WalkIdleState3(Task* task)
{
}

/// Leaves the walking pose unchanged in ACTOR_403100_WALK_STOPPED.
///
/// Empty dispatch callback; the caller still performs its separate root-X correction.
/// It neither reads the task nor advances the stride or stage.
static void _actor403100WalkStopped(Task* task)
{
}
