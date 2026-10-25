#include "helicopter_landing_pad_no_9_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

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
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
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
#include "../../event_flags.h"

#include "assets/enemies/golem/no_9/helipad/body_schema.h"
#include "assets/enemies/golem/no_9/sounds_akropolis.h"
#include "assets/enemies/golem/no_9/helipad/cable_schema.h"
#include "assets/enemies/golem/no_9/helipad/player_grenade_schema.h"

static void _no9GolemAimHead(const Task* actor);
static void _no9GolemDrawShadow(const Task* task);

/// Steps within the selected combat state; values are stored as signed halfwords.
enum {
    OPENING_INTRO                 = 0,
    OPENING_WINDUP                = 1,
    OPENING_FIRST_SLASH           = 2,
    OPENING_SECOND_SLASH          = 3,
    OPENING_LUNGE                 = 4,
    PATROL_WAIT                   = 0,
    PATROL_WALK_START             = 1,
    PATROL_WALK                   = 2,
    PATROL_WALK_FIRE              = 3,
    PATROL_CATCH_UP               = 4,
    PATROL_SHOTGUN_RESPONSE_START = 5,
    PATROL_SHOTGUN_RESPONSE_WALK  = 6,
    PATROL_SHOTGUN_RESPONSE_END   = 7,
    PATROL_BACKSTEP               = 8,
    SLASH_WINDUP                  = 0,
    SLASH_FIRST                   = 1,
    SLASH_SECOND                  = 2,
    SLASH_RECOVER                 = 3,
    FLAME_LUNGE_WINDUP            = 0,
    FLAME_LUNGE_CHARGE            = 1,
    DASH_START                    = 0,
    DASH_RUN                      = 1,
    DASH_STRIKE                   = 2,
    RECOIL_ENTER                  = 0,
    RECOIL_PLAY                   = 1,
    DEATH_ENTER                   = 0,
    SPARK_RECOIL_ENTER            = 0,
    SPARK_RECOIL_PLAY             = 1,
    SPARK_STUN_ENTER              = 0,
    SPARK_STUN_PLAY               = 1,
    SPARK_STUN_RECOVER            = 2,
    BUILDUP_STUN_WAIT             = 0,
    BUILDUP_STUN_RECOVER          = 1,
    FLINCH_ENTER                  = 0,
    FLINCH_PLAY                   = 1,
};

/// Attack-table indices packed into both weapon and forearm collision keys.
enum {
    ATTACK_DASH        = 0,
    ATTACK_FLAME_LUNGE = 1,
    ATTACK_SLASH       = 2,
    ATTACK_LETHAL      = 3,
    ATTACK_GRENADE     = 4,
    ATTACK_IGNITION    = 5,
};

/// Script keys before the placement index is packed into bits 8..11.
enum {
    SOUND_STEP_CUE_2       = SOUND_CHARACTER(SOUND_BANK_NO_9_AKROPOLIS, NO_9_AKROPOLIS_SOUND_FOOTSTEP_A),
    SOUND_STEP_CUE_1       = SOUND_CHARACTER(SOUND_BANK_NO_9_AKROPOLIS, NO_9_AKROPOLIS_SOUND_FOOTSTEP_B),
    SOUND_SHOTGUN_RESPONSE = SOUND_CHARACTER(SOUND_BANK_NO_9_AKROPOLIS, NO_9_AKROPOLIS_SOUND_SHOTGUN_REACT),
    SOUND_RECOIL           = SOUND_CHARACTER(SOUND_BANK_NO_9_AKROPOLIS, NO_9_AKROPOLIS_SOUND_HURT_RECOIL),
    SOUND_WINDUP           = SOUND_CHARACTER(SOUND_BANK_NO_9_AKROPOLIS, NO_9_AKROPOLIS_SOUND_WINDUP),
    SOUND_FLAME_STRIKE     = SOUND_CHARACTER(SOUND_BANK_NO_9_AKROPOLIS, NO_9_AKROPOLIS_SOUND_FLAME_STRIKE),
    SOUND_BACKSTEP         = SOUND_CHARACTER(SOUND_BANK_NO_9_AKROPOLIS, NO_9_AKROPOLIS_SOUND_BACKSTEP),
    SOUND_SPARK_LOOP       = SOUND_CHARACTER(SOUND_BANK_NO_9_AKROPOLIS, NO_9_AKROPOLIS_SOUND_SPARK_LOOP),
    SOUND_SLASH_RECOVER    = SOUND_CHARACTER(SOUND_BANK_NO_9_AKROPOLIS, NO_9_AKROPOLIS_SOUND_SLASH_RECOVER),
    SOUND_SLASH            = SOUND_CHARACTER(SOUND_BANK_NO_9_AKROPOLIS, NO_9_AKROPOLIS_SOUND_SLASH),
    SOUND_DASH_STRIKE      = SOUND_CHARACTER(SOUND_BANK_NO_9_AKROPOLIS, NO_9_AKROPOLIS_SOUND_DASH_STRIKE),
    SOUND_FLAME_LOOP       = SOUND_CHARACTER(SOUND_BANK_NO_9_AKROPOLIS, NO_9_AKROPOLIS_SOUND_FLAME_LOOP),
    SOUND_FIRE             = SOUND_CHARACTER(SOUND_BANK_NO_9_AKROPOLIS, NO_9_AKROPOLIS_SOUND_FIRE),
    SOUND_DASH_START       = SOUND_CHARACTER(SOUND_BANK_NO_9_AKROPOLIS, NO_9_AKROPOLIS_SOUND_DASH_START),
};

/// Frame thresholds shared by the spark and buildup reactions.
enum {
    SPARK_MIX_INTERVAL     = 3,
    STUN_RECOVER_END_FRAME = 59,
};

/// Lap and range thresholds in world units; speeds are world units per frame.
enum {
    FIRST_TRANSFORMER_SLASH_DISTANCE  = 0x4B00,
    SECOND_TRANSFORMER_SLASH_DISTANCE = 0x7F00,
    TRANSFORMER_SLASH_WINDOW          = 0x120,
    LAST_ATTACK_DISTANCE              = 0xAE9C,
    LETHAL_ATTACK_DISTANCE            = 0xB477,
    PATROL_END_DISTANCE               = 0xB478,
    SLASH_RANGE                       = 0xAF0,
    FLAME_LUNGE_RANGE                 = 0xED8,
    MIXED_ATTACK_RANGE                = 0x12C0,
    PATROL_WALK_SPEED                 = 0x1D,
    DASH_SPEED                        = 0x84,
    MAX_LUNGE_DISTANCE                = 0x1388,
    LAP_MIN_DISTANCE                  = 0xC8,
    LAP_MAX_DISTANCE                  = 0xB66C,
};

/// The latch's two positive values select transformer 0 or 1 for a slash.
enum {
    TRANSFORMER_SLASH_NONE   = 0,
    TRANSFORMER_SLASH_FIRST  = 1,
    TRANSFORMER_SLASH_SECOND = 2,
};

/// The ammunition row interval that triggers the patrol's shotgun response.
enum {
    SHOTGUN_FIRST_AMMO_ROW = 10,
    SHOTGUN_AMMO_ROW_COUNT = 3,
};

/// The three attached models have initialization and update states only.
enum {
    ATTACHMENT_UPDATE      = 1,
    ATTACHMENT_STATE_COUNT = 2,
};

/// Script message selecting this golem's show, fight-start or hide command.
enum { MESSAGE_SET_ACTIVATION = 2007 };

/// Task states of the grenade and the two stationary child hazards.
enum {
    CHILD_TASK_RUNNING = 1,
    GRENADE_TASK_BURST = 2,
};

/// Progress of the lethal attack; once struck, incoming damage cannot kill the golem.
enum {
    LETHAL_PHASE_NONE   = 0,
    LETHAL_PHASE_WINDUP = 1,
    LETHAL_PHASE_STRUCK = 2,
};

/// Hazard rows whose blast contacts this golem consumes.
enum {
    HAZARD_TRANSFORMER_BLAST = 2,
    HAZARD_SOURCE_RECOIL     = 3,
    HAZARD_SOURCE_STUN       = 4,
};

/// States sent to the transformer's adopted spark controller.
enum {
    CABLE_SPARKS_BURSTS_ONLY = 1,
    CABLE_SPARKS_RELEASE     = 2,
};

/// States sent to the blast source's adopted flare controller.
enum {
    SOURCE_FLARE_FLICKERING = 0,
    SOURCE_FLARE_RAW_COLOR  = 1,
    SOURCE_FLARE_EMBERS     = 2,
    SOURCE_FLARE_RELEASE    = 3,
};

static s32 _helicopterLandingPadNo9SetActivation(Task* task, s32 messageId, s32 command, s32 unusedArg);

static s32 _helicopterLandingPadNo9ReleaseGrenadeHold(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);

static s32 _helicopterLandingPadNo9PlayEventAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedSecondArg);

static s32 _helicopterLandingPadNo9SetModelDraw(Task* task, s32 messageId, s32 drawEnabled, s32 unusedSecondArg);

static s32 _helicopterLandingPadNo9IsPresent(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);

enum {
    TRANSFORMER_COUNT     = 3,
    VIEWS_PER_TRANSFORMER = 3,
};

/// Mapped camera-view IDs that enable each transformer's active update; the blast source shares the third transformer's first.
typedef struct {
    u16  transformerViews[TRANSFORMER_COUNT][VIEWS_PER_TRANSFORMER]; // Mapped camera-view IDs per transformer; compared with the current view's low byte
    byte field_12[2];                                                // Unaccessed zero bytes; original type and role unproven
} _HelipadViews;
STATIC_ASSERT_SIZEOF(_HelipadViews, 0x14);

/// Scratch block of the step that carries the golem's three wall faces in the room's collision grid to where the golem
/// stands.
typedef struct {
    SVECTOR origin;  // The faces' origin: first its offset from the golem in the golem's axes, then its world position, held within 0x1770 of the world origin in X and Z
    SVECTOR rotated; // Latest vector turned by the golem's facing: the origin's offset, then each corner in turn
} _GridFacesScratch;
STATIC_ASSERT_SIZEOF(_GridFacesScratch, 0x10);

/// Values of `_TransformerWork::state`.
enum {
    TRANSFORMER_INTACT   = 0, // Whole: a shot or the golem's slash breaks it
    TRANSFORMER_SPARKING = 1, // Broken and throwing sparks; its blast can strike once
    TRANSFORMER_DONE     = 2, // The sparks have run out, or the golem is gone
};

/// Work block of one of the three transformers, kept at `Task::work` of the cable model's task.
typedef struct {
    AnimationContext      anim;                                   // Context bound to `slots`, `poses` and the model's part coordinates
    AnimationSlot         slots[11];                              // Playback state of the model part at the same index; slots 1..10 are driven
    u8                    poses[11][ANIMATION_POSE_BUFFER_BYTES]; // Encoded transition pose of the slot at the same index
    MATRIX                colorMtx;                               // Colour matrix lent to the transformer's cable
    MATRIX                lightMtx;                               // Light matrix lent to the same model
    WorldCollisionBody    body;                                   // Sphere of radius 0xC8 on the cable's tip, on list 2, receiving attacks; enabled only while the transformer is intact and in view
    WorldCollisionContact bodyContacts[1];                        // Contact of `body`, read and cleared every frame the transformer is intact
    WorldCollisionBody    blast;                                  // Sphere of radius 0x15E on the same part, on list 8, keyed 0x50002; enabled from the break until its first contact or the end of the sparks
    WorldCollisionContact blastContacts[1];                       // Contact of `blast`
    Task*                 sparksTask;                             // Spark effect started at the break and adopted as a child of the transformer's task; `NULL` before the break, if it did not spawn, or once it has been told to stop
    s16                   state;                                  // `TRANSFORMER_*`
    s16                   sparkFrames;                            // Frames of sparks left, 0x78 at the break; a frame the camera spends in another view also takes one off
    s16                   transformerIndex;                       // Which of the three transformers this is (0..2), from the spawn argument; picks its place, facing and views
    s16                   status;                                 // `AKROPOLIS_HELIPAD_TRANSFORMER_*`, published every frame in view: by transformers 0 and 1 to `EVENT_FLAG_AKROPOLIS_HELIPAD_TRANSFORMER_1_STATE` and `_2_STATE`, by the third to `HelicopterLandingPadNo9Work::blastTransformerStatus`
} _TransformerWork;
STATIC_ASSERT_SIZEOF(_TransformerWork, 0x338);

/// Values of `_GrenadeWork::phase` while the grenade flies.
enum {
    GRENADE_FLIGHT_BELOW_PEAK = 0, // Has not been above height 0x514 yet
    GRENADE_FLIGHT_PEAKED     = 1, // Has been above it: dropping back below bursts the grenade
};

/// Values of `_GrenadeWork::phase` once the grenade has burst.
enum {
    GRENADE_BURST_SPREADING = 0, // 0x10 frames with the attack sphere off, then it widens to radius 0x258
    GRENADE_BURST_CATCHING  = 1, // Up to 0x1F frames in which the widened sphere can catch the player
    GRENADE_BURST_HOLDING   = 2, // The player is caught: `holdStep` runs
    GRENADE_BURST_ENDING    = 3, // 0x1F frames, then the grenade's task ends
};

/// Values of `_GrenadeWork::holdStep`.
enum {
    GRENADE_HOLD_REQUEST = 0, // Asks the player for the hold; on acceptance deals the damage and starts the stunned animation
    GRENADE_HOLD_STUNNED = 1, // Until the player has escaped, 0x3C frames have passed or the golem is gone; then starts the recovery animation
    GRENADE_HOLD_RECOVER = 2, // 0x14 frames, then waits for the animation to end and releases the player
};

/// Work block of the golem's stun grenade, kept at `Task::work` of the grenade model's task.
typedef struct {
    MATRIX                colorMtx;             // Colour matrix lent to the grenade's model
    MATRIX                lightMtx;             // Light matrix lent to the same model
    WorldCollisionBody    attack;               // Sphere on list 3 at the grenade: radius 0xC8 in flight, 0x258 once the burst has spread; a player contact bursts the grenade or starts the hold
    WorldCollisionContact attackContacts[1];    // Contact of `attack`
    WorldCollisionBody    gridProbe;            // Capsule on list 3 tested against the room's collision grid only; unlinked at the burst
    WorldCollisionCapsule gridProbeCapsule;     // Shape of `gridProbe`: 0x1F4 along the grenade's Y axis from its origin, radius 1
    WorldCollisionContact gridProbeContacts[1]; // Contact of `gridProbe`; any contact bursts the grenade
    u16                   frames;               // Frames counted in the current phase; in flight, frames since the last smoke puff of the trail (one every third frame)
    s16                   phase;                // `GRENADE_FLIGHT_*` in flight, `GRENADE_BURST_*` after the burst
    s16                   holdStep;             // `GRENADE_HOLD_*`
    s16                   phaseCounter;         // In flight, the pitch added each frame (4096 a turn); during the hold, frames spent in `holdStep`
} _GrenadeWork;
STATIC_ASSERT_SIZEOF(_GrenadeWork, 0xD0);

/// Values of `_BlastSourceWork::state`.
enum {
    BLAST_SOURCE_DORMANT = 0, // Waiting for the third transformer to break
    BLAST_SOURCE_ARMED   = 1, // Can be shot
    BLAST_SOURCE_SHOT    = 2, // The frame after the shot: the flare is given its starting state
    BLAST_SOURCE_FLARING = 3, // `flareFrames` runs down
    BLAST_SOURCE_SPENT   = 4, // The flare has been told to end
    BLAST_SOURCE_STOPPED = 5, // The golem is gone and the flare released
};

/// Work block of the blast source beside the third transformer, kept at `Task::work` of a task that has a coordinate and no
/// model.
typedef struct {
    WorldCollisionBody    body;             // Sphere of radius 0x12C at the source on list 2, receiving attacks; enabled only while armed and in view
    WorldCollisionContact bodyContacts[1];  // Contact of `body`, read and cleared every frame the source is armed
    WorldCollisionBody    blast;            // Sphere of radius 0x200 raised 0x200 above the source, on list 8; keyed 0x50003 or 0x50004 by the shot and enabled from it until its first contact or the end of the flare
    WorldCollisionContact blastContacts[1]; // Contact of `blast`
    Task*                 flareTask;        // Flare effect started by the shot and adopted as a child of the source's task; `NULL` before the shot, if it did not spawn, or once it has been released
    s16                   state;            // `BLAST_SOURCE_*`
    s16                   flareFrames;      // Frames the flare still burns, 0x3C at the shot; a frame the camera spends in another view also takes one off
    s16                   flareTaskState;   // `Task::state` the flare starts in, chosen at the shot (1 if the third transformer was already spent, 0 if it was still sparking)
    byte                  pad_7A[0x2];
} _BlastSourceWork;
STATIC_ASSERT_SIZEOF(_BlastSourceWork, 0x7C);

/// One corner of the square lap the golem walks, in world X and Z.
typedef struct {
    s16 x; // World X of the corner
    s16 z; // World Z of the corner
} _LapCorner;
STATIC_ASSERT_SIZEOF(_LapCorner, 0x4);

/// The direction one side of the lap runs in, from its corner to the next; every side is parallel to an axis.
typedef struct {
    s8 x; // World X covered per unit of distance along the side
    s8 z; // World Z covered per unit of distance along the side
} _LapDirection;
STATIC_ASSERT_SIZEOF(_LapDirection, 0x2);

/// The strip of the landing pad that runs along one side of the lap, as a world-space rectangle; all four edges are
/// exclusive.
typedef struct {
    s16 minX; // Edge of the strip towards negative X
    s16 maxX; // Edge towards positive X
    s16 minZ; // Edge towards negative Z
    s16 maxZ; // Edge towards positive Z
} _LapStrip;
STATIC_ASSERT_SIZEOF(_LapStrip, 0x8);

/// The three face normals `helicopterLandingPadNo9RestoreGridFaces` copies into `gWorldCollisionActiveGrid->normals`,
/// restoring the collision grid this actor edited.
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

static void _helicopterLandingPadNo9ExitTransformer(Task* task);
static void _helicopterLandingPadNo9ExitBlastSource(Task* task);

static void _helicopterLandingPadNo9BodyTask(Task* task);
static void _helicopterLandingPadNo9PropTask(Task* task);
static void _helicopterLandingPadNo9WeaponTask(Task* task);
static void _helicopterLandingPadNo9ChestModelTask(Task* task);
static void _helicopterLandingPadNo9GrenadeTask(Task* task);
static void _helicopterLandingPadNo9TransformerTask(Task* task);
static void _helicopterLandingPadNo9BlastSourceTask(Task* task);

EnemyParams gHelicopterLandingPadNo9EnemyParams = { gHelicopterLandingPadNo9Attacks, 1600, 500, 800, 30, 50, 3, 0, 0 };

/// Walk distances in world units, sampled with bits 16..19 of a fresh LCG draw.
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
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _helicopterLandingPadNo9TransformerTask, { .model = &gHelicopterLandingPadNo9TransformerCableModel } },
    { { { TASK_BODY_COORD, TASK_PRIORITY_ACTOR } }, _helicopterLandingPadNo9BlastSourceTask, { .value = 0 } },
};

TaskMessageEntry gHelicopterLandingPadNo9BodyMessages[7] = {
    { ACTOR_MESSAGE_RELEASE_HOLD, _helicopterLandingPadNo9ReleaseGrenadeHold },
    { ACTOR_MESSAGE_PLAY_ANIMATION, _helicopterLandingPadNo9PlayEventAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRotMatrix },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _helicopterLandingPadNo9SetModelDraw },
    { MESSAGE_SET_ACTIVATION, _helicopterLandingPadNo9SetActivation },
    { ACTOR_MESSAGE_IS_PRESENT, _helicopterLandingPadNo9IsPresent },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#include "assets/enemies/golem/no_9/helipad/body_bank.inc"

/// Player clips borrowed during the stun grenade's hold: 0 unused, 1 stunned, 2 recovery.
/// The player plays from this bank while the grenade holds them, so the overlay must stay loaded until the hold ends.
#include "assets/enemies/golem/no_9/helipad/player_grenade_bank.inc"

/// Whole-frame transition durations indexed by the body's animation ID 0..33.
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
static EffectSpawnArg _gHelicopterLandingPadNo9LethalHitEffectArg = { NULL, 300, 1 };

/// World X/Z origins of lap sides 0..3, in walking order.
static _LapCorner _gHelicopterLandingPadNo9LapCorners[4] = {
    { -6600, -6600 },
    { 6600, -6600 },
    { 6600, 6600 },
    { -6600, 6600 },
};

/// Axis-aligned unit X/Z directions from each lap corner to the next.
static _LapDirection _gHelicopterLandingPadNo9LapDirections[4] = {
    { 1, 0 },
    { 0, 1 },
    { -1, 0 },
    { 0, -1 },
};

/// Facing angles of lap sides 0..3, in units of 4096 per turn. Within 1000 of a corner the golem turns to the next
/// side's; the lap's end clamp keeps that lookahead inside the four.
static u16 _gHelicopterLandingPadNo9LapYaws[4] = {
    ACTOR_TRANSFORM_ANGLE_TURN / 4,
    0,
    ACTOR_TRANSFORM_ANGLE_TURN * 3 / 4,
    ACTOR_TRANSFORM_ANGLE_HALF_TURN,
};

/// Exclusive world X/Z rectangles assigning the player to lap sides 0..3.
static _LapStrip _gHelicopterLandingPadNo9LapStrips[4] = {
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
static WorldCollisionGridFace _gHelicopterLandingPadNo9ExtraGridFaces[1] = {
#include "assets/helicopter_landing_pad_no_9_collision_35E68.inc"
};

/// Grenade pitch-step magnitudes per flight frame, in angle units of 4096 per turn.
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

/// Transformer-cable animation bank: 0 unused, 1 intact, 2 breaking.
#include "assets/enemies/golem/no_9/helipad/cable_bank.inc"

/// Root positions of transformers 0..2, in room-coordinate units beneath `gGfxViewCoord`.
static SVECTOR _gHelicopterLandingPadNo9TransformerPositions[3] = {
    { 6458, -2658, 535, 0 },
    { -1336, -2658, 6552, 0 },
    { -6441, -2658, -100, 0 },
};

/// Root Y rotations of transformers 0..2, in angle units of 4096 per turn.
static u16 _gHelicopterLandingPadNo9TransformerYaws[4] = {
    ACTOR_TRANSFORM_ANGLE_HALF_TURN,
    ACTOR_TRANSFORM_ANGLE_TURN / 4,
    0,
    0,
};

/// Mapped-view rows shared by the three transformers and the third transformer's blast source.
static _HelipadViews _gHelicopterLandingPadNo9HelipadViews = {
    { { 5, 6, 7 }, { 7, 8, 9 }, { 10, 10, 10 } },
    { 0 },
};

/// `EVENT_FLAG_AKROPOLIS_HELIPAD_TRANSFORMER_3_STATE` values by blast-source phase and third-transformer status; row 6 has no state that selects it.
static u16 _gHelicopterLandingPadNo9Transformer3FlagValues[7][4] = {
    { AKROPOLIS_HELIPAD_TRANSFORMER_3_INTACT, AKROPOLIS_HELIPAD_TRANSFORMER_3_SHOT, AKROPOLIS_HELIPAD_TRANSFORMER_3_SPENT, AKROPOLIS_HELIPAD_TRANSFORMER_3_STOPPED },
    { AKROPOLIS_HELIPAD_TRANSFORMER_3_INTACT, AKROPOLIS_HELIPAD_TRANSFORMER_3_SHOT, AKROPOLIS_HELIPAD_TRANSFORMER_3_SPENT, AKROPOLIS_HELIPAD_TRANSFORMER_3_STOPPED },
    { AKROPOLIS_HELIPAD_TRANSFORMER_3_INTACT, AKROPOLIS_HELIPAD_TRANSFORMER_3_SOURCE_FLARING, AKROPOLIS_HELIPAD_TRANSFORMER_3_SOURCE_FLARING, AKROPOLIS_HELIPAD_TRANSFORMER_3_STOPPED },
    { AKROPOLIS_HELIPAD_TRANSFORMER_3_INTACT, AKROPOLIS_HELIPAD_TRANSFORMER_3_SOURCE_FLARING, AKROPOLIS_HELIPAD_TRANSFORMER_3_SOURCE_FLARING, AKROPOLIS_HELIPAD_TRANSFORMER_3_STOPPED },
    { AKROPOLIS_HELIPAD_TRANSFORMER_3_INTACT, AKROPOLIS_HELIPAD_TRANSFORMER_3_SOURCE_SPENT, AKROPOLIS_HELIPAD_TRANSFORMER_3_SOURCE_SPENT, AKROPOLIS_HELIPAD_TRANSFORMER_3_STOPPED },
    { AKROPOLIS_HELIPAD_TRANSFORMER_3_INTACT, AKROPOLIS_HELIPAD_TRANSFORMER_3_SOURCE_SPENT, AKROPOLIS_HELIPAD_TRANSFORMER_3_SOURCE_SPENT, AKROPOLIS_HELIPAD_TRANSFORMER_3_STOPPED },
    { 0, 0, 0, 0 },
};

static void _helicopterLandingPadNo9TickGrenadeHold(Task* task);

static void _helicopterLandingPadNo9TickTransformerBreak(Task* task);

static s32 _helicopterLandingPadNo9CheckTransformerView(Task* task);

static void _helicopterLandingPadNo9UpdateTransformerLighting(Task* task, const GfxCoord* rootCoord);

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
static void _helicopterLandingPadNo9InitTransformer(Enemy* enemy, Task* task);
static void _helicopterLandingPadNo9TickTransformer(Enemy* enemy, Task* task);
static void _helicopterLandingPadNo9InitBlastSource(Enemy* enemy, Task* task);
static void _helicopterLandingPadNo9TickBlastSource(Enemy* enemy, Task* task);

/// Enables the weapon and forearm attack spheres with the same damage-table key.
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
static inline void _helicopterLandingPadNo9AttachProp(GfxCoord* propCoord, GfxCoord* handCoord)
{
    gfxSetRotIdentity(&propCoord->coord);
    propCoord->coord.t[0] = 0;
    propCoord->coord.t[1] = 0;
    propCoord->coord.t[2] = 0;
    propCoord->parent     = handCoord;
}

/// Spawns a flash burst along the body's negative Y axis at a masked random offset.
static inline void _helicopterLandingPadNo9SpawnReactionSpark(GfxCoord* bodyCoord, SVECTOR* offset)
{
    enum {
        HEIGHT_MASK   = 0x2FF,
        HEIGHT_OFFSET = 1664,
        BASE_SIZE     = 256,
    };
    offset->vx = 0;
    offset->vz = 0;
    RANDOM_LCG_ADVANCE();
    offset->vy = (RANDOM_LCG_DRAW() & HEIGHT_MASK) - HEIGHT_OFFSET;
    effectSpawn(EFFECT_FLASH_BURST, bodyCoord, BASE_SIZE, offset);
}

/// Ends grenade catching when the golem parent disappears.
static inline void _helicopterLandingPadNo9CancelGrenadeCatch(Task* task, _GrenadeWork* work)
{
    work->attack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionUnlinkBody(&work->gridProbe);
    work->frames           = 0;
    task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    task->state            = GRENADE_TASK_BURST;
    work->phase            = GRENADE_BURST_ENDING;
}

/// Places a grenade at the chest's launch offset in the view coordinate's frame.
static inline void _helicopterLandingPadNo9PlaceGrenade(GfxCoord* grenadeCoord, GfxCoord* launchCoord, ActorChildPlaceScratch* placement)
{
    enum {
        GRENADE_LAUNCH_X     = -165,
        GRENADE_LAUNCH_Y     = -565,
        GRENADE_LAUNCH_Z     = 160,
        GRENADE_LAUNCH_PITCH = -352,
    };

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    launchCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(launchCoord);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &launchCoord->workm, &grenadeCoord->coord);

    placement->operand.vx = GRENADE_LAUNCH_X;
    placement->operand.vy = GRENADE_LAUNCH_Y;
    placement->operand.vz = GRENADE_LAUNCH_Z;
    gte_SetRotMatrix(&grenadeCoord->coord);
    gte_ldv0(&placement->operand);
    gte_rtv0();
    gte_stlvnl(&placement->offset);
    grenadeCoord->parent      = &gGfxViewCoord;
    grenadeCoord->coord.t[0] += placement->offset.vx;
    grenadeCoord->coord.t[1] += placement->offset.vy;
    grenadeCoord->coord.t[2] += placement->offset.vz;

    placement->operand.vx = GRENADE_LAUNCH_PITCH;
    placement->operand.vy = 0;
    placement->operand.vz = 0;
    RotMatrix(&placement->operand, &placement->rotation);
    // Store product columns while the GTE retains the original launch basis.
    gte_MulMatrix0(&grenadeCoord->coord, &placement->rotation, &grenadeCoord->coord);
}

/// Post-multiplies the chest's local rotation by its residual hit twist.
static inline void _helicopterLandingPadNo9ComposeHitRotation(MATRIX* chestRotation, const MATRIX* hitRotation)
{
    gte_MulMatrix0(chestRotation, hitRotation, chestRotation);
}

/// Seeds the chest's residual pitch and yaw for a hit twist.
static inline void _helicopterLandingPadNo9StartHitTwist(HelicopterLandingPadNo9Work* work)
{
    enum {
        MIN_ANGLE  = 64,
        ANGLE_MASK = 127,
    };
    u32 randomState;
    u32 randomBits;
    s32 pitchTwist;
    s32 yawBits;
    s32 yawTwist;

    randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
    randomBits      = randomState >> 16;
    pitchTwist      = (randomBits & ANGLE_MASK) + MIN_ANGLE;
    gRandomLcgState = randomState;
    if (!(randomBits & 1)) {
        pitchTwist = -pitchTwist;
    }
    work->hitTwist.vx = pitchTwist;
    yawBits           = (s16)randomBits >> 8;
    yawTwist          = (yawBits & ANGLE_MASK) + MIN_ANGLE;
    if (!(yawBits & 1)) {
        yawTwist = -yawTwist;
    }
    work->hitTwist.vy    = yawTwist;
    work->hitTwistActive = 1;
}

/// Selects the lethal strike's forward and retreat movement from its frame windows.
static inline void _helicopterLandingPadNo9SetLethalLapSpeed(HelicopterLandingPadNo9Work* work, s32 frame, s32 retreatSpeed)
{
    enum {
        FORWARD_FIRST_FRAME = 75,
        FORWARD_FRAME_COUNT = 16,
        RETREAT_FIRST_FRAME = 143,
        RETREAT_FRAME_COUNT = 15,
        FORWARD_SPEED       = 60,
    };
    if (frame - FORWARD_FIRST_FRAME < (u32)FORWARD_FRAME_COUNT) {
        work->lapSpeed = FORWARD_SPEED;
    } else if (((frame - RETREAT_FIRST_FRAME) & 0xFFFF) < (u32)RETREAT_FRAME_COUNT) {
        work->lapSpeed = retreatSpeed;
    } else {
        work->lapSpeed = 0;
    }
}

/// Applies incoming attack and helipad-hazard contacts and latches outgoing hits. Once the lethal strike has started,
/// damage leaves the golem at least one HP.
static void _helicopterLandingPadNo9ResolveBodyContacts(Task* task)
{
    enum {
        HIT_TWIST                       = 0,
        HIT_FLINCH                      = 1,
        HIT_RECOIL                      = 2,
        ATTACK_ATTRIBUTE_REDUCED_DAMAGE = 4,
        ATTACK_ATTRIBUTE_DOUBLE_DAMAGE  = 5,
        SOUND_HIT_FLINCH                = SOUND_CHARACTER(SOUND_BANK_NO_9_AKROPOLIS, NO_9_AKROPOLIS_SOUND_HURT),
        ATTACHMENT_KEY_BIT              = 0x8000,
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
    hitReaction   = HIT_TWIST;
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
                if (work->bodyContacts[contactIndex].key.value & ATTACHMENT_KEY_BIT) {
                    if ((u8)work->bodyContacts[contactIndex].key.value - 1 < 6U) {
                        hitReaction = HIT_RECOIL;
                    }
                    damage = (s16)damageComputePlayerAttack(work->bodyContacts[contactIndex].key.value, 0, 0, 0) >> 1;
                    damageAccumulateLifeDrainHp(task->spawnArg2.pointer, work->bodyContacts[contactIndex].key.value, damage, 0);
                } else {
                    playerOffset->vx = gPlayerStatus.coordMtx->t[0] - bodyCoord->coord.t[0];
                    playerOffset->vy = gPlayerStatus.coordMtx->t[1] - bodyCoord->coord.t[1];
                    playerOffset->vz = gPlayerStatus.coordMtx->t[2] - bodyCoord->coord.t[2];
                    unscaledDamage   = damageComputePlayerAttack(work->bodyContacts[contactIndex].key.value, SquareRoot0(playerOffset->vx * playerOffset->vx + playerOffset->vy * playerOffset->vy + playerOffset->vz * playerOffset->vz), 0, 0);
                    damage           = unscaledDamage;
                    if ((u16)attackAttribute == ATTACK_ATTRIBUTE_DOUBLE_DAMAGE) {
                        damage = unscaledDamage * 2;
                        effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[CHEST_PART], 2, NULL);
                    }
                    if (damageRollCriticalHit(enemy, work->bodyContacts[contactIndex].key.value, 0) != 0) {
                        damage *= 4;
                        if ((u16)attackAttribute != ATTACK_ATTRIBUTE_DOUBLE_DAMAGE) {
                            effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[CHEST_PART], 0, NULL);
                        }
                        if (work->buildupStunned == 0) {
                            hitReaction = HIT_FLINCH;
                        }
                    }
                }
                switch ((u16)attackAttribute) {
                    case DAMAGE_PLAYER_REACTION_NONE:
                    case ATTACK_ATTRIBUTE_DOUBLE_DAMAGE:
                    case DAMAGE_PLAYER_REACTION_EXPLOSION:
                    case DAMAGE_PLAYER_REACTION_INCENDIARY:
                    case 8:
                    case 9:
                        break;
                    case DAMAGE_PLAYER_REACTION_STAGGER:
                        if (work->buildupStunned == 0) {
                            RANDOM_LCG_ADVANCE();
                            if (RANDOM_LCG_DRAW() & 1) {
                                hitReaction = HIT_FLINCH;
                            }
                        }
                        break;
                    case DAMAGE_PLAYER_REACTION_BUILDUP:
                        if (work->lethalAttackPhase == LETHAL_PHASE_NONE) {
                            damageStartEnemyBuildup(enemy, work->bodyContacts[contactIndex].key.value, 0);
                        }
                        break;
                    case DAMAGE_PLAYER_REACTION_POISON:
                        if (work->buildupStunned == 0) {
                            hitReaction = HIT_RECOIL;
                        }
                        break;
                    case ATTACK_ATTRIBUTE_REDUCED_DAMAGE:
                        damage = damage * 75 / 100;
                        break;
                }
                enemy->hp -= damage;
                worldTargetAddReadoutAmount(&enemy->node, damage, 0);
                if (enemy->hp <= 0) {
                    if (work->lethalAttackPhase < LETHAL_PHASE_STRUCK) {
                        work->lethalAttackPhase = LETHAL_PHASE_NONE;
                    } else {
                        enemy->hp = 1;
                    }
                }
                if (hitReaction != HIT_TWIST) {
                    RANDOM_LCG_ADVANCE();
                    if (!(RANDOM_LCG_DRAW() & 1)) {
                        soundKey = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_HIT_FLINCH;
                    } else {
                        soundKey = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_RECOIL;
                    }
                    audioPan = (s8)worldCoordGetOriginAudioPan(bodyCoord);
                    sndEvtRequestScriptStart(soundKey, audioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                }
                if (work->sparkSound != 0) {
                    sndEvtRequestScriptStop(work->sparkSound, SOUND_SCRIPT_STOP_NO_FADE);
                    work->sparkSound = 0;
                }
                if (work->lethalAttackPhase == LETHAL_PHASE_WINDUP) {
                    hitReaction = HIT_TWIST;
                }
                switch (hitReaction) {
                    case HIT_TWIST: {
                        _helicopterLandingPadNo9StartHitTwist(work);
                        if (enemy->hp <= 0) {
                            work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_DEATH;
                            work->subState    = DEATH_ENTER;
                            work->animationId = BODY_ANIMATION_DEATH;
                            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        }
                        break;
                    }
                    case HIT_FLINCH:
                        work->state                = HELICOPTER_LANDING_PAD_NO_9_STATE_FLINCH;
                        work->subState             = FLINCH_ENTER;
                        work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        if (enemy->hp <= 0) {
                            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        }
                        break;
                    case HIT_RECOIL:
                        work->state                = HELICOPTER_LANDING_PAD_NO_9_STATE_RECOIL;
                        work->subState             = RECOIL_ENTER;
                        work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        if (enemy->hp <= 0) {
                            work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        }
                        break;
                }
                if (lastEffectKey != work->bodyContacts[contactIndex].key.value) {
                    lastEffectKey = work->bodyContacts[contactIndex].key.value;
                    effectSpawnHit(damageGetPlayerAttackEffectId(lastEffectKey), &task->extra.tmd->coords[CHEST_PART], NULL, &work->hitEffectArg);
                }
                cooldownFrames = damageGetPlayerAttackHitCooldown(work->bodyContacts[contactIndex].key.value);
                if (cooldownFrames > 0) {
                    work->hitCooldown = cooldownFrames;
                }
                break;
            case DAMAGE_HAZARD_CATEGORY >> 16:
                hazardApplies = 0;
                switch ((u32)(u16)work->bodyContacts[contactIndex].key.value) {
                    case HAZARD_TRANSFORMER_BLAST:
                        hazardApplies  = 1;
                        work->state    = HELICOPTER_LANDING_PAD_NO_9_STATE_SPARK_RECOIL;
                        work->subState = SPARK_RECOIL_ENTER;
                        break;
                    case HAZARD_SOURCE_RECOIL:
                        hazardApplies  = 1;
                        work->state    = HELICOPTER_LANDING_PAD_NO_9_STATE_RECOIL;
                        work->subState = RECOIL_ENTER;
                        break;
                    case HAZARD_SOURCE_STUN:
                        hazardApplies  = 1;
                        work->state    = HELICOPTER_LANDING_PAD_NO_9_STATE_SPARK_STUN;
                        work->subState = SPARK_STUN_ENTER;
                        break;
                }
                if (hazardApplies) {
                    work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    hazardDamage               = damageGetHazardDamage((u16)work->bodyContacts[contactIndex].key.value, DAMAGE_HAZARD_VICTIM_ENEMY);
                    enemy->hp                 -= hazardDamage;
                    worldTargetAddReadoutAmount(&enemy->node, hazardDamage, 0);
                    if (enemy->hp <= 0) {
                        if (work->lethalAttackPhase < LETHAL_PHASE_STRUCK) {
                            work->lethalAttackPhase = LETHAL_PHASE_NONE;
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
static void _helicopterLandingPadNo9TickOpening(Task* task)
{
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoord;
    s32                          soundKey;

    work      = task->work;
    bodyCoord = task->extra.tmd->coords;
    switch (work->subState) {
        case OPENING_INTRO:
            if (work->animationFrame == 0xA) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_WINDUP;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x64) {
                work->subState    = OPENING_WINDUP;
                work->animationId = BODY_ANIMATION_SLASH_WINDUP;
            }
            break;
        case OPENING_WINDUP:
            if (work->animationFrame >= 0x14) {
                work->subState    = OPENING_FIRST_SLASH;
                work->animationId = BODY_ANIMATION_SLASH_FIRST;
                _helicopterLandingPadNo9ArmAttack(work, ATTACK_SLASH);
            }
            break;
        case OPENING_FIRST_SLASH:
            work->lapSpeed = ((u32)((u16)work->animationFrame - 9) < 0x13U) ? 0x38 : 0;
            if (work->attackLanded != 0) {
                work->lapSpeed = 0;
            }
            if (work->animationFrame == 0xA) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_SLASH;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x1D) {
                if (work->attackLanded != 0) {
                    work->state                = HELICOPTER_LANDING_PAD_NO_9_STATE_SLASH;
                    work->subState             = SLASH_RECOVER;
                    work->animationId          = BODY_ANIMATION_SLASH_RECOVER;
                    work->attackLanded         = 0;
                    work->flameFrames          = 0;
                    work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    work->subState    = OPENING_SECOND_SLASH;
                    work->animationId = BODY_ANIMATION_SLASH_SECOND;
                }
            }
            break;
        case OPENING_SECOND_SLASH:
            work->lapSpeed = ((u32)((u16)work->animationFrame - 5) < 0x13U) ? 0x38 : 0;
            if (work->attackLanded != 0) {
                work->lapSpeed = 0;
            }
            if (work->animationFrame == 6) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_SLASH;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x27) {
                if (work->attackLanded != 0) {
                    work->state                = HELICOPTER_LANDING_PAD_NO_9_STATE_SLASH;
                    work->subState             = SLASH_RECOVER;
                    work->animationId          = BODY_ANIMATION_SLASH_RECOVER;
                    work->attackLanded         = 0;
                    work->flameFrames          = 0;
                    work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    work->subState = OPENING_LUNGE;
                }
            }
            break;
        case OPENING_LUNGE:
            work->animationId = BODY_ANIMATION_FLAME_LUNGE;
            work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_FLAME_LUNGE;
            work->subState    = FLAME_LUNGE_CHARGE;
            break;
    }
}

/// Walks the lap, fires from the weapon and selects attacks between walks.
static void _helicopterLandingPadNo9TickPatrol(Task* task)
{
    enum {
        WALK_WITHOUT_FIRE    = 0,
        WALK_WITH_FIRE       = 1,
        FIRST_SHOT_FRAMES    = 29,
        SHOT_INTERVAL_FRAMES = 40,
    };
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoord;
    EffectWork*                  muzzleFlash;
    s32                          soundKey;
    s16                          catchUpSpeed;

    work      = task->work;
    bodyCoord = task->extra.tmd->coords;
    if (work->playerSide != work->lapSide && work->subState != PATROL_BACKSTEP && work->sideRemaining >= 0xBB8) {
        work->subState = PATROL_CATCH_UP;
    }
    switch (work->subState) {
        // Pick an attack only at a wait boundary; otherwise prepare another walk.
        case PATROL_WAIT:
            work->lapSpeed     = 0;
            work->attackLanded = 0;
            if (--work->stateCounter <= 0) {
                if (work->flameMode == HELICOPTER_LANDING_PAD_NO_9_FLAME_BLAST) {
                    work->flameMode = HELICOPTER_LANDING_PAD_NO_9_FLAME_BURNING;
                }
                if (_helicopterLandingPadNo9TrySelectAttack(task) == 0) {
                    if (work->sideRemaining < 0x7D0) {
                        work->subState     = PATROL_WALK_START;
                        work->animationId  = BODY_ANIMATION_WALK_START;
                        work->stateCounter = WALK_WITHOUT_FIRE;
                    } else if (((Enemy*)task->spawnArg2.pointer)->node.state.parts.targeted == 1 && (u32)(gPlayerStatus.weaponSlotItem - SHOTGUN_FIRST_AMMO_ROW) < (u32)SHOTGUN_AMMO_ROW_COUNT) {
                        work->subState    = PATROL_SHOTGUN_RESPONSE_START;
                        work->animationId = BODY_ANIMATION_SHOTGUN_RESPONSE_START;
                        soundKey          = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_SHOTGUN_RESPONSE;
                        sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                                 (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                    } else {
                        work->subState     = PATROL_WALK_START;
                        work->animationId  = BODY_ANIMATION_WALK_START;
                        work->stateCounter = WALK_WITH_FIRE;
                    }
                }
            }
            break;
        case PATROL_WALK_START:
            work->lapSpeed = 0;
            if (work->animationFrame >= 0xD) {
                if (work->stateCounter == WALK_WITHOUT_FIRE) {
                    work->subState    = PATROL_WALK;
                    work->animationId = BODY_ANIMATION_WALK;
                } else {
                    work->subState      = PATROL_WALK_FIRE;
                    work->animationId   = BODY_ANIMATION_WALK_FIRE;
                    work->shotCountdown = FIRST_SHOT_FRAMES;
                }
                work->stateCounter = _gHelicopterLandingPadNo9PatrolWalkDistances[((RANDOM_LCG_ADVANCE()) >> 16) & 0xF];
            }
            break;
        case PATROL_WALK:
            work->lapSpeed      = PATROL_WALK_SPEED;
            work->stateCounter -= PATROL_WALK_SPEED;
            if (work->stateCounter <= 0 || work->lapDistance > (u32)PATROL_END_DISTANCE) {
                work->animationId  = BODY_ANIMATION_PATROL_IDLE;
                work->subState     = PATROL_WAIT;
                work->stateCounter = _gHelicopterLandingPadNo9PatrolWaitFrames[((RANDOM_LCG_ADVANCE()) >> 16) & 0xF];
            }
            // The binary retains this empty lap-distance interval.
            if (work->sideRemaining >= 0x7D0 && work->playerDistance < 0x3E8 && work->lapDistance <= (u32)LAST_ATTACK_DISTANCE &&
                work->lapDistance > (u32)LETHAL_ATTACK_DISTANCE) {
                work->subState     = PATROL_WAIT;
                work->animationId  = BODY_ANIMATION_PATROL_IDLE;
                work->stateCounter = 0;
            }
            break;
        case PATROL_WALK_FIRE:
            work->lapSpeed      = PATROL_WALK_SPEED;
            work->stateCounter -= PATROL_WALK_SPEED;
            if (--work->shotCountdown == 0) {
                muzzleFlash = effectSpawn((EFFECT_NO9_MUZZLE_FLASH | EFFECT_SPAWN_UNLIMITED), &task->extra.tmd->coords[RIGHT_HAND_PART], 0, NULL);
                if (muzzleFlash != NULL) {
                    taskReparent(task, muzzleFlash->task);
                }
                work->shotCountdown = SHOT_INTERVAL_FRAMES;
                soundKey            = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_FIRE;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (((Enemy*)task->spawnArg2.pointer)->node.state.parts.targeted == 1 && (u32)(gPlayerStatus.weaponSlotItem - SHOTGUN_FIRST_AMMO_ROW) < (u32)SHOTGUN_AMMO_ROW_COUNT) {
                work->subState    = PATROL_SHOTGUN_RESPONSE_WALK;
                work->animationId = BODY_ANIMATION_SHOTGUN_RESPONSE_START;
                soundKey          = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_SHOTGUN_RESPONSE;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->stateCounter <= 0 || work->lapDistance > (u32)PATROL_END_DISTANCE) {
                work->animationId  = BODY_ANIMATION_PATROL_IDLE;
                work->subState     = PATROL_WAIT;
                work->stateCounter = _gHelicopterLandingPadNo9PatrolWaitFrames[((RANDOM_LCG_ADVANCE()) >> 16) & 0xF];
            }
            if (work->sideRemaining >= 0x7D0 && work->playerDistance < 0x3E8 && work->lapDistance <= (u32)LAST_ATTACK_DISTANCE &&
                work->lapDistance > (u32)LETHAL_ATTACK_DISTANCE) {
                work->subState     = PATROL_WAIT;
                work->animationId  = BODY_ANIMATION_PATROL_IDLE;
                work->stateCounter = 0;
            }
            break;
        case PATROL_CATCH_UP:
            catchUpSpeed      = 0;
            work->animationId = BODY_ANIMATION_CATCH_UP;
            if (work->animationFrame >= 8) {
                catchUpSpeed = 0xA0;
            }
            work->lapSpeed = catchUpSpeed;
            if (work->sideRemaining < 0x5DC) {
                work->subState     = PATROL_WAIT;
                work->animationId  = BODY_ANIMATION_PATROL_IDLE;
                work->stateCounter = 0;
            }
            if (work->sideRemaining >= 0x7D0 && work->playerDistance < 0x3E8 && work->lapDistance <= (u32)LAST_ATTACK_DISTANCE &&
                work->lapDistance > (u32)LETHAL_ATTACK_DISTANCE) {
                work->subState     = PATROL_WAIT;
                work->animationId  = BODY_ANIMATION_PATROL_IDLE;
                work->stateCounter = 0;
            }
            break;
        case PATROL_SHOTGUN_RESPONSE_START:
            if (work->animationFrame >= 0xF) {
                work->subState     = PATROL_SHOTGUN_RESPONSE_WALK;
                work->animationId  = BODY_ANIMATION_SHOTGUN_RESPONSE_WALK;
                work->stateCounter = _gHelicopterLandingPadNo9PatrolWalkDistances[((RANDOM_LCG_ADVANCE()) >> 16) & 0xF];
            }
            break;
        case PATROL_SHOTGUN_RESPONSE_WALK:
            work->lapSpeed      = PATROL_WALK_SPEED;
            work->stateCounter -= PATROL_WALK_SPEED;
            if (work->stateCounter <= 0 || work->lapDistance > (u32)PATROL_END_DISTANCE ||
                (work->sideRemaining >= 0x7D0 && work->playerDistance < 0x3E8 && work->lapDistance <= (u32)LAST_ATTACK_DISTANCE)) {
                work->subState    = PATROL_SHOTGUN_RESPONSE_END;
                work->animationId = BODY_ANIMATION_SHOTGUN_RESPONSE_END;
            }
            break;
        case PATROL_SHOTGUN_RESPONSE_END:
            work->lapSpeed = 0;
            if (work->animationFrame >= 0xD) {
                work->subState    = PATROL_WAIT;
                work->animationId = BODY_ANIMATION_PATROL_IDLE;
                if (work->sideRemaining >= 0x7D0 && work->playerDistance < 0x3E8 && work->lapDistance <= (u32)LAST_ATTACK_DISTANCE &&
                    work->lapDistance > (u32)LETHAL_ATTACK_DISTANCE) {
                    work->stateCounter = 0;
                } else {
                    work->stateCounter = _gHelicopterLandingPadNo9PatrolWaitFrames[((RANDOM_LCG_ADVANCE()) >> 16) & 0xF];
                }
            }
            break;
        case PATROL_BACKSTEP:
            if (work->sideTravelled < 0xC8 || work->playerSide != work->lapSide || work->animationFrame < 0xB ||
                work->animationFrame >= 0x18) {
                work->lapSpeed = 0;
            } else {
                work->lapSpeed = -0xA7;
            }
            if (work->animationFrame == 0x19) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_BACKSTEP;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x32) {
                work->state        = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                work->subState     = PATROL_WAIT;
                work->animationId  = BODY_ANIMATION_PATROL_IDLE;
                work->stateCounter = 0;
            }
            break;
    }
}

/// Selects an attack from lap progress, player range and relative facing; the end-of-lap lethal strike comes first.
static s32 _helicopterLandingPadNo9TrySelectAttack(Task* task)
{
    enum { ATTACK_INITIAL_STEP = 0 };
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoord;
    s32                          slashDistanceError;
    s32                          selected;
    s16                          playerDistance;

    work      = task->work;
    bodyCoord = task->extra.tmd->coords;
    // Scripted transformer slashes and the lethal attack precede range-based choices.
    if (work->slashedTransformer == TRANSFORMER_SLASH_NONE) {
        slashDistanceError = work->lapDistance - FIRST_TRANSFORMER_SLASH_DISTANCE;
        if (slashDistanceError < 0) {
            slashDistanceError = -slashDistanceError;
        }
        if (slashDistanceError < TRANSFORMER_SLASH_WINDOW) {
            work->slashedTransformer = TRANSFORMER_SLASH_FIRST;
            work->state              = HELICOPTER_LANDING_PAD_NO_9_STATE_SLASH;
            work->subState           = SLASH_WINDUP;
            work->animationId        = BODY_ANIMATION_SLASH_WINDUP;
            return 1;
        }
        slashDistanceError = work->lapDistance - SECOND_TRANSFORMER_SLASH_DISTANCE;
        if (slashDistanceError < 0) {
            slashDistanceError = -slashDistanceError;
        }
        if (slashDistanceError < TRANSFORMER_SLASH_WINDOW) {
            work->slashedTransformer = TRANSFORMER_SLASH_SECOND;
            work->state              = HELICOPTER_LANDING_PAD_NO_9_STATE_SLASH;
            work->subState           = SLASH_WINDUP;
            work->animationId        = BODY_ANIMATION_SLASH_WINDUP;
            return 1;
        }
    }
    if (work->lapDistance > (u32)LETHAL_ATTACK_DISTANCE) {
        work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_LETHAL_ATTACK;
        work->subState    = ATTACK_INITIAL_STEP;
        work->animationId = BODY_ANIMATION_FLAME_WINDUP;
        return 1;
    }
    if (work->lapDistance > (u32)LAST_ATTACK_DISTANCE) {
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
        if (playerDistance < SLASH_RANGE) {
            selected          = 1;
            work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_SLASH;
            work->subState    = SLASH_WINDUP;
            work->animationId = BODY_ANIMATION_SLASH_WINDUP;
        } else if (playerDistance < FLAME_LUNGE_RANGE) {
            selected          = 1;
            work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_FLAME_LUNGE;
            work->subState    = FLAME_LUNGE_WINDUP;
            work->animationId = BODY_ANIMATION_FLAME_WINDUP;
        } else if (playerDistance < MIXED_ATTACK_RANGE) {
            RANDOM_LCG_ADVANCE();
            if ((gRandomLcgState >> 0x10) & 1) {
                work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_FLAME_LUNGE;
                work->subState    = FLAME_LUNGE_WINDUP;
                work->animationId = BODY_ANIMATION_FLAME_WINDUP;
            } else {
                work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_GRENADE;
                work->subState    = ATTACK_INITIAL_STEP;
                work->animationId = BODY_ANIMATION_GRENADE_THROW;
            }
            selected = 1;
        } else {
            selected = 0;
            if (work->grenadeLive == 0) {
                selected          = 1;
                work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_GRENADE;
                work->subState    = ATTACK_INITIAL_STEP;
                work->animationId = BODY_ANIMATION_GRENADE_THROW;
            }
        }
    } else {
        if (work->playerDistance < SLASH_RANGE) {
            work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_SLASH;
            work->subState    = SLASH_WINDUP;
            work->animationId = BODY_ANIMATION_SLASH_WINDUP;
        } else {
            RANDOM_LCG_ADVANCE();
            if (((gRandomLcgState >> 0x10) & 0xF) < 0xAU) {
                work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_GRENADE;
                work->subState    = ATTACK_INITIAL_STEP;
                work->animationId = BODY_ANIMATION_GRENADE_THROW;
            } else {
                work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_DASH;
                work->subState    = DASH_START;
                work->animationId = BODY_ANIMATION_DASH_START;
            }
        }
        selected = 1;
    }
    return selected;
}

/// Advances alternating slash cycles and their recovery after a player hit.
static void _helicopterLandingPadNo9TickSlash(Task* task)
{
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoord;
    s32                          soundKey;

    work      = task->work;
    bodyCoord = task->extra.tmd->coords;
    switch (work->subState) {
        case SLASH_WINDUP:
            work->lapSpeed = 0;
            if (work->animationFrame >= 0x14) {
                work->subState     = SLASH_FIRST;
                work->animationId  = BODY_ANIMATION_SLASH_FIRST;
                work->stateCounter = 0;
            }
            break;
        case SLASH_FIRST:
            work->lapSpeed = ((u32)((u16)work->animationFrame - 9) < 0x13U) ? 0x38 : 0;
            if (work->attackLanded != 0) {
                work->lapSpeed = 0;
            }
            if (work->animationFrame == 0xA) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_SLASH;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if ((work->slashedTransformer > TRANSFORMER_SLASH_NONE) && (work->animationFrame == 8)) {
                work->transformerSlashStruck = 1;
            }
            if (work->animationFrame == 9) {
                _helicopterLandingPadNo9ArmAttack(work, ATTACK_SLASH);
            }
            if (work->animationFrame >= 0x1D) {
                if (work->attackLanded != 0) {
                    work->subState             = SLASH_RECOVER;
                    work->animationId          = BODY_ANIMATION_SLASH_RECOVER;
                    work->attackLanded         = 0;
                    work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    RANDOM_LCG_ADVANCE();
                    if (_gHelicopterLandingPadNo9SlashContinuationThresholds[work->stateCounter] < (s32)((gRandomLcgState >> 0x10) & 0xF)) {
                        u16* patrolWaitFrames = _gHelicopterLandingPadNo9PostAttackWaitFrames;
                        work->state           = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                        work->animationId     = BODY_ANIMATION_PATROL_IDLE;
                        work->subState        = PATROL_WAIT;
                        RANDOM_LCG_ADVANCE();
                        work->stateCounter         = patrolWaitFrames[(gRandomLcgState >> 0x10) & 0xF];
                        work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    } else {
                        work->subState    = SLASH_SECOND;
                        work->animationId = BODY_ANIMATION_SLASH_SECOND;
                        work->stateCounter++;
                    }
                }
            }
            break;
        case SLASH_SECOND:
            work->lapSpeed = ((u32)((u16)work->animationFrame - 5) < 0x13U) ? 0x38 : 0;
            if (work->attackLanded != 0) {
                work->lapSpeed = 0;
            }
            if (work->animationFrame == 6) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_SLASH;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x27) {
                if (work->attackLanded != 0) {
                    work->subState             = SLASH_RECOVER;
                    work->animationId          = BODY_ANIMATION_SLASH_RECOVER;
                    work->attackLanded         = 0;
                    work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    if ((_gHelicopterLandingPadNo9SlashContinuationThresholds[work->stateCounter] <
                         (s32)(((RANDOM_LCG_ADVANCE()) >> 0x10) & 0xF)) ||
                        (work->stateCounter >= (s32)ARRAY_SIZE(_gHelicopterLandingPadNo9SlashContinuationThresholds) - 1)) {
                        u16* patrolWaitFrames = _gHelicopterLandingPadNo9PostAttackWaitFrames;
                        work->state           = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                        work->subState        = PATROL_WAIT;
                        work->animationId     = BODY_ANIMATION_PATROL_IDLE;
                        RANDOM_LCG_ADVANCE();
                        work->stateCounter         = patrolWaitFrames[(gRandomLcgState >> 0x10) & 0xF];
                        work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    } else {
                        work->stateCounter++;
                        work->subState    = SLASH_FIRST;
                        work->animationId = BODY_ANIMATION_SLASH_FIRST;
                    }
                }
            }
            break;
        case SLASH_RECOVER:
            if (work->animationFrame == 0x2D) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_SLASH_RECOVER;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x5A) {
                u16* patrolWaitFrames = _gHelicopterLandingPadNo9PostAttackWaitFrames;
                work->state           = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                work->subState        = PATROL_WAIT;
                work->animationId     = BODY_ANIMATION_PATROL_IDLE;
                RANDOM_LCG_ADVANCE();
                work->stateCounter = patrolWaitFrames[(gRandomLcgState >> 0x10) & 0xF];
            }
            break;
    }
}

/// Ignites the weapon, charges toward the player and recovers into patrol.
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
        case FLAME_LUNGE_WINDUP:
            work->lapSpeed = 0;
            if (work->animationFrame == 0x39) {
                work->flameMode   = HELICOPTER_LANDING_PAD_NO_9_FLAME_BURNING;
                work->flameFrames = 0x55;
                soundKey          = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_FIRE;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                work->flameSound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_FLAME_LOOP;
                sndEvtRequestScriptStart(work->flameSound, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                _helicopterLandingPadNo9ArmAttack(work, ATTACK_IGNITION);
            }
            if (work->animationFrame == 0x41) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_WINDUP;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x5A) {
                work->animationId = BODY_ANIMATION_FLAME_LUNGE;
                work->subState    = FLAME_LUNGE_CHARGE;
            }
            break;
        case FLAME_LUNGE_CHARGE:
            if (work->animationFrame == 0xE) {
                // Keep the raw distance separate from the default clamp result.
                playerDistance = work->playerDistance;
                lungeDistance  = MAX_LUNGE_DISTANCE;
                if (playerDistance < MAX_LUNGE_DISTANCE + 1) {
                    lungeDistance = playerDistance;
                }
                work->stateCounter = (lungeDistance - 0x4B0) / 24;
            }
            work->lapSpeed = ((u32)((u16)work->animationFrame - 0x17) < 0x19U) ? work->stateCounter : 0;
            if (work->animationFrame == 0x19) {
                _helicopterLandingPadNo9ArmAttack(work, ATTACK_FLAME_LUNGE);
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_FLAME_STRIKE;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame == 0x2F) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_BACKSTEP;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x46) {
                if (work->attackLanded != 0) {
                    work->state        = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                    work->subState     = PATROL_BACKSTEP;
                    work->attackLanded = 0;
                    work->animationId  = BODY_ANIMATION_BACKSTEP;
                } else {
                    work->state        = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                    work->animationId  = BODY_ANIMATION_PATROL_IDLE;
                    work->subState     = PATROL_WAIT;
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
static void _helicopterLandingPadNo9TickGrenadeThrow(Task* task)
{
    enum {
        GRENADE_THROW_FRAME       = 20,
        GRENADE_THROW_END_FRAME   = 70,
        GRENADE_DESCRIPTOR        = 4,
        GRENADE_DASH_MIN_DISTANCE = 3751,
        GRENADE_DASH_ROLL_LIMIT   = 12,
        SOUND_GRENADE_THROW       = SOUND_CHARACTER(SOUND_BANK_NO_9_AKROPOLIS, NO_9_AKROPOLIS_SOUND_GRENADE_THROW),
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
    if (work->animationFrame == GRENADE_THROW_FRAME) {
        work->grenadeLive  = 1;
        grenadeModel       = enemySpawnFromTable(gHelicopterLandingPadNo9TaskDescriptors, GRENADE_DESCRIPTOR, dashNext, enemy)->task->extra.tmd;
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
        soundKey = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_GRENADE_THROW;
        audioPan = (s8)worldCoordGetOriginAudioPan(bodyCoord);
        sndEvtRequestScriptStart(soundKey, audioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
    }
    if (work->animationFrame >= GRENADE_THROW_END_FRAME) {
        if (work->playerDistance >= GRENADE_DASH_MIN_DISTANCE) {
            RANDOM_LCG_ADVANCE();
            if (((gRandomLcgState >> 0x10) & 0xF) < (u32)GRENADE_DASH_ROLL_LIMIT) {
                dashNext = 1;
            }
        }
        if (dashNext == 0) {
            work->state        = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
            work->animationId  = BODY_ANIMATION_PATROL_IDLE;
            work->subState     = PATROL_WAIT;
            work->stateCounter = _gHelicopterLandingPadNo9PostAttackWaitFrames[((u32)(randomState = RANDOM_LCG_NEXT(gRandomLcgState)) >> 0x10) & 0xF];
            gRandomLcgState    = randomState;
            return;
        }
        work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_DASH;
        work->subState    = DASH_START;
        work->animationId = BODY_ANIMATION_DASH_START;
    }
}

/// Dashes along the lap until the player is close enough for a strike.
static void _helicopterLandingPadNo9TickDash(Task* task)
{
    HelicopterLandingPadNo9Work* work;
    GfxCoord*                    bodyCoord;
    s32                          soundKey;
    s32                          patrolWaitFrames;

    work      = task->work;
    bodyCoord = task->extra.tmd->coords;
    switch (work->subState) {
        case DASH_START:
            work->lapSpeed = (work->animationFrame < 0x47) ? 0 : DASH_SPEED;
            if (work->animationFrame == 0x4A) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_DASH_START;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x52) {
                work->animationId  = BODY_ANIMATION_DASH_RUN;
                work->subState     = DASH_RUN;
                work->stateCounter = 0xEA6;
            }
            break;
        case DASH_RUN:
            work->lapSpeed = DASH_SPEED;
            if (work->playerDistance < 0x514) {
                work->subState    = DASH_STRIKE;
                work->animationId = BODY_ANIMATION_DASH_STRIKE;
                _helicopterLandingPadNo9ArmAttack(work, ATTACK_DASH);
            } else {
                work->stateCounter -= DASH_SPEED;
                if (work->stateCounter < 0 || work->sideRemaining < 0x384) {
                    work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                    work->subState    = PATROL_WAIT;
                    work->animationId = BODY_ANIMATION_PATROL_IDLE;
                    RANDOM_LCG_ADVANCE();
                    patrolWaitFrames           = _gHelicopterLandingPadNo9PostAttackWaitFrames[RANDOM_LCG_DRAW() & 0xF];
                    work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->stateCounter         = patrolWaitFrames;
                }
            }
            break;
        case DASH_STRIKE:
            work->lapSpeed = 0;
            if (work->animationFrame == 0xE) {
                soundKey = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_DASH_STRIKE;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= 0x3B) {
                work->subState             = PATROL_BACKSTEP;
                work->animationId          = BODY_ANIMATION_BACKSTEP;
                work->state                = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
    }
}

/// Runs the lap-ending ignition, lethal strike and player-death sequence.
static void _helicopterLandingPadNo9TickLethalAttack(Task* task)
{
    enum {
        LETHAL_IGNITE              = 0,
        PLAYER_HEAD_PART           = 4, // Of Aya's rig: where the splatter of the lethal strike lands
        LETHAL_WINDUP              = 1,
        LETHAL_STRIKE              = 2,
        LETHAL_PLAYER_DEAD         = 3,
        DEATH_SOUND_IDLE           = 0,
        DEATH_SOUND_REQUEST        = 1,
        DEATH_SOUND_WAIT           = 2,
        LETHAL_IGNITION_FRAME      = 57,
        LETHAL_IGNITION_END_FRAME  = 90,
        LETHAL_WINDUP_SOUND_FRAME  = 10,
        LETHAL_WINDUP_END_FRAME    = 100,
        LETHAL_ATTACK_ON_FRAME     = 80,
        LETHAL_ATTACK_SOUND_FRAME  = 83,
        LETHAL_VIBRATION_FRAME     = 88,
        LETHAL_ATTACK_OFF_FRAME    = 96,
        LETHAL_CONTACT_FIRST_FRAME = 81,
        LETHAL_MISS_END_FRAME      = 179,
        LETHAL_RECOVER_FRAME       = 170,
        LETHAL_FLAME_FRAMES        = 270,
        LETHAL_RESTART_DELAY       = 128,
        LETHAL_HIT_EFFECT_INTERVAL = 6,
        PLAYER_DEATH_SOUND_FILE    = 30,
        SOUND_LETHAL_FLAME_STRIKE  = SOUND_CHARACTER(SOUND_BANK_NO_9_AKROPOLIS, NO_9_AKROPOLIS_SOUND_LETHAL_STRIKE),
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
        case LETHAL_IGNITE:
            work->lethalAttackPhase = LETHAL_PHASE_WINDUP;
            work->lapSpeed          = 0;
            if (work->animationFrame == LETHAL_IGNITION_FRAME) {
                work->flameMode   = HELICOPTER_LANDING_PAD_NO_9_FLAME_BURNING;
                work->flameFrames = LETHAL_FLAME_FRAMES;
                soundKey          = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_FIRE;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                work->flameSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_FLAME_LOOP;
                sndEvtRequestScriptStart(work->flameSound, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                _helicopterLandingPadNo9ArmAttack(work, ATTACK_IGNITION);
            }
            if (work->animationFrame >= LETHAL_IGNITION_END_FRAME) {
                work->animationId = BODY_ANIMATION_FIGHT_START;
                work->subState    = LETHAL_WINDUP;
            }
            break;
        case LETHAL_WINDUP:
            if (work->animationFrame == LETHAL_WINDUP_SOUND_FRAME) {
                soundKey = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_WINDUP;
                sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            if (work->animationFrame >= LETHAL_WINDUP_END_FRAME) {
                work->animationId = BODY_ANIMATION_LETHAL_STRIKE;
                work->subState    = LETHAL_STRIKE;
            }
            break;
        case LETHAL_STRIKE:
            if (work->animationFrame == LETHAL_ATTACK_ON_FRAME) {
                // Arm the fatal hit before the death-delay and sound holds.
                work->lethalAttackPhase = LETHAL_PHASE_STRUCK;
                _helicopterLandingPadNo9ArmAttack(work, ATTACK_LETHAL);

                gGameSession->deathRestartDelay   = LETHAL_RESTART_DELAY;
                gGameSession->deathSoundCountdown = GAME_SESSION_DEATH_SOUND_HOLD;
            }
            if (work->animationFrame == LETHAL_ATTACK_SOUND_FRAME) {
                if (work->flameMode == HELICOPTER_LANDING_PAD_NO_9_FLAME_BURNING) {
                    soundKey = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_LETHAL_FLAME_STRIKE;
                    sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                             (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                } else {
                    soundKey = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_DASH_STRIKE;
                    sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                             (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                }
            }
            if (work->animationFrame == LETHAL_VIBRATION_FRAME) {
                padScriptSpawn(gAkropolisHelicopterLandingPadLethalStrikeVibrationCommands, &gAkropolisHelicopterLandingPadLethalStrikeVibrationSegment);
            }
            if (work->animationFrame == LETHAL_ATTACK_OFF_FRAME) {
                work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            animationValue = (u16)work->animationFrame;
            _helicopterLandingPadNo9SetLethalLapSpeed(work, animationValue, -0x40);
            if (work->animationFrame >= LETHAL_CONTACT_FIRST_FRAME) {
                if (work->attackLanded == 1) {
                    work->subState           = LETHAL_PLAYER_DEAD;
                    work->attackLanded       = 0;
                    work->deathSoundLoadStep = DEATH_SOUND_REQUEST;
                    gPlayerStatus.hp         = 0;
                    roomEffectRequestCancelPe();
                }
            } else {
                work->attackLanded = 0;
            }
            if (work->animationFrame >= LETHAL_MISS_END_FRAME) {
                work->state        = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                work->animationId  = BODY_ANIMATION_PATROL_IDLE;
                work->subState     = PATROL_WAIT;
                work->stateCounter = _gHelicopterLandingPadNo9PostAttackWaitFrames[((u32)(randomState = RANDOM_LCG_NEXT(gRandomLcgState)) >> 0x10) & 0xF];
                gRandomLcgState    = randomState;
            }
            break;
        case LETHAL_PLAYER_DEAD:
            animationValue = work->animationId;
            if (animationValue == BODY_ANIMATION_LETHAL_STRIKE) {
                animationValue = (u16)work->animationFrame;
                _helicopterLandingPadNo9SetLethalLapSpeed(work, animationValue, -0x10);
                if (work->animationFrame >= LETHAL_RECOVER_FRAME) {
                    work->animationId = BODY_ANIMATION_SLASH_RECOVER;
                }
            }
            work->stateCounter++;
            if (work->stateCounter >= LETHAL_HIT_EFFECT_INTERVAL) {
                work->stateCounter                                = 0;
                _gHelicopterLandingPadNo9LethalHitEffectArg.coord = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
                effectSpawnHit(EFFECT_HIT_KIND_SPLATTER, &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[PLAYER_HEAD_PART], NULL,
                               &_gHelicopterLandingPadNo9LethalHitEffectArg);
            }
            switch (work->deathSoundLoadStep) {
                case DEATH_SOUND_IDLE:
                    break;
                case DEATH_SOUND_REQUEST:
                    cdCmdEnqueueDisplayResource(9, PLAYER_DEATH_SOUND_FILE, CD_COMMAND_DISPLAY_LOAD_DEFAULT);
                    work->deathSoundLoadStep = DEATH_SOUND_WAIT;
                    break;
                case DEATH_SOUND_WAIT:
                    if (cdCmdIsIdle() == 1) {
                        bodyCoord = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
                        sndEvtRequestScriptStart(SOUND_PLAYER_DEATH, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                                 (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                        work->deathSoundLoadStep = DEATH_SOUND_IDLE;
                    }
                    break;
            }
            break;
    }

    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// Steps the hit-recoil animation backward along the current lap side.
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
        case RECOIL_ENTER:
            work->animationId = BODY_ANIMATION_RECOIL;
            work->lapSpeed    = 0;
            work->subState    = RECOIL_PLAY;
            soundKey          = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_RECOIL;
            audioPan          = (s8)worldCoordGetOriginAudioPan(bodyCoord);
            sndEvtRequestScriptStart(soundKey, audioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            break;
        case RECOIL_PLAY:
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
                    work->animationId = BODY_ANIMATION_DEATH;
                } else {
                    work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                    work->animationId = BODY_ANIMATION_PATROL_IDLE;
                    work->subState    = PATROL_WAIT;
                    work->stateCounter =
                        _gHelicopterLandingPadNo9ReactionWaitFrames[((u32)(randomState = RANDOM_LCG_NEXT(gRandomLcgState)) >> 16) & 0xF];
                    gRandomLcgState = randomState;
                }
            }
            break;
    }
}

/// Steps the helipad-blast recoil, trailing sparks and retreating along the lap.
static void _helicopterLandingPadNo9TickSparkRecoil(Task* task)
{
    enum {
        RETREAT_FRAME  = 71,
        RETREAT_FRAMES = 7,
        RETREAT_SPEED  = -83,
        END_FRAME      = 81,
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
        case SPARK_RECOIL_ENTER:
            work->animationId  = BODY_ANIMATION_SPARK_RECOIL;
            work->lapSpeed     = 0;
            work->subState     = SPARK_RECOIL_PLAY;
            work->stateCounter = 0;
            work->sparkSound   = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_SPARK_LOOP;
            startAudioPan      = (s8)worldCoordGetOriginAudioPan(bodyCoord);
            sndEvtRequestScriptStart(work->sparkSound, startAudioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            break;
        case SPARK_RECOIL_PLAY:
            if (work->animationFrame & 1) {
                _helicopterLandingPadNo9SpawnReactionSpark(bodyCoord, sparkOffset);
            }
            work->stateCounter++;
            if (work->stateCounter >= SPARK_MIX_INTERVAL) {
                loopAudioPan = (s8)worldCoordGetOriginAudioPan(bodyCoord);
                sndEvtRequestScriptMix(work->sparkSound, loopAudioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                work->stateCounter = 0;
            }
            if ((work->sideTravelled < LAP_MIN_DISTANCE) || (work->playerSide != work->lapSide)) {
                work->lapSpeed = 0;
            } else {
                retreatSpeed = 0;
                if ((u32)((u16)work->animationFrame - RETREAT_FRAME) < RETREAT_FRAMES) {
                    retreatSpeed = RETREAT_SPEED;
                }
                work->lapSpeed = retreatSpeed;
            }
            if (work->animationFrame >= END_FRAME) {
                if (work->sparkSound != 0) {
                    sndEvtRequestScriptStop(work->sparkSound, SOUND_SCRIPT_STOP_NO_FADE);
                    work->sparkSound = 0;
                }
                if (enemy->hp <= 0) {
                    work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_DEATH;
                    work->subState    = DEATH_ENTER;
                    work->animationId = BODY_ANIMATION_DEATH;
                } else {
                    work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                    work->animationId = BODY_ANIMATION_PATROL_IDLE;
                    work->subState    = PATROL_WAIT;
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
static void _helicopterLandingPadNo9TickSparkStun(Task* task)
{
    enum {
        SPARK_STUN_END_FRAME = 30,
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
        case SPARK_STUN_ENTER:
            work->lapSpeed     = 0;
            work->animationId  = BODY_ANIMATION_STUN;
            work->subState     = SPARK_STUN_PLAY;
            work->stateCounter = 0;
            work->sparkSound   = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_SPARK_LOOP;
            startAudioPan      = (s8)worldCoordGetOriginAudioPan(bodyCoord);
            sndEvtRequestScriptStart(work->sparkSound, startAudioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            break;
        case SPARK_STUN_PLAY:
            if (work->animationFrame & 1) {
                _helicopterLandingPadNo9SpawnReactionSpark(bodyCoord, sparkOffset);
            }
            work->stateCounter++;
            if (work->stateCounter >= SPARK_MIX_INTERVAL) {
                loopAudioPan = (s8)worldCoordGetOriginAudioPan(bodyCoord);
                sndEvtRequestScriptMix(work->sparkSound, loopAudioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
                work->stateCounter = 0;
            }
            if (work->animationFrame >= SPARK_STUN_END_FRAME) {
                if (work->sparkSound != 0) {
                    sndEvtRequestScriptStop(work->sparkSound, SOUND_SCRIPT_STOP_NO_FADE);
                    work->sparkSound = 0;
                }
                work->animationId = BODY_ANIMATION_STUN_RECOVER;
                work->subState    = SPARK_STUN_RECOVER;
            }
            break;
        case SPARK_STUN_RECOVER:
            if (work->animationFrame >= STUN_RECOVER_END_FRAME) {
                if (enemy->hp <= 0) {
                    work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_DEATH;
                    work->subState    = DEATH_ENTER;
                    work->animationId = BODY_ANIMATION_DEATH;
                } else {
                    work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                    work->animationId = BODY_ANIMATION_PATROL_IDLE;
                    work->subState    = PATROL_WAIT;
                    work->stateCounter =
                        _gHelicopterLandingPadNo9ReactionWaitFrames[((u32)(randomState = RANDOM_LCG_NEXT(gRandomLcgState)) >> 0x10) & 0xF];
                    gRandomLcgState = randomState;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Silences the dying golem, removes its target and marks it absent. If the player has died first, it gets one HP back
/// and patrols instead.
static void _helicopterLandingPadNo9TickDeath(Task* task)
{
    enum {
        DEATH_SOUND_FRAME = 112,
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
        work->animationId                     = BODY_ANIMATION_PATROL_IDLE;
        work->subState                        = PATROL_WAIT;
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
    if (work->animationFrame == DEATH_SOUND_FRAME) {
        soundKey = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_FLAME_STRIKE;
        audioPan = (s8)worldCoordGetOriginAudioPan(bodyCoord);
        sndEvtRequestScriptStart(soundKey, audioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
    }
    gAttachmentState.flags &= ATTACHMENT_FLAG_EVENT_LOCK;
    work->present           = 0;
}

/// Updates the player's lap side and the horizontal ranges used by combat.
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
static void _helicopterLandingPadNo9TurnAlongLap(Task* task)
{
    enum {
        DISTANCE = 1000,
        STEP     = 30,
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
        if (work->sideRemaining < DISTANCE) {
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
            if (differenceMagnitude < (STEP + 1)) {
                work->yaw = targetYaw;
            } else {
                previousYaw = work->yaw;
                if (signedDifference > 0) {
                    work->yaw = previousYaw + STEP;
                } else {
                    work->yaw = previousYaw - STEP;
                }
            }
        } else if (signedDifference > 0 ? (ACTOR_TRANSFORM_ANGLE_TURN - signedDifference) < (STEP + 1) : (signedDifference + ACTOR_TRANSFORM_ANGLE_TURN) < (STEP + 1)) {
            work->yaw = targetYaw;
        } else {
            wrappedPreviousYaw = work->yaw;
            if (shortDifference > 0) {
                work->yaw = wrappedPreviousYaw - STEP;
            } else {
                work->yaw = wrappedPreviousYaw + STEP;
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
    if (nextDistance < LAP_MIN_DISTANCE) {
        work->lapDistance = LAP_MIN_DISTANCE;
    } else if (nextDistance > LAP_MAX_DISTANCE) {
        work->lapDistance = LAP_MAX_DISTANCE;
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
            soundKey     = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_STEP_CUE_2;
            cue2AudioPan = (s8)worldCoordGetOriginAudioPan(bodyCoord);
            sndEvtRequestScriptStart(soundKey, cue2AudioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
        }
        if (!(record->flags & ANIMATION_RECORD_CUE_1) && (work->lastCueFlags & ANIMATION_RECORD_CUE_1)) {
            soundKey     = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_STEP_CUE_1;
            cue1AudioPan = (s8)worldCoordGetOriginAudioPan(bodyCoord);
            sndEvtRequestScriptStart(soundKey, cue1AudioPan, (s8)worldCoordGetOriginAudioDepth(bodyCoord));
        }
        work->lastCueFlags = (u16)(record->flags & ANIMATION_RECORD_CUE_MASK);
    }
}

#include "../../../../shared/creatures/golem/no9_golem_aim_head.inc.c"

/// Applies the residual hit rotation to the chest and relaxes pitch and yaw.
static void _helicopterLandingPadNo9ApplyHitTwist(Task* task)
{
    enum {
        HIT_TWIST_STEP = 32,
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
    _helicopterLandingPadNo9ComposeHitRotation(&bodyCoords[CHEST_PART].coord, twistMatrix);

    pitch = work->hitTwist.vx;
    if (pitch != 0) {
        pitchMagnitude = __builtin_abs(pitch);
        if (pitchMagnitude < (HIT_TWIST_STEP + 1)) {
            work->hitTwist.vx = 0;
        } else {
            nextPitch = pitch - HIT_TWIST_STEP;
            if (pitch <= 0) {
                nextPitch = pitch + HIT_TWIST_STEP;
            }
            work->hitTwist.vx = nextPitch;
            stillTwisted      = 1;
        }
    }

    yaw = work->hitTwist.vy;
    if (yaw != 0) {
        yawMagnitude = __builtin_abs(yaw);
        if (yawMagnitude < (HIT_TWIST_STEP + 1)) {
            work->hitTwist.vy = 0;
        } else {
            nextYaw = yaw - HIT_TWIST_STEP;
            if (yaw <= 0) {
                nextYaw = yaw + HIT_TWIST_STEP;
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
static void _helicopterLandingPadNo9UpdateGridFaces(Task* task)
{
    enum {
        OFFSET_X = -600,
        OFFSET_Z = 640,
        LIMIT    = 6000,
    };
    _GridFacesScratch* scratch;
    GfxCoord*          bodyCoord;
    SVECTOR*           normals;
    SVECTOR*           vertices;
    s32                vectorIndex;

    scratch   = SCRATCH_STACK_RESERVE_BLOCK(_GridFacesScratch);
    bodyCoord = task->extra.tmd->coords;
    normals   = gWorldCollisionActiveGrid->normals;
    vertices  = gWorldCollisionActiveGrid->vertices;

    scratch->origin.vx = OFFSET_X;
    scratch->origin.vy = 0;
    scratch->origin.vz = OFFSET_Z;

    gte_SetRotMatrix(&bodyCoord->coord);
    gte_ldv0(&scratch->origin);
    gte_rtv0();
    gte_stsv(&scratch->rotated);

    scratch->origin.vx = bodyCoord->coord.t[0] + scratch->rotated.vx;
    scratch->origin.vy = bodyCoord->coord.t[1] + scratch->rotated.vy;
    scratch->origin.vz = bodyCoord->coord.t[2] + scratch->rotated.vz;

    if (scratch->origin.vx > LIMIT) {
        scratch->origin.vx = LIMIT;
    } else if (scratch->origin.vx < -LIMIT) {
        scratch->origin.vx = -LIMIT;
    }
    if (scratch->origin.vz > LIMIT) {
        scratch->origin.vz = LIMIT;
    } else if (scratch->origin.vz < -LIMIT) {
        scratch->origin.vz = -LIMIT;
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

    SCRATCH_STACK_RELEASE_BLOCK(_GridFacesScratch);
}

/// Handles message 2007: show (0), start the fight (1), or hide (2). Starting ignites the flame only if it has never
/// burned.
static s32 _helicopterLandingPadNo9SetActivation(Task* task, s32 messageId, s32 command, s32 unusedArg)
{
    enum {
        ACTIVATION_SHOW        = 0,
        ACTIVATION_START_FIGHT = 1,
        ACTIVATION_HIDE        = 2,
        PUT_AWAY               = 0,
        SHOWN                  = 1,
        FIGHTING               = 2,
        FIGHT_START_DISTANCE   = 4600,
        FIGHT_START_YAW        = 1024,
        INITIAL_FLAME_FRAMES   = 240,
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
        case ACTIVATION_SHOW:
            sceneAcquireBattleRef(0x1B);
            bodyModel->flags                       = 0;
            work->weaponTask->extra.tmd->flags     = 0;
            work->chestModelTask->extra.tmd->flags = 0;
            work->activation                       = SHOWN;
            break;
        case ACTIVATION_START_FIGHT:
            bodyModel->flags                       = 0;
            work->weaponTask->extra.tmd->flags     = 0;
            work->chestModelTask->extra.tmd->flags = 0;
            work->activation                       = FIGHTING;
            work->lapDistance                      = FIGHT_START_DISTANCE;
            vector                                 = rotation;
            work->animationId                      = BODY_ANIMATION_FIGHT_START;
            work->seededAnimationId                = BODY_ANIMATION_FIGHT_START;
            work->yaw                              = FIGHT_START_YAW;
            work->state                            = HELICOPTER_LANDING_PAD_NO_9_STATE_OPENING;
            work->subState                         = OPENING_INTRO;
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
                work->flameFrames = INITIAL_FLAME_FRAMES;
                _helicopterLandingPadNo9ArmAttack(work, ATTACK_IGNITION);
                work->flameSound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_FLAME_LOOP;
                sndEvtRequestScriptStart(work->flameSound, (s8)worldCoordGetOriginAudioPan(bodyCoord),
                                         (s8)worldCoordGetOriginAudioDepth(bodyCoord));
            }
            break;
        case ACTIVATION_HIDE:
            bodyModel->flags                       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->weaponTask->extra.tmd->flags     = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->chestModelTask->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->activation                       = PUT_AWAY;
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
static void _helicopterLandingPadNo9UpdateProp(Enemy* enemy, Task* task)
{
    // The reserved 32-byte scratch frame has no directly accessed object here.
    enum { PROP_SCRATCH_BYTES = 0x20 };
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
    if (parentWork->animationId < BODY_ANIMATION_EVENT_1) {
        propModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        return;
    }
    SCRATCH_STACK_RESERVE_BYTES(PROP_SCRATCH_BYTES);
    switch (parentWork->animationId) {
        case BODY_ANIMATION_EVENT_1:
        case BODY_ANIMATION_EVENT_2:
            break;

        case BODY_ANIMATION_PROP_REVEAL:
            if (parentWork->animationFrame == 0xC8) {
                propModel->flags = 0;
            }
            break;

        case BODY_ANIMATION_PROP_TOSS:
            handCoord      = &task->parent->extra.tmd->coords[LEFT_HAND_PART];
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
    SCRATCH_STACK_RELEASE_BYTES(PROP_SCRATCH_BYTES);
}

/// Initializes the golem's chest-launched stun grenade and its collision bodies.
static void _helicopterLandingPadNo9InitGrenade(Enemy* enemy, Task* task)
{
    enum {
        PITCH_BUCKET_DISTANCE = 1000,
        FLIGHT_RADIUS         = 200,
        GRID_PROBE_LENGTH     = 500,
    };

    _GrenadeWork*           work;
    ActorChildPlaceScratch* placement;
    TmdObject*              grenadeModel;
    GfxCoord*               grenadeCoord;
    GfxCoord*               bodyCoords;
    GfxCoord*               chestCoord;
    s32                     playerDeltaX;
    s32                     playerDeltaZ;
    s32                     pitchBucket;

    grenadeModel = task->extra.tmd;
    grenadeCoord = grenadeModel->coords;
    bodyCoords   = task->parent->extra.tmd->coords;
    chestCoord   = &bodyCoords[CHEST_PART];
    work         = memCalloc(sizeof(_GrenadeWork), false);
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
    pitchBucket          = SquareRoot0((playerDeltaX * playerDeltaX) + (playerDeltaZ * playerDeltaZ)) / PITCH_BUCKET_DISTANCE;
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
    work->attack.radius           = FLIGHT_RADIUS;
    work->attack.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attack);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);

    work->gridProbeCapsule.ends[0].vx = 0;
    work->gridProbeCapsule.ends[0].vy = 0;
    work->gridProbeCapsule.ends[0].vz = 0;
    work->gridProbeCapsule.ends[1].vx = 0;
    work->gridProbeCapsule.ends[1].vy = GRID_PROBE_LENGTH;
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

    task->state = CHILD_TASK_RUNNING;
    SCRATCH_STACK_RELEASE_BLOCK(ActorChildPlaceScratch);
}

/// Flies the grenade along its pitching launch axis until it bursts.
static void _helicopterLandingPadNo9TickGrenadeFlight(Enemy* enemy, Task* task)
{
    enum {
        GRENADE_FLIGHT_STEP            = 150,
        GRENADE_ROTATION_FRACTION_BITS = 12,
        GRENADE_BURST_HEIGHT           = 1300,
        GRENADE_TRAIL_INTERVAL         = 3,
        GRENADE_TRAIL_OFFSET_Y         = 100,
        GRENADE_TRAIL_OPTIONS          = 0x01001600,
        GRENADE_EXPLOSION_OPTIONS      = 0x10002200,
        GRENADE_BURST_SMOKE_OPTIONS    = 0xC1001200,
        SOUND_GRENADE_BURST            = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_HELICOPTER_LANDING_PAD, 9),
    };
    VECTOR                       samplePosition;
    EffectWork*                  burstEffect;
    GfxCoord*                    grenadeCoord;
    ActorEulerTurnScratch*       pitchScratch;
    TmdObject*                   grenadeModel;
    _GrenadeWork*                work;
    HelicopterLandingPadNo9Work* parentWork;
    s32                          pitchStep;
    s32                          soundKey;
    s32                          crossedBurstHeight;
    u16                          trailFrames;

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
    grenadeCoord->coord.t[0]  += -(grenadeCoord->coord.m[0][1] * GRENADE_FLIGHT_STEP) >> GRENADE_ROTATION_FRACTION_BITS;
    grenadeCoord->coord.t[1]  += -(grenadeCoord->coord.m[1][1] * GRENADE_FLIGHT_STEP) >> GRENADE_ROTATION_FRACTION_BITS;
    grenadeCoord->coord.t[2]  += -(grenadeCoord->coord.m[2][1] * GRENADE_FLIGHT_STEP) >> GRENADE_ROTATION_FRACTION_BITS;
    trailFrames                = work->frames + 1;
    work->frames               = trailFrames;
    if (trailFrames >= GRENADE_TRAIL_INTERVAL) {
        pitchScratch->angles.vx = 0;
        pitchScratch->angles.vy = GRENADE_TRAIL_OFFSET_Y;
        pitchScratch->angles.vz = 0;
        effectSpawn(EFFECT_SMOKE_PUFF, grenadeCoord, GRENADE_TRAIL_OPTIONS, &pitchScratch->angles);
        work->frames = 0;
    }
    samplePosition.vx = grenadeCoord->workm.t[0];
    samplePosition.vy = grenadeCoord->workm.t[1];
    samplePosition.vz = grenadeCoord->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &samplePosition, 0, 0);
    if (grenadeCoord->coord.t[1] < -GRENADE_BURST_HEIGHT) {
        work->phase = GRENADE_FLIGHT_PEAKED;
    }
    if (work->phase != GRENADE_FLIGHT_BELOW_PEAK && grenadeCoord->coord.t[1] >= -GRENADE_BURST_HEIGHT + 1) {
        crossedBurstHeight = 1;
    }
    // Hide the model, adopt the impact effect, and retain the spreading/hold task.
    if (crossedBurstHeight != 0 || (work->attackContacts[0].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY || work->gridProbeContacts[0].key.value != 0) {
        effectSpawn(EFFECT_EXPLOSION, grenadeCoord, GRENADE_EXPLOSION_OPTIONS, NULL);
        effectSpawn(EFFECT_SMOKE_PUFF, grenadeCoord, GRENADE_BURST_SMOKE_OPTIONS, NULL);
        burstEffect = effectSpawn((EFFECT_HELICOPTER_LANDING_PAD_NO_9_IMPACT_BURST | EFFECT_SPAWN_UNLIMITED), grenadeCoord, 0, NULL);
        if (burstEffect != NULL) {
            taskReparent(task, burstEffect->task);
        }
        if (work->attackContacts[0].key.value != 0) {
            work->phase = GRENADE_BURST_HOLDING;
        } else {
            work->phase = GRENADE_BURST_SPREADING;
        }
        work->attack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(work->attackContacts);
        worldCollisionUnlinkBody(&work->gridProbe);
        work->frames           = 0;
        task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        soundKey               = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_GRENADE_BURST;
        sndEvtRequestScriptStart(soundKey, (s8)worldCoordGetOriginAudioPan(grenadeCoord), (s8)worldCoordGetOriginAudioDepth(grenadeCoord));
        task->state = GRENADE_TASK_BURST;
    }
    worldCollisionClearContacts(work->attackContacts);
    if (parentWork->present == 0) {
        _helicopterLandingPadNo9CancelGrenadeCatch(task, work);
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorEulerTurnScratch);
}

/// Steps the hidden grenade's spreading blast, player hold and delayed teardown.
static void _helicopterLandingPadNo9TickGrenadeBurst(Enemy* enemy, Task* task)
{
    enum {
        SPREAD_FRAMES = 16,
        CATCH_FRAMES  = 31,
        END_FRAMES    = 31,
        BURST_RADIUS  = 600,
    };

    _GrenadeWork*                work;
    HelicopterLandingPadNo9Work* parent;
    u16                          phaseFrames;

    work   = task->work;
    parent = task->parent->work;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        switch (work->phase) {
            case GRENADE_BURST_SPREADING:
                phaseFrames  = work->frames + 1;
                work->frames = phaseFrames;
                if (phaseFrames >= SPREAD_FRAMES) {
                    work->attack.radius = BURST_RADIUS;
                    work->phase         = GRENADE_BURST_CATCHING;
                    work->frames        = 0;
                    work->attack.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    return;
                }
                if (parent->present == 0) {
                    _helicopterLandingPadNo9CancelGrenadeCatch(task, work);
                    return;
                }
                break;
            case GRENADE_BURST_CATCHING:
                if ((work->attackContacts[0].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
                    work->phase         = GRENADE_BURST_HOLDING;
                    work->holdStep      = GRENADE_HOLD_REQUEST;
                    work->attack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                } else {
                    phaseFrames  = work->frames + 1;
                    work->frames = phaseFrames;
                    // A timed-out catch keeps pair tests enabled during the ending delay.
                    if (phaseFrames >= CATCH_FRAMES) {
                        work->phase  = GRENADE_BURST_ENDING;
                        work->frames = 0;
                    } else if (parent->present == 0) {
                        _helicopterLandingPadNo9CancelGrenadeCatch(task, work);
                    }
                }
                worldCollisionClearContacts(work->attackContacts);
                return;
            case GRENADE_BURST_HOLDING:
                _helicopterLandingPadNo9TickGrenadeHold(task);
                return;
            case GRENADE_BURST_ENDING:
                phaseFrames  = work->frames + 1;
                work->frames = phaseFrames;
                if (phaseFrames >= END_FRAMES) {
                    parent->grenadeLive = 0;
                    worldCollisionUnlinkBody(&work->attack);
                    enemyDestroy(enemy, task);
                }
                break;
        }
    }
}

/// Steps the stun grenade's button-press hold and recovery of the player.
static void _helicopterLandingPadNo9TickGrenadeHold(Task* task)
{
    enum {
        GRENADE_HOLD_PRESS_COUNT   = 12,
        GRENADE_HOLD_STUN_LIMIT    = 61,
        GRENADE_HOLD_RECOVER_LIMIT = 21,
        SOUND_GRENADE_HOLD         = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_HELICOPTER_LANDING_PAD, 10),
    };
    _GrenadeWork*                work;
    HelicopterLandingPadNo9Work* parent;
    Task*                        player;
    ActorPlayerHoldScratch*      scratch;
    GfxCoord*                    playerCoord;
    u16                          nextFrame;
    s32                          soundKey;
    s32                          audioPan;

    work    = task->work;
    parent  = task->parent->work;
    player  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch = SCRATCH_STACK_RESERVE_BLOCK(ActorPlayerHoldScratch);

    switch (work->holdStep) {
        case GRENADE_HOLD_REQUEST:
            if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
                // All hold and animation payloads are borrowed during dispatch only.
                scratch->buttonPressHold.pressCount = GRENADE_HOLD_PRESS_COUNT;
                if (TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &scratch->buttonPressHold, 0) != 0) {
                    work->phase = GRENADE_BURST_ENDING;
                    break;
                }
                taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(gHelicopterLandingPadNo9Attacks, ATTACK_GRENADE), 0);
                scratch->playerAnim.source.sets          = _gHelicopterLandingPadNo9PlayerGrenadeAnimationSets;
                scratch->playerAnim.animationId          = PLAYER_GRENADE_ANIMATION_STUN;
                scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;
                scratch->playerAnim.blendFrames          = 0;
                scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &scratch->playerAnim, 0);
                work->holdStep     = GRENADE_HOLD_STUNNED;
                work->phaseCounter = 0;
                playerCoord        = player->extra.tmd->coords;
                soundKey           = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_GRENADE_HOLD;
                audioPan           = (s8)worldCoordGetOriginAudioPan(playerCoord);
                sndEvtRequestScriptStart(soundKey, audioPan, (s8)worldCoordGetOriginAudioDepth(playerCoord));
            }
            break;
        case GRENADE_HOLD_STUNNED:
            nextFrame          = work->phaseCounter + 1;
            work->phaseCounter = nextFrame;
            if ((s16)nextFrame < GRENADE_HOLD_STUN_LIMIT && parent->playerEscaped != 1 && parent->present != 0) {
                break;
            }
            parent->playerEscaped                    = 0;
            scratch->playerAnim.source.sets          = _gHelicopterLandingPadNo9PlayerGrenadeAnimationSets;
            scratch->playerAnim.animationId          = PLAYER_GRENADE_ANIMATION_RECOVER;
            scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;
            scratch->playerAnim.blendFrames          = 0;
            scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &scratch->playerAnim, 0);
            work->holdStep     = GRENADE_HOLD_RECOVER;
            work->phaseCounter = 0;
            break;
        case GRENADE_HOLD_RECOVER:
            nextFrame          = work->phaseCounter + 1;
            work->phaseCounter = nextFrame;
            if ((s16)nextFrame >= GRENADE_HOLD_RECOVER_LIMIT) {
                if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                    taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                    work->phase = GRENADE_BURST_ENDING;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorPlayerHoldScratch);
}

/// Initializes one of the three destructible transformers and its blast sphere.
static void _helicopterLandingPadNo9InitTransformer(Enemy* enemy, Task* task)
{
    enum {
        CONTACT_PART = 10,
        BODY_RADIUS  = 200,
        BLAST_RADIUS = 350,
        HAZARD_ID    = 2,
    };

    TmdObject*        model;
    GfxCoord*         partCoords;
    _TransformerWork* work;
    GfxCoord*         contactCoord;
    SVECTOR*          spawnRotation;
    s32               slotIndex;

    model        = task->extra.tmd;
    partCoords   = model->coords;
    work         = memCalloc(sizeof(_TransformerWork), false);
    contactCoord = &partCoords[CONTACT_PART];
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
    enemy->bodyPos.vx             = -BODY_RADIUS;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    work->transformerIndex        = task->spawnArg1.value;
    spawnRotation->vx             = 0;
    spawnRotation->vy             = _gHelicopterLandingPadNo9TransformerYaws[work->transformerIndex];
    spawnRotation->vz             = 0;
    RotMatrix(spawnRotation, &partCoords->coord);
    slotIndex              = 1;
    partCoords->coord.t[0] = _gHelicopterLandingPadNo9TransformerPositions[work->transformerIndex].vx;
    partCoords->coord.t[1] = _gHelicopterLandingPadNo9TransformerPositions[work->transformerIndex].vy;
    partCoords->coord.t[2] = _gHelicopterLandingPadNo9TransformerPositions[work->transformerIndex].vz;
    partCoords->parent     = &gGfxViewCoord;
    animationInitContext(&work->anim, _gHelicopterLandingPadNo9TransformerCableAnimationSets, model, work->poses, work->slots);
    do {
        animationResetSlot(&work->anim, slotIndex, CABLE_ANIMATION_INTACT);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->slots));
    work->body.pos.vx           = -BODY_RADIUS;
    work->body.coord            = contactCoord;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.context.contacts = work->bodyContacts;
    work->body.key              = 0;
    work->body.radius           = BODY_RADIUS;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);
    work->blast.key              = DAMAGE_HAZARD_CATEGORY | HAZARD_ID;
    work->blast.coord            = contactCoord;
    work->blast.pos.vx           = 0;
    work->blast.pos.vy           = 0;
    work->blast.pos.vz           = 0;
    work->blast.context.contacts = work->blastContacts;
    work->blast.radius           = BLAST_RADIUS;
    work->blast.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->body.flags            &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_BLASTS, &work->blast);
    worldCollisionInitContacts(work->blastContacts, ARRAY_SIZE(work->blastContacts), 0);
    work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->exitCallback = _helicopterLandingPadNo9ExitTransformer;
    task->state        = CHILD_TASK_RUNNING;
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Advances a transformer's animation, break state and published status: transformers 0 and 1 to
/// `EVENT_FLAG_AKROPOLIS_HELIPAD_TRANSFORMER_1_STATE` and `_2_STATE`, the third to the golem for the blast source beside it.
static void _helicopterLandingPadNo9TickTransformer(Enemy* enemy, Task* task)
{
    enum {
        FLAGGED_TRANSFORMER_COUNT = 2, // The third reports to the golem, whose blast source publishes both
    };
    TmdObject*                   model;
    _TransformerWork*            work;
    GfxCoord*                    rootCoord;
    HelicopterLandingPadNo9Work* bodyWork;
    s32                          slotIndex;

    model     = task->extra.tmd;
    work      = task->work;
    rootCoord = model->coords;
    bodyWork  = task->parent->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (_helicopterLandingPadNo9CheckTransformerView(task) == 0) {
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
            _helicopterLandingPadNo9UpdateTransformerLighting(task, rootCoord);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    // Publish damage progress only on the full in-view update.
    _helicopterLandingPadNo9TickTransformerBreak(task);
    if (work->transformerIndex < FLAGGED_TRANSFORMER_COUNT) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_HELIPAD_TRANSFORMER_1_STATE + work->transformerIndex, work->status);
    } else {
        bodyWork->blastTransformerStatus = work->status;
    }
    slotIndex = 1;
    do {
        animationTickSlot(&work->anim, slotIndex);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->slots));
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    _helicopterLandingPadNo9UpdateTransformerLighting(task, rootCoord);
}

/// Breaks a shot or slashed transformer and runs its sparks and one-hit blast.
static void _helicopterLandingPadNo9TickTransformerBreak(Task* task)
{
    enum {
        CABLE_SPARK_FRAMES        = 120,
        CABLE_TIP_PART            = 10, // Of the cable's rig: the cable's end, where it sparks; the four segments before it draw spark lines
        CABLE_SPARK_OFFSET_Y      = 128,
        CABLE_EXPLOSION_SIZE      = 512,
        TRANSFORMER_SHOT_DISTANCE = 1000,
        SOUND_TRANSFORMER_BREAK   = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_HELICOPTER_LANDING_PAD, 4),
    };
    _TransformerWork*            work;
    HelicopterLandingPadNo9Work* parent;
    Enemy*                       enemy;
    ActorFaceScratch*            sparkScratch;
    ActorFaceScratch*            scratchEnd;
    GfxCoord*                    contactCoord;
    EffectWork*                  sparksEffect;
    Task*                        sparksTask;
    s16                          doneState;
    s32                          breakRequested;
    s32                          slotIndex;
    s32                          soundKey;
    s32                          audioPan;
    s32                          attackKey;
    s16                          transformerState;

    breakRequested                         = 0;
    scratchEnd                             = SCRATCH_STACK_CURSOR(ActorFaceScratch);
    contactCoord                           = &task->extra.tmd->coords[CABLE_TIP_PART];
    SCRATCH_STACK_CURSOR(ActorFaceScratch) = scratchEnd - 1;
    sparkScratch                           = scratchEnd - 1;
    work                                   = task->work;
    enemy                                  = task->spawnArg2.pointer;
    transformerState                       = work->state;
    parent                                 = task->parent->work;
    switch (transformerState) {
        case TRANSFORMER_INTACT:
            if (parent->present == 0) {
                work->status = AKROPOLIS_HELIPAD_TRANSFORMER_STOPPED;
                work->state  = TRANSFORMER_DONE;
                break;
            }
            enemy->node.state.parts.flags = gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED;
            attackKey                     = work->bodyContacts[0].key.value;
            work->body.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            if ((attackKey & (WORLD_COLLISION_CONTACT_KIND_MASK | 0x8000)) == WORLD_COLLISION_CONTACT_ATTACK && enemy->node.state.parts.targeted == 1 &&
                damageComputePlayerAttack(attackKey, TRANSFORMER_SHOT_DISTANCE, 0, 0) != 0) {
                breakRequested = 1;
            }
            if (parent->slashedTransformer == work->transformerIndex + 1 && parent->transformerSlashStruck == 1) {
                breakRequested             = 1;
                parent->slashedTransformer = -1;
            }
            if (breakRequested == 1) {
                work->status = AKROPOLIS_HELIPAD_TRANSFORMER_SHOT;
                work->state  = TRANSFORMER_SPARKING;
                slotIndex    = 1;
                do {
                    animationSeekSlotWithBlend(&work->anim, slotIndex, CABLE_ANIMATION_BREAK, 0, 0);
                    slotIndex++;
                } while (slotIndex < ARRAY_SIZE(work->slots));
                sparkScratch->rot.vx = 0;
                sparkScratch->rot.vy = CABLE_SPARK_OFFSET_Y;
                sparkScratch->rot.vz = 0;
                sparksEffect         = effectSpawn((EFFECT_HELIPAD_TRANSFORMER_SPARKS | EFFECT_SPAWN_UNLIMITED), contactCoord, 0, &sparkScratch->rot);
                if (sparksEffect != NULL) {
                    sparksTask       = sparksEffect->task;
                    work->sparksTask = sparksTask;
                    taskReparent(task, sparksTask);
                }
                effectSpawn(EFFECT_EXPLOSION, contactCoord, CABLE_EXPLOSION_SIZE, &sparkScratch->rot);
                work->sparkFrames  = CABLE_SPARK_FRAMES;
                work->body.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->blast.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                soundKey           = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_TRANSFORMER_BREAK;
                audioPan           = (s8)worldCoordGetOriginAudioPan(contactCoord);
                sndEvtRequestScriptStart(soundKey, audioPan, (s8)worldCoordGetOriginAudioDepth(contactCoord));
            }
            worldCollisionClearContacts(work->bodyContacts);
            break;
        case TRANSFORMER_SPARKING:
            if (worldCollisionFindContactIndex(work->blastContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            worldCollisionClearContacts(work->blastContacts);
            akropolisHelicopterLandingPadDrawLowerSparkLine(&task->extra.tmd->coords[CABLE_TIP_PART - 1]);
            akropolisHelicopterLandingPadDrawLowerSparkLine(&task->extra.tmd->coords[CABLE_TIP_PART - 2]);
            akropolisHelicopterLandingPadDrawLowerSparkLine(&task->extra.tmd->coords[CABLE_TIP_PART - 3]);
            akropolisHelicopterLandingPadDrawLowerSparkLine(&task->extra.tmd->coords[CABLE_TIP_PART - 4]);
            work->sparkFrames--;
            doneState = TRANSFORMER_DONE;
            if (work->sparkFrames <= 0) {
                Task* sparksTask;

                work->status       = AKROPOLIS_HELIPAD_TRANSFORMER_SPENT;
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                sparksTask         = work->sparksTask;
                if (sparksTask != NULL) {
                    sparksTask->state = CABLE_SPARKS_BURSTS_ONLY;
                }
            } else {
                Task* sparksTask;

                if (parent->present != 0) {
                    break;
                }
                work->status       = AKROPOLIS_HELIPAD_TRANSFORMER_SPENT;
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                sparksTask         = work->sparksTask;
                if (sparksTask != NULL) {
                    sparksTask->state = CABLE_SPARKS_RELEASE;
                    work->sparksTask  = NULL;
                }
            }
            work->state = doneState;
            break;
        case TRANSFORMER_DONE:
            if (parent->present == 0) {
                Task* sparksTask;

                sparksTask = work->sparksTask;
                if (sparksTask != NULL) {
                    sparksTask->state = CABLE_SPARKS_RELEASE;
                    work->sparksTask  = NULL;
                }
            }
            break;
    }
    if (work->transformerIndex == 2) {
        parent->blastTransformerState = work->state;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

/// Initializes the coordinate-only blast source beside the third transformer.
static void _helicopterLandingPadNo9InitBlastSource(Enemy* enemy, Task* task)
{
    GfxCoord*         sourceCoord;
    _BlastSourceWork* work;

    sourceCoord = task->extra.coordBody->coord;
    work        = memCalloc(sizeof(_BlastSourceWork), false);
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
    task->state        = CHILD_TASK_RUNNING;
}

/// Updates the helipad blast source only while its view and combat are active.
static void _helicopterLandingPadNo9TickBlastSource(Enemy* enemy, Task* task)
{
    _BlastSourceWork*            work;
    HelicopterLandingPadNo9Work* parentWork;
    u32                          randomState;
    Task*                        flareTask;

    work       = task->work;
    parentWork = task->parent->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if ((viewGetMappedIndex() & 0xFF) != _gHelicopterLandingPadNo9HelipadViews.transformerViews[2][0]) {
                enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->blast.flags            &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                if (work->flareFrames != 0) {
                    work->flareFrames--;
                }
                flareTask = work->flareTask;
                if (flareTask != NULL) {
                    flareTask->state = SOURCE_FLARE_RELEASE;
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
    gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_HELIPAD_TRANSFORMER_3_STATE, _gHelicopterLandingPadNo9Transformer3FlagValues[work->state][parentWork->blastTransformerStatus]);
    randomState     = RANDOM_LCG_NEXT(gRandomLcgState);
    gRandomLcgState = randomState;
    if ((u16)((randomState >> 16) % 3) == 0) {
        effectSpawn(EFFECT_ACROPOLIS_HELIPAD_EMBER, task->extra.coordBody->coord, 0, NULL);
    }
}

/// Grenade task-state callbacks: 0 initialize, 1 flight, 2 hidden burst and player hold. The grenade and the effects it
/// adopts need the overlay loaded.
static const EnemyTaskFuncTable3 _gHelicopterLandingPadNo9GrenadeTaskStates = {
    { _helicopterLandingPadNo9InitGrenade, _helicopterLandingPadNo9TickGrenadeFlight, _helicopterLandingPadNo9TickGrenadeBurst },
};

/// Arms the helipad blast source after the third transformer breaks, then runs its shot flare.
static void _helicopterLandingPadNo9TickBlastSourcePhases(Task* task)
{
    enum {
        SOURCE_FLARE_FRAMES  = 60,
        SOURCE_SHOT_DISTANCE = 1000,
        SOUND_SOURCE_SHOT    = SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_HELICOPTER_LANDING_PAD, 2),
    };
    _BlastSourceWork*            work;
    GfxCoord*                    sourceCoord;
    HelicopterLandingPadNo9Work* parentWork;
    Enemy*                       enemy;
    EffectWork*                  flareEffect;
    Task*                        flareAfterShot;
    Task*                        endingFlare;
    Task*                        stoppedFlare;
    Task*                        releasedFlare;
    Task*                        spawnedFlare;
    s32                          attackKey;
    s32                          targeted;
    s32                          soundKey;
    s32                          audioPan;
    u16                          flareFrames;

    work        = task->work;
    sourceCoord = task->extra.coordBody->coord;
    parentWork  = task->parent->work;
    enemy       = task->spawnArg2.pointer;

    switch (work->state) {
        case BLAST_SOURCE_DORMANT:
            if (parentWork->blastTransformerState == TRANSFORMER_SPARKING) {
                work->state = BLAST_SOURCE_ARMED;
            }
            break;
        case BLAST_SOURCE_ARMED:
            enemy->node.state.parts.flags = gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_ENGAGED;
            attackKey                     = work->bodyContacts[0].key.value;
            work->body.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            if ((attackKey & ~0x7FFF) == WORLD_COLLISION_CONTACT_ATTACK) {
                targeted = enemy->node.state.parts.targeted;
                if (targeted == 1 && damageComputePlayerAttack(attackKey, SOURCE_SHOT_DISTANCE, 0, 0) != 0) {
                    work->state       = BLAST_SOURCE_SHOT;
                    work->flareFrames = SOURCE_FLARE_FRAMES;
                    effectSpawn(EFFECT_IMPACT_SPARK, sourceCoord, 0, NULL);
                    flareEffect = effectSpawn((EFFECT_05F | EFFECT_SPAWN_UNLIMITED), sourceCoord, 0, NULL);
                    if (flareEffect != NULL) {
                        spawnedFlare    = flareEffect->task;
                        work->flareTask = spawnedFlare;
                        taskReparent(task, spawnedFlare);
                    }
                    work->body.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->blast.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    if (parentWork->blastTransformerState == TRANSFORMER_DONE) {
                        work->flareTaskState = SOURCE_FLARE_RAW_COLOR;
                        work->blast.key      = DAMAGE_HAZARD_CATEGORY | HAZARD_SOURCE_RECOIL;
                    } else {
                        work->flareTaskState = SOURCE_FLARE_FLICKERING;
                        work->blast.key      = DAMAGE_HAZARD_CATEGORY | HAZARD_SOURCE_STUN;
                    }
                    soundKey = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_SOURCE_SHOT;
                    audioPan = (s8)worldCoordGetOriginAudioPan(sourceCoord);
                    sndEvtRequestScriptStart(soundKey, audioPan, (s8)worldCoordGetOriginAudioDepth(sourceCoord));
                }
            }
            worldCollisionClearContacts(work->bodyContacts);
            break;
        case BLAST_SOURCE_SHOT:
            flareAfterShot = work->flareTask;
            work->state    = BLAST_SOURCE_FLARING;
            if (flareAfterShot != NULL) {
                flareAfterShot->state = work->flareTaskState;
            }
            if (worldCollisionFindContactIndex(work->blastContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            worldCollisionClearContacts(work->blastContacts);
            break;
        case BLAST_SOURCE_FLARING:
            if (worldCollisionFindContactIndex(work->blastContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            worldCollisionClearContacts(work->blastContacts);
            flareFrames       = work->flareFrames - 1;
            work->flareFrames = flareFrames;
            if ((s16)flareFrames <= 0) {
                work->state        = BLAST_SOURCE_SPENT;
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                endingFlare        = work->flareTask;
                if (endingFlare != NULL) {
                    endingFlare->state = SOURCE_FLARE_EMBERS;
                }
                break;
            }
            if (parentWork->present == 0) {
                work->state        = BLAST_SOURCE_STOPPED;
                work->blast.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                stoppedFlare       = work->flareTask;
                if (stoppedFlare != NULL) {
                    stoppedFlare->state = SOURCE_FLARE_RELEASE;
                    work->flareTask     = NULL;
                }
            }
            break;
        case BLAST_SOURCE_SPENT:
            if (parentWork->present == 0) {
                work->state   = BLAST_SOURCE_STOPPED;
                releasedFlare = work->flareTask;
                if (releasedFlare != NULL) {
                    releasedFlare->state = SOURCE_FLARE_RELEASE;
                    work->flareTask      = NULL;
                }
            }
            break;
        case BLAST_SOURCE_STOPPED:
            break;
    }
}

/// Dispatches the Akropolis No. 9 GOLEM body's initialization and frame update.
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
static void _helicopterLandingPadNo9TickBody(Enemy* enemy, Task* task)
{
    if (gGameSession->eventState != 0) {
        helicopterLandingPadNo9TickEvent(enemy, task);
        return;
    }
    _helicopterLandingPadNo9TickCombat(enemy, task);
}

/// Runs the active golem's combat, motion, animation and presentation update.
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
        bodyCoord->composeStamp                           = GRAPHICS_COORD_DIRTY;
        task->extra.tmd->coords[PELVIS_PART].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(bodyCoord);
        _no9GolemDrawShadow(task);
        helicopterLandingPadNo9UpdateLighting(task, bodyCoord);
        _helicopterLandingPadNo9UpdateGridFaces(task);
        _helicopterLandingPadNo9UpdateFlameJet(task);
    }
}

/// Consumes enemy reaction requests and enters the golem's buildup stun.
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
        work->subState        = BUILDUP_STUN_WAIT;
        work->buildupStunned  = 1;
    }
    reactionFlags = enemy->reactionFlags;
    if (reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        enemy->reactionFlags = reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
    }
}

/// Dispatches the golem's thirteen combat states to their per-frame handlers.
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
static void _helicopterLandingPadNo9TickBuildupStun(Task* task)
{
    HelicopterLandingPadNo9Work* work;
    s32                          subState;
    s32                          randomState;

    work     = task->work;
    subState = work->subState;
    switch (subState) {
        case BUILDUP_STUN_WAIT:
            work->lapSpeed    = 0;
            work->animationId = BODY_ANIMATION_STUN;
            if (damageTickEnemyBuildup(task->spawnArg2.pointer) != 0) {
                work->animationId    = BODY_ANIMATION_STUN_RECOVER;
                work->subState       = BUILDUP_STUN_RECOVER;
                work->buildupStunned = 0;
            }
            break;
        case BUILDUP_STUN_RECOVER:
            if (work->animationFrame >= STUN_RECOVER_END_FRAME) {
                work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                work->subState    = PATROL_WAIT;
                work->animationId = BODY_ANIMATION_PATROL_IDLE;
                work->stateCounter =
                    _gHelicopterLandingPadNo9ReactionWaitFrames[((u32)(randomState = RANDOM_LCG_NEXT(gRandomLcgState)) >> 16) & 0xF];
                gRandomLcgState = randomState;
            }
            break;
    }
}

/// Plays the stationary flinch and selects death or patrol when it finishes.
static void _helicopterLandingPadNo9TickFlinch(Task* task)
{
    enum {
        FLINCH_END_FRAME = 80,
    };
    HelicopterLandingPadNo9Work* work;
    s32                          subState;
    Enemy*                       enemy;
    s32                          randomState;

    work     = task->work;
    subState = work->subState;
    enemy    = task->spawnArg2.pointer;
    switch (subState) {
        case FLINCH_ENTER:
            work->animationId = BODY_ANIMATION_FLINCH;
            work->lapSpeed    = 0;
            work->subState    = FLINCH_PLAY;
            break;
        case FLINCH_PLAY:
            if (work->animationFrame >= FLINCH_END_FRAME) {
                if (enemy->hp <= 0) {
                    work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_DEATH;
                    work->subState    = DEATH_ENTER;
                    work->animationId = BODY_ANIMATION_DEATH;
                } else {
                    work->state       = HELICOPTER_LANDING_PAD_NO_9_STATE_PATROL;
                    work->subState    = PATROL_WAIT;
                    work->animationId = BODY_ANIMATION_PATROL_IDLE;
                    work->stateCounter =
                        _gHelicopterLandingPadNo9ReactionWaitFrames[((u32)(randomState = RANDOM_LCG_NEXT(gRandomLcgState)) >> 16) & 0xF];
                    gRandomLcgState = randomState;
                }
            }
            break;
    }
}

/// Restarts the body slots on an animation change, otherwise advances their poses.
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
static s32 _helicopterLandingPadNo9PlayEventAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedSecondArg)
{
    enum {
        BLEND_FRAMES = 8,
    };

    HelicopterLandingPadNo9Work* work;
    s32                          blendFrames;
    s32                          slotIndex;

    blendFrames       = (request->blend != ANIMATION_BLEND_RESET) * BLEND_FRAMES;
    work              = task->work;
    work->animationId = request->animationId + (BODY_ANIMATION_EVENT_1 - 1);
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animationId, 0, blendFrames);
    }
    work->animationFrame = 0;
    return 0;
}

#include "../../../../shared/actor/actor_messages_place_rot_matrix.inc.c"

/// Sets whether the body model participates in active drawing.
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
static s32 _helicopterLandingPadNo9IsPresent(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    const HelicopterLandingPadNo9Work* work = task->work;

    return work->present;
}

/// Dispatches initialization or update of the golem's off-hand prop model.
static void _helicopterLandingPadNo9PropTask(Task* task)
{
    EnemyTaskFunc states[ATTACHMENT_STATE_COUNT] = { _helicopterLandingPadNo9InitProp, _helicopterLandingPadNo9UpdateProp };

    states[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the initially hidden prop to the off hand and borrows body lighting.
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
    propCoord->parent       = &task->parent->extra.tmd->coords[LEFT_HAND_PART];
    propCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    propModel->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    propModel->lightMtx     = &parentWork->light;
    propModel->colorMtx     = &parentWork->color;
    task->state             = ATTACHMENT_UPDATE;
}

/// Dispatches initialization or update of the golem's attached weapon model.
static void _helicopterLandingPadNo9WeaponTask(Task* task)
{
    EnemyTaskFunc states[ATTACHMENT_STATE_COUNT] = { _helicopterLandingPadNo9InitWeapon, _helicopterLandingPadNo9UpdateWeapon };

    states[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the hidden weapon to the right hand and borrows body lighting.
static void _helicopterLandingPadNo9InitWeapon(Enemy* enemy, Task* task)
{
    TmdObject*                   weaponModel;
    HelicopterLandingPadNo9Work* parentWork;

    weaponModel                 = task->extra.tmd;
    parentWork                  = task->parent->work;
    weaponModel->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    weaponModel->coords->parent = &task->parent->extra.tmd->coords[RIGHT_HAND_PART];
    weaponModel->lightMtx       = &parentWork->light;
    weaponModel->colorMtx       = &parentWork->color;
    task->state                 = ATTACHMENT_UPDATE;
}

/// Copies body visibility to the weapon and composes its attached coordinate.
static void _helicopterLandingPadNo9UpdateWeapon(Enemy* enemy, Task* task)
{
    task->extra.tmd->flags                = task->parent->extra.tmd->flags;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
}

/// Dispatches initialization or update of the golem's attached chest model.
static void _helicopterLandingPadNo9ChestModelTask(Task* task)
{
    EnemyTaskFunc states[ATTACHMENT_STATE_COUNT] = { _helicopterLandingPadNo9InitChestModel, _helicopterLandingPadNo9UpdateChestModel };

    states[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the hidden chest model to the chest and borrows body lighting.
static void _helicopterLandingPadNo9InitChestModel(Enemy* enemy, Task* task)
{
    TmdObject*                   chestModel;
    HelicopterLandingPadNo9Work* parentWork;

    chestModel                 = task->extra.tmd;
    parentWork                 = task->parent->work;
    chestModel->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    chestModel->coords->parent = &task->parent->extra.tmd->coords[CHEST_PART];
    chestModel->lightMtx       = &parentWork->light;
    chestModel->colorMtx       = &parentWork->color;
    task->state                = ATTACHMENT_UPDATE;
}

/// Copies body visibility to the chest model and composes its attached coordinate.
static void _helicopterLandingPadNo9UpdateChestModel(Enemy* enemy, Task* task)
{
    task->extra.tmd->flags                = task->parent->extra.tmd->flags;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
}

/// Dispatches the grenade's initialization, flight and hidden burst phases.
static void _helicopterLandingPadNo9GrenadeTask(Task* task)
{
    EnemyTaskFuncTable3 states;

    states = _gHelicopterLandingPadNo9GrenadeTaskStates;
    states.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Dispatches a destructible transformer's initialization and frame update.
static void _helicopterLandingPadNo9TransformerTask(Task* task)
{
    EnemyTaskFunc handlers[2] = {
        _helicopterLandingPadNo9InitTransformer,
        _helicopterLandingPadNo9TickTransformer,
    };

    handlers[task->state](task->spawnArg2.pointer, task);
}

/// Returns one when a transformer runs in the current mapped camera view.
static s32 _helicopterLandingPadNo9CheckTransformerView(Task* task)
{
    TmdObject*        model;
    _TransformerWork* work;
    Enemy*            enemy;
    s32               viewIndex;
    u8                viewMisses;
    u8                mappedView;

    model      = task->extra.tmd;
    work       = task->work;
    enemy      = task->spawnArg2.pointer;
    viewMisses = 0;
    mappedView = viewGetMappedIndex();
    for (viewIndex = 0; viewIndex < ARRAY_SIZE(_gHelicopterLandingPadNo9HelipadViews.transformerViews[0]); viewIndex++) {
        if (mappedView != _gHelicopterLandingPadNo9HelipadViews.transformerViews[work->transformerIndex][viewIndex]) {
            viewMisses++;
        }
    }

    if (viewMisses != ARRAY_SIZE(_gHelicopterLandingPadNo9HelipadViews.transformerViews[0])) {
        return 1;
    }

    model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->blast.flags            &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    if (work->sparksTask != NULL) {
        work->sparksTask->state = CABLE_SPARKS_RELEASE;
        work->sparksTask        = NULL;
    }
    if (work->sparkFrames != 0) {
        work->sparkFrames--;
    }
    return 0;
}

/// Refreshes helipad-light model lighting at its composed root position.
static void _helicopterLandingPadNo9UpdateTransformerLighting(Task* task, const GfxCoord* rootCoord)
{
    VECTOR composedPosition;

    composedPosition.vx = rootCoord->workm.t[0];
    composedPosition.vy = rootCoord->workm.t[1];
    composedPosition.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(task->extra.tmd, &composedPosition, 0, 3);
}

/// Unregisters the transformer's target and collision bodies, then destroys it.
static void _helicopterLandingPadNo9ExitTransformer(Task* task)
{
    Enemy*            enemy = task->spawnArg2.pointer;
    _TransformerWork* work  = task->work;

    worldTargetUnlinkNode(&enemy->node);
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->blast);
    enemyDestroy(enemy, task);
}

/// Dispatches the coordinate-only helipad blast source's initialization and update.
static void _helicopterLandingPadNo9BlastSourceTask(Task* task)
{
    EnemyTaskFunc states[2] = {
        _helicopterLandingPadNo9InitBlastSource,
        _helicopterLandingPadNo9TickBlastSource,
    };

    states[task->state](task->spawnArg2.pointer, task);
}

/// Unregisters the blast source's target and collision bodies, then destroys it.
static void _helicopterLandingPadNo9ExitBlastSource(Task* task)
{
    Enemy*            enemy = task->spawnArg2.pointer;
    _BlastSourceWork* work  = task->work;

    worldTargetUnlinkNode(&enemy->node);
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->blast);
    enemyDestroy(enemy, task);
}
