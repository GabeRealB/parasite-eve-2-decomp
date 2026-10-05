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
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
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
#include "gameplay/object_fields.h"
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

extern GpuImageUpload D_actor_403100_801555EC[2];

static void func_actor_403100_80132064(Task* arg0, SVECTOR* first, SVECTOR* second, s32 arg3);
static void func_actor_403100_8013B5E0(Task* arg0, s16 arg1);
static void func_actor_403100_8013CEAC(u16* arg0, s32 arg1, s32 arg2, s16 arg3);
static void func_actor_403100_8013CF60(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
static void func_actor_403100_8013D06C(void);

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

static void func_actor_403100_8013480C(Task* arg0, s32 arg1);

static void func_actor_403100_80133C94(Task* task);
static void func_actor_403100_80133D88(Task* arg0);
static void func_actor_403100_80133E88(Task* arg0);
static void func_actor_403100_8013E5FC(Task* task);
static void func_actor_403100_8013E624(Task* arg0);

static void func_actor_403100_80132C3C(Task* arg0, s16 arg1, s16 arg2, s16 arg3, s32 arg4);

/* This routine ignores its incoming arguments; callers use both forms. */
static void func_actor_403100_801327CC();
static void func_actor_403100_801328DC(Task* arg0);
static void func_actor_403100_8013B128(Task* arg0);
static void func_actor_403100_8013B3C4(Task* arg0);
static void func_actor_403100_8013D11C(Task* arg0);
static void func_actor_403100_8013D0B8(s16 arg0, s16 arg1, s16 arg2, s16 arg3);
static void func_actor_403100_8013D1B8(s16 arg0, s16 arg1);
static void func_actor_403100_8013D24C(void);
static s32  func_actor_403100_8013D2F4(GfxCoord* coord, MATRIX* matrix);
static s32  func_actor_403100_8013E33C(GfxCoord* arg0, MATRIX* arg1, GfxCoord* arg2);
static void func_actor_403100_8013D770(Task* arg0);
static void func_actor_403100_8013E02C(s16 arg0, s16 arg1, s16 arg2);
static void func_actor_403100_8013F610(void);
static void func_actor_403100_8013F658(void);
static void func_actor_403100_8013B5E0(Task* arg0, s16 arg1);

extern DamageAttack D_actor_403100_80147614[6];
static void         func_actor_403100_801342B4(Task* arg0);
static void         func_actor_403100_8013C7B4(Task* arg0);

extern u8 D_actor_403100_801557A8[];

extern TaskDesc D_actor_403100_8015560C[];
extern s16      D_actor_403100_80155794[5][2];

static void func_actor_403100_8013E964(Task* task);
static void func_actor_403100_8013E96C(Task* arg0);
static void func_actor_403100_8013E9D8(Task* arg0);
static void func_actor_403100_8013EA60(Task* arg0);
static void func_actor_403100_8013EAD4(Task* arg0);
static void func_actor_403100_8013EB68(Task* arg0);
static void func_actor_403100_8013EBC8(Task* arg0);
static void func_actor_403100_8013EC4C(Task* arg0);
static void func_actor_403100_8013ECD0(Task* arg0);
static void func_actor_403100_8013ED48(Task* task);

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
static s32              func_actor_403100_8013D9C4(s16 x, s16 z, _Actor403100Zone* zone);

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
s32                 func_actor_403100_8013D564(Task*, s32, u16*, s32);
s32                 func_actor_403100_8013D5F4(Task*, s32, s32, s32);
void                func_actor_403100_8013D608(Task*, s32, s32, s32);

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
void             func_actor_403100_8013E04C(Task*);
void             func_actor_403100_8013E0A4(Task*);
void             func_actor_403100_8013E0FC(Task*);

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
    0x2020202,
    0x2020202,
    0x2E372B61,
    0x362D3535,
    0x2513553,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x142A2B2A,
    0x4D4D4D4D,
    0xBB353535,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x36382A2A,
    0x534C4C35,
    0x61363535,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x7E020202,
    0x1B292B42,
    0x53345335,
    0x4D535353,
    0x202026C,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x34020202,
    0x4D1D2B13,
    0x1A4B4C64,
    0x355D644C,
    0x2020253,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2D020202,
    0x1B082A13,
    0x4B251A4A,
    0x61843535,
    0x2027F36,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xD020202,
    0x1A2A2A42,
    0x4A254C4A,
    0x6079531B,
    0x2683535,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x42020202,
    0x4C292A2A,
    0x4B6B6E25,
    0x6053334D,
    0x261367B,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xD470202,
    0x1A1B4F2B,
    0x4B6B6B25,
    0x7B356434,
    0xBB848484,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xE340202,
    0x1A1B1C0E,
    0x6B6B2324,
    0x844B254B,
    0x84ADADAD,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x42290202,
    0x4C4E1C2B,
    0x67236B24,
    0xAD848484,
    0xADADADAD,
    0x2020247,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x420E0202,
    0x251B4F4F,
    0x8C223E25,
    0x84ADADAD,
    0xADAFADAD,
    0x20202AD,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2B0E6802,
    0x344E1D4F,
    0x71226E4C,
    0xADAD9999,
    0xAFAFADAD,
    0x20284AD,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2A134C02,
    0x49130E50,
    0x3B8D495D,
    0x84999970,
    0xAFAFADAD,
    0x26C84AD,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2B133402,
    0x53130E2B,
    0x7170475B,
    0xAD996F71,
    0xAFAFADAD,
    0x8484ADAF,
    0x6868897A,
    0x3535349D,
    0x36353535,
    0x37373737,
    0x2E143737,
    0x35353535,
    0x6C49372E,
    0x27E3551,
    0x2020202,
    0xD2C2902,
    0xD132C2B,
    0x6F5E604D,
    0x84998B71,
    0xAFAFADAD,
    0x84ADADAF,
    0x84847884,
    0x647B7B7A,
    0x64363636,
    0x38386415,
    0x64646464,
    0x64366464,
    0x7A783564,
    0x2BB5336,
    0x2020202,
    0xD420E02,
    0x1D1E0D42,
    0x7E81292B,
    0xAD998A62,
    0xAFADADAD,
    0xADADADAF,
    0x58849999,
    0x7B7A5884,
    0x7B367B7B,
    0x78647864,
    0x58587878,
    0x64647B7B,
    0x665D8736,
    0x2687A2E,
    0x2020202,
    0x2C2C0E68,
    0x53502B42,
    0x4A0B121B,
    0xADAD997D,
    0xAFADA9A9,
    0xADADAFAF,
    0xA9ADADAD,
    0x5884ADAD,
    0x7B7B8F78,
    0xA6A9A978,
    0xB6B6A6A6,
    0x6478B6B6,
    0x3661857B,
    0x2688561,
    0x2020202,
    0x2C0E1334,
    0x2D0D2B2A,
    0x261C1C64,
    0xA9AD7D56,
    0xA9A9A9A9,
    0xAFAFAFA9,
    0xA9ADADAD,
    0xA9A9A9A9,
    0xA98FACA9,
    0xACA9A9A9,
    0xA9A9ACAC,
    0x7BB6A9A9,
    0x61667985,
    0x202B6B6,
    0x2020202,
    0x2C0E134C,
    0x2D171F2A,
    0x60606036,
    0xA9ADADAD,
    0xAFA9A9A9,
    0xA9ADA9A9,
    0xABA9A9A9,
    0xADADABAB,
    0xA9ADA900,
    0xACA9A9A9,
    0xAC00A900,
    0xB6A9A9AC,
    0xB6B6525B,
    0x283ACA9,
    0x2020202,
    0xE2B420D,
    0x2D1E1F2B,
    0x1C997D60,
    0xADA9A960,
    0xA9ADA6AF,
    0xA9A9A9A9,
    0xABABABAB,
    0xABABABAB,
    0xA9A900AD,
    0xA9A9A9,
    0xA6A678AC,
    0xA9A9ACA6,
    0xA9A9B6B6,
    0x298ACAC,
    0x2020202,
    0x2C1E2B0E,
    0x2D30300D,
    0x1CAD997D,
    0xAFADA960,
    0xADA68FA6,
    0xABABA9A9,
    0xABABABAB,
    0xABABABAB,
    0xA900ADAB,
    0xA9A9A9A9,
    0x8F8FA678,
    0xA9A9A68F,
    0xACACA9A9,
    0x29097AC,
    0x47020202,
    0xD2D2A0E,
    0x642D300D,
    0x60ADADAD,
    0xADADA9A9,
    0xA9ADAFAF,
    0xABABABA9,
    0xACACA9A9,
    0xABA9A9AC,
    0xA9ADA9C2,
    0xA6ACA9A9,
    0x8FA6ACAC,
    0xA9A9A98F,
    0xACACACAC,
    0x290A8A7,
    0x4C020202,
    0x7B604F13,
    0x7D602D2D,
    0xADADADAD,
    0xA9A9A9A9,
    0xA9A9ADA9,
    0xA9ABABA9,
    0x8F8F8FA4,
    0xA9A9AC8F,
    0xADA9ABAB,
    0x8FA6A9A9,
    0x8FACACA6,
    0xAFADA9A6,
    0xADA9A9A9,
    0x290A4AD,
    0x4C020202,
    0x992D4F0E,
    0xAD999999,
    0xADADADAD,
    0xADA9607D,
    0xA9A9A9A9,
    0x99A9ABAB,
    0x4C4C4C7A,
    0xA9AC587A,
    0xADABABA9,
    0xACACADA9,
    0xA6A6A6AC,
    0xA9AFADA9,
    0x19538FA6,
    0x2059217,
    0xD020202,
    0x60294F2C,
    0xADADA9A9,
    0xADADADAD,
    0xAD7D60AD,
    0xA9A9A9AD,
    0x7A99ABAB,
    0x290D294C,
    0xAC588129,
    0xA9ABABA9,
    0xADADADA9,
    0xA9928FA6,
    0xA9AFACA9,
    0x1719538F,
    0x2AB926D,
    0xE020202,
    0x7B15642A,
    0xADADA97D,
    0xADADA9A9,
    0xA6A6ACAD,
    0xA9ADADAC,
    0x2758ABAB,
    0xD4C4C0D,
    0xAC584C29,
    0xADABABA9,
    0xADA6A68F,
    0xA9A9A9A6,
    0xA9A9A9A9,
    0xADA9A9,
    0x2ABABAB,
    0x2C7E0202,
    0x1515362B,
    0xADA97D7B,
    0xADA96060,
    0x8F8FACAD,
    0xADAD8F92,
    0x2758ABAB,
    0xD4C810D,
    0xAC584C29,
    0xADABABA9,
    0xACADA6A6,
    0xA9A6928F,
    0x8FA9A9AD,
    0x8282928F,
    0x2ABAB8F,
    0x42600202,
    0x60602B0D,
    0xA9AD9960,
    0xA96060AD,
    0x928FACAD,
    0xA9AD8F95,
    0x6A98ABAB,
    0xD0D0D4C,
    0xAC588129,
    0xADABABA9,
    0xA6ACADAD,
    0xADA9A692,
    0x7CA9A9AF,
    0x6D198292,
    0x2ABAB8F,
    0xE340202,
    0x60602B0D,
    0xA9ADAD99,
    0xA9A9A9A9,
    0xA9A9A9A9,
    0xA9A9A9A9,
    0x58A9ABAB,
    0x4C27276A,
    0xA9AC9F7B,
    0xADA9ABA9,
    0x8FA6ADA9,
    0xAFA9ADA6,
    0x8FACA98F,
    0x8F92927C,
    0x2ABABAB,
    0xE150202,
    0x60360D2B,
    0xADADAD99,
    0xA9A9A9AD,
    0xADA9A9A9,
    0xA9A9ADAD,
    0xA9ABABAB,
    0x8F8F8F8F,
    0xA9A9AC8F,
    0xA9ADA9AB,
    0xA68FACA9,
    0xADA9A9A9,
    0xA9A9A9AD,
    0xABA7A900,
    0x2ABABAB,
    0xE4F0202,
    0x604C1515,
    0xA6A6AD99,
    0xA9A9ADAD,
    0x92ACADA9,
    0xA9ADACA6,
    0xABABABAB,
    0xACACACA9,
    0xABA9A9AC,
    0xA6A9AB,
    0xA6A6AD,
    0xAFADAB00,
    0xA6A6A98F,
    0xAB51458F,
    0x2ABABAB,
    0xE0E6102,
    0x99604C15,
    0x8F8FADAD,
    0xA9A9A9AD,
    0xA6ACADA9,
    0xADADAC8F,
    0xABABABA9,
    0xA9A9ABAB,
    0xABABA9A9,
    0xA6ACA9,
    0xACAC,
    0xADA9ABAB,
    0x92A6A9AF,
    0xAB171992,
    0x2ABABAB,
    0x150E6102,
    0xA9A96060,
    0xA6A6ADA9,
    0xADA9A9AD,
    0xACADA9AD,
    0xA9A9ADAC,
    0xABABA9A9,
    0xABABABAB,
    0xACA9ABAB,
    0xADA6AC,
    0xABA900AC,
    0xA9A9A9AB,
    0xA6ADA900,
    0xAB51968F,
    0x2ABABAB,
    0x4C0D1402,
    0xA9A9994C,
    0xADADADA9,
    0xA97DA9AD,
    0xADA9ADAD,
    0xA9A9A9A9,
    0xA9A9ADAD,
    0xABABABAB,
    0xACACA9A9,
    175,
    0xACACA900,
    0xACACADAD,
    0xA900AC,
    0xA1AFA900,
    0x2ABAB8F,
    0x4C2B4202,
    0xA9A99960,
    0xA9ADADA9,
    0x7D60A9A9,
    0xADADADA9,
    0xADADADAD,
    0xA9ADADAD,
    0xA9ABABA9,
    0xA6ACA6AC,
    0xA900A9AD,
    0xADAFAFAC,
    0xAFAFACAD,
    0xA9ACADAF,
    0x828296A7,
    0x2ABAB51,
    0x4C0D1E84,
    0xA9A99960,
    0xA9A9A9A9,
    0x49A9ADA9,
    0xACADAD7D,
    0xACA6ACAC,
    0xADADACAC,
    0xACA9ABA9,
    0xA9ADAFAF,
    0xAFA9A9AF,
    0xABADAFAF,
    0xA9ABABAB,
    0xA900AD,
    0x6D42958F,
    0x2ABAB8F,
    0x150D2C61,
    0xA9A9604C,
    0xADA999A9,
    0xACA6A6AD,
    0xACAC8FAD,
    0xA68FA6AF,
    0xADACAFA6,
    0xA6ADABA9,
    0xAFA9AD8F,
    0xACAFA9AB,
    0xABABABA9,
    0xABABABAB,
    0xACAFA9,
    0x519292A4,
    0x2ABABAB,
    0x36152C34,
    0xA9604C36,
    0xADA9A9A9,
    0xAC9292A6,
    0xAF8F928F,
    0x95928FA6,
    0xADACA68F,
    0x97A6ADA6,
    0xAFAFA9AD,
    0xADAFA7A9,
    0xABABABAB,
    0xABABABAB,
    0xA900A9,
    0xA9ACAF00,
    0x2A9A9A9,
    0x2C0E0D1E,
    0xA94C3615,
    0xADA9A9A9,
    0xACA6A6AD,
    0xAFAC8FAC,
    0x8F8FA6AF,
    0xA6ACAFA6,
    0x8F95ADA9,
    0x8FAFA9A9,
    0xADAFA700,
    0xABABABAB,
    0xADABABAB,
    0x97AF8FA6,
    0xABABA78F,
    0x2A9ACAB,
    0x36152C2B,
    0x7B614C36,
    0xADA9A97D,
    0xADADADAD,
    0xACACACAD,
    0xACACACAC,
    0xA9ADADAD,
    0xAC97A6A9,
    0x97AFADA9,
    0xA6AFAC00,
    0xADAF8F8F,
    0xAFADADA9,
    0x8FAFAFAF,
    0xABABA78F,
    0x2A9ACAB,
    0x150D0D2B,
    0x60153636,
    0xA9A9997B,
    0xADA9A9AD,
    0xA9ADADAD,
    0xA9A9ADAD,
    0xA9A9A9A9,
    0xAD8FA6AC,
    0x8FA7ACA9,
    0x8FA6ACAC,
    0x8F8F9797,
    0xAFA6A6AF,
    0xACAC00AD,
    0xACACACAC,
    0x2A90000,
    0xD2C422B,
    0x6060150D,
    0xA9A9A97B,
    0xA9A9A9A9,
    0xA9A9A9A9,
    0xADADA6AD,
    0xA9A9A9AD,
    0xADAFA6AC,
    0xACA7ACA9,
    0x8FA6AFAD,
    0xA68F8F8F,
    0xADAFAFAF,
    0xACA900A9,
    0xACACACAC,
    0x2A90000,
    0xE0D422B,
    0x61362B1E,
    0x99A9A97D,
    0x997D7D99,
    0xA9A9A9A9,
    0xA6A6ADAD,
    0xA9ADADA6,
    0xAD8FAFA9,
    0xA6A6ADA9,
    0xACA6AC00,
    0xABABABAB,
    0xADA9ABAB,
    0x97AFACAD,
    0xABABA78F,
    0x2A9ACAB,
    0x2C2B0E34,
    0x36132C2C,
    0x787D7D7B,
    0x7D7D7860,
    0xA9A9A984,
    0x92A6A9A9,
    0xADADA68F,
    0x8FACACA9,
    0xAFACA9AD,
    0xACACAFA9,
    0xABABABAB,
    0xABABABAB,
    0x8FA9A9A9,
    0xABABA78F,
    0x2A9ACAB,
    0x420D2C34,
    0x380E0E0E,
    0x64786136,
    0xA978607B,
    0xA9ADA9A9,
    0x95A6A9A9,
    0xADADA67C,
    0x97ADA9AB,
    0xACA9A9AD,
    0xA7AFA9AF,
    0xABABABA9,
    0xABABABAB,
    0xAFAFA6,
    0xA9ACACAC,
    0x2A9A9A9,
    0x42132A5D,
    0x612B130E,
    0x7B61614E,
    0xA9A9787B,
    0xA9ADADAD,
    0xA6ADA9A9,
    0xA9A9ADA6,
    0xADA9ABA9,
    0xADA9ADA9,
    0xAFA9ADAD,
    0xA9A9A9AF,
    0xABABABAB,
    0xA90000A9,
    0x6D6D96AF,
    0x2AB8F51,
    0x2B0E2B58,
    0x61611313,
    0x1B1B4C4E,
    0xA9A9A978,
    0xA9A9A9A9,
    0xADADADA9,
    0xA9A9A9AD,
    0xA9ABA9A9,
    0xADA9A9A9,
    0xADADACA6,
    0xA9ADA7AF,
    0xACA9A9A9,
    0xA6ACA6,
    0x8282A8AC,
    0x2ABA66D,
    0x2B0E2B58,
    0x2A4E130E,
    0x1A38362B,
    0x787DA97B,
    0x7D847D7D,
    0xA9A9A9A9,
    0xA9A9ADA9,
    0xABABABAB,
    0xA9ADADAB,
    0xADACA6AD,
    0xAFAFAFAD,
    0xA98FA6AF,
    0xACA9A9A9,
    0xA9A9A900,
    0x2ABABA9,
    0x2B133431,
    0x2B0D130D,
    0x6034371E,
    0x6078604C,
    0x7D787D7B,
    0xA9A9A999,
    0xABA9ADAD,
    0xABABABAB,
    0xADABABAB,
    0xACA6A9A9,
    0xA9AFADAD,
    0x5DA9ACAD,
    0xA9A9A95D,
    0x36929296,
    0x2AB886D,
    0x42136102,
    0x38383842,
    0x34363838,
    0x7B647B52,
    0xA960787B,
    0xADA9A9A9,
    0xAB83ADAF,
    0xABABABAB,
    0xABABABAB,
    0xA6A9A9A9,
    0xADADADAC,
    0x7BA9ADA9,
    0xA95E5D4B,
    0x6D3696A9,
    0x2AB5D6D,
    0xD0D7E02,
    0x36363638,
    0x52363652,
    0x7D7A7B4C,
    0xA9A9A97D,
    0xAFADADA9,
    0xABC1AF7C,
    0xA9A9ABAB,
    0xABADA9A9,
    0xA9A9ADAB,
    0xACACACAC,
    0x5EA9ACAC,
    0x5E4B4B4B,
    0xACACAC5E,
    0x2ABABAB,
    0xD1E0202,
    0x52523636,
    0x7A7A5236,
    0xA97D7D7D,
    0xA9A9A9A9,
    0xADA9A9A9,
    0xABABADAF,
    0xAD58A9AB,
    0xADA9ADAD,
    0xA9ADABAB,
    0xACACACA9,
    0xA98FA6A6,
    0x5D5D5EA9,
    0xA9A9A9A9,
    0x2ABA9A9,
    0x2B2A0202,
    0x7A7A5236,
    0xA9A9A97D,
    0xA9A9A9A9,
    0x844799A9,
    0xA9A9A999,
    0xABABA9A9,
    0x586BACA9,
    0xA9A9ADA9,
    0xA9A9ABAB,
    0xADA9A9A9,
    0xADACADAD,
    0xA9A9A9AC,
    0xA7A9A9A9,
    0x2AB96A7,
    0xD340202,
    0x7D345236,
    0x99ADADAD,
    0xA9A99999,
    0x4A807899,
    0xAFA9A984,
    0xA9ABABAD,
    0x8129A9AC,
    0xA9AD9FA9,
    0xACA6A9AD,
    0xACACACAC,
    0xA9ADA9AC,
    0xACADA9A9,
    0x8FA9A6A6,
    0x2AB3845,
    0xE340202,
    0x7D343636,
    0xADADADAD,
    0x992E6414,
    0x4C805D46,
    0xA6ADA999,
    0xA9ABABAD,
    0x4C29A9AC,
    0xA9AD9FA9,
    0xA9A9ABA9,
    0xADA9A9A9,
    0xA9A9A9AC,
    0xA9A9A9A9,
    0xA6ACA9A9,
    0x2AB968F,
    0xD7E0202,
    0x7A34362A,
    0xADA97D7D,
    0x7B4E53AD,
    0x84678060,
    0xA6AFADA9,
    0xA9ABABAD,
    0x4C29A9AC,
    0xA9AD9FA9,
    0xA6A6A9AD,
    0xA6ACACAC,
    0xA9A9ACA6,
    0xA7A1A7AC,
    0xACA4ACAC,
    0x2ADADAD,
    0x2B020202,
    0x34342B0D,
    0xA9AD7A,
    0x4CA9ADA9,
    0xA984494A,
    0xAFAFAFAD,
    0xA9ABABAD,
    0x8129A9AC,
    0xA9AD9FA9,
    0xA9A9ABAD,
    0xACA9A9A9,
    0xA965A6A6,
    0xA7A1ACA9,
    0x161645AC,
    0x28F1616,
    0x2B020202,
    0x36360D13,
    0xA97A3434,
    0xADADAD00,
    0xA9A9A9A9,
    0xADADADAD,
    0xA9ABA9AD,
    0x586BACA9,
    0xA9A9ADA9,
    0xACADA9AD,
    0xADACACA6,
    0x4B5EA6AC,
    0xA7ACA95D,
    0x162D7CAC,
    0x28F1616,
    0x4C020202,
    0x4D2A1313,
    0x7A343436,
    0xADAD00A9,
    0xA9A9A9A9,
    0xA9A9A9A9,
    0xABABADA9,
    0xAD58A9A9,
    0xADA9A9AD,
    0xA6ADA9AB,
    0xADADADA6,
    0x4B5EACAD,
    0xACA9654B,
    0xADADACA7,
    0x2ADADAD,
    0x4C020202,
    0x4E421313,
    0x34381434,
    0xA9A97A34,
    0xA9A90000,
    0xADACACA9,
    0xABA9A9A9,
    0xA9A9A9AB,
    0xA9ADADA9,
    0xA6A9A9AB,
    0xADADADAC,
    0x655EADA9,
    0xA9654B4B,
    0xA7ADADA9,
    0x29797A7,
    0x7E020202,
    0x2A2B130E,
    0x36302D35,
    0x7A7A3634,
    0x848453,
    0xA9A9ADAD,
    0xA9A9A9A9,
    0xABABABAB,
    0xABABA9AB,
    0xA9A9A9A9,
    0xACADADA9,
    0x5EADA9AC,
    0x654B4B5E,
    0xA7A9A9A9,
    0x2A19296,
    0x2020202,
    0x2B4F2C1E,
    0x2A37532A,
    0x78523736,
    0xA9845178,
    0xA900A9A9,
    0xA9A9A9A9,
    0xABA9A9A9,
    0xA9ABABAB,
    0xADADA9A9,
    0xACA6ACAC,
    0xADA9ACAC,
    0x4B655DA9,
    0xA9A96565,
    0x2ADADA9,
    0x2020202,
    0x2A4F2B4F,
    0x162E140D,
    0x997A301E,
    0x67538784,
    0xAFAF616A,
    0xA9A9A9A9,
    0xA9ADADA9,
    0xA6ADA9A9,
    0xA6ACADAD,
    0xA6A6A6A6,
    0xA9ACACAC,
    0x627D5EA9,
    0x7A5D5D9C,
    0x2927CAD,
    0x2020202,
    0x152B2C61,
    0x1E361E1E,
    0x87676617,
    0x8D6A337A,
    0xAFAF466B,
    0xADA9AFAF,
    0xADA9ADAD,
    0xADADADAD,
    0xACACACAD,
    0xA6A6A6A6,
    0xADADACA6,
    0x225D7D7D,
    0x615D467E,
    0x2537A7D,
    0x2020202,
    0x6B0D1334,
    0x161E1F15,
    0x2E366753,
    0x3C675352,
    0x678A6871,
    0x7A79151A,
    0xADADADAD,
    0xADADADAD,
    0xADADADAD,
    0x85A6A6AC,
    0x52ADAD85,
    0x4B5D5D5D,
    0x61615B80,
    0x27D7D37,
    0x2020202,
    0x490E137E,
    0x16161E4E,
    0x714847B,
    0x8A716E69,
    0x398AB8B8,
    0x61524E4A,
    0x9A58847B,
    0x9A9A9AAD,
    0x857D7DAD,
    0x85858585,
    0x36155337,
    0x352E5252,
    0x1549857A,
    0x2515315,
    0x2020202,
    0x4F130E02,
    0x42521515,
    0x1579842A,
    0xB8BD3C08,
    0x9D70BCBB,
    0x34533381,
    0x9A9A9A52,
    0x345D5D5D,
    0x16363434,
    0x3452301F,
    0x36141414,
    0x35355236,
    0x348A5D34,
    0x2513635,
    0x2020202,
    0x2B2C0902,
    0x1552132C,
    0x64537B2C,
    0xB9B9BB39,
    0x478BBD6F,
    0x2794A60,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2025802,
    0x2020202,
    0x6B153402,
    0x2D2A1309,
    0x4B157738,
    0x6FBFB69A,
    0x60493B71,
    0x2688787,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xB6025802,
    0x2020202,
    0x642B3402,
    0x38131315,
    0x4D4C842D,
    0xBDBD6F67,
    0x354B6A8C,
    0x202687B,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xB6020202,
    0xA9A4A663,
    0x2020202,
    0x132C7E02,
    0x70E091E,
    0x1B158434,
    0xBDBD7067,
    0x1C334A3B,
    0x2020252,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xB6020202,
    0xB4B498B6,
    0xABABABAB,
    0x2020202,
    0x132B0202,
    0x1E301509,
    0x26147B49,
    0x70BD708C,
    0x51344A67,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xB6020202,
    0x989898B6,
    0xABABA900,
    0xAAABABAB,
    0x2020202,
    0x134D0202,
    0x3A38534F,
    0x4B2A3534,
    0x71BD708C,
    0x2346167,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x98989802,
    0xA9ADACAC,
    0xABABABAB,
    0xA900AAAB,
    0x2020202,
    0x2C600202,
    0x3616380D,
    0x4A4E0D15,
    0x8D71708D,
    0x26C5281,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xADADACA0,
    0xABAAADAD,
    0xAB00AAAB,
    0xABAAA9A9,
    0x2020202,
    0xE340202,
    0x35301F0E,
    0x4A351536,
    0x6A8D718D,
    0x202604A,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xADACACA0,
    0xC200A9AC,
    0xABAAA9A9,
    0xAAABABAB,
    0x2020202,
    0x42470202,
    0x36370E2B,
    0x6A1B3653,
    0x616B8D8C,
    0x2027E33,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xACADADA0,
    0xABA9ABAD,
    0xABABABAB,
    0xA9ABABAB,
    0x2020202,
    0x1E020202,
    0x30161F4F,
    0x6B263653,
    0x644B6A8D,
    0x2020264,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xACAC00A0,
    0xABABAB00,
    0xABAAABAB,
    0xABABAAAB,
    0x2020202,
    0x1E020202,
    0x1E1F2A15,
    0x6E341453,
    0x53334A6B,
    0x2020260,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xADACAD31,
    0xABABAB00,
    0xABABAAAA,
    0xABABABAB,
    0x2020202,
    0x61020202,
    0x1E1F1E2A,
    0x4C533536,
    0x34644B4B,
    0x20202BB,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xADADADA0,
    0xABABAB00,
    0xABABABAA,
    0xABABABAA,
    0x2020202,
    0x60020202,
    0x16170D2A,
    0x354E3515,
    0x64536464,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xAC00ADA4,
    0xABABABA9,
    0xABA9ABAB,
    0xABABA9A9,
    0x2020202,
    0x6C020202,
    0x16171F15,
    0x53363515,
    0x7F53642E,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xACAC00A4,
    0xABAB00AB,
    0xABA9ABAA,
    0xABABABA9,
    0x2020202,
    0x2020202,
    0x161E1F36,
    0x35353637,
    0x2345236,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xADAFADA0,
    0xAD00AC00,
    0xABABA9AC,
    0xABABABAB,
    0x2020202,
    0x2020202,
    0x16300D15,
    0x36353636,
    0x2605253,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xADA7ACA0,
    0xACACACAD,
    0xABABACAC,
    0xABABABAB,
    0x2020202,
    0x2020202,
    0x38382B61,
    0x36365336,
    0x27E6434,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xACACAFA4,
    0xACADADAD,
    0xABAAACAC,
    0xABABABAB,
    0x2020202,
    0x2020202,
    0x151E2B51,
    0x2E355336,
    0x2025253,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xACADAFA4,
    0xA9ABAA00,
    0xABAAABA9,
    0xABABABAB,
    0x2020202,
    0x2020202,
    0x151E166C,
    0x52343636,
    0x2024735,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xADACACA4,
    0xABABA9A9,
    0xABA9ABAB,
    0xABABA9AB,
    0x2020202,
    0x2020202,
    0x362A3802,
    0x61343637,
    0x2026C35,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xAFACA0,
    0xABABA9A9,
    0xABABAAAB,
    0xABAB00AB,
    0x2020202,
    0x2020202,
    0x36151602,
    0x66355337,
    0x2020260,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xAFACA0,
    0xABA9ABAA,
    0xABAB00AB,
    0xABABABA9,
    0x2020202,
    0x2020202,
    0x36386102,
    0x35366436,
    0x202029C,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xACAFAD31,
    0xAAA9ABAA,
    0xABABABAB,
    0xABABABAB,
    0x2020202,
    0x2020202,
    0x36155102,
    0x53666635,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2B69898,
    0x2020202,
    0xACAF0031,
    0xABABABAB,
    0xABAAABAB,
    0x55AB00AB,
    0x2020202,
    0x2020202,
    0x37369C02,
    0x51363553,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x298AD98,
    0x2020202,
    0xACAF0031,
    0xABABABA9,
    0xABAAAAAB,
    0xB2ABAAAB,
    0x2020202,
    0x2020202,
    0x36360202,
    0xBB355353,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2B4AD98,
    0x2020202,
    0xACACAD31,
    0xABABAB00,
    0xABAB00AB,
    0xA3ABABA9,
    0x2020202,
    0x2020202,
    0x53530202,
    0x2515336,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x56ABACAC,
    0x2020202,
    0xAD00ACA4,
    0xABABABAD,
    0xABABA9AB,
    0xAEABABA9,
    0x2020202,
    0x2020202,
    0x4D7E0202,
    0x27E3436,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xABABACAC,
    0xA6983198,
    0xADADA6,
    0xABAB0000,
    0xABABABA9,
    0x55ABABAA,
    0x2020202,
    0x2020202,
    0x36470202,
    0x2026153,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xABAAAC98,
    0xA60000AD,
    0xAF00A7,
    0xABA9ADAB,
    0xABABABAA,
    0x4ABA9A9,
    0x2020202,
    0x2020202,
    0x366C0202,
    0x2027F53,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xC2ADAFB4,
    0xA600ADAD,
    0xADAFADA4,
    0xA9ADAB,
    0xABA9A9AB,
    0xABABAB00,
    0x2020202,
    0x2020202,
    0x2D020202,
    0x2029C53,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xABA4AFB4,
    0xA6A4AD00,
    0xACACACA9,
    0xA9AA,
    0xABABAB,
    0xABA9ABAD,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xABAFAC05,
    0xACA7ADAB,
    0xADADADA9,
    0xACA9A900,
    0xACABABA9,
    0xA9ADA9AD,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xADAC05,
    0xADAC00AB,
    0xADADACAD,
    0xADA900AD,
    0xA9ABABAD,
    0xACAC00,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xADADB4,
    0xADAD00A9,
    0xACAC00,
    0xA900AC00,
    0xABA9ADAD,
    0xAD0000AA,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xADADA9B4,
    0xACAD00A9,
    0xACACABA9,
    0xACACAB,
    0xADADA9,
    0xA90000,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xADADA998,
    0xAC000000,
    0xACACAB00,
    0xADAD0000,
    0xA9A900,
    0xACADAD00,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xAD000098,
    0xA900AD,
    0xACAC0000,
    0xAD0000AC,
    0xABA9ADAD,
    0xACACA9AB,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xADA900B4,
    0xA900AC,
    0xACACADAD,
    0xAC0000AC,
    0xA9AC0000,
    169,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xAD00ADB4,
    0xABA9ADAC,
    0xACACAC00,
    0xACA9A9AD,
    0xAAADA900,
    0xADADAFA9,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xAD98,
    0xAAADACAC,
    0xACAC00AB,
    0xA900AD,
    0xA9ACADAC,
    0xAF0000AB,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xADADAC,
    0xACACAD,
    0xADA9ABAB,
    0xAB00ADAD,
    0xACACA900,
    0xAF00A900,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xACACADAC,
    0xADADADAD,
    0xABABA900,
    0xA900ACAD,
    0xADADABAB,
    0xAFAD00A9,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0x2020202,
    0xACACAC98,
    0xAC00ADAD,
    0xC2AB00AD,
    0xAD000000,
    0xA900ABAB,
    0xACABA9AB,
    0x2020202,
    0x2020202,
    0xAC020202,
    0xADADADAC,
    0xADA9ADAD,
    0xADADADAD,
    0xADA9A9A9,
    0xADADADAD,
    0x915E7EAD,
    0xAFACACAC,
    0xA9A900AC,
    0xABA9ADAC,
    0xA9A9,
    0xABA9AA,
    0xABA9A9,
    0x2020202,
    0x2020202,
    0xAD020202,
    0xADA9A9A9,
    0xA9A9ADAC,
    0xA9ADADA9,
    0xACA9A9A9,
    0xA9ADADAC,
    0xA961ACA9,
    0xA7ACAFA7,
    0xABA9ADAC,
    0xACAC00,
    0xA9A9AAA9,
    0xABA900,
    0xADAAAB00,
    0x2020202,
    0x2020202,
    0xA9AC0202,
    0xADADA9A9,
    0xA9A9A9AC,
    0xACACACA9,
    0xACA6A6A6,
    0xA9ADADAC,
    0xA961A9A9,
    0xA7A7A7A7,
    0xA9ADACAC,
    0xADAC00AB,
    0xAAA9AD,
    0xA9ABAB00,
    0xACABA9,
    0x2020202,
    0x2020202,
    0xA9ADAC02,
    0xACACADA9,
    0xA7ADADAD,
    0xA7A6ACAC,
    0xA9A6A7A7,
    0xA9A9A9AD,
    0xA9A9A9A9,
    0xA7A8A79F,
    0xADACADAC,
    169,
    173,
    0xABABABA9,
    0xAB00ABAB,
    0x2020202,
    0x2020202,
    0xA9A9AC02,
    0xACA7ACAD,
    0xA7ADADAD,
    0xA7A6ACAD,
    0x4AA9A7A7,
    0xA9A9ACA9,
    0xA9A9A9A9,
    0xA1A8A77E,
    0xADADADA7,
    0xABABADAC,
    0xA90000A9,
    0xAAABABAB,
    0xABABAAAB,
    0x2020202,
    0x2020202,
    0xA9A9ADAC,
    0xACA7ACA9,
    0xADADA7AD,
    0xA6ACACA9,
    0x5BA9A6A6,
    0x7EAC5B4A,
    0x7EA9A9A9,
    0xA1AF787E,
    0xAFA8,
    0xA900ADAC,
    0xA9AB,
    0xABABAAA9,
    0xABABABAB,
    0x2020202,
    0x2020202,
    0xADA9A9AD,
    0xA9A9ADAD,
    0xADADADAD,
    0xACACADA9,
    0xA9A9ACAC,
    0x4A99495B,
    0x7E5C997E,
    0xA8A64B7B,
    0xADA7A8,
    0xA900ADAD,
    0xA9AB00A9,
    0xABAB00A9,
    0xABABABAB,
    0x2020202,
    0xAC020202,
    0xA7ADADAD,
    0x5B64ADAD,
    0xACACACA9,
    0xADA962A9,
    0xA9ACA9AD,
    0x5B4A80A9,
    0x7E447E6E,
    0xA760647A,
    0xAFA7A8,
    0xACAD00,
    0xA9A9AD00,
    0xABABABAB,
    0xABABABAB,
    0x2020202,
    0xAD020202,
    0xADADA7AD,
    0xA95BADAD,
    0xA6A7ACAD,
    0xA96223A9,
    0x4A5BACA9,
    0x5B226799,
    0x6B99656B,
    0x8F356187,
    0xACA7A7A7,
    0xADAD0000,
    0xACAD00,
    0xABABABAB,
    0xABABABAB,
    0x2020202,
    0xADBB0202,
    0xACACADAD,
    0xACACACAD,
    0x7CA7A7A7,
    0xAC9923A9,
    0x811B5BAC,
    0x3B5B5B4A,
    0x6B5C6723,
    0x344E8187,
    0xA7A7A7A6,
    0xAC00A9AD,
    0xADAD00AD,
    0xABABA9A9,
    0xABABABAB,
    0x2020202,
    0xA9AC0202,
    0xADA7ADAD,
    0xA95DACAC,
    0x8F8FA7A7,
    0x672362AD,
    0x814D615B,
    0x397E995B,
    0x4A5E8D3C,
    0x15648149,
    0xA7AFAF7B,
    0xADA9ADAF,
    0xADA900AC,
    0xABAB00AD,
    0xABABAAAB,
    0x2020202,
    0x31310202,
    0xADADA7A9,
    0xA96478AC,
    0xA97C8FA7,
    0x234A6762,
    0x494A4A47,
    0x707E9999,
    0x498A8D3B,
    0x4E5D8025,
    0xA7AF7A1C,
    0xA9A9A7A8,
    0xA9ADAD,
    0xA900ACAD,
    0xABA900A9,
    0x2020202,
    0x7B0E7E02,
    0xA7A7ADA9,
    0xA96446AC,
    0x99A97CAD,
    0x99259999,
    0x67474799,
    0x718D7E5B,
    0x7E8A3B39,
    0x605D4A1A,
    0xA78F3637,
    0xA9ACA7A8,
    0xAD00AA,
    0xACACAC00,
    0xAAA900AD,
    0x2020202,
    0x7B133402,
    0xADA9A9A9,
    0xA978ACAC,
    0x4762ADAD,
    0x47254E99,
    0x67474799,
    0x8D396223,
    0x9A8A8D39,
    0x7A874B6E,
    0xA7641534,
    0xADA7A8A7,
    0xAD00AAAB,
    0xACAC0000,
    0xABA900AD,
    0x2020202,
    0x7A1E2902,
    0xA9A9A9A9,
    0x7BACACAD,
    0x2562ADAD,
    0x4D4E2967,
    0x49492547,
    0x8C8A8C6B,
    0x889D3C3B,
    0x7B4A2881,
    0x7A14537A,
    0xA7A8A7A7,
    0xA9AC,
    0xAD00A9AD,
    0xADAD,
    0x2020202,
    0x61150E68,
    0x5661ACA9,
    0x6436AC31,
    0x2925ADAD,
    0x1C422A25,
    0x49611669,
    0x8B8A6725,
    0x883C3C8D,
    0x611C2862,
    0x352E8760,
    0xA7A7A79F,
    0xA90000A7,
    0xADAD,
    0xADADAD00,
    0x2020202,
    0xAD371334,
    0x773834A9,
    0x161E7B83,
    0x423562AD,
    0x2B41150D,
    0x81281B35,
    0x8A392224,
    0x623D3C67,
    0x4B2A4C9A,
    0x537A5D61,
    0xA7A7A77B,
    0xA900ACA7,
    0xADACAD00,
    0xAD000000,
    0x2020202,
    0xAD370E4C,
    0x2D155DA9,
    0x1E1E7757,
    0x41363562,
    0x2C2A2A2C,
    0x4B1C5236,
    0x8B3C6781,
    0x6A3D7362,
    0x6128489A,
    0x7A853361,
    0xAFA77A35,
    0xACAFA7,
    0xAD00A9A9,
    0xA9A90000,
    0x2020202,
    0xAC371E0D,
    0x3A517878,
    0x16515760,
    0x15152A1F,
    0x2B4F1E0D,
    0x4B4C354F,
    0x8C739D22,
    0x3D233C89,
    0x4A4A9A67,
    0x5B7A157A,
    0xA78F4C61,
    0xACAFA7A7,
    0xA9A9A9AD,
    0xA9A90000,
    0xBB020202,
    0xAC362A2C,
    0x15778579,
    0x2E787A15,
    0x292A2A0E,
    0x2A36421E,
    0x496B4E29,
    0x8D3C9D23,
    0x3D222289,
    0x497F7E3E,
    0x8436367F,
    0x9F523385,
    0xA4AFA7A7,
    0xAAA900AC,
    0xA9A9AB,
};

GpuImageUpload D_actor_403100_801555EC[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 384, 256, 30, 130 }, D_actor_403100_80153774 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

TaskDesc D_actor_403100_8015560C[2] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_403100_8013E0FC, { .model = &_gActor403100BurnerBody } },
    { { { TASK_BODY_COORD, 96 } }, func_actor_403100_8013E04C, { .value = 0 } },
};

TaskDesc D_actor_403100_80155624 = { { { TASK_BODY_COORD, 96 } }, func_actor_403100_8013E0A4, { .value = 0 } };

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

// Message-table callbacks use the argument views required by this TU.

TaskMessageEntry D_actor_403100_801556EC[4] = {
    { 2014, func_actor_403100_8013D5F4 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_403100_8013D564 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_403100_8013D608 },
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

static void func_actor_403100_801326DC(Actor403100Work* work);

static void func_actor_403100_8013712C(Task* arg0);

static void func_actor_403100_8013C008(s16 arg0, s16 arg1);

static void func_actor_403100_8013D74C(Task* arg0);

static s32 func_actor_403100_80133928(void);

static void func_actor_403100_801345E0(Task* arg0, Task* arg1);

static void func_actor_403100_8013E6F0(Task* arg0);

static void func_actor_403100_8013F12C(Task* task);

static void func_actor_403100_8013E16C(void);

static void func_actor_403100_8013E174(void);

static void func_actor_403100_8013E1E4(void);

static void func_actor_403100_8013E2BC(void);

static void func_actor_403100_8013BA64(Task* arg0);

static void func_actor_403100_8013C214(Task* arg0);

static void func_actor_403100_8013CBE0(Task* task);

static void func_actor_403100_8013CDC0(void);

static void func_actor_403100_8013D2A0(s16 arg0);

static void func_actor_403100_8013D6B4(Task* arg0);

static void func_actor_403100_8013D700(Task* arg0);

static void func_actor_403100_80136830(Task* arg0);

static void func_actor_403100_80137268(Task* task);

static void func_actor_403100_80137310(Task* task);

static void func_actor_403100_8013BB8C(Task* arg0);

static void func_actor_403100_8013BDE4(Task* arg0);

static void func_actor_403100_8013BEF0(Task* arg0);

static void func_actor_403100_8013D88C(Task* arg0);

static void func_actor_403100_8013D8F4(Task* arg0);

static void func_actor_403100_8013DA6C(Task* task);

static void func_actor_403100_8013DAC4(Task* arg0);

static void func_actor_403100_8013DB48(Task* arg0);

static void func_actor_403100_8013DC18(Task* arg0);

static void func_actor_403100_8013DCAC(Task* arg0);

static void func_actor_403100_8013DD78(Task* arg0);

static void func_actor_403100_8013DE0C(Task* arg0);

static void func_actor_403100_8013DEA0(Task* arg0);

static void func_actor_403100_8013DF0C(Task* task);

static void func_actor_403100_8013DF64(Task* task);

static void func_actor_403100_8013DFBC(Task* arg0);

static void func_actor_403100_8013E6A0(Task* arg0);

static void func_actor_403100_8013E784(Task* arg0);

static void func_actor_403100_8013E7C8(Task* arg0);

static void func_actor_403100_8013E88C(Task* arg0);

static void func_actor_403100_8013E920(Task* arg0);

static void func_actor_403100_8013ED50(Task* arg0);

static void func_actor_403100_8013EDDC(Task* task);

static void func_actor_403100_8013EE28(Task* arg0);

static void func_actor_403100_8013EEB0(Task* task);

static void func_actor_403100_8013EEB8(Task* arg0);

static void func_actor_403100_8013EF24(Task* task);

static void func_actor_403100_8013EF2C(Task* task);

static void func_actor_403100_8013EF34(Task* task);

static void func_actor_403100_8013EF58(Task* task);

static void func_actor_403100_8013EF60(Task* task);

static void func_actor_403100_8013EFC0(Task* task);

static void func_actor_403100_8013EFC8(Task* arg0);

static void func_actor_403100_8013F034(Task* arg0);

static void func_actor_403100_8013F0A8(Task* arg0);

static void func_actor_403100_8013F18C(Task* task);

static void func_actor_403100_8013F1D8(Task* task);

static void func_actor_403100_8013F230(Task* task);

static void func_actor_403100_8013F270(Task* task);

static void func_actor_403100_8013F2D8(Task* task);

static void func_actor_403100_8013F344(Task* task);

static void func_actor_403100_8013F3AC(Task* task);

static void func_actor_403100_8013F3EC(Task* arg0);

static void func_actor_403100_8013F488(Task* task);

static void func_actor_403100_8013F4E0(Task* task);

static void func_actor_403100_8013F520(Task* task);

static void func_actor_403100_8013F588(Task* task);

static void func_actor_403100_8013F6B0(Task* task);

static void func_actor_403100_8013F6F4(Task* task);

static void func_actor_403100_8013F76C(Task* task);

static void func_actor_403100_8013F7AC(Task* task);

static void func_actor_403100_8013F7B4(Task* task);

static void func_actor_403100_8013F7BC(Task* task);

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

static __inline__ s32  Actor403100_FindRegion(s16 x, s16 z);
static __inline__ s32  Actor403100_FindEffectRegion(s16 x, s16 z);
static __inline__ s32  Actor403100_AccumulateRotation(GfxCoord* arg0, MATRIX* arg1, GfxCoord* arg2);
static __inline__ s32  Actor403100_LocalizeRotation(GfxCoord* arg0, MATRIX* arg1, GfxCoord* arg2);
static __inline__ s16  Actor403100_TestFlags(void);
static __inline__ s16  Actor403100_TestFlags104(void);
static __inline__ s16  Actor403100_TestFlags12C(void);
static void            func_actor_403100_80132320(Task* arg0);
static void            func_actor_403100_80132528(Task* arg0);
static void            func_actor_403100_801331D4(Task* arg0);
static void            func_actor_403100_8013335C(Task* arg0);
static inline void     _actor403100SetRootYaw(Task* task);
static inline void     _actor403100ScaleRoot(Task* task, s16 factor);
static inline void     _actor403100UpdateColor(Task* task, GfxCoord* coord);
static void            func_actor_403100_801339EC(Task* arg0);
static __inline__ void _actor403100PlaceSpawned(GfxCoord* coord, s32 i);
static __inline__ void _actor403100SetObjFlags(WorldCollisionBody* obj, s32 mask, s32 bits);
static void            func_actor_403100_80134D50(Task* arg0);
static void            func_actor_403100_8013506C(Task* arg0);
static void            func_actor_403100_801351F8(Task* arg0);
static void            func_actor_403100_8013539C(Task* arg0);
static void            func_actor_403100_801354A0(Task* arg0);
static void            func_actor_403100_801355D4(Task* arg0);
static void            func_actor_403100_801356F4(Task* arg0);
static void            func_actor_403100_8013588C(Task* arg0);
static void            func_actor_403100_801359DC(Task* arg0);
static void            func_actor_403100_80135AE0(Task* arg0);
static void            func_actor_403100_80135C00(Task* arg0);
static void            func_actor_403100_80135F30(Task* arg0);
static void            func_actor_403100_80136100(Task* arg0);
static void            func_actor_403100_8013631C(Task* arg0);
static void            func_actor_403100_80136610(Task* arg0);
static inline void     _actor403100RunHook(void);
static inline void     _actor403100TurnPart6(Task* task);
static inline void     _actor403100PitchArms(Task* task);
static void            func_actor_403100_801375B8(Task* task);
static void            func_actor_403100_801376D8(Task* arg0);
static void            func_actor_403100_801379B4(Task* arg0);
static void            func_actor_403100_80137CA8(Task* task);
static void            func_actor_403100_80137DC4(Task* arg0);
static void            func_actor_403100_80137F4C(Task* task);
static void            func_actor_403100_80138048(Task* arg0);
static void            func_actor_403100_8013842C(Task* arg0);
static void            func_actor_403100_80138610(Task* arg0);
static void            func_actor_403100_801386DC(Task* arg0);
static void            func_actor_403100_80138790(Task* arg0);
static void            func_actor_403100_80138844(Task* arg0);
static void            func_actor_403100_80138AB4(Task* task);
static void            func_actor_403100_80138C18(Task* task);
static void            func_actor_403100_80138D08(Task* arg0);
static void            func_actor_403100_80138DB0(Task* arg0);
static void            func_actor_403100_80138F88(Task* arg0);
static void            func_actor_403100_8013922C(Task* arg0);
static void            func_actor_403100_801395EC(Task* arg0);
static void            func_actor_403100_80139818(Task* arg0);
static void            func_actor_403100_80139E80(Task* arg0);
static void            func_actor_403100_8013A064(Task* arg0);
static void            func_actor_403100_8013A254(Task* task);
static void            func_actor_403100_8013A4C8(Task* arg0);
static void            func_actor_403100_8013A5AC(Task* arg0);
static void            func_actor_403100_8013A81C(Task* arg0);
static void            func_actor_403100_8013AA04(Task* arg0);
static void            func_actor_403100_8013AC04(Task* task);
static void            func_actor_403100_8013AE28(Task* task);
static inline void     _actor403100StepRoot(TmdObject* obj, GfxCoord* coords);
static inline s32      Actor403100CoordToViewInline(GfxCoord* coord, SVECTOR* pos);
static inline void     Actor403100ResetStateInline(s16 anim, s16 angle, s16 frame);
static s32             func_actor_403100_8013E450(GfxCoord* arg0, MATRIX* arg1, GfxCoord* arg2);

static __inline__ s32 Actor403100_FindRegion(s16 x, s16 z)
{
    _Actor403100Zone* zone;
    for (zone = D_actor_403100_80155698; zone->id != ACTOR_403100_ZONE_END; zone++) {
        if (x >= zone->x && x <= zone->x + zone->width &&
            z >= zone->z && z <= zone->z + zone->depth) {
            return zone->id;
        }
    }
    return 0;
}

static __inline__ s32 Actor403100_FindEffectRegion(s16 x, s16 z)
{
    _Actor403100Zone* zone;
    for (zone = D_actor_403100_80155638; zone->id != ACTOR_403100_ZONE_END; zone++) {
        if (x >= zone->x && x <= zone->x + zone->width && z >= zone->z && z <= zone->z + zone->depth)
            return zone->id;
    }
    return 0;
}

/* Inline forms of this actor's rotation traversal helpers. */
static __inline__ s32 Actor403100_AccumulateRotation(GfxCoord* arg0, MATRIX* arg1, GfxCoord* arg2)
{
    MATRIX    matrix;
    GfxCoord* coord;

    coord = arg0->parent;
    *arg1 = arg0->coord;
    while (1) {
        if (coord == NULL) {
            return 0;
        }
        if (coord == arg2) {
            return 1;
        }
        gte_SetRotMatrix(&coord->coord);
        MulRotMatrix(arg1);
        MatrixNormal(arg1, &matrix);
        *arg1 = matrix;
        coord = coord->parent;
    }
}

static __inline__ s32 Actor403100_LocalizeRotation(GfxCoord* arg0, MATRIX* arg1, GfxCoord* arg2)
{
    MATRIX    matrix;
    MATRIX    normal;
    MATRIX    transposed;
    GfxCoord* coord;

    coord = arg0->parent;
    if (coord == &gGfxViewCoord) {
        return 0;
    }
    matrix = coord->coord;
    while (1) {
        coord = coord->parent;
        if (coord == NULL) {
            return 0;
        }
        if (coord == arg2) {
            break;
        }
        gte_SetRotMatrix(&coord->coord);
        MulRotMatrix(&matrix);
        MatrixNormal(&matrix, &normal);
        matrix = normal;
    }
    gte_TransposeMatrix(&matrix, &transposed);
    gte_SetRotMatrix(&transposed);
    MulRotMatrix(arg1);
    return 1;
}

/* Resolved through `configs/USA/sym/actors.imports.txt`. */

/* Defined later in this file. */

static __inline__ s16 Actor403100_TestFlags(void)
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

static __inline__ s16 Actor403100_TestFlags104(void)
{
    if (D_actor_403100_80155808->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        return 1;
    }
    if (D_actor_403100_80155808->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED)) {
        return 1;
    }
    return 0;
}

static __inline__ s16 Actor403100_TestFlags12C(void)
{
    if (D_actor_403100_80155808->rig.slots[2].status.fields.flags & 0x100) {
        return 1;
    }
    return 0;
}

/// Per-frame hooks run before the behaviour mode, indexed by `playerReactionStage`.
static const _Actor403100PlayerReactionTable D_actor_403100_80131E24 = {
    {
        func_actor_403100_8013E16C,
        func_actor_403100_8013E174,
        func_actor_403100_8013E1E4,
        func_actor_403100_8013E2BC,
    },
};

static void func_actor_403100_80132064(Task* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3)
{
    SVECTOR                end;
    GfxMatrix              matrix;
    u16                    mode;
    GfxMatrix*             identity;
    GfxCoord*              joint;
    s32                    i;
    u32                    random;
    WorldCollisionContact* contacts;
    WorldCollisionBody*    body;
    GfxCoord*              coord;

    i      = 0;
    mode   = arg3;
    joint  = &arg0->extra.tmd->coords[3];
    end.vx = arg1->vx + arg2->vx;
    end.vy = arg1->vy + arg2->vy;
    end.vz = arg1->vz + arg2->vz;
    for (; i < ARRAY_SIZE(D_actor_403100_80155814); i++) {
        if (D_actor_403100_80155814[i].active == 0) {
            coord                                 = &D_actor_403100_80155814[i].coord;
            D_actor_403100_80155814[i].active     = 1;
            D_actor_403100_80155814[i].age        = 0;
            random                                = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState                       = random;
            D_actor_403100_80155814[i].spriteStep = (s16)((random >> 0x10) & 0xF);
            coordLocalToWorld(joint, arg1);
            identity = &matrix;
            coordLocalToWorld(joint, &end);
            D_actor_403100_80155814[i].velocity.vx           = (s16)(end.vx - arg1->vx);
            D_actor_403100_80155814[i].velocity.vy           = (s16)(end.vy - arg1->vy);
            D_actor_403100_80155814[i].velocity.vz           = (s16)(end.vz - arg1->vz);
            D_actor_403100_80155814[i].position.vx           = (u16)arg1->vx;
            D_actor_403100_80155814[i].position.vy           = (u16)arg1->vy;
            D_actor_403100_80155814[i].position.vz           = (u16)arg1->vz;
            D_actor_403100_80155814[i].coord.parent          = &gGfxViewCoord;
            matrix.rotationWords.m00M01                      = ONE;
            matrix.rotationWords.m02M10                      = 0;
            identity->rotationWords.m11M12                   = ONE;
            matrix.rotationWords.m20M21                      = 0;
            identity->rotationWords.m22                      = ONE;
            matrix.mat.t[0]                                  = (s32)(s16)arg1->vx;
            matrix.mat.t[1]                                  = (s32)(s16)arg1->vy;
            contacts                                         = D_actor_403100_80155814[i].contacts;
            matrix.mat.t[2]                                  = (s32)(s16)arg1->vz;
            D_actor_403100_80155814[i].coord.coord           = matrix.mat;
            D_actor_403100_80155814[i].body.coord            = coord;
            D_actor_403100_80155814[i].body.context.contacts = contacts;
            D_actor_403100_80155814[i].body.pos.vx           = 0;
            D_actor_403100_80155814[i].body.pos.vy           = 0;
            D_actor_403100_80155814[i].body.pos.vz           = 0;
            D_actor_403100_80155814[i].body.key              = Gp_PackPair(D_actor_403100_80147614, 1);
            D_actor_403100_80155814[i].body.radius           = 0x32;
            D_actor_403100_80155814[i].body.flags            = WORLD_COLLISION_BODY_SPHERE;
            body                                             = &D_actor_403100_80155814[i].body;
            Gp_LinkObj(3, body);
            worldCollisionInitContacts(contacts, ARRAY_SIZE(D_actor_403100_80155814[i].contacts), 0);
            if ((mode << 0x10) == 0) {
                body->flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            } else {
                body->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            actorRenderComposeCoord(&D_actor_403100_80155814[i].coord);
            D_actor_403100_80155814[i].coord.composeStamp = GRAPHICS_COORD_DIRTY;
            D_actor_403100_80155814[i].coord.coord.t[0]   = (s32)(s16)D_actor_403100_80155814[i].position.vx;
            D_actor_403100_80155814[i].coord.coord.t[1]   = (s32)(s16)D_actor_403100_80155814[i].position.vy;
            D_actor_403100_80155814[i].coord.coord.t[2]   = (s32)(s16)D_actor_403100_80155814[i].position.vz;
            break;
        }
    }
}

static void func_actor_403100_80132320(Task* arg0)
{
    D_actor_403100_80155808->headBody.coord            = &arg0->extra.tmd->coords[3];
    D_actor_403100_80155808->headBody.context.contacts = D_actor_403100_80155808->hitContacts;
    D_actor_403100_80155808->headBody.pos.vz           = 0x300;
    D_actor_403100_80155808->headBody.pos.vx           = 0;
    D_actor_403100_80155808->headBody.pos.vy           = 0;
    D_actor_403100_80155808->headBody.key              = 0x3001F;
    D_actor_403100_80155808->headBody.radius           = 0x400;
    D_actor_403100_80155808->headBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &D_actor_403100_80155808->headBody);
    worldCollisionInitContacts(D_actor_403100_80155808->hitContacts, ARRAY_SIZE(D_actor_403100_80155808->hitContacts), 0);
    D_actor_403100_80155808->headBody.flags            |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    D_actor_403100_80155808->trunkBody.coord            = &arg0->extra.tmd->coords[1];
    D_actor_403100_80155808->trunkBody.context.contacts = D_actor_403100_80155808->hitContacts;
    D_actor_403100_80155808->trunkBody.pos.vx           = 0;
    D_actor_403100_80155808->trunkBody.pos.vy           = 0;
    D_actor_403100_80155808->trunkBody.pos.vz           = 0;
    D_actor_403100_80155808->trunkBody.key              = 0x3001F;
    D_actor_403100_80155808->trunkBody.radius           = 0x800;
    D_actor_403100_80155808->trunkBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &D_actor_403100_80155808->trunkBody);
    worldCollisionInitContacts(D_actor_403100_80155808->initializedContacts, ARRAY_SIZE(D_actor_403100_80155808->initializedContacts), 0);
    D_actor_403100_80155808->handAttack.key              = 0x3001F;
    D_actor_403100_80155808->trunkBody.flags            |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    D_actor_403100_80155808->handAttack.coord            = &arg0->extra.tmd->coords[7];
    D_actor_403100_80155808->handAttack.context.contacts = D_actor_403100_80155808->handContacts;
    D_actor_403100_80155808->handAttack.pos.vx           = -0x200;
    D_actor_403100_80155808->handAttack.pos.vy           = 0;
    D_actor_403100_80155808->handAttack.pos.vz           = 0x200;
    D_actor_403100_80155808->handAttack.radius           = 0x3A0;
    D_actor_403100_80155808->handAttack.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &D_actor_403100_80155808->handAttack);
    worldCollisionInitContacts(D_actor_403100_80155808->handContacts, ARRAY_SIZE(D_actor_403100_80155808->handContacts), 0);
    D_actor_403100_80155808->forearmAttack.key              = 0x3001F;
    D_actor_403100_80155808->handAttack.flags              &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    D_actor_403100_80155808->forearmAttack.coord            = &arg0->extra.tmd->coords[6];
    D_actor_403100_80155808->forearmAttack.context.contacts = D_actor_403100_80155808->forearmContacts;
    D_actor_403100_80155808->forearmAttack.pos.vx           = -0x200;
    D_actor_403100_80155808->forearmAttack.pos.vy           = 0;
    D_actor_403100_80155808->forearmAttack.pos.vz           = 0x180;
    D_actor_403100_80155808->forearmAttack.radius           = 0x3A0;
    D_actor_403100_80155808->forearmAttack.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &D_actor_403100_80155808->forearmAttack);
    worldCollisionInitContacts(D_actor_403100_80155808->forearmContacts, ARRAY_SIZE(D_actor_403100_80155808->forearmContacts), 0);
    D_actor_403100_80155808->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}
static void func_actor_403100_80132528(Task* arg0)
{
    SVECTOR    pos;
    SVECTOR    rotation;
    MATRIX     matrix;
    GameActor* player;
    GfxCoord*  joint;
    GfxCoord*  playerCoord;
    s32        rate;
    s32        anim;
    u16        savedRate;
    GfxCoord*  coords;

    anim                                   = D_actor_403100_80155808->animationId;
    playerCoord                            = (*gPlayerActorTasks)->extra.tmd->coords;
    coords                                 = arg0->extra.tmd->coords;
    player                                 = (*gPlayerActorTasks)->work;
    rate                                   = D_actor_403100_80155808->animationRate;
    savedRate                              = (u16)D_actor_403100_80155808->animationRate;
    D_actor_403100_80155808->animationRate = rate * 2;
    func_actor_403100_8013E02C(anim, D_actor_403100_80155808->animationRate, 0);
    func_actor_403100_801327CC(arg0);
    func_actor_403100_801328DC(arg0);
    func_actor_403100_8013D770(arg0);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    joint                  = &coords[7];
    coords[7].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(joint);
    pos.vx = -0x290;
    pos.vy = 0x1E8;
    pos.vz = 0x220;
    coordLocalToWorld(joint, &pos);
    func_actor_403100_8013D2F4(joint, &matrix);
    Gp_MtxToEuler(&matrix, &rotation);
    player->rotation.vx = rotation.vx;
    player->rotation.vy = rotation.vy;
    player->rotation.vz = rotation.vz;
    RotMatrix(&rotation, &playerCoord->coord);
    playerCoord->coord.t[0]   = pos.vx;
    playerCoord->coord.t[1]   = pos.vy;
    playerCoord->coord.t[2]   = pos.vz;
    playerCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(playerCoord);
    D_actor_403100_80155808->animationRate = (-rate) << 1;
    func_actor_403100_8013E02C(D_actor_403100_80155808->animationId, D_actor_403100_80155808->animationRate, 0);
    func_actor_403100_801327CC(arg0);
    D_actor_403100_80155808->animationRate = (s16)savedRate;
    func_actor_403100_8013E02C(D_actor_403100_80155808->animationId, rate, 0);
}
static void func_actor_403100_801326DC(Actor403100Work* work)
{
    s32 i;

    if (D_actor_403100_80155808->appliedAnimation == D_actor_403100_80155808->animationId) {
        for (i = 1; i < ARRAY_SIZE(D_actor_403100_80155808->rig.slots); i++) {
            D_actor_403100_80155808->rig.slots[i].rate = D_actor_403100_80155808->animationRate;
        }
    } else {
        for (i = 1; i < ARRAY_SIZE(D_actor_403100_80155808->rig.slots); i++) {
            D_actor_403100_80155808->rig.slots[i].rate = D_actor_403100_80155808->animationRate;
            animationSeekSlotWithBlend(&D_actor_403100_80155808->rig.anim, i, D_actor_403100_80155808->animationId, 0, D_actor_403100_80155808->animationBlendFrames);
        }
        D_actor_403100_80155808->animationBlendFrames = 0;
    }
    D_actor_403100_80155808->appliedAnimation = D_actor_403100_80155808->animationId;
}
static void func_actor_403100_801327CC()
{
    s32 i;
    if (D_actor_403100_80155808->animationRequest == ACTOR_403100_ANIMATION_REQUEST_BLEND) {
        func_actor_403100_801326DC(D_actor_403100_80155808);
        D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_PLAYING;
        D_actor_403100_80155808->animationFrames  = 0;
    } else if (D_actor_403100_80155808->animationRequest == ACTOR_403100_ANIMATION_REQUEST_RESET) {
        for (i = 1; i < ARRAY_SIZE(D_actor_403100_80155808->rig.slots); i++) {
            animationResetSlot(&D_actor_403100_80155808->rig.anim, i, D_actor_403100_80155808->animationId);
            D_actor_403100_80155808->rig.slots[i].rate = D_actor_403100_80155808->animationRate;
        }
        D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_PLAYING;
        D_actor_403100_80155808->animationFrames  = 0;
        D_actor_403100_80155808->appliedAnimation = D_actor_403100_80155808->animationId;
    } else if (D_actor_403100_80155808->animationRequest == ACTOR_403100_ANIMATION_REQUEST_PLAYING) {
        D_actor_403100_80155808->animationFrames += 1;
    }
    for (i = 1; i < ARRAY_SIZE(D_actor_403100_80155808->rig.slots); i++) {
        animationTickSlot(&D_actor_403100_80155808->rig.anim, i);
    }
}
static void func_actor_403100_801328DC(Task* arg0)
{
    GfxCoord* root;
    GfxCoord* joint;
    MATRIX*   rotation;
    MATRIX*   dest;

    root                                                                       = arg0->extra.tmd->coords;
    *(MATRIX**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) -= 1;
    rotation                                                                   = *(MATRIX**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET);
    joint                                                                      = &root[5];
    Actor403100_AccumulateRotation(joint, rotation, root);
    RotMatrixX(D_actor_403100_80155808->armPitch, rotation);
    RotMatrixY(D_actor_403100_80155808->armYaw, rotation);
    Actor403100_LocalizeRotation(joint, rotation, root);
    dest                = &joint->coord;
    dest->m[0][0]       = rotation->m[0][0];
    dest->m[0][1]       = rotation->m[0][1];
    dest->m[0][2]       = rotation->m[0][2];
    dest->m[1][0]       = rotation->m[1][0];
    dest->m[1][1]       = rotation->m[1][1];
    dest->m[1][2]       = rotation->m[1][2];
    dest->m[2][0]       = rotation->m[2][0];
    dest->m[2][1]       = rotation->m[2][1];
    dest->m[2][2]       = rotation->m[2][2];
    joint->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(joint);
    *(MATRIX**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) += 1;
}
static void func_actor_403100_80132C3C(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s32 height)
{
    MATRIX    firstMatrix;
    MATRIX    secondMatrix;
    SVECTOR   first;
    SVECTOR   second;
    SVECTOR   corner0;
    SVECTOR   corner1;
    SVECTOR   corner2;
    SVECTOR   corner3;
    long      screen0;
    long      screen1;
    long      screen2;
    long      screen3;
    long      perspective;
    long      flags;
    s16       lastZ;
    s16       angle;
    GfxCoord* secondCoord;
    GfxCoord* firstCoord;
    s32       offset0;
    s32       offset1;
    s32       offset2;
    s32       offset3;
    s32       halfX;
    s32       halfZ;
    s32       depth;
    GfxCoord* coords;
    POLY_FT4* poly;

    coords      = task->extra.tmd->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        actorRenderComposeCoord(firstCoord);
        actorRenderComposeCoord(secondCoord);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &firstCoord->workm, &firstMatrix);
        gfxMakeRelativeTransform(&gGfxViewCoord.workm, &secondCoord->workm, &secondMatrix);
        first.vy   = (s16)height;
        second.vy  = (s16)height;
        first.vx   = firstMatrix.t[0];
        first.vz   = firstMatrix.t[2];
        second.vx  = secondMatrix.t[0];
        second.vz  = secondMatrix.t[2];
        angle      = ratan2((s16)secondMatrix.t[0] - (s16)firstMatrix.t[0], (s16)secondMatrix.t[2] - (s16)firstMatrix.t[2]);
        halfX      = (first.vx - second.vx) / 2;
        halfZ      = (first.vz - second.vz) / 2;
        offset0    = rcos(angle) * width;
        corner0.vy = (s16)height;
        corner0.vx = halfX + (first.vx - (offset0 >> 0xC));
        corner0.vz = halfZ + (first.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1    = rcos(angle) * width;
        corner1.vy = (s16)height;
        corner1.vx = halfX + (first.vx + (offset1 >> 0xC));
        corner1.vz = halfZ + (first.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2    = rcos(angle) * width;
        corner2.vy = (s16)height;
        corner2.vx = (second.vx - (offset2 >> 0xC)) - halfX;
        corner2.vz = (second.vz + ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        offset3    = rcos(angle) * width;
        corner3.vy = (s16)height;
        corner3.vx = (second.vx + (offset3 >> 0xC)) - halfX;
        lastZ      = (second.vz - ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        corner3.vz = lastZ;
        if ((corner0.vz < 0x23F0) && (corner1.vz < 0x23F0) && (corner2.vz < 0x23F0) && (lastZ < 0x23F0)) {
            if (corner0.vx >= -0x144F) {
                corner0.vx = -0x1450;
            }
            if (corner1.vx >= -0x144F) {
                corner1.vx = -0x1450;
            }
            if (corner2.vx >= -0x144F) {
                corner2.vx = -0x1450;
            }
            if (corner3.vx >= -0x144F) {
                corner3.vx = -0x1450;
            }
        } else if ((corner0.vx >= -0x144F) && (corner1.vx >= -0x144F) && (corner2.vx >= -0x144F) && (corner3.vx >= -0x144F)) {
            if (corner0.vz < 0x23F0) {
                corner0.vz = 0x23F0;
            }
            if (corner1.vz < 0x23F0) {
                corner1.vz = 0x23F0;
            }
            if (corner2.vz < 0x23F0) {
                corner2.vz = 0x23F0;
            }
            if (corner3.vz < 0x23F0) {
                corner3.vz = 0x23F0;
            }
        }
        gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&gGfxViewCoord);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        depth = RotTransPers4(&corner0, &corner1, &corner2, &corner3, &screen0, &screen1, &screen2, &screen3, &perspective, &flags);
        if (flags >= 0) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = poly + 1;
            setlen(poly, 9);
            poly->code                     = 0x2E;
            GPU_PRIMITIVE_XY_WORD(poly, 0) = screen0;
            poly->tpage                    = 0x48;
            GPU_PRIMITIVE_XY_WORD(poly, 1) = screen1;
            poly->clut                     = 0x4283;
            GPU_PRIMITIVE_XY_WORD(poly, 2) = screen2;
            GPU_PRIMITIVE_XY_WORD(poly, 3) = screen3;
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            setRGB0(poly, 0xFF, 0xFF, 0xFF);
            addPrim((&gGpuCurrentOt[((((u32)(depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
        }
    }
}
static void func_actor_403100_801331D4(Task* arg0)
{
    SVECTOR   pos;
    GfxCoord* joint;
    GfxCoord* playerCoord;
    s16       dx;
    s16       dx2;
    s16       dz;
    s16       dz2;
    GfxCoord* coords;

    coords = arg0->extra.tmd->coords;
    joint  = &coords[3];
    if (*gPlayerActorTasks != NULL) {
        playerCoord                                = (*gPlayerActorTasks)->extra.tmd->coords;
        D_actor_403100_80155808->playerPosition.vx = (u16)playerCoord->coord.t[0];
        D_actor_403100_80155808->playerPosition.vy = (u16)playerCoord->coord.t[1];
        D_actor_403100_80155808->playerPosition.vz = (u16)playerCoord->coord.t[2];
        D_actor_403100_80155808->aimTarget.vx      = (u16)playerCoord->coord.t[0];
        D_actor_403100_80155808->aimTarget.vy      = (u16)playerCoord->coord.t[1];
        D_actor_403100_80155808->aimTarget.vz      = (u16)playerCoord->coord.t[2];
        dx                                         = (u16)playerCoord->coord.t[0] - (u16)coords->coord.t[0];
        pos.vx                                     = dx;
        pos.vy                                     = (u16)playerCoord->coord.t[1] - (u16)coords->coord.t[1];
        dz                                         = (u16)playerCoord->coord.t[2] - (u16)coords->coord.t[2];
        pos.vz                                     = dz;
        D_actor_403100_80155808->playerDistance    = SquareRoot0((dx * dx) + (dz * dz));
        coordLocalToWorld(joint, &pos);
        dx2                                  = (u16)playerCoord->coord.t[0] - (u16)pos.vx;
        pos.vx                               = dx2;
        pos.vy                               = (u16)playerCoord->coord.t[1] - pos.vy;
        dz2                                  = (u16)playerCoord->coord.t[2] - (u16)pos.vz;
        pos.vz                               = dz2;
        D_actor_403100_80155808->hitDistance = SquareRoot0((dx2 * dx2) + (dz2 * dz2));
    }
}
static void func_actor_403100_8013335C(Task* arg0)
{
    s16 damage;
    s16 scaledDamage;
    s32 hitId;
    s16 effectKind;
    s32 i;
    u16 hp;
    u32 tickDamage;
    u32 kind;
    s32 expired;

    effectKind                        = 0;
    D_actor_403100_80155808->hitTaken = 0;
    i                                 = 0;
    for (; i < ARRAY_SIZE(D_actor_403100_80155808->hitContacts); i++) {
        hitId = D_actor_403100_80155808->hitContacts[i].key.value;
        if ((hitId & 0xFFFF0000) == 0x20000) {
            if (D_actor_403100_80155808->hitCooldown == 0) {
                D_actor_403100_80155808->hitTaken    = 1;
                damage                               = Gp_ComputeDamage(D_actor_403100_80155808->hitContacts[i].key.value, D_actor_403100_80155808->hitDistance / 2, 0, 0);
                scaledDamage                         = damage;
                D_actor_403100_80155808->hitCooldown = Gp_GetIdParam2(D_actor_403100_80155808->hitContacts[i].key.value);
                if (Gp_RollEnemyChance(D_actor_403100_8015580C, D_actor_403100_80155808->hitContacts[i].key.value, 0) != 0) {
                    scaledDamage = damage * 4;
                    effectKind   = 1;
                }
                if (D_actor_403100_80155808->vulnerable != 0) {
                    effectKind   = 2;
                    scaledDamage = scaledDamage * 2;
                }
                kind = effectKind;
                if (kind == 1)
                    goto effect1;
                if (kind == 2)
                    goto effect2;
                goto effect_end;
            effect1:
                Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[4], 0, 0);
                goto effect_end;
            effect2:
                Gp_SpawnEff(EFFECT_CRITICAL_HIT, &arg0->extra.tmd->coords[4], 3, 0);
            effect_end:
                func_800E2C78(D_actor_403100_8015580C, D_actor_403100_80155808->hitContacts[i].key.value, scaledDamage, 0);
                func_800DA6E8(&D_actor_403100_8015580C->node, scaledDamage, 0);
                do {
                    hp                          = (u16)D_actor_403100_8015580C->hp - scaledDamage;
                    D_actor_403100_8015580C->hp = hp;
                    if ((s16)hp < 0) {
                        D_actor_403100_8015580C->hp = 0U;
                    }
                    func_800FDB18(Gp_GetIdParam1(D_actor_403100_80155808->hitContacts[i].key.value) & 0xFFFF, &arg0->extra.tmd->coords[4], 0, &D_actor_403100_80155630);
                    D_actor_403100_80155808->hitReaction = 1;
                    kind                                 = Gp_GetIdParam0(D_actor_403100_80155808->hitContacts[i].key.value) & 0xFFFF;
                } while (0);
                switch (kind) {
                    case 0:
                        break;
                    case 1:
                        Gp_SetObjFlag1(D_actor_403100_8015580C);
                        break;
                    case 2:
                        Gp_SetObjFlag2(D_actor_403100_8015580C, D_actor_403100_80155808->hitContacts[i].key.value, 0);
                        break;
                    case 3:
                        Gp_SetObjFlag4(D_actor_403100_8015580C, D_actor_403100_80155808->hitContacts[i].key.value, 0);
                        break;
                    case 4:
                        D_actor_403100_80155808->hitReaction = 1;
                        break;
                    case 5:
                        D_actor_403100_80155808->hitReaction = 1;
                        break;
                    case 6:
                        D_actor_403100_80155808->hitReaction = 1;
                        break;
                    case 7:
                        D_actor_403100_80155808->hitReaction = 1;
                        break;
                    case 8:
                        D_actor_403100_80155808->hitReaction = 0;
                        D_actor_403100_80155808->hitTaken    = 0;
                        break;
                    case 9:
                        D_actor_403100_80155808->hitReaction = 2;
                        break;
                }
                if (D_actor_403100_80155808->hitReaction != 0) {
                    D_actor_403100_80155808->hitColorFrames = 0x10;
                    Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);
                }
            } else if ((Gp_GetIdParam1(hitId) & 0xFFFF) == 0xD) {
                func_800FDB18(0xD, &arg0->extra.tmd->coords[4], 0, &D_actor_403100_80155630);
            }
        }
        if (D_actor_403100_80155808->hitTaken != 0)
            break;
    }
    if (D_actor_403100_8015580C->reactionFlags & ENEMY_REACTION_STAGGER) {
        D_actor_403100_8015580C->reactionFlags &= ~ENEMY_REACTION_STAGGER;
        D_actor_403100_80155808->hitReaction    = 2;
    }
    if (D_actor_403100_8015580C->reactionFlags & ENEMY_REACTION_BUILDUP) {
        D_actor_403100_8015580C->reactionFlags &= ~ENEMY_REACTION_BUILDUP;
        D_actor_403100_80155808->hitColorFrames = 0x5A;
        D_actor_403100_80155808->hitReaction    = 3;
    }
    if (D_actor_403100_8015580C->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        tickDamage = (u32)Gp_TickObjFlag4(D_actor_403100_8015580C) >> 2;
        if ((s16)tickDamage != 0) {
            D_actor_403100_8015580C->hp = (u16)((u16)D_actor_403100_8015580C->hp - tickDamage);
            func_800DA6E8(&D_actor_403100_8015580C->node, (s16)tickDamage, 0);
            if ((s16)D_actor_403100_8015580C->hp < 0) {
                D_actor_403100_8015580C->hp = 0U;
            }
            D_actor_403100_80155808->hitTaken    = 1;
            D_actor_403100_80155808->hitReaction = 2;
        }
        expired = Gp_ObjFlag4Expired(D_actor_403100_8015580C);
        if (expired != 0) {
            D_actor_403100_8015580C->reactionFlags = (u8)(D_actor_403100_8015580C->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR);
        }
    }
    if (Gp_FindRec18(D_actor_403100_80155808->handAttack.context.contacts, 0) != 0) {
        for (i = 0; i < ARRAY_SIZE(D_actor_403100_80155808->handContacts); i++) {
            if ((D_actor_403100_80155808->handContacts[i].key.value & 0xFFFF0000) == 0x10000) {
                D_actor_403100_80155808->handTouchedPlayer = 1;
            }
        }
    }
    if (Gp_FindRec18(D_actor_403100_80155808->forearmAttack.context.contacts, 0) != 0) {
        for (i = 0; i < ARRAY_SIZE(D_actor_403100_80155808->forearmContacts); i++) {
            if ((D_actor_403100_80155808->forearmContacts[i].key.value & 0xFFFF0000) == 0x10000) {
                D_actor_403100_80155808->forearmTouchedPlayer = 1;
            }
        }
    }
    Gp_ClearRec18Occupied(D_actor_403100_80155808->handContacts);
    Gp_ClearRec18Occupied(D_actor_403100_80155808->forearmContacts);
    Gp_ClearRec18Occupied(D_actor_403100_80155808->hitContacts);
    if (D_actor_403100_80155808->hitCooldown > 0) {
        D_actor_403100_80155808->hitCooldown = (s16)((u16)D_actor_403100_80155808->hitCooldown - 1);
        return;
    }
    D_actor_403100_80155808->hitCooldown = 0;
}

static s32 func_actor_403100_80133928(void)
{
    s16 state;
    s8  mode;

    mode = D_actor_403100_80155808->hitTaken;
    if (mode == 1) {
        state = D_actor_403100_80155808->hitReaction;
        if (state == mode) {
            D_actor_403100_80155808->hitReaction = 0;
            func_actor_403100_8013D24C();
            return 0;
        }
        if (state == 2) {
            SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 0xA);
            func_actor_403100_8013D24C();
            D_actor_403100_80155808->hitReaction = 0;
            D_actor_403100_80155808->state       = 8;
            D_actor_403100_80155808->subState    = 0;
            return 1;
        }
        if (state == 3) {
            SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 0xA);
            func_actor_403100_8013D24C();
            D_actor_403100_80155808->hitReaction = 0;
            D_actor_403100_80155808->state       = 0xA;
            D_actor_403100_80155808->subState    = 0;
            return 1;
        }
        D_actor_403100_80155808->hitReaction = 0;
        return 0;
    }
    return 0;
}
/// Wraps the root yaw `rotation.vy` to 12 bits and replaces the rotation of the
/// root coordinate with the one `RotMatrixY` builds from it.
static inline void _actor403100SetRootYaw(Task* task)
{
    MATRIX    rotation;
    MATRIX*   dest;
    GfxCoord* coords;

    coords                               = task->extra.tmd->coords;
    D_actor_403100_80155808->rotation.vy = (s32)((u16)D_actor_403100_80155808->rotation.vy << 20) >> 20;
    gfxSetRotIdentity(&rotation);
    RotMatrixY(D_actor_403100_80155808->rotation.vy, &rotation);
    dest                 = &coords->coord;
    dest->m[0][0]        = rotation.m[0][0];
    dest->m[0][1]        = rotation.m[0][1];
    dest->m[0][2]        = rotation.m[0][2];
    dest->m[1][0]        = rotation.m[1][0];
    dest->m[1][1]        = rotation.m[1][1];
    dest->m[1][2]        = rotation.m[1][2];
    dest->m[2][0]        = rotation.m[2][0];
    dest->m[2][1]        = rotation.m[2][1];
    dest->m[2][2]        = rotation.m[2][2];
    coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Scales the root coordinate's matrix by `factor` on all three axes.
static inline void _actor403100ScaleRoot(Task* task, s16 factor)
{
    VECTOR    scale;
    MATRIX    scaling;
    GfxCoord* coords;

    coords   = task->extra.tmd->coords;
    scale.vx = factor;
    scale.vy = scale.vx;
    scale.vz = scale.vx;
    gfxSetRotIdentity(&scaling);
    ScaleMatrix(&scaling, &scale);
    MulMatrix(&coords->coord, &scaling);
}

/// Hands the world position of `coord` to `Gp_UpdateActorColor`, staged in a
/// `VECTOR` taken off the scratch stack.
static inline void _actor403100UpdateColor(Task* task, GfxCoord* coord)
{
    VECTOR* pos;

    pos                          = SCRATCH_STACK_CURSOR(VECTOR) - 1;
    pos->vx                      = coord->workm.t[0];
    pos->vy                      = coord->workm.t[1];
    pos->vz                      = coord->workm.t[2];
    SCRATCH_STACK_CURSOR(VECTOR) = pos;
    Gp_UpdateActorColor(task->spawnArg2.pointer, pos, 0, 0);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(VECTOR));
}

static void func_actor_403100_801339EC(Task* arg0)
{
    void (*handlers[5])(Task*) = {
        func_actor_403100_80133C94,
        func_actor_403100_80133D88,
        func_actor_403100_80133E88,
        func_actor_403100_8013E5FC,
        func_actor_403100_8013E624
    };
    GfxCoord* coords;
    GfxCoord* side;
    GfxCoord* center;
    s32       flash;
    s32       brightness;

    handlers[(s16)D_actor_403100_80155808->state](arg0);
    func_actor_403100_801327CC(arg0);
    _actor403100SetRootYaw(arg0);
    _actor403100ScaleRoot(arg0, D_actor_403100_80155808->defeatScale);
    func_actor_403100_801328DC(arg0);
    coords                 = arg0->extra.tmd->coords;
    coords[8].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[7].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[6].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    side                   = &coords[4];
    center                 = &coords[3];
    actorRenderComposeCoord(&coords[8]);
    actorRenderComposeCoord(side);
    _actor403100UpdateColor(arg0, center);
    flash = D_actor_403100_80155808->shakeFrames;
    if (flash != 0) {
        if (flash >= 16) {
            brightness = rsin(gDisplayState.animFrame << 9) << 13;
        } else {
            brightness = rsin(gDisplayState.animFrame << 9) << 12;
        }
        displaySetShakeY((s8)(brightness >> 24));
        D_actor_403100_80155808->shakeFrames = (u16)D_actor_403100_80155808->shakeFrames - 1;
    } else {
        displaySetShakeY(0);
    }
}
static void func_actor_403100_80133C94(Task* task)
{
    s32 i;

    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 0xA);
    worldTargetUnlinkNode(&D_actor_403100_8015580C->node);
    D_actor_403100_80155810 = 0;
    for (i = 0; i < ARRAY_SIZE(D_actor_403100_80155814); i++) {
        if (D_actor_403100_80155814[i].active != 0) {
            D_actor_403100_80155814[i].active = 0;
            worldCollisionUnlinkBody(&D_actor_403100_80155814[i].body);
        }
    }
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 1);
    D_actor_403100_80155808->animationBlendFrames = 0x20;
    D_actor_403100_80155808->animationRate        = ANIMATION_RATE_ONE;
    D_actor_403100_80155808->animationId          = 0x11;
    D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
    D_actor_403100_80155808->defeatScale          = 0x1400;
    D_actor_403100_80155808->stateFrames          = 0;
    D_actor_403100_80155808->armPitch             = 0;
    D_actor_403100_80155808->armYaw               = 0;
    D_actor_403100_80155808->state               += 1;
}
static void func_actor_403100_80133D88(Task* arg0)
{
    GfxCoord* coord;
    u16       frame;

    coord                                = arg0->extra.tmd->coords;
    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    if ((s16)frame >= 0xA0) {
        coord->coord.t[1] += 0xA;
    }
    if ((s16)D_actor_403100_80155808->stateFrames >= 0x82) {
        func_actor_403100_801345E0(arg0, arg0);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x12C) {
        D_actor_403100_80155808->defeatScale      = 0x1600;
        coord->coord.t[0]                         = -0xA28;
        D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
        D_actor_403100_80155808->animationId      = 0xC;
        D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
        coord->coord.t[1]                         = 0x1390;
        coord->coord.t[2]                         = 0x1130;
        D_actor_403100_80155808->rotation.vy      = -0x6B0;
        D_actor_403100_80155808->armPitch         = -0x140;
        D_actor_403100_80155808->armYaw           = -0x100;
        D_actor_403100_80155808->stateFrames      = 0;
        D_actor_403100_80155808->state            = D_actor_403100_80155808->state + 1;
    }
}

/* Puts a newly spawned task's coordinate at point `i` of the spawn point
   table, on the ground plane. */
static __inline__ void _actor403100PlaceSpawned(GfxCoord* coord, s32 i)
{
    coord->coord.t[0] = D_actor_403100_80155794[i][0];
    coord->coord.t[1] = 0;
    coord->coord.t[2] = D_actor_403100_80155794[i][1];
}

static void func_actor_403100_80133E88(Task* arg0)
{
    SVECTOR   pos;
    Task*     task;
    GfxCoord* coord;
    s16       frame;
    GfxCoord* rootCoord;

    rootCoord                          = arg0->extra.tmd->coords;
    D_actor_403100_80155808->armPitch -= 2;
    rootCoord->composeStamp            = GRAPHICS_COORD_DIRTY;
    rootCoord->coord.t[1]             += 0xC;
    if ((D_actor_403100_80155808->stateFrames & 0x7F) == 0x28) {
        task = taskSpawnFromTable(D_actor_403100_8015560C, 1, 0, 0);
        if (task != NULL) {
            coord = task->extra.tmd->coords;
            _actor403100PlaceSpawned(coord, 0);
        }
    }
    if ((D_actor_403100_80155808->stateFrames & 0x3F) == 0x20) {
        task = taskSpawnFromTable(D_actor_403100_8015560C, 1, 0, 0);
        if (task != NULL) {
            coord = task->extra.tmd->coords;
            _actor403100PlaceSpawned(coord, 1);
        }
    }
    if ((D_actor_403100_80155808->stateFrames & 0x7F) == 8) {
        task = taskSpawnFromTable(D_actor_403100_8015560C, 1, 0, 0);
        if (task != NULL) {
            coord = task->extra.tmd->coords;
            _actor403100PlaceSpawned(coord, 2);
        }
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 2) {
        task = taskSpawnFromTable(D_actor_403100_8015560C, 1, 0, 0);
        if (task != NULL) {
            coord = task->extra.tmd->coords;
            _actor403100PlaceSpawned(coord, 1);
            pos.vx              = 0x578;
            pos.vy              = -0xFA0;
            pos.vz              = -0xAF0;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 3, &pos);
        }
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x1E) {
        task = taskSpawnFromTable(D_actor_403100_8015560C, 1, 0, 0);
        if (task != NULL) {
            coord = task->extra.tmd->coords;
            _actor403100PlaceSpawned(coord, 1);
            pos.vx              = 0x3E8;
            pos.vy              = -0xFA0;
            pos.vz              = -0x1130;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 4, &pos);
        }
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x3C) {
        task = taskSpawnFromTable(D_actor_403100_8015560C, 1, 0, 0);
        if (task != NULL) {
            coord = task->extra.tmd->coords;
            _actor403100PlaceSpawned(coord, 1);
            pos.vx              = 0;
            pos.vy              = -0xFA0;
            pos.vz              = -0xFA0;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 5, &pos);
        }
    }
    if ((D_actor_403100_80155808->stateFrames & 0x7F) == 0x40) {
        task = taskSpawnFromTable(D_actor_403100_8015560C, 1, 0, 0);
        if (task != NULL) {
            coord = task->extra.tmd->coords;
            _actor403100PlaceSpawned(coord, 4);
        }
    }
    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    if (frame == 0xBE) {
        gameFlagSetNibble(GAME_FLAG_BURNER_DEFEATED, 1);
        D_actor_403100_80155808->state++;
    }
}
static void func_actor_403100_801342B4(Task* arg0)
{
    SVECTOR   pos1, pos2, offset1, offset2;
    GfxCoord* coords;
    GfxCoord* coord1;
    GfxCoord* coord2;
    s32       i;

    coords                     = arg0->extra.tmd->coords;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    coord1                 = &coords[8];
    coords[8].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord1);
    pos1.vx = offset1.vx = 0x160;
    pos1.vy = offset1.vy = 0x148;
    i                    = 3;
    pos1.vz = offset1.vz = 0x2C0;
    coordLocalToWorld(coord1, &pos1);
    coord2  = &coords[7];
    pos2.vx = offset2.vx = 0;
    pos2.vy = offset2.vy = 0;
    pos2.vz = offset2.vz = 0;
    coordLocalToWorld(coord2, &pos2);
    for (; i < 9; i++) {
        if (Actor403100_FindRegion(pos1.vx, pos1.vz) == i) {
            if (D_actor_403100_80155808->sectionDamaged[i] == 0) {
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_BREAK, coord1, 0, &offset1);
                func_dryfield_night_motel_balcony_8017E250((s16)i, 1);
                D_actor_403100_80155808->sectionDamaged[i] = 1;
            } else {
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_BREAK, coord1, 1, &offset1);
            }
        }
    }
    for (i = 3; i < 9; i++) {
        if (Actor403100_FindRegion(pos2.vx, pos2.vz) == i) {
            if (D_actor_403100_80155808->sectionDamaged[i] == 0) {
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_BREAK, coord2, 0, &offset2);
                func_dryfield_night_motel_balcony_8017E250((s16)i, 1);
                D_actor_403100_80155808->sectionDamaged[i] = 1;
            } else {
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_BREAK, coord2, 1, &offset2);
            }
        }
    }
}
static void func_actor_403100_801345E0(Task* arg0, Task* arg1)
{
    Task*     task;
    GfxCoord* coord;

    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (!(D_actor_403100_80155808->stateFrames & 0x3F)) {
        task = taskSpawnFromTable(D_actor_403100_8015560C, 1, 0, 0);
        if (task != NULL) {
            coord = task->extra.tmd->coords;
            _actor403100PlaceSpawned(coord, 0);
        }
        if (!(D_actor_403100_80155808->stateFrames & 0x3F)) {
            task = taskSpawnFromTable(D_actor_403100_8015560C, 1, 0, 0);
            if (task != NULL) {
                coord = task->extra.tmd->coords;
                _actor403100PlaceSpawned(coord, 1);
            }
            if (!(D_actor_403100_80155808->stateFrames & 0x3F)) {
                task = taskSpawnFromTable(D_actor_403100_8015560C, 1, 0, 0);
                if (task != NULL) {
                    coord = task->extra.tmd->coords;
                    _actor403100PlaceSpawned(coord, 2);
                }
                if (!(D_actor_403100_80155808->stateFrames & 0x3F)) {
                    task = taskSpawnFromTable(D_actor_403100_8015560C, 1, 0, 0);
                    if (task != NULL) {
                        coord = task->extra.tmd->coords;
                        _actor403100PlaceSpawned(coord, 3);
                    }
                    if (!(D_actor_403100_80155808->stateFrames & 0x3F)) {
                        task = taskSpawnFromTable(D_actor_403100_8015560C, 1, 0, 0);
                        if (task != NULL) {
                            coord = task->extra.tmd->coords;
                            _actor403100PlaceSpawned(coord, 4);
                        }
                    }
                }
            }
        }
    }
}

/* Rewrites a body's pass-enable bits: keeps those in `mask`, then sets `bits`. */
static __inline__ void _actor403100SetObjFlags(WorldCollisionBody* obj, s32 mask, s32 bits)
{
    obj->flags = (obj->flags & mask) | bits;
}

static void func_actor_403100_8013480C(Task* arg0, s32 arg1)
{
    SVECTOR             pos;
    WorldCollisionDelta delta;
    s32                 screen;
    s32                 flag;
    s32                 depth;
    s16                 size;
    s32                 growth;
    s32                 baseSize = arg1;
    s32                 collision;
    s32                 i;
    s32                 j;
    _Actor403100Flame*  flame;

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    for (i = 0; i < D_actor_403100_80155808->flameLifetime; i++) {
        flame = &D_actor_403100_80155814[i];
        if (flame->active == 0) {
            continue;
        }
        if (D_actor_403100_80155808->sectionDamaged[2] == 0 &&
            Actor403100_FindEffectRegion(flame->position.vx, flame->position.vz) == 2) {
            func_dryfield_night_motel_balcony_8017E250(2, 1);
            D_actor_403100_80155808->sectionDamaged[2] = 1;
        }
        pos.vx = flame->position.vx;
        pos.vy = flame->position.vy;
        pos.vz = flame->position.vz;
        gte_ldv0(&pos);
        gte_rtps();
        gte_stsxy(&screen);
        gte_stflg(&flag);
        gte_stszotz(&depth);
        if ((s16)D_actor_403100_80155808->state == 5) {
            growth = flame->age * 0xF0 + 0x90;
            size   = baseSize + growth;
        } else {
            growth = flame->age * 0x3C + 0x90;
            size   = baseSize + growth;
        }
        if (flag >= 0) {
            func_dryfield_night_motel_balcony_8017F6C8(screen, (depth << 0xC) >> 0x10, (s16)((size << 0x10 >> 1) / (depth * 4)), flame->spriteStep);
        } else {
            _actor403100SetObjFlags(&D_actor_403100_80155814[i].body, (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED), WORLD_COLLISION_BODY_GRID_ENABLED);
        }
        if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
            if (Gp_FindRec18(flame->body.context.contacts, 0) != 0) {
                for (j = 0; j < ARRAY_SIZE(flame->contacts); j++) {
                    if ((D_actor_403100_80155814[i].contacts[j].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == 0x10000 && D_actor_403100_80155810 == 0) {
                        D_actor_403100_80155810 = 0xA;
                    }
                }
            }
            collision = func_800E0C10(D_actor_403100_80155814[i].contacts, &delta, ARRAY_SIZE(flame->contacts), 0);
            if (collision == 0) {
                flame->position.vx += flame->velocity.vx;
                flame->position.vy += flame->velocity.vy;
                flame->position.vz += flame->velocity.vz;
            } else if (collision >= 0) {
                if (collision < 3) {
                    D_actor_403100_80155814[i].position.vy -= 0x100 + D_actor_403100_80155814[i].velocity.vy;
                }
            }
            actorRenderComposeCoord(&D_actor_403100_80155814[i].coord);
            flame->coord.coord.t[0]   = flame->position.vx;
            flame->coord.composeStamp = GRAPHICS_COORD_DIRTY;
            flame->coord.coord.t[1]   = flame->position.vy;
            flame->coord.coord.t[2]   = flame->position.vz;
            flame->body.radius        = size / 3;
            if (D_actor_403100_80155810 == 0) {
                _actor403100SetObjFlags(&D_actor_403100_80155814[i].body, WORLD_COLLISION_BODY_FLAGS_MASK, WORLD_COLLISION_BODY_PAIR_ENABLED);
            } else {
                _actor403100SetObjFlags(&D_actor_403100_80155814[i].body, (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED), 0);
            }
            _actor403100SetObjFlags(&D_actor_403100_80155814[i].body, WORLD_COLLISION_BODY_FLAGS_MASK, WORLD_COLLISION_BODY_GRID_ENABLED);
            Gp_ClearRec18Occupied(D_actor_403100_80155814[i].contacts);
            flame->age++;
            flame->spriteStep++;
            if (flame->age == D_actor_403100_80155808->flameLifetime) {
                flame->active = 0;
                worldCollisionUnlinkBody(&D_actor_403100_80155814[i].body);
            }
        } else {
            _actor403100SetObjFlags(&D_actor_403100_80155814[i].body, (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)), 0);
        }
    }
    if (D_actor_403100_80155810 != 0) {
        D_actor_403100_80155810--;
    }
}

/// States of the task `func_actor_403100_8013E04C` runs, by `Task::state`.
static const TaskFuncTable3 D_actor_403100_80131E70 = {
    {
        func_actor_403100_8013E6A0,
        func_actor_403100_8013E6F0,
        func_actor_403100_8013E784,
    },
};

/// States of the task `func_actor_403100_8013E0A4` runs, by `Task::state`.
static const TaskFuncTable3 D_actor_403100_80131E7C = {
    {
        func_actor_403100_8013E7C8,
        func_actor_403100_8013E88C,
        func_actor_403100_8013E920,
    },
};

static void func_actor_403100_80134D50(Task* arg0)
{
    TmdObject* object                 = arg0->extra.tmd;
    void       (*handlers[10])(Task*) = {
        func_actor_403100_8013E964,
        func_actor_403100_8013E96C,
        func_actor_403100_8013E9D8,
        func_actor_403100_8013EA60,
        func_actor_403100_8013EAD4,
        func_actor_403100_8013EB68,
        func_actor_403100_8013EBC8,
        func_actor_403100_8013EC4C,
        func_actor_403100_8013ECD0,
        func_actor_403100_8013ED48
    };
    GfxCoord* coords;
    GfxCoord* side;
    GfxCoord* center;
    s32       flash;
    s32       brightness;
    s32       timer;

    D_actor_403100_80155808 = arg0->work;
    D_actor_403100_8015580C = arg0->spawnArg2.pointer;
    handlers[(s16)D_actor_403100_80155808->state](arg0);
    _actor403100SetRootYaw(arg0);
    _actor403100ScaleRoot(arg0, D_actor_403100_80155808->sceneScale);
    func_actor_403100_8013480C(arg0, 0x96);
    coords                 = arg0->extra.tmd->coords;
    coords[8].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[7].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[6].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    side                   = &coords[4];
    center                 = &coords[3];
    actorRenderComposeCoord(&coords[8]);
    actorRenderComposeCoord(side);
    _actor403100UpdateColor(arg0, center);
    flash = D_actor_403100_80155808->shakeFrames;
    if (flash != 0) {
        if (flash >= 16) {
            brightness = rsin(gDisplayState.animFrame << 9) << 13;
        } else {
            brightness = rsin(gDisplayState.animFrame << 9) << 12;
        }
        displaySetShakeY((s8)(brightness >> 24));
        D_actor_403100_80155808->shakeFrames = (u16)D_actor_403100_80155808->shakeFrames - 1;
    } else {
        displaySetShakeY(0);
    }
    timer = D_actor_403100_80155808->bufferReleaseDelay;
    if (timer >= 0) {
        if (timer == 0) {
            tmdFreePrimitiveBuffer(object);
        }
        D_actor_403100_80155808->bufferReleaseDelay = (u16)D_actor_403100_80155808->bufferReleaseDelay - 1;
    }
}
static void func_actor_403100_8013506C(Task* arg0)
{
    GfxCoord* coord;
    u16       frame;
    s32       sound;
    s32       pan;
    s32       sound2;
    s32       pan2;

    coord                                = arg0->extra.tmd->coords;
    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    coord->coord.t[1]                    = (s16)(rcos((s16)frame * 0x20) << 0xD >> 0x10) - 0x300;
    coord->coord.t[0]                   += 0x40;
    if ((s16)D_actor_403100_80155808->stateFrames == 0x40) {
        D_actor_403100_80155808->stateFrames = 0;
        Gp_SpawnPadLerp(0x1E, 0xFF, 8);
        D_actor_403100_80155808->shakeFrames = 0x1E;
        func_dryfield_night_motel_balcony_8017E128(0);
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0001;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(sound, pan, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]) / 2));
        sound2 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0002;
        pan2   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[4]);
        sndEvtRequestScriptStart(sound2, pan2, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[4]) / 2));
        D_actor_403100_80155808->subState += 1;
    }
}
static void func_actor_403100_801351F8(Task* arg0)
{
    u16 frame;
    s32 sound;
    s32 pan;
    s32 sound2;
    s32 pan2;

    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    if ((s16)frame == 0x3E) {
        func_dryfield_night_motel_balcony_8017E128(0);
        Gp_SpawnPadLerp(0x1E, 0xFF, 8);
        D_actor_403100_80155808->shakeFrames = 0x1E;
        sound                                = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0001;
        pan                                  = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(sound, pan, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]) / 2));
        sound2 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0002;
        pan2   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[4]);
        sndEvtRequestScriptStart(sound2, pan2, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[4]) / 2));
        D_actor_403100_80155808->subState += 1;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x28) {
        D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_TRACK_FAST;
    }
    if ((s16)D_actor_403100_80155808->stateFrames >= 0x28) {
        D_actor_403100_80155808->aimTarget.vx = -0x3E8;
        D_actor_403100_80155808->aimTarget.vy = 0;
        D_actor_403100_80155808->aimTarget.vz = -0x960;
    }
}
static void func_actor_403100_8013539C(Task* arg0)
{
    GfxCoord* coord;
    s32       i;

    D_actor_403100_80155810 = 0;
    coord                   = arg0->extra.tmd->coords;
    for (i = 0; i < ARRAY_SIZE(D_actor_403100_80155814); i++) {
        if (D_actor_403100_80155814[i].active != 0) {
            D_actor_403100_80155814[i].active = 0;
            worldCollisionUnlinkBody(&D_actor_403100_80155814[i].body);
        }
    }
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 1);
    coord->coord.t[0]                         = -0x74E;
    coord->coord.t[2]                         = -0x1C51;
    coord->coord.t[1]                         = 0;
    D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
    D_actor_403100_80155808->animationId      = 2;
    D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
    D_actor_403100_80155808->rotation.vy      = 0;
    D_actor_403100_80155808->stateFrames      = 0;
    D_actor_403100_80155808->sceneScale       = 0x1910;
    func_actor_403100_801327CC(arg0);
    D_actor_403100_80155808->subState += 1;
}
static void func_actor_403100_801354A0(Task* arg0)
{
    s32 sound;
    s32 pan;
    u16 frame;

    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    if ((s16)frame == 0xC) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0001;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(sound, pan, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]) / 2));
        Gp_SpawnPadLerp(0x1E, 0xFFU, 8U);
        D_actor_403100_80155808->shakeFrames = 0x1E;
        func_dryfield_night_motel_balcony_8017E128(1);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x18) {
        func_dryfield_night_motel_balcony_8017E128(0);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x28) {
        D_actor_403100_80155808->subState += 1;
    }
    func_actor_403100_801327CC(arg0);
}
static void func_actor_403100_801355D4(Task* arg0)
{
    GfxCoord* coord;
    s32       i;

    D_actor_403100_80155810                = 0;
    coord                                  = arg0->extra.tmd->coords;
    D_actor_403100_80155808->aimMode       = ACTOR_403100_AIM_TRACK_FAST;
    D_actor_403100_80155808->flameLifetime = 0x1C;
    for (i = 0; i < ARRAY_SIZE(D_actor_403100_80155814); i++) {
        if (D_actor_403100_80155814[i].active != 0) {
            D_actor_403100_80155814[i].active = 0;
            worldCollisionUnlinkBody(&D_actor_403100_80155814[i].body);
        }
    }
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 1);
    coord->coord.t[0]                         = -0x74E;
    coord->coord.t[2]                         = -0x1770;
    coord->coord.t[1]                         = 0;
    D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
    D_actor_403100_80155808->animationId      = 7;
    D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
    D_actor_403100_80155808->rotation.vy      = 0;
    D_actor_403100_80155808->stateFrames      = 0;
    D_actor_403100_80155808->sceneScale       = 0x1400;
    func_actor_403100_801327CC(arg0);
    D_actor_403100_80155808->subState += 1;
}

static void func_actor_403100_801356F4(Task* arg0)
{
    SVECTOR first;
    SVECTOR second;
    s32     sound;
    s32     pan;

    D_actor_403100_80155808->stateFrames += 1;
    D_actor_403100_80155808->aimTarget.vx = -0xFA0;
    D_actor_403100_80155808->aimTarget.vy = 0;
    D_actor_403100_80155808->aimTarget.vz = 0x7B2;
    if ((s16)D_actor_403100_80155808->stateFrames == 1) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0004;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[4]);
        sndEvtRequestScriptStart(sound, pan, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[4]) / 2));
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x32) {
        SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 0xA);
    }
    if (D_actor_403100_80155808->stateFrames < 0x33U) {
        first.vy  = -0x1F0;
        first.vz  = 0x620;
        second.vy = -0x20;
        first.vx  = 0;
        second.vx = 0;
        second.vz = 0xF0;
        func_actor_403100_80132064(arg0, &first, &second, 1);
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->subState += 1;
    }
}
static void func_actor_403100_8013588C(Task* arg0)
{
    s32 sound;
    s32 pan;
    u16 frame;

    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    if ((s16)frame == 0xE) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0001;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(sound, pan, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]) / 2));
        Gp_SpawnPadLerp(0x1E, 0xFFU, 8U);
        D_actor_403100_80155808->shakeFrames = 0x1E;
    }
    if ((u32)(D_actor_403100_80155808->stateFrames - 0xE) < 7U) {
        func_dryfield_night_motel_balcony_8017E128(D_actor_403100_801557A8[D_actor_403100_80155808->auxFrames]);
        D_actor_403100_80155808->auxFrames += 1;
    }
    if ((s16)D_actor_403100_80155808->stateFrames >= 0x15) {
        D_actor_403100_80155808->subState += 1;
    }
}
static void func_actor_403100_801359DC(Task* arg0)
{
    GfxCoord*        coord;
    GfxCoord*        joint;
    s32              i;
    Actor403100Work* work;
    s32              value;

    value                   = 0x10;
    D_actor_403100_80155810 = 0;
    coord                   = arg0->extra.tmd->coords;
    joint                   = &coord[6];
    for (i = 0; i < ARRAY_SIZE(D_actor_403100_80155814); i++) {
        if (D_actor_403100_80155814[i].active != 0) {
            D_actor_403100_80155814[i].active = 0;
            worldCollisionUnlinkBody(&D_actor_403100_80155814[i].body);
        }
    }
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 1);
    joint->coord.t[0]      = -0x807;
    coord->coord.t[0]      = -0x384;
    work                   = D_actor_403100_80155808;
    coord->coord.t[2]      = 0x1130;
    coord->coord.t[1]      = 0;
    work->animationRate    = value;
    work->animationId      = 0x13;
    work->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
    work->stateFrames      = 0;
    work->sceneScale       = 0x1400;
    work->subState        += 1;
    Gp_ApplyAreaRecs(D_dryfield_night_motel_balcony_8018F2CC);
}
static void func_actor_403100_80135AE0(Task* arg0)
{
    s32 sound;
    s32 pan;
    u16 angle;

    angle                                = (u16)D_actor_403100_80155808->rotation.vy;
    D_actor_403100_80155808->rotation.vy = angle + ((s16)(-0x4000 - angle * 0x10) >> 9);
    if ((s16)D_actor_403100_80155808->stateFrames == 0xBE) {
        sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0001;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(sound, (s32)pan, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]) / 2));
        Gp_SpawnPadLerp(0x1E, 0xFFU, 8U);
        D_actor_403100_80155808->shakeFrames = 0x1E;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0xF0) {
        D_actor_403100_80155808->subState += 1;
    }
    D_actor_403100_80155808->stateFrames += 1;
}

static void func_actor_403100_80135C00(Task* arg0)
{
    s32 sound;
    s32 sound_2;
    s32 sound_3;
    s32 sound_4;
    s32 pan;
    s32 pan_2;
    s32 pan_3;
    s32 pan_4;
    u16 frame;
    s32 depth;
    s32 depth_2;
    s32 depth_3;
    s32 depth_4;

    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    if ((s16)frame == 0x1E) {
        func_dryfield_night_motel_balcony_8018257C();
        sound = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0004;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[4]);
        depth = worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[4]);
        sndEvtRequestScriptStart(sound, (s32)pan, (s8)(depth / 2));
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x22) {
        SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 0xA);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x28) {
        func_dryfield_night_motel_balcony_8018257C();
        sound_2 = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0004;
        pan_2   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[4]);
        depth_2 = worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[4]);
        sndEvtRequestScriptStart(sound_2, (s32)pan_2, (s8)(depth_2 / 2));
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x30) {
        SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 0xA);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x3C) {
        func_dryfield_night_motel_balcony_8018257C();
        sound_3 = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0004;
        pan_3   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[4]);
        depth_3 = worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[4]);
        sndEvtRequestScriptStart(sound_3, (s32)pan_3, (s8)(depth_3 / 2));
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x3F) {
        SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 0xA);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x5A) {
        func_dryfield_night_motel_balcony_8018257C();
        sound_4 = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0004;
        pan_4   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[4]);
        depth_4 = worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[4]);
        sndEvtRequestScriptStart(sound_4, (s32)pan_4, (s8)(depth_4 / 2));
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x5E) {
        SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 0xA);
    }
    if (Actor403100_TestFlags12C()) {
        D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
        D_actor_403100_80155808->animationId      = 0x15;
        D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
        D_actor_403100_80155808->stateFrames      = 0;
        D_actor_403100_80155808->subState        += 1;
    }
}
static void func_actor_403100_80135F30(Task* arg0)
{
    s32        sound;
    s32        sound2;
    s32        pan;
    s32        pan2;
    u16        frame;
    s32        depth;
    s32        depth2;
    TmdObject* obj;

    obj                                  = arg0->extra.tmd;
    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    if ((s16)frame == 0xA) {
        func_dryfield_night_motel_balcony_8018257C();
        sound = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0004;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[4]);
        depth = worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[4]);
        sndEvtRequestScriptStart(sound, (s32)pan, (s8)(depth / 2));
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0xE) {
        SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 0xA);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x26) {
        sound2 = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0002;
        pan2   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[4]);
        depth2 = worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[4]);
        sndEvtRequestScriptStart(sound2, (s32)pan2, (s8)(depth2 / 2));
    }
    if (Actor403100_TestFlags12C()) {
        D_actor_403100_80155808->animationBlendFrames = 0x18;
        D_actor_403100_80155808->animationRate        = 0x18;
        D_actor_403100_80155808->animationId          = 0x16;
        D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
        D_actor_403100_80155808->stateFrames          = 0;
        D_actor_403100_80155808->stridePhase          = 0;
        D_actor_403100_80155808->subState            += 1;
        obj->otOffset                                 = 0;
    }
}
static void func_actor_403100_80136100(Task* arg0)
{
    s16       angle;
    s32       sound;
    s32       y;
    s32       z;
    s32       pan;
    u16       frame;
    s32       depth;
    GfxCoord* coord;

    coord                                 = arg0->extra.tmd->coords;
    angle                                 = ((u16)D_actor_403100_80155808->stridePhase + 0x20) & 0x7FF;
    D_actor_403100_80155808->stateFrames += 1;
    D_actor_403100_80155808->stridePhase  = angle;
    y                                     = -((s32)(rsin((s32)angle) << 0xD) >> 0x10);
    coord->coord.t[1]                     = y;
    if (y == 0) {
        sound = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0001;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        depth = worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(sound, (s32)pan, (s8)(depth / 2));
        Gp_SpawnPadLerp(0x1E, 0xFFU, 8U);
        D_actor_403100_80155808->shakeFrames = 0x1E;
    }
    if ((s16)D_actor_403100_80155808->stateFrames < 0x11) {
        D_actor_403100_80155808->rotation.vy = (u16)D_actor_403100_80155808->rotation.vy - 0x34;
        coord->coord.t[2]                    = (s32)(coord->coord.t[2] - 0x10);
        coord->coord.t[0]                    = (s32)(coord->coord.t[0] - 8);
    }
    frame = D_actor_403100_80155808->stateFrames;
    if ((u32)(frame - 0x11) < 0xAU) {
        D_actor_403100_80155808->rotation.vy = (u16)D_actor_403100_80155808->rotation.vy - 0x10;
        z                                    = coord->coord.t[2] - 0x14;
    } else if ((u16)(frame - 0x1B) < 0xAU) {
        D_actor_403100_80155808->rotation.vy = (u16)D_actor_403100_80155808->rotation.vy - 0x10;
        z                                    = coord->coord.t[2] - 0x20;
    } else {
        if ((u16)(frame - 0x25) < 0xAU) {
            D_actor_403100_80155808->rotation.vy -= 0x10;
        } else if ((u16)(frame - 0x2F) < 0xAU) {
            D_actor_403100_80155808->rotation.vy -= 0xC;
        } else if ((u16)(frame - 0x39) < 0xAU) {
            D_actor_403100_80155808->rotation.vy -= 8;
        }
        z = coord->coord.t[2] - 0x40;
    }
    coord->coord.t[2] = z;
}
static void func_actor_403100_8013631C(Task* arg0)
{
    s16       frame;
    s16       step;
    s32       sound;
    s32       pan;
    s32       depth;
    GfxCoord* coord;

    frame                                = D_actor_403100_80155808->stateFrames + 1;
    coord                                = arg0->extra.tmd->coords;
    D_actor_403100_80155808->stateFrames = (u16)frame;
    D_actor_403100_80155808->auxFrames  += 1;
    D_actor_403100_80155808->stridePhase = (u16)((D_actor_403100_80155808->stridePhase + 0x20) & 0x7FF);
    if (D_actor_403100_80155808->auxFrames < 0xA) {
        coord->coord.t[2] -= 0x6;
    } else if (D_actor_403100_80155808->auxFrames < 0x14) {
        coord->coord.t[2] -= 0xA;
    } else if (D_actor_403100_80155808->auxFrames < 0x1E) {
        coord->coord.t[2] -= 0x12;
    } else if (D_actor_403100_80155808->auxFrames < 0x28) {
        coord->coord.t[2] -= 0x6;
    } else if (D_actor_403100_80155808->auxFrames < 0x32) {
        coord->coord.t[2] -= 0x6;
    } else if (D_actor_403100_80155808->auxFrames < 0x3C) {
        coord->coord.t[2] -= 0xA;
    } else if (D_actor_403100_80155808->auxFrames < 0x46) {
        coord->coord.t[2] -= 0x12;
    } else if (D_actor_403100_80155808->auxFrames < 0x5D) {
        coord->coord.t[2] -= 0x4;
    } else if (D_actor_403100_80155808->auxFrames < 0x69) {
        coord->coord.t[2] -= 0x8;
    } else if (D_actor_403100_80155808->auxFrames < 0x75) {
        coord->coord.t[2] -= 0x10;
    } else if (D_actor_403100_80155808->auxFrames < 0x81) {
        coord->coord.t[2] -= 0x28;
    } else if (D_actor_403100_80155808->auxFrames < 0x92) {
        coord->coord.t[2] -= 0x18;
    } else if (D_actor_403100_80155808->auxFrames < 0x9C) {
        coord->coord.t[2] -= 0x6;
    } else if (D_actor_403100_80155808->auxFrames < 0xA6) {
        coord->coord.t[2] -= 0xA;
    } else if (D_actor_403100_80155808->auxFrames < 0xB0) {
        coord->coord.t[2] -= 0x12;
    } else if (D_actor_403100_80155808->auxFrames < 0xBA) {
        coord->coord.t[2] -= 0x6;
    } else if (D_actor_403100_80155808->auxFrames < 0xC4) {
        coord->coord.t[2] -= 0x6;
    }
    step = (s16)D_actor_403100_80155808->auxFrames;
    if ((step == 0x2D) || (step == 0xE6) || (step == 0xB4) || (step == 0x57)) {
        sound = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0001;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        depth = worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(sound, pan, (s8)(depth / 2));
        Gp_SpawnPadLerp(0x1E, 0xFF, 8);
        D_actor_403100_80155808->shakeFrames = 0x1E;
    }
    if (Actor403100_TestFlags12C()) {
        D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
        D_actor_403100_80155808->animationId      = 0x18;
        D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
        D_actor_403100_80155808->auxFrames        = 0U;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x118) {
        D_actor_403100_80155808->subState = (u16)(D_actor_403100_80155808->subState + 1);
    }
}
static void func_actor_403100_80136610(Task* arg0)
{
    Actor403100Work* work;
    TmdObject*       obj;
    Enemy*           enemy;
    s32              i;
    u16*             flags;
    s32              kind;
    GfxCoord*        coord;

    obj   = arg0->extra.tmd;
    coord = obj->coords;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_KEY(0xFF, 0xFF, 0xFF, 0)) != GAME_LOCATION_KEY(3, 29, 2, 0) ||
        (arg0->work = memCalloc(sizeof(Actor403100Work), false)) == NULL) {
        enemyDestroy(D_actor_403100_8015580C, arg0);
        return;
    }
    enemy                               = arg0->spawnArg2.pointer;
    D_actor_403100_8015580C             = enemy;
    work                                = arg0->work;
    obj->lightMtx                       = &work->light;
    D_actor_403100_80155808             = work;
    obj->colorMtx                       = &work->color;
    arg0->msgTable                      = D_actor_403100_801556EC;
    work->savedView                     = gGameSession->location.loc.view;
    enemy->field_48                     = 0;
    enemy->field_4                      = &coord->coord;
    D_actor_403100_8015580C->bodyPos.vx = 0;
    D_actor_403100_8015580C->bodyPos.vy = 0;
    D_actor_403100_8015580C->bodyPos.vz = 0x300;
    D_actor_403100_8015580C->coord      = &arg0->extra.tmd->coords[3];
    Gp_LinkNode(&D_actor_403100_8015580C->node);
    kind = 9;
    TOUCH_REG(kind);
    D_actor_403100_8015580C->node.state.parts.flags = kind;
    D_actor_403100_8015580C->param                  = &D_actor_403100_8014762C;
    D_actor_403100_8015580C->recs                   = D_actor_403100_80155808->hitContacts;
    D_actor_403100_80155630.coord                   = arg0->extra.tmd->coords;
    flags                                           = &obj->flags;
    *flags                                          = 0;
    D_actor_403100_80155808->bufferReleaseDelay     = -1;
    animationInitContext(&D_actor_403100_80155808->rig.anim, D_actor_403100_8015572C, obj, D_actor_403100_80155808->rig.poses, D_actor_403100_80155808->rig.slots);
    D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
    D_actor_403100_80155808->animationId      = 1;
    D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
    func_actor_403100_801327CC(arg0);
    coord->parent = &gGfxViewCoord;
    func_actor_403100_80132320(arg0);
    for (i = ARRAY_SIZE(D_actor_403100_80155814) - 1; i >= 0; i--) {
        D_actor_403100_80155814[i].active = 0;
    }
    func_dryfield_night_motel_balcony_8017E4B8();
    func_dryfield_night_motel_balcony_8017E3C8();
    D_actor_403100_80155810 = 0;
    {
        Enemy* activeEnemy = D_actor_403100_8015580C;
        u16    hp          = D_actor_403100_8014762C.hpMax;
        activeEnemy->hpMax = hp;
        activeEnemy->hp    = (s16)hp;
    }
    arg0->state                       = 1;
    D_actor_403100_80155808->state    = 0;
    D_actor_403100_80155808->subState = 0;
}
/// Steps of the behaviour mode `func_actor_403100_8013E96C`, indexed by `subState`.
static const TaskFuncTable3 D_actor_403100_80131EB0 = {
    {
        func_actor_403100_8013ED50,
        func_actor_403100_8013506C,
        func_actor_403100_8013EDDC,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013E9D8`, indexed by `subState`.
static const TaskFuncTable3 D_actor_403100_80131EBC = {
    {
        func_actor_403100_8013EE28,
        func_actor_403100_801351F8,
        func_actor_403100_8013EEB0,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013EA60`, indexed by `subState`.
static const TaskFuncTable4 D_actor_403100_80131EC8 = {
    {
        func_actor_403100_8013539C,
        func_actor_403100_8013EEB8,
        func_actor_403100_801354A0,
        func_actor_403100_8013EF24,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013EAD4`, indexed by `subState`.
static const TaskFuncTable3 D_actor_403100_80131ED8 = {
    {
        func_actor_403100_801355D4,
        func_actor_403100_801356F4,
        func_actor_403100_8013EF2C,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013EB68`, indexed by `subState`.
static const TaskFuncTable3 D_actor_403100_80131EE4 = {
    {
        func_actor_403100_8013EF34,
        func_actor_403100_8013588C,
        func_actor_403100_8013EF58,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013EBC8`, indexed by `subState`.
static const TaskFuncTable4 D_actor_403100_80131EF0 = {
    {
        func_actor_403100_801359DC,
        func_actor_403100_80135AE0,
        func_actor_403100_8013EF60,
        func_actor_403100_8013EFC0,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013EC4C`, indexed by `subState`.
static const TaskFuncTable4 D_actor_403100_80131F00 = {
    {
        func_actor_403100_8013EFC8,
        func_actor_403100_80135C00,
        func_actor_403100_80135F30,
        func_actor_403100_80136100,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013ECD0`, indexed by `subState`.
static const TaskFuncTable3 D_actor_403100_80131F10 = {
    {
        func_actor_403100_8013F034,
        func_actor_403100_8013631C,
        func_actor_403100_8013F0A8,
    },
};

/// The actor's six top-level states, by `Task::state`; run by
/// `func_actor_403100_8013E0FC`.
static const TaskFuncTable6 D_actor_403100_80131F1C = {
    {
        func_actor_403100_80136610,
        func_actor_403100_80134D50,
        func_actor_403100_80136830,
        func_actor_403100_801339EC,
        func_actor_403100_8013D8F4,
        func_actor_403100_8013D88C,
    },
};

/// Behaviour modes of the actor's main state, indexed by `state`. Each
/// mode steps through its own table by `subState`.
static const TaskFuncTable11 D_actor_403100_80131F34 = {
    {
        func_actor_403100_8013DA6C,
        func_actor_403100_8013DAC4,
        func_actor_403100_8013DB48,
        func_actor_403100_8013DC18,
        func_actor_403100_8013DCAC,
        func_actor_403100_8013DD78,
        func_actor_403100_8013DE0C,
        func_actor_403100_8013DEA0,
        func_actor_403100_8013DF0C,
        func_actor_403100_8013DF64,
        func_actor_403100_8013DFBC,
    },
};

/// Runs the per-frame hook `D_actor_403100_80131E24` selects by `playerReactionStage`.
static inline void _actor403100RunHook(void)
{
    _Actor403100PlayerReactionTable hooks = D_actor_403100_80131E24;

    hooks.handlers[D_actor_403100_80155808->playerReactionStage]();
}

/// Adds the angles of `forearmTurn` to the rotation of
/// model part 6 and updates the part.
static inline void _actor403100TurnPart6(Task* task)
{
    SVECTOR   angles;
    MATRIX    rotation;
    MATRIX*   dest;
    GfxCoord* coords;

    coords               = task->extra.tmd->coords + 6;
    dest                 = &coords->coord;
    coords->composeStamp = GRAPHICS_COORD_DIRTY;
    gfxSetRotIdentity(&rotation);
    Gp_MtxToEuler(dest, &angles);
    angles.vz += D_actor_403100_80155808->forearmTurn.vz;
    angles.vy += D_actor_403100_80155808->forearmTurn.vy;
    angles.vx += D_actor_403100_80155808->forearmTurn.vx;
    RotMatrix(&angles, &rotation);
    dest->m[0][0] = rotation.m[0][0];
    dest->m[0][1] = rotation.m[0][1];
    dest->m[0][2] = rotation.m[0][2];
    dest->m[1][0] = rotation.m[1][0];
    dest->m[1][1] = rotation.m[1][1];
    dest->m[1][2] = rotation.m[1][2];
    dest->m[2][0] = rotation.m[2][0];
    dest->m[2][1] = rotation.m[2][1];
    dest->m[2][2] = rotation.m[2][2];
    actorRenderComposeCoord(coords);
}

/// Adds `jawPitchOffset` to the X angle of model part 4 and `headPitchOffset` to that of
/// part 3, reusing one workspace for both.
static inline void _actor403100PitchArms(Task* task)
{
    SVECTOR   angles;
    MATRIX    rotation;
    MATRIX*   dest;
    GfxCoord* coords;

    coords = task->extra.tmd->coords;
    gfxSetRotIdentity(&rotation);
    dest = &coords[4].coord;
    Gp_MtxToEuler(dest, &angles);
    angles.vx += D_actor_403100_80155808->jawPitchOffset;
    RotMatrix(&angles, &rotation);
    dest->m[0][0] = rotation.m[0][0];
    dest->m[0][1] = rotation.m[0][1];
    dest->m[0][2] = rotation.m[0][2];
    dest->m[1][0] = rotation.m[1][0];
    dest->m[1][1] = rotation.m[1][1];
    dest->m[1][2] = rotation.m[1][2];
    dest->m[2][0] = rotation.m[2][0];
    dest->m[2][1] = rotation.m[2][1];
    dest->m[2][2] = rotation.m[2][2];

    coords = task->extra.tmd->coords;
    gfxSetRotIdentity(&rotation);
    dest = &coords[3].coord;
    Gp_MtxToEuler(dest, &angles);
    angles.vx += D_actor_403100_80155808->headPitchOffset;
    RotMatrix(&angles, &rotation);
    dest->m[0][0] = rotation.m[0][0];
    dest->m[0][1] = rotation.m[0][1];
    dest->m[0][2] = rotation.m[0][2];
    dest->m[1][0] = rotation.m[1][0];
    dest->m[1][1] = rotation.m[1][1];
    dest->m[1][2] = rotation.m[1][2];
    dest->m[2][0] = rotation.m[2][0];
    dest->m[2][1] = rotation.m[2][1];
    dest->m[2][2] = rotation.m[2][2];
}

static void func_actor_403100_80136830(Task* arg0)
{
    TaskFuncTable11 stateHandlers;
    PlayerStatus*   config = &gPlayerStatus;
    s32             flashTimer;
    s32             scale;
    s16             lightTimer;
    s16             armTimer;
    s32             countdown;
    s32             flash;
    GfxCoord*       side;
    Task*           player;
    GfxCoord*       coordinates;
    GfxCoord*       center;
    TmdObject*      obj;
    GfxCoord*       playerCoord;

    obj           = arg0->extra.tmd;
    player        = *gPlayerActorTasks;
    stateHandlers = D_actor_403100_80131F34;
    armTimer      = D_actor_403100_80155808->engageDelay;
    if (armTimer != 0) {
        if ((armTimer == 1) && (D_actor_403100_8015580C->hp > 0)) {
            Gp_ArmStateF0(1);
        }
        D_actor_403100_80155808->engageDelay = (s16)((u16)D_actor_403100_80155808->engageDelay - 1);
    }
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (player == NULL) {
                D_actor_403100_80155808->playerRegion = 0;
            }
            func_actor_403100_801331D4(arg0);
            countdown = D_actor_403100_80155808->fightFramesLeft;
            if (countdown >= 0) {
                D_actor_403100_80155808->fightFramesLeft = (s32)(countdown - 1);
            }
            if ((config->hp > 0) && (D_actor_403100_80155808->playerReactionStage == ACTOR_403100_PLAYER_REACTION_NONE)) {
                if (D_actor_403100_8015580C->hp < (s16)(((s16)D_actor_403100_8015580C->hpMax * 0x23) / 100)) {
                    D_actor_403100_80155808->lowHealth = 1U;
                } else {
                    D_actor_403100_80155808->lowHealth = 0U;
                }
            }
            playerCoord                           = player->extra.tmd->coords;
            D_actor_403100_80155808->playerRegion = func_actor_403100_8013D9C4((s16)playerCoord->coord.t[0], (s16)playerCoord->coord.t[2], D_actor_403100_80155638);
            _actor403100RunHook();
            stateHandlers.funcs[(s16)D_actor_403100_80155808->state](arg0);
            if ((D_actor_403100_80155808->lowHealth != 0) && (D_actor_403100_80155808->phaseChangeDone == 0) && (D_actor_403100_80155808->holdingPlayer == 0)) {
                D_actor_403100_80155808->state    = 9;
                D_actor_403100_80155808->subState = 0;
            }
            func_actor_403100_8013CBE0(arg0);
            func_actor_403100_8013CDC0();
            func_actor_403100_8013BA64(arg0);
            func_actor_403100_801327CC(arg0);
            flashTimer                                      = D_actor_403100_80155808->shakeFrames;
            D_actor_403100_80155808->previousAnimationFlags = D_actor_403100_80155808->rig.slots[1].status.fields.flags;
            if (flashTimer != 0) {
                if (flashTimer >= 0x10) {
                    flash = rsin(gDisplayState.animFrame << 9) << 0xD;
                } else {
                    flash = rsin(gDisplayState.animFrame << 9) << 0xC;
                }
                displaySetShakeY(flash >> 0x18);
                D_actor_403100_80155808->shakeFrames = (s16)((u16)D_actor_403100_80155808->shakeFrames - 1);
            } else {
                displaySetShakeY(0);
            }
            func_actor_403100_8013B5E0(arg0, D_actor_403100_80155808->aimMode);
            _actor403100SetRootYaw(arg0);
            scale = 0x1400;
            _actor403100ScaleRoot(arg0, scale);
            func_actor_403100_801328DC(arg0);
            _actor403100TurnPart6(arg0);
            _actor403100PitchArms(arg0);
            func_actor_403100_8013335C(arg0);
            if (D_actor_403100_80155808->hitColorFrames != 0) {
                lightTimer = --D_actor_403100_80155808->hitColorFrames;
                if ((s16)lightTimer == 0) {
                    Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
                }
            }
            if ((D_actor_403100_8015580C->hp <= 0) && (D_actor_403100_80155808->vulnerable == 0) && (D_actor_403100_80155808->playerReactionStage == ACTOR_403100_PLAYER_REACTION_NONE)) {
                if (config->hp <= 0) {
                    D_actor_403100_8015580C->hp = 0x3E8;
                } else {
                    D_actor_403100_80155808->bufferReleaseDelay = -1;
                    D_actor_403100_80155808->sceneScale         = 0x1400;
                    gGameSession->suppressDeathChecks           = 0;
                    Gp_StateC08.flags                           = (u8)(Gp_StateC08.flags | ATTACHMENT_FLAG_EVENT_LOCK);
                    gGameSession->suppressViewTriggers          = 0;
                    D_actor_403100_8015580C->reactionFlags      = 0;
                    Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
                    D_actor_403100_8015580C->node.state.parts.flags = (WORLD_TARGET_HIDE_HP | WORLD_TARGET_NOT_LOCKABLE);
                    func_800E8614(D_actor_335800_80166098, 0);
                    arg0->state                       = 1;
                    D_actor_403100_80155808->state    = 0;
                    D_actor_403100_80155808->subState = 0;
                    D_actor_403100_80155808->state    = 9;
                    D_actor_403100_80155808->subState = 0;
                    case 1:
                }
            }
            func_actor_403100_8013480C(arg0, 0x96);
            coordinates                 = arg0->extra.tmd->coords;
            coordinates[8].composeStamp = GRAPHICS_COORD_DIRTY;
            coordinates[7].composeStamp = GRAPHICS_COORD_DIRTY;
            coordinates[6].composeStamp = GRAPHICS_COORD_DIRTY;
            coordinates[5].composeStamp = GRAPHICS_COORD_DIRTY;
            coordinates[4].composeStamp = GRAPHICS_COORD_DIRTY;
            coordinates[3].composeStamp = GRAPHICS_COORD_DIRTY;
            coordinates[2].composeStamp = GRAPHICS_COORD_DIRTY;
            coordinates[1].composeStamp = GRAPHICS_COORD_DIRTY;
            coordinates[0].composeStamp = GRAPHICS_COORD_DIRTY;
            side                        = coordinates + 4;
            center                      = coordinates + 3;
            actorRenderComposeCoord(coordinates + 8);
            actorRenderComposeCoord(side);
            _actor403100UpdateColor(arg0, center);
            obj->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
}
static void func_actor_403100_8013712C(Task* arg0)
{
    _Actor403100Flame*  flames;
    WorldCollisionBody* body;
    s32                 i;
    GfxCoord*           coord;
    Actor403100Work*    work;

    coord                                           = arg0->extra.tmd->coords;
    D_actor_403100_8015580C->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
    D_actor_403100_80155808->engageDelay            = 0x1E;
    D_actor_403100_80155808->walkSpeed              = 0x20;
    D_actor_403100_80155808->stridePhase            = 0;
    (Gp_IncStateF0Ref)(0);
    i                                        = 0;
    flames                                   = D_actor_403100_80155814;
    body                                     = &flames->body;
    work                                     = *(Actor403100Work* volatile*)&D_actor_403100_80155808;
    coord->coord.t[0]                        = -0x44C;
    coord->coord.t[2]                        = 0x980;
    *(volatile s16*)&D_actor_403100_80155810 = 0;
    coord->coord.t[1]                        = 0;
    work->rotation.vy                        = 0xC00;
    work->fightFramesLeft                    = 0x1518;
    work->animationRate                      = ANIMATION_RATE_ONE;
    work->animationId                        = 1;
    work->rotation.vx                        = 0;
    work->rotation.vz                        = 0;
    work->animationRequest                   = ACTOR_403100_ANIMATION_REQUEST_RESET;
    work->stateFrames                        = 0;
    for (; i < ARRAY_SIZE(D_actor_403100_80155814); i++) {
        if (flames[i].active != 0) {
            flames[i].active = 0;
            worldCollisionUnlinkBody(body);
        }
        /* The collision body walks as its own pointer, one flame at a time:
           spelled `&flames[i].body` it folds into the walk of `flames[i]`,
           and the ROM keeps the two apart. */
        body = &(PARENT_OF(body, _Actor403100Flame, body) + 1)->body;
    }
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 1);
    D_actor_403100_80155808->subState += 1;
}
/// Steps of the behaviour mode `func_actor_403100_8013DAC4`, indexed by `subState`.
static const TaskFuncTable3 D_actor_403100_80131F60 = {
    {
        func_actor_403100_8013F18C,
        func_actor_403100_80137268,
        func_actor_403100_80137310,
    },
};

static void func_actor_403100_80137268(Task* task)
{
    s16 state;

    state = D_actor_403100_80155808->playerRegion - 1;
    switch (state) {
        case 0:
            if (D_actor_403100_80155808->playerDistance < 0x1F40) {
                D_actor_403100_80155808->subState += 1;
            }
            break;
        case 1:
        case 5:
            D_actor_403100_80155808->subState += 1;
            break;
        case 2:
        case 3:
        case 4:
            if (D_actor_403100_80155808->playerDistance < 0x17D4) {
                D_actor_403100_80155808->subState += 1;
            }
            break;
    }
}
static void func_actor_403100_80137310(Task* task)
{
    s32 halfHealth;
    s16 phase;
    s16 previousState;
    s16 state;
    u32 random1;
    u32 random2;

    halfHealth = gPlayerStatus.hpMax / 2;
    phase      = D_actor_403100_80155808->playerRegion;
    if (phase != 2 && phase != 6) {
        if ((gPlayerStatus.hp < halfHealth) || (D_actor_403100_80155808->lowHealth != 0)) {
            random1                           = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState                   = random1;
            D_actor_403100_80155808->state    = D_actor_403100_801557B0.states[ACTOR_403100_ATTACK_ODDS_WEAKENED][(random1 >> 16) & (ACTOR_403100_ATTACK_DRAWS - 1)];
            D_actor_403100_80155808->subState = 0;
        } else {
            random2                           = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState                   = random2;
            D_actor_403100_80155808->state    = D_actor_403100_801557B0.states[ACTOR_403100_ATTACK_ODDS_NORMAL][(random2 >> 16) & (ACTOR_403100_ATTACK_DRAWS - 1)];
            D_actor_403100_80155808->subState = 0;
        }
        if ((s16)D_actor_403100_80155808->state == 4) {
            if (D_actor_403100_80155808->lowHealth != 0) {
                D_actor_403100_80155808->state    = 2;
                D_actor_403100_80155808->subState = 0;
            }
        }
        if ((s16)D_actor_403100_80155808->state == 5) {
            if (D_actor_403100_80155808->playerRegion != 1) {
                D_actor_403100_80155808->state    = 7;
                D_actor_403100_80155808->subState = 0;
            }
        }
        if (((s16)D_actor_403100_80155808->state == 7) && ((u32)((u16)D_actor_403100_80155808->playerRegion - 3) >= 2U)) {
            D_actor_403100_80155808->state    = 4;
            D_actor_403100_80155808->subState = 0;
        }
        D_actor_403100_80155808->recentStates[D_actor_403100_80155808->recentStateCursor] = (u16)D_actor_403100_80155808->state;
        previousState                                                                     = D_actor_403100_80155808->recentStates[0];
        if ((previousState == D_actor_403100_80155808->recentStates[1]) && (previousState == D_actor_403100_80155808->recentStates[2])) {
            state = (s16)D_actor_403100_80155808->state;
            if (state == 2 || state == 4 || state == 7) {
                D_actor_403100_80155808->state    = 3;
                D_actor_403100_80155808->subState = 0;
            } else if (state == 3) {
                if (D_actor_403100_80155808->lowHealth != 0) {
                    D_actor_403100_80155808->state    = 2;
                    D_actor_403100_80155808->subState = 0;
                } else {
                    D_actor_403100_80155808->state    = 4;
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
        D_actor_403100_80155808->state    = 6;
        D_actor_403100_80155808->subState = 0;
    }
}
static void func_actor_403100_801375B8(Task* task)
{
    u16 angle;
    u32 random2;
    u32 random1;

    random1                               = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    random2                               = (random1 * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
    gRandomLcgState                       = random2;
    D_actor_403100_80155808->stateCounter = ((random1 >> 0x10) & 1) + 1 + ((random2 >> 0x10) & 1);
    D_actor_403100_80155808->walkSpeed    = 0x20;
    angle                                 = (u16)D_actor_403100_80155808->headAim.vy;
    D_actor_403100_80155808->walkStage    = 0;
    D_actor_403100_80155808->aimMode      = ACTOR_403100_AIM_YAW_ONLY_FAST;
    D_actor_403100_80155808->armPitch     = 0;
    D_actor_403100_80155808->armYaw       = 0;
    D_actor_403100_80155808->armYawTarget = (s16)angle;
    if ((s16)angle >= 0xD1) {
        D_actor_403100_80155808->armYawTarget = 0xD0;
    }
    if (D_actor_403100_80155808->armYawTarget < -0x160) {
        D_actor_403100_80155808->armYawTarget = -0x160;
    }
    D_actor_403100_80155808->animationId          = 4;
    D_actor_403100_80155808->animationRate        = 0xC;
    D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_RESET;
    D_actor_403100_80155808->stateFrames          = 0;
    D_actor_403100_80155808->handAttack.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    D_actor_403100_80155808->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    D_actor_403100_80155808->handTouchedPlayer    = 0;
    D_actor_403100_80155808->forearmTouchedPlayer = 0;
    D_actor_403100_80155808->subState            += 1;
    D_actor_403100_80155808->jawPitchPhase        = ACTOR_403100_PITCH_PHASE_REST;
    D_actor_403100_80155808->headPitchPhase       = ACTOR_403100_PITCH_PHASE_REST;
    D_actor_403100_80155808->swingConnected       = 0;
}
static void func_actor_403100_801376D8(Task* arg0)
{
    Task* task;
    s32   sound;
    s32   pan;
    u16   counter;
    u16   angle;

    if ((u32)(D_actor_403100_80155808->stateFrames - 0x36) < 4U) {
        func_actor_403100_8013C7B4(arg0);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x39) {
        func_actor_403100_801342B4(arg0);
        sound = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0003;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[7]);
        sndEvtRequestScriptStart(sound, pan, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[7]) / 2));
        Gp_SpawnPadLerp(0x12, 0xFFU, 8U);
        D_actor_403100_80155808->shakeFrames = 0x12;
    }
    D_actor_403100_80155808->stateFrames += 1;
    if (D_actor_403100_80155808->handTouchedPlayer != 0 || D_actor_403100_80155808->forearmTouchedPlayer != 0) {
        D_actor_403100_80155808->walkStage = 4;
        if (D_actor_403100_80155808->playerReactionStage == ACTOR_403100_PLAYER_REACTION_NONE) {
            Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
            func_actor_403100_8013D1B8(5, 0x3F4);
            D_actor_403100_80155808->playerReactionFrames = 0x17;
            D_actor_403100_80155808->playerReactionStage  = ACTOR_403100_PLAYER_REACTION_HIT_HELD;
            task                                          = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            if (taskMessageDispatch(task, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackPair(D_actor_403100_80147614, 0), 0) == 1) {
                ((GameActor*)(*gPlayerActorTasks)->work)->state = 0xA;
            }
        }
        D_actor_403100_80155808->handTouchedPlayer    = 0;
        D_actor_403100_80155808->forearmTouchedPlayer = 0;
        D_actor_403100_80155808->swingConnected       = 1;
        func_actor_403100_8013D2A0(1);
    }
    D_actor_403100_80155808->armYaw += (s16)((D_actor_403100_80155808->armYawTarget - D_actor_403100_80155808->armYaw) << 4) >> 7;
    if (Actor403100_TestFlags104()) {
        if (D_actor_403100_80155808->swingConnected != 0) {
            D_actor_403100_80155808->animationRate        = ANIMATION_RATE_ONE;
            D_actor_403100_80155808->animationId          = 5;
            D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_RESET;
            D_actor_403100_80155808->subState            += 2;
            D_actor_403100_80155808->handAttack.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            D_actor_403100_80155808->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            return;
        }
        counter                                                    = D_actor_403100_80155808->subState;
        *(volatile s16*)&D_actor_403100_80155808->animationRate    = 0xA;
        *(volatile s16*)&D_actor_403100_80155808->animationId      = 2;
        *(volatile s16*)&D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
        angle                                                      = *(volatile u16*)&D_actor_403100_80155808->headAim.vy;
        D_actor_403100_80155808->stateFrames                       = 0;
        D_actor_403100_80155808->armYawTarget                      = (s16)angle;
        D_actor_403100_80155808->subState                          = counter + 1;
        if ((s16)angle >= 0xD1) {
            D_actor_403100_80155808->armYawTarget = 0xD0;
        }
        if (D_actor_403100_80155808->armYawTarget < -0x160) {
            D_actor_403100_80155808->armYawTarget = -0x160;
        }
    }
}
static void func_actor_403100_801379B4(Task* arg0)
{
    Task* task;
    s32   sound;
    s32   pan;
    s8    count;
    u16   angle;

    if ((u32)(D_actor_403100_80155808->stateFrames - 0x3C) < 9U) {
        func_actor_403100_8013C7B4(arg0);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x44) {
        func_actor_403100_801342B4(arg0);
        sound = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0003;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[7]);
        sndEvtRequestScriptStart(sound, pan, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[7]) / 2));
        Gp_SpawnPadLerp(0x12, 0xFFU, 8U);
        D_actor_403100_80155808->shakeFrames = 0x12;
    }
    D_actor_403100_80155808->stateFrames += 1;
    if (D_actor_403100_80155808->handTouchedPlayer != 0 || D_actor_403100_80155808->forearmTouchedPlayer != 0) {
        D_actor_403100_80155808->walkStage = 4;
        if (D_actor_403100_80155808->playerReactionStage == ACTOR_403100_PLAYER_REACTION_NONE) {
            Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
            func_actor_403100_8013D1B8(5, 0x3F4);
            D_actor_403100_80155808->playerReactionFrames = 0x17;
            D_actor_403100_80155808->playerReactionStage  = ACTOR_403100_PLAYER_REACTION_HIT_HELD;
            task                                          = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            if (taskMessageDispatch(task, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackPair(D_actor_403100_80147614, 0), 0) == 1) {
                ((GameActor*)(*gPlayerActorTasks)->work)->state = 0xA;
            }
        }
        D_actor_403100_80155808->handTouchedPlayer    = 0;
        D_actor_403100_80155808->forearmTouchedPlayer = 0;
        func_actor_403100_8013D2A0(1);
        D_actor_403100_80155808->swingConnected = 1;
        D_actor_403100_80155808->stateCounter   = 1;
    }
    D_actor_403100_80155808->armYaw += (s16)((D_actor_403100_80155808->armYawTarget - D_actor_403100_80155808->armYaw) << 4) >> 7;
    if (Actor403100_TestFlags104()) {
        count                                 = D_actor_403100_80155808->stateCounter - 1;
        D_actor_403100_80155808->stateCounter = count;
        if (!(count & 0xFF)) {
            D_actor_403100_80155808->animationRate        = ANIMATION_RATE_ONE;
            D_actor_403100_80155808->animationId          = 5;
            D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_RESET;
            D_actor_403100_80155808->subState            += 1;
            D_actor_403100_80155808->handAttack.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            D_actor_403100_80155808->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            return;
        }
        D_actor_403100_80155808->animationRate    = 0xA;
        angle                                     = (u16)D_actor_403100_80155808->headAim.vy;
        D_actor_403100_80155808->stateFrames      = 0;
        D_actor_403100_80155808->animationId      = 2;
        D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
        D_actor_403100_80155808->armYawTarget     = (s16)angle;
        if ((s16)angle >= 0xD1) {
            D_actor_403100_80155808->armYawTarget = 0xD0;
        }
        if (D_actor_403100_80155808->armYawTarget < -0x140) {
            D_actor_403100_80155808->armYawTarget = -0x140;
        }
    }
}
static void func_actor_403100_80137CA8(Task* task)
{
    u16 angleY;
    u16 angleX;
    u32 random;

    angleX                            = (u16)D_actor_403100_80155808->armPitch;
    angleY                            = (u16)D_actor_403100_80155808->armYaw;
    D_actor_403100_80155808->armPitch = angleX + ((s32) - (angleX << 0x14) >> 0x17);
    D_actor_403100_80155808->armYaw   = angleY + ((s32) - (angleY << 0x14) >> 0x17);
    if (Actor403100_TestFlags104()) {
        if (D_actor_403100_80155808->swingConnected != 0) {
            D_actor_403100_80155808->stateFrames = 0;
            random                               = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState                      = random;
            if (!((random >> 0x10) & 3)) {
                D_actor_403100_80155808->animationBlendFrames = 0x14;
                D_actor_403100_80155808->animationRate        = 0x1C;
                D_actor_403100_80155808->animationId          = 3;
                D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
                D_actor_403100_80155808->subState            += 1;
                return;
            }
            D_actor_403100_80155808->animationBlendFrames = 8;
            D_actor_403100_80155808->animationRate        = 0x20;
            D_actor_403100_80155808->animationId          = 1;
            D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
            D_actor_403100_80155808->subState            += 2;
            return;
        }
        D_actor_403100_80155808->state    = 1;
        D_actor_403100_80155808->subState = 0;
    }
}
static void func_actor_403100_80137DC4(Task* arg0)
{
    s32 sound;
    s32 sound2;
    s32 pan;
    s32 pan2;
    u16 frame;

    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    if ((s16)frame == 0x31) {
        sound = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0002;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[4]);
        sndEvtRequestScriptStart(sound, pan, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[4]) / 2));
        sound2 = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0001;
        pan2   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(sound2, pan2, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]) / 2));
        Gp_SpawnPadLerp(0x12, 0xFFU, 8U);
        D_actor_403100_80155808->shakeFrames = 0x12;
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->state    = 1;
        D_actor_403100_80155808->subState = 0;
    }
}
static void func_actor_403100_80137F4C(Task* task)
{
    _Actor403100Flame*  flame;
    WorldCollisionBody* body;
    _Actor403100Flame*  flames;
    s32                 i;

    D_actor_403100_80155808->walkStage = 0;
    if (D_actor_403100_80155808->lowHealth == 0) {
        D_actor_403100_80155808->walkSpeed = 0x1C;
    } else {
        D_actor_403100_80155808->walkSpeed = 0x30;
    }
    i                                      = 0;
    flames                                 = D_actor_403100_80155814;
    body                                   = &D_actor_403100_80155814->body;
    flame                                  = flames;
    D_actor_403100_80155810                = 0;
    D_actor_403100_80155808->flameLifetime = 0x14;
    for (; i < ARRAY_SIZE(D_actor_403100_80155814); i++) {
        if (flame->active != 0) {
            flame->active = 0;
            worldCollisionUnlinkBody(body);
        }
        body = &(PARENT_OF(body, _Actor403100Flame, body) + 1)->body;
        flame++;
    }
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 1);
    D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
    D_actor_403100_80155808->animationId      = 7;
    D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
    D_actor_403100_80155808->aimMode          = ACTOR_403100_AIM_YAW_ONLY;
    D_actor_403100_80155808->stateFrames      = 0;
    D_actor_403100_80155808->subState        += 1;
}
static void func_actor_403100_80138048(Task* arg0)
{
    SVECTOR   offset;
    SVECTOR   velocity;
    GfxCoord* effectCoord;
    s32       sound;
    s32       sound2;
    s32       state;
    s32       pan;
    s32       pan2;

    if (D_actor_403100_80155810 == 1) {
        D_actor_403100_80155808->subState = 3;
        return;
    }
    state                                 = D_actor_403100_80155808->hitTaken;
    D_actor_403100_80155808->stateFrames += 1;
    if (state == 1) {
        if (D_actor_403100_80155808->headPitchPhase == ACTOR_403100_PITCH_PHASE_REST) {
            D_actor_403100_80155808->headPitchPhase = state;
        }
    }
    if ((s16)D_actor_403100_80155808->stateFrames < 0x33) {
        if (D_actor_403100_80155808->playerRegion == 4) {
            D_actor_403100_80155808->aimTarget.vx = -0x1770;
            D_actor_403100_80155808->aimTarget.vy = -0xC80;
            D_actor_403100_80155808->aimTarget.vz = -0x1B58;
            D_actor_403100_80155808->aimMode      = ACTOR_403100_AIM_TRACK_FAST;
        }
        if (D_actor_403100_80155808->playerRegion == 5) {
            D_actor_403100_80155808->aimTarget.vx = 0x500;
            D_actor_403100_80155808->aimTarget.vy = -0xC80;
            D_actor_403100_80155808->aimTarget.vz = 0x2710;
            D_actor_403100_80155808->aimMode      = ACTOR_403100_AIM_TRACK_FAST;
        }
        if ((s16)D_actor_403100_80155808->stateFrames < 0x33) {
            func_dryfield_night_motel_balcony_80182730();
        }
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x33) {
        D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_TRACK_STEPPED;
        if (D_actor_403100_80155808->lowHealth) {
            D_actor_403100_80155808->aimYawStep = 0x10;
        } else {
            D_actor_403100_80155808->aimYawStep = 8;
        }
    }
    if (((s16)D_actor_403100_80155808->stateFrames == 0x3D) && (D_actor_403100_80155808->lowHealth == 0)) {
        D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_TRACK;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x7C) {
        D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_YAW_ONLY;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x33) {
        sound = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0004;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[4]);
        sndEvtRequestScriptStart(sound, (s32)pan, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[4]) / 2));
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x7C) {
        SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 0xA);
    }
    if ((u32)(D_actor_403100_80155808->stateFrames - 0x34) < 0x48U) {
        offset.vy   = -0x1F0;
        offset.vz   = 0x620;
        velocity.vy = -0x20;
        offset.vx   = 0;
        velocity.vx = 0;
        velocity.vz = 0xF0;
        func_actor_403100_80132064(arg0, &offset, &velocity, 0);
        func_actor_403100_8013D11C(arg0);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x7E) {
        sound2 = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0002;
        pan2   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[4]);
        sndEvtRequestScriptStart(sound2, (s32)pan2, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[4]) / 2));
    }
    if ((u32)(D_actor_403100_80155808->stateFrames - 0x7D) < 0xBU) {
        effectCoord = &arg0->extra.tmd->coords[3];
        offset.vy   = -0x140;
        offset.vx   = 0;
        offset.vz   = 0x400;
        Gp_SpawnEff(EFFECT_SMOKE_PUFF, effectCoord, -0x3FFCB400, &offset);
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->aimMode     = ACTOR_403100_AIM_YAW_ONLY;
        D_actor_403100_80155808->stateFrames = 0;
        D_actor_403100_80155808->subState   += 1;
    }
}
static void func_actor_403100_8013842C(Task* arg0)
{
    SVECTOR   offset;
    s32       sound;
    s32       sound2;
    s32       pan;
    GfxCoord* effectCoord;
    s32       pan2;
    u16       frame;

    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    if ((s16)frame == 0x31) {
        sound = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0002;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[4]);
        sndEvtRequestScriptStart(sound, pan, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[4]) / 2));
        sound2 = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0001;
        pan2   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(sound2, pan2, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]) / 2));
        Gp_SpawnPadLerp(0x12, 0xFFU, 8U);
        D_actor_403100_80155808->shakeFrames    = 0x12;
        D_actor_403100_80155808->headPitchPhase = ACTOR_403100_PITCH_PHASE_START;
    }
    if ((u32)(D_actor_403100_80155808->stateFrames - 0x32) < 0xBU) {
        effectCoord = &arg0->extra.tmd->coords[3];
        offset.vy   = -0x140;
        offset.vx   = 0;
        offset.vz   = 0x400;
        Gp_SpawnEff(EFFECT_SMOKE_PUFF, effectCoord, -0x3FFCB400, &offset);
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->state    = 1;
        D_actor_403100_80155808->subState = 0;
    }
}
static void func_actor_403100_80138610(Task* arg0)
{
    u16       counter;
    u16       timer;
    u16       angle;
    GfxCoord* coord;

    coord = arg0->extra.tmd->coords;
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        coord->coord.t[1]                   += 0x30;
        timer                                = D_actor_403100_80155808->stateFrames;
        D_actor_403100_80155808->stateFrames = timer + 1;
        if ((s16)timer >= 0x11) {
            counter                                                    = *(volatile u16*)&D_actor_403100_80155808->subState;
            *(volatile s16*)&D_actor_403100_80155808->jumpAcceleration = 0;
            *(volatile s16*)&D_actor_403100_80155808->jumpAcceleration = 2;
            angle                                                      = *(volatile u16*)&D_actor_403100_80155808->headAim.vy;
            D_actor_403100_80155808->stateFrames                       = 0;
            D_actor_403100_80155808->jumpSpeed                         = 0;
            D_actor_403100_80155808->armYawTarget                      = (s16)angle;
            D_actor_403100_80155808->subState                          = counter + 1;
            if ((s16)angle >= 0xD1) {
                D_actor_403100_80155808->armYawTarget = 0xD0;
            }
            if (D_actor_403100_80155808->armYawTarget < -0x160) {
                D_actor_403100_80155808->armYawTarget = -0x160;
            }
        }
    }
}
static void func_actor_403100_801386DC(Task* arg0)
{
    u16       velocity;
    u16       accel;
    GfxCoord* coord;

    coord = arg0->extra.tmd->coords;
    func_actor_403100_8013D24C();
    D_actor_403100_80155808->armYaw =
        (u16)D_actor_403100_80155808->armYaw +
        ((s32)(((u16)D_actor_403100_80155808->armYawTarget - (u16)D_actor_403100_80155808->armYaw) << 0x14) >> 0x17);
    D_actor_403100_80155808->stateFrames     += 1;
    accel                                     = D_actor_403100_80155808->jumpAcceleration + 4;
    velocity                                  = D_actor_403100_80155808->jumpSpeed + accel;
    D_actor_403100_80155808->jumpSpeed        = velocity;
    D_actor_403100_80155808->jumpAcceleration = accel;
    coord->coord.t[1]                        -= (s16)velocity;
    if ((s16)D_actor_403100_80155808->stateFrames >= 6) {
        D_actor_403100_80155808->stateFrames = 0;
        D_actor_403100_80155808->subState   += 1;
    }
}
static void func_actor_403100_80138790(Task* arg0)
{
    u16       velocity;
    u16       accel;
    GfxCoord* coord;

    coord = arg0->extra.tmd->coords;
    func_actor_403100_8013D24C();
    D_actor_403100_80155808->armYaw =
        (u16)D_actor_403100_80155808->armYaw +
        ((s32)(((u16)D_actor_403100_80155808->armYawTarget - (u16)D_actor_403100_80155808->armYaw) << 0x14) >> 0x17);
    D_actor_403100_80155808->stateFrames     += 1;
    accel                                     = D_actor_403100_80155808->jumpAcceleration - 4;
    velocity                                  = D_actor_403100_80155808->jumpSpeed + accel;
    D_actor_403100_80155808->jumpSpeed        = velocity;
    D_actor_403100_80155808->jumpAcceleration = accel;
    coord->coord.t[1]                        -= (s16)velocity;
    if ((s16)D_actor_403100_80155808->stateFrames >= 6) {
        D_actor_403100_80155808->subState += 1;
    }
}
static void func_actor_403100_80138844(Task* arg0)
{
    Task*     player;
    s32       sound;
    s32       sound2;
    s32       y;
    s32       pan;
    s32       pan2;
    u16       velocity;
    u16       accel;
    GfxCoord* coord;

    coord = arg0->extra.tmd->coords;
    func_actor_403100_8013D24C();
    if ((D_actor_403100_80155808->handTouchedPlayer != 0 || D_actor_403100_80155808->forearmTouchedPlayer != 0) && (D_actor_403100_80155808->playerReactionStage == ACTOR_403100_PLAYER_REACTION_NONE)) {
        Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
        func_actor_403100_8013D1B8(5, 0x3F4);
        D_actor_403100_80155808->playerReactionFrames = 0x17;
        D_actor_403100_80155808->playerReactionStage  = ACTOR_403100_PLAYER_REACTION_HIT_HELD;
        player                                        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        if (taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackPair(D_actor_403100_80147614, 0), 0) == 1) {
            ((GameActor*)(*gPlayerActorTasks)->work)->state = 0xA;
        }
    }
    func_actor_403100_8013C7B4(arg0);
    D_actor_403100_80155808->stateFrames += 1;
    D_actor_403100_80155808->armYaw =
        (u16)D_actor_403100_80155808->armYaw +
        ((s32)(((u16)D_actor_403100_80155808->armYawTarget - (u16)D_actor_403100_80155808->armYaw) << 0x14) >> 0x17);
    accel                                     = D_actor_403100_80155808->jumpAcceleration - 4;
    velocity                                  = D_actor_403100_80155808->jumpSpeed + accel;
    D_actor_403100_80155808->jumpSpeed        = velocity;
    D_actor_403100_80155808->jumpAcceleration = accel;
    y                                         = coord->coord.t[1] - (s16)velocity;
    coord->coord.t[1]                         = y;
    if (y >= 0) {
        D_actor_403100_80155808->handTouchedPlayer    = 0;
        D_actor_403100_80155808->forearmTouchedPlayer = 0;
        func_actor_403100_801342B4(arg0);
        Gp_SpawnPadLerp(0x1E, 0xFFU, 8U);
        D_actor_403100_80155808->shakeFrames = 0x1E;
        sound                                = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0001;
        pan                                  = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(sound, pan, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]) / 2));
        sound2 = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0003;
        pan2   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[7]);
        sndEvtRequestScriptStart(sound2, pan2, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[7]) / 2));
        coord->coord.t[1]                    = 0;
        D_actor_403100_80155808->stateFrames = 0;
        D_actor_403100_80155808->subState   += 1;
    }
}
static void func_actor_403100_80138AB4(Task* task)
{
    Task* player;
    u16   frame;

    func_actor_403100_8013D24C();
    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    if (((s16)frame < 0x20) && (D_actor_403100_80155808->handTouchedPlayer != 0) && (D_actor_403100_80155808->playerReactionStage == ACTOR_403100_PLAYER_REACTION_NONE)) {
        Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
        func_actor_403100_8013D1B8(5, 0x3F4);
        D_actor_403100_80155808->playerReactionFrames = 0x17;
        D_actor_403100_80155808->playerReactionStage  = ACTOR_403100_PLAYER_REACTION_HIT_HELD;
        player                                        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        if (taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackPair(D_actor_403100_80147614, 0), 0) == 1) {
            ((GameActor*)(*gPlayerActorTasks)->work)->state = 0xA;
        }
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
        D_actor_403100_80155808->animationId      = 5;
        D_actor_403100_80155808->walkStage        = 0;
        D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
        D_actor_403100_80155808->subState        += 1;
    }
}
static void func_actor_403100_80138C18(Task* task)
{
    u16 velocityZ;
    u16 velocityX;
    u32 random;

    func_actor_403100_8013D24C();
    velocityX                         = (u16)D_actor_403100_80155808->armPitch;
    velocityZ                         = (u16)D_actor_403100_80155808->armYaw;
    D_actor_403100_80155808->armPitch = velocityX + ((s32) - (velocityX << 0x14) >> 0x17);
    D_actor_403100_80155808->armYaw   = velocityZ + ((s32) - (velocityZ << 0x14) >> 0x17);
    if (Actor403100_TestFlags104()) {
        random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        gRandomLcgState = random;
        if ((random >> 0x10) & 1) {
            func_actor_403100_8013D2A0(1);
        }
        D_actor_403100_80155808->animationBlendFrames = 8;
        D_actor_403100_80155808->animationRate        = ANIMATION_RATE_ONE;
        D_actor_403100_80155808->stateFrames          = 0;
        D_actor_403100_80155808->animationId          = 1;
        D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
        D_actor_403100_80155808->subState            += 1;
    }
}
static void func_actor_403100_80138D08(Task* arg0)
{
    TmdObject*       obj;
    Actor403100Work* work;

    obj = arg0->extra.tmd;
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        obj->otOffset                                 = 0;
        work                                          = D_actor_403100_80155808;
        D_actor_403100_80155808->handTouchedPlayer    = 0;
        work->walkSpeed                               = 0x40;
        work->animationRate                           = ANIMATION_RATE_ONE;
        work->animationId                             = 9;
        work->walkStage                               = 0;
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
static void func_actor_403100_80138DB0(Task* arg0)
{
    s32              sound;
    s32              pan;
    s32              depth;
    Task*            player;
    Actor403100Work* work;

    player                               = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    D_actor_403100_80155808->stateFrames = (u16)(D_actor_403100_80155808->stateFrames + 1);
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        if ((s16)D_actor_403100_80155808->stateFrames < 0x101) {
            func_actor_403100_8013C7B4(arg0);
            work = D_actor_403100_80155808;
            if (work->handTouchedPlayer != 0) {
                work->vulnerable                       = 1;
                D_actor_403100_80155808->holdingPlayer = 1;
                Gp_StateC08.flags                      = (u8)(Gp_StateC08.flags | ATTACHMENT_FLAG_EVENT_LOCK);
                sound                                  = (((u16)((Enemy*)player->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 7;
                pan                                    = (s8)worldCoordGetOriginAudioPan(&player->extra.tmd->coords[1]);
                depth                                  = worldCoordGetOriginAudioDepth(&player->extra.tmd->coords[1]);
                sndEvtRequestScriptStart(sound, pan, (s8)(depth / 2));
                D_actor_403100_80155808->releaseRequested = 0;
                D_actor_403100_80155808->promptPending    = 1;
                func_actor_403100_8013D1B8(1, 0x3F4);
                D_actor_403100_80155808->stateFrames  = 0U;
                D_actor_403100_80155808->stateCounter = 0;
                D_actor_403100_80155808->subState    += 1;
            } else if (work->forearmTouchedPlayer != 0) {
                work->animationRate = -ANIMATION_RATE_ONE;
                work->subState      = 0xA;
            }
            D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_TRACK;
        } else {
            D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_ANIMATED;
        }
        if (Actor403100_TestFlags104()) {
            D_actor_403100_80155808->state    = 1;
            D_actor_403100_80155808->subState = 0U;
        }
    }
}
static void func_actor_403100_80138F88(Task* arg0)
{
    s32              message[6];
    Actor403100Work* work;
    s32              sound;
    s32              pan;
    s32              depth;
    GfxCoord*        coords;
    GfxCoord*        part;
    long*            translation;

    coords = arg0->extra.tmd->coords;
    part   = coords + 6;
    func_actor_403100_8013C008(D_actor_403100_80155808->overlayX, D_actor_403100_80155808->overlayY);
    D_actor_403100_80155808->overlayX    = (u16)D_actor_403100_80155808->overlayX - 1;
    D_actor_403100_80155808->overlayY    = (u16)D_actor_403100_80155808->overlayY + 6;
    coords->coord.t[1]                  += -coords->coord.t[1] >> 6;
    D_actor_403100_80155808->rotation.vx = 0;
    D_actor_403100_80155808->rotation.vy = 0xA00;
    D_actor_403100_80155808->rotation.vz = 0;
    func_actor_403100_80132528(arg0);
    if (Actor403100_TestFlags104()) {
        sound = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F000C;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[4]);
        depth = worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[4]);
        sndEvtRequestScriptStart(sound, pan, (s8)(depth / 2));
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0xB;
        translation                                                = coords->coord.t;
        *translation++                                             = -0x44C;
        work                                                       = D_actor_403100_80155808;
        SOFT_TOUCH_REG(work);
        *translation++         = 0;
        *translation           = 0x1770;
        work->rotation.vy      = 0xC00;
        work->animationRate    = ANIMATION_RATE_ONE;
        work->animationId      = 0xC;
        work->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
        work->armPitch         = 0xD0;
        work->armYaw           = -0x350;
        work->forearmTurn.vx   = -0x110;
        work->forearmTurn.vy   = 0x290;
        work->rotation.vx      = 0;
        work->rotation.vz      = 0;
        work->stateFrames      = 0;
        work->forearmTurn.vz   = 0x60;
        work->subState        += 1;
        part->coord.t[0]       = -0xBD0;
        work->armPitch         = 0x30;
        work->armYaw           = -0xD0;
        work->forearmTurn.vx   = -0x150;
        work->forearmTurn.vy   = 0x270;
        work->forearmTurn.vz   = -0xA0;
        part->coord.t[0]       = -0x1120;
        work->headBody.radius  = 0x500;
        func_actor_403100_8013D74C(arg0);
        D_actor_403100_80155808->forearmStrokeDone = 0;
        D_actor_403100_80155808->releaseRequested  = 0;
        D_actor_403100_80155808->promptPending     = 0;
        D_actor_403100_80155808->jawPitchOffset    = 0;
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        func_actor_403100_8013D1B8(1, 0x3FF);
        message[5] = 0x28;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, message, 0);
        D_actor_403100_80155808->holdStartHp = D_actor_403100_8015580C->hp;
    }
}
static void func_actor_403100_8013922C(Task* arg0)
{
    s32       message[6];
    s16       health;
    s32       damage;
    u16       frame;
    u32       random;
    u8        request;
    GfxCoord* coords;
    GfxCoord* part;

    coords                                = arg0->extra.tmd->coords;
    part                                  = coords + 6;
    D_actor_403100_80155808->stateFrames += 1;
    func_actor_403100_8013D6B4(arg0);
    func_actor_403100_8013C214(arg0);
    func_actor_403100_8013C214(arg0);
    D_actor_403100_80155808->rotation.vy = 0xC00;
    D_actor_403100_80155808->rotation.vx = 0;
    D_actor_403100_80155808->rotation.vz = 0;
    if (D_actor_403100_80155808->playerKilled == 0) {
        request = D_actor_403100_80155808->releaseRequested;
        if ((request == 1) && (D_actor_403100_80155808->forearmStrokeDone == request)) {
            random                                 = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState                        = random;
            D_actor_403100_80155808->repromptDelay = (((random >> 0x10) & 0x1F) + 0x3C) * 3;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x400, 0, 0);
            D_actor_403100_80155808->releaseRequested = 0;
        }
    } else {
        frame                                      = D_actor_403100_80155808->playerDeathFrames + 1;
        D_actor_403100_80155808->playerDeathFrames = frame;
        if ((s16)frame == 0x3C) {
            func_actor_403100_8013D2A0(1);
        }
        if ((s16)D_actor_403100_80155808->playerDeathFrames >= 0x79) {
            gGameSession->suppressDeathChecks = 0;
        }
    }
    func_actor_403100_80132528(arg0);
    func_actor_403100_8013D700(arg0);
    func_actor_403100_8013C214(arg0);
    if (D_actor_403100_80155808->hitTaken != 0) {
        if (D_actor_403100_80155808->animationId != 0xD) {
            D_actor_403100_80155808->animationId      = 0xD;
            D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
            D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
            if (D_actor_403100_80155808->jawPitchPhase == ACTOR_403100_PITCH_PHASE_REST) {
                D_actor_403100_80155808->jawKickSound  = 1;
                D_actor_403100_80155808->jawPitchPhase = ACTOR_403100_PITCH_PHASE_START;
            }
        }
    } else {
        if (Actor403100_TestFlags()) {
            D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
            D_actor_403100_80155808->animationId      = 0xC;
            D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
        }
    }
    if (D_actor_403100_80155808->promptPending != 0) {
        D_actor_403100_80155808->promptPending = 0;
        func_actor_403100_8013D1B8(1, 0x3FF);
        message[5] = 0x28;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, message, 0);
        D_actor_403100_80155808->stateCounter = D_actor_403100_80155808->stateCounter + 1;
    }
    if ((D_actor_403100_80155808->stateCounter != 0) ||
        ((D_actor_403100_80155808->forearmStrokeDone == 1) &&
         ((s16)D_actor_403100_80155808->stateFrames >= 0x1C2) &&
         (D_actor_403100_80155808->playerKilled == 0)) ||
        (D_actor_403100_8015580C->hp <= 0)) {
        damage = D_actor_403100_80155808->holdStartHp - D_actor_403100_8015580C->hp;
        health = D_actor_403100_8015580C->hp;
        if ((damage >= 0x3C) && (health > 0)) {
            D_actor_403100_80155808->stateFrames = 0;
            D_actor_403100_80155808->subState   += 1;
            return;
        }
        func_actor_403100_8013D1B8(1, 0x3F4);
        D_actor_403100_80155808->stateFrames                       = 0;
        D_actor_403100_80155808->animationRate                     = 8;
        D_actor_403100_80155808->animationId                       = 0xE;
        D_actor_403100_80155808->animationRequest                  = ACTOR_403100_ANIMATION_REQUEST_RESET;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0xC;
        coords->coord.t[0]                                         = -0x44C;
        coords->coord.t[2]                                         = 0x1770;
        coords->coord.t[1]                                         = 0;
        D_actor_403100_80155808->rotation.vy                       = 0xC00;
        D_actor_403100_80155808->rotation.vx                       = 0;
        D_actor_403100_80155808->rotation.vz                       = 0;
        D_actor_403100_80155808->armPitch                          = 0;
        D_actor_403100_80155808->armYaw                            = 0;
        D_actor_403100_80155808->forearmTurn.vx                    = 0;
        D_actor_403100_80155808->forearmTurn.vy                    = 0;
        D_actor_403100_80155808->forearmTurn.vz                    = 0;
        part->coord.t[0]                                           = -0x877;
        D_actor_403100_80155808->subState                          = 8;
    }
}
static void func_actor_403100_801395EC(Task* arg0)
{
    _Actor403100Flame*  flame;
    _Actor403100Flame*  flames;
    WorldCollisionBody* body;
    s32                 x;
    s32                 i;
    u16                 frame;
    TmdObject*          model;
    GfxCoord*           part;
    GfxCoord*           coords;

    model                                   = arg0->extra.tmd;
    coords                                  = model->coords;
    part                                    = coords + 6;
    D_actor_403100_80155808->armPitch       = (u16)D_actor_403100_80155808->armPitch + ((s32)(-0x2E0 - D_actor_403100_80155808->armPitch) >> 3);
    D_actor_403100_80155808->armYaw         = (u16)D_actor_403100_80155808->armYaw + ((s32)(0x10 - D_actor_403100_80155808->armYaw) >> 3);
    D_actor_403100_80155808->forearmTurn.vx = (u16)D_actor_403100_80155808->forearmTurn.vx + ((s32)(-0x150 - D_actor_403100_80155808->forearmTurn.vx) >> 3);
    D_actor_403100_80155808->forearmTurn.vy = (u16)D_actor_403100_80155808->forearmTurn.vy + ((s32)(0x280 - D_actor_403100_80155808->forearmTurn.vy) >> 3);
    D_actor_403100_80155808->forearmTurn.vz = (u16)D_actor_403100_80155808->forearmTurn.vz + ((s32)(0x140 - D_actor_403100_80155808->forearmTurn.vz) >> 3);
    x                                       = part->coord.t[0];
    part->coord.t[0]                        = (s32)(x + ((s32)(-0xBE0 - x) >> 3));
    D_actor_403100_80155808->rotation.vx    = 0;
    D_actor_403100_80155808->rotation.vy    = 0xC00;
    D_actor_403100_80155808->rotation.vz    = 0;
    func_actor_403100_80132528(arg0);
    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    i                                    = 0;
    if (((s32)(frame << 0x10) >> 0x10) >= 0x1F) {
        flames                                                     = D_actor_403100_80155814;
        body                                                       = &flames->body;
        flame                                                      = flames;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x18;
        coords->coord.t[1]                                         = -0x1388;
        D_actor_403100_80155808->jawPitchOffset                    = 0;
        D_actor_403100_80155808->armPitch                          = 0;
        D_actor_403100_80155808->armYaw                            = 0;
        D_actor_403100_80155808->forearmTurn.vx                    = 0;
        D_actor_403100_80155808->forearmTurn.vy                    = 0;
        D_actor_403100_80155808->forearmTurn.vz                    = 0;
        part->coord.t[0]                                           = -0x877;
        D_actor_403100_80155808->animationId                       = 7;
        D_actor_403100_80155808->animationRequest                  = ACTOR_403100_ANIMATION_REQUEST_RESET;
        D_actor_403100_80155808->flameLifetime                     = 0x14;
        D_actor_403100_80155808->headAim.vy                        = 0x200;
        D_actor_403100_80155808->stateFrames                       = 0;
        D_actor_403100_80155808->animationRate                     = ANIMATION_RATE_ONE;
        D_actor_403100_80155808->aimMode                           = ACTOR_403100_AIM_TRACK;
        D_actor_403100_80155808->headAim.vx                        = 0;
        D_actor_403100_80155808->headAim.vz                        = 0;
        D_actor_403100_80155810                                    = 0;
        D_actor_403100_80155808->subState                         += 1;
        coords->coord.t[0]                                         = -0x44C;
        coords->coord.t[2]                                         = 0x2710;
        coords->coord.t[1]                                         = -0x1388;
        D_actor_403100_80155808->rotation.vx                       = 0;
        D_actor_403100_80155808->rotation.vy                       = 0xA00;
        D_actor_403100_80155808->rotation.vz                       = 0;
        do {
            if (flame->active != 0) {
                flame->active = 0;
                worldCollisionUnlinkBody(body);
            }
            body = &(PARENT_OF(body, _Actor403100Flame, body) + 1)->body;
            i   += 1;
            flame++;
        } while (i < ARRAY_SIZE(D_actor_403100_80155814));
        SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 1);
    }
}
static void func_actor_403100_80139818(Task* arg0)
{
    SVECTOR          position;
    SVECTOR          velocity;
    Task*            playerTask;
    Task*            task;
    s16              deathFrame;
    s32              sound;
    s32              sound2;
    s32              sound3;
    s32              sound4;
    s32              sound5;
    s32              pan;
    s32              pan2;
    s32              pan3;
    s32              pan4;
    s32              pan5;
    u16              frame;
    s32              depth;
    s32              depth2;
    s32              depth3;
    s32              depth4;
    s32              depth5;
    GfxCoord*        effectCoords;
    Actor403100Work* work;
    PlayerStatus*    config;
    GfxCoord*        part;
    GfxCoord*        coords;

    playerTask = *gPlayerActorTasks;
    coords     = arg0->extra.tmd->coords;
    part       = coords + 6;
    config     = &gPlayerStatus;
    if (D_actor_403100_80155808->playerKilled == 0) {
        if (D_actor_403100_80155808->releaseRequested == 1) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x400, 0, 0);
            D_actor_403100_80155808->releaseRequested = 0;
        }
    }
    D_actor_403100_80155808->overlayX = 0;
    D_actor_403100_80155808->overlayY = 0x3C;
    func_actor_403100_8013C008(D_actor_403100_80155808->overlayX, 0x3C);
    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    if ((s16)frame == 0x33) {
        D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_TRACK;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x7C) {
        D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_YAW_ONLY;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x33) {
        sound = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0004;
        pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords + 4);
        depth = worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords + 4);
        sndEvtRequestScriptStart(sound, pan, (s8)(depth / 2));
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x7C) {
        SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 0xA);
    }
    if ((u32)(D_actor_403100_80155808->stateFrames - 0x34) < 0x48U) {
        position.vy = -0x1F0;
        position.vz = 0x620;
        velocity.vy = -0x20;
        position.vx = 0;
        velocity.vx = 0;
        velocity.vz = 0xE0;
        func_actor_403100_80132064(arg0, &position, &velocity, 0);
    }
    if ((u32)(D_actor_403100_80155808->stateFrames - 0x7D) < 0xBU) {
        effectCoords = arg0->extra.tmd->coords;
        position.vy  = -0x140;
        position.vx  = 0;
        position.vz  = 0x400;
        Gp_SpawnEff(EFFECT_SMOKE_PUFF, effectCoords + 3, -0x3FFCB400, &position);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x64) {
        func_8010B2A0(0, 3);
        func_actor_403100_8013D1B8(1, 0x3F4);
        task = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        taskMessageDispatch(task, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackPair(D_actor_403100_80147614, 4), 0);
        if (config->hp <= 0) {
            sound2 = (((u16)((Enemy*)playerTask->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x531D000B;
            pan2   = (s8)worldCoordGetOriginAudioPan(playerTask->extra.tmd->coords + 1);
            depth2 = worldCoordGetOriginAudioDepth(playerTask->extra.tmd->coords + 1);
            sndEvtRequestScriptStart(sound2, pan2, (s8)(depth2 / 2));
            gGameSession->deathSoundCountdown = GAME_SESSION_DEATH_SOUND_HOLD;
            work                              = D_actor_403100_80155808;
            work->playerKilled                = 1;
            work->playerDeathFrames           = 0;
            gGameSession->suppressDeathChecks = 1;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x400, 0, 0);
            func_actor_403100_8013D1B8(6, 0x3FF);
        } else {
            sound3 = (((u16)((Enemy*)playerTask->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 7;
            pan3   = (s8)worldCoordGetOriginAudioPan(playerTask->extra.tmd->coords + 1);
            depth3 = worldCoordGetOriginAudioDepth(playerTask->extra.tmd->coords + 1);
            sndEvtRequestScriptStart(sound3, pan3, (s8)(depth3 / 2));
        }
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x80) {
        sound4 = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F000D;
        pan4   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords + 4);
        depth4 = worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords + 4);
        sndEvtRequestScriptStart(sound4, pan4, (s8)(depth4 / 2));
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0xC9) {
        sound5 = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F000E;
        pan5   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords + 1);
        depth5 = worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords + 1);
        sndEvtRequestScriptStart(sound5, pan5, (s8)(depth5 / 2));
        Gp_SpawnPadLerp(0x12, 0xFFU, 8U);
        D_actor_403100_80155808->shakeFrames = 0x12;
    }
    D_actor_403100_80155808->armPitch       = -0x20;
    D_actor_403100_80155808->armYaw         = 0x450;
    D_actor_403100_80155808->forearmTurn.vx = 0x290;
    D_actor_403100_80155808->forearmTurn.vy = 0x210;
    D_actor_403100_80155808->forearmTurn.vz = -0x160;
    part->coord.t[0]                        = -0xE27;
    coords->coord.t[0]                      = -0x44C;
    coords->coord.t[1]                      = -0x1388;
    coords->coord.t[2]                      = 0x2710;
    D_actor_403100_80155808->rotation.vx    = 0;
    D_actor_403100_80155808->rotation.vy    = 0xA00;
    D_actor_403100_80155808->rotation.vz    = 0;
    func_actor_403100_80132528(arg0);
    if (D_actor_403100_80155808->playerKilled == 0) {
        if (Actor403100_TestFlags()) {
            func_actor_403100_8013D1B8(1, 0x3F4);
            D_actor_403100_80155808->stateFrames = 0;
            D_actor_403100_80155808->aimMode     = ACTOR_403100_AIM_TRACK;
            D_actor_403100_80155808->overlayY    = 0x3C;
            D_actor_403100_80155808->subState   += 1;
        }
    } else {
        deathFrame                                 = (u16)D_actor_403100_80155808->playerDeathFrames + 1;
        D_actor_403100_80155808->playerDeathFrames = deathFrame;
        if (deathFrame == 0x3C) {
            func_actor_403100_8013D2A0(0);
        }
        if (D_actor_403100_80155808->playerDeathFrames >= 0x79) {
            gGameSession->suppressDeathChecks = 0;
        }
    }
}
static void func_actor_403100_80139E80(Task* arg0)
{
    s16       angle;
    s32       y;
    s32       x;
    u16       velocityX;
    u16       rotationX;
    u16       rotationZ;
    u16       velocityZ;
    u16       rotationY;
    u16       frame;
    GfxCoord* part;
    GfxCoord* coords;

    coords                            = arg0->extra.tmd->coords;
    D_actor_403100_80155808->overlayX = (u16)D_actor_403100_80155808->overlayX - 1;
    angle                             = (u16)D_actor_403100_80155808->overlayY + 6;
    D_actor_403100_80155808->overlayY = angle;
    func_actor_403100_8013C008(D_actor_403100_80155808->overlayX, angle);
    part                                    = coords + 6;
    y                                       = coords->coord.t[1];
    coords->coord.t[1]                      = (s32)(y + ((s32)-y >> 6));
    velocityX                               = (u16)D_actor_403100_80155808->armPitch;
    velocityZ                               = (u16)D_actor_403100_80155808->armYaw;
    D_actor_403100_80155808->armPitch       = velocityX + ((s32)(-0x1100 - (s16)(velocityX * 0x10)) >> 7);
    rotationX                               = (u16)D_actor_403100_80155808->forearmTurn.vx;
    D_actor_403100_80155808->armYaw         = velocityZ + ((s32)(0x4E00 - (s16)(velocityZ * 0x10)) >> 7);
    rotationY                               = (u16)D_actor_403100_80155808->forearmTurn.vy;
    D_actor_403100_80155808->forearmTurn.vx = rotationX + ((s32)(0x2400 - (s16)(rotationX * 0x10)) >> 7);
    rotationZ                               = (u16)D_actor_403100_80155808->forearmTurn.vz;
    D_actor_403100_80155808->forearmTurn.vy = rotationY + ((s32)(-0x1D00 - (s16)(rotationY * 0x10)) >> 7);
    D_actor_403100_80155808->forearmTurn.vz = rotationZ + ((s32)(0x2E00 - (s16)(rotationZ * 0x10)) >> 7);
    x                                       = part->coord.t[0];
    part->coord.t[0]                        = (s32)(x + ((s32)(-0x807 - x) >> 3));
    func_actor_403100_80132528(arg0);
    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    if ((s16)frame >= 0x1F) {
        D_actor_403100_80155808->animationRate                     = 8;
        D_actor_403100_80155808->animationId                       = 0xE;
        D_actor_403100_80155808->animationRequest                  = ACTOR_403100_ANIMATION_REQUEST_RESET;
        D_actor_403100_80155808->stateFrames                       = 0;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0xC;
        coords->coord.t[0]                                         = -0x44C;
        coords->coord.t[2]                                         = 0x1770;
        coords->coord.t[1]                                         = 0;
        D_actor_403100_80155808->rotation.vy                       = 0xC00;
        D_actor_403100_80155808->rotation.vx                       = 0;
        D_actor_403100_80155808->rotation.vz                       = 0;
        D_actor_403100_80155808->armPitch                          = 0;
        D_actor_403100_80155808->armYaw                            = 0;
        D_actor_403100_80155808->forearmTurn.vx                    = 0;
        D_actor_403100_80155808->forearmTurn.vy                    = 0;
        D_actor_403100_80155808->forearmTurn.vz                    = 0;
        part->coord.t[0]                                           = -0x877;
        D_actor_403100_80155808->subState                         += 1;
    }
}
static void func_actor_403100_8013A064(Task* arg0)
{
    GameActor* actor;
    Task*      task;
    s16        state;
    s16        message;
    s32        sound;
    s32        pan;
    u16        frame;
    s32        depth;

    actor                                = (*gPlayerActorTasks)->work;
    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    if ((s16)frame == 0x3C) {
        sound = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F000F;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[8]);
        depth = worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[8]);
        sndEvtRequestScriptStart(sound, (s32)pan, (s8)(depth / 2));
    }
    D_actor_403100_80155808->rotation.vx = 0;
    D_actor_403100_80155808->rotation.vy = 0xC00;
    D_actor_403100_80155808->rotation.vz = 0;
    func_actor_403100_80132528(arg0);
    if ((s16)D_actor_403100_80155808->stateFrames >= 0x44) {
        D_actor_403100_80155808->animationBlendFrames              = 0x14;
        D_actor_403100_80155808->animationRate                     = ANIMATION_RATE_ONE;
        D_actor_403100_80155808->animationId                       = 5;
        D_actor_403100_80155808->animationRequest                  = ACTOR_403100_ANIMATION_REQUEST_BLEND;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x17;
        D_actor_403100_80155808->subState                         += 1;
        func_actor_403100_8013D0B8(-0x1BBC, -0xC80, -0x4B0, 0x400);
        task = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        if (taskMessageDispatch(task, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackPair(D_actor_403100_80147614, 5), 0) != 0) {
            gGameSession->suppressDeathChecks = 1;
            state                             = 7;
            if (D_actor_403100_8015580C->hp <= 0) {
                D_actor_403100_8015580C->hp = 0x3E8;
            }
            message                               = 0x3FF;
            D_actor_403100_80155808->playerKilled = 1;
            actor->state                          = 0xA;
        } else {
            state   = 2;
            message = 0x3F4;
        }
        func_actor_403100_8013D1B8(state, message);
        D_actor_403100_80155808->stateFrames = 0;
    }
}
static void func_actor_403100_8013A254(Task* task)
{
    Task*            actor;
    Actor403100Work* work;
    s32              sound;
    s32              sound2;
    s32              pan;
    s32              pan2;
    u16              frame;
    s32              depth;
    s32              depth2;

    actor                                = *gPlayerActorTasks;
    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    if ((s16)frame == 0xB) {
        if (D_actor_403100_80155808->sectionDamaged[1] == 0) {
            func_dryfield_night_motel_balcony_8017E250(1, 1);
            D_actor_403100_80155808->sectionDamaged[1] = 1;
        } else {
            func_dryfield_night_motel_balcony_8017E250(1, 2);
        }
        Gp_SpawnPadLerp(8, 0xFFU, 8U);
        D_actor_403100_80155808->shakeFrames = 8;
        sound                                = (((u16)((Enemy*)(actor)->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0006;
        pan                                  = (s8)worldCoordGetOriginAudioPan(actor->extra.tmd->coords + 1);
        depth                                = worldCoordGetOriginAudioDepth(actor->extra.tmd->coords + 1);
        sndEvtRequestScriptStart(sound, (s32)pan, (s8)(depth / 2));
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x1C) {
        sound2 = (((u16)((Enemy*)(actor)->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0007;
        pan2   = (s8)worldCoordGetOriginAudioPan(actor->extra.tmd->coords + 1);
        depth2 = worldCoordGetOriginAudioDepth(actor->extra.tmd->coords + 1);
        sndEvtRequestScriptStart(sound2, (s32)pan2, (s8)(depth2 / 2));
    }
    D_actor_403100_80155808->releaseRequested = 0;
    if (D_actor_403100_80155808->playerKilled == 0) {
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
            if (D_actor_403100_8015580C->hp > 0) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 6;
            }
            D_actor_403100_80155808->field_632         = 0;
            D_actor_403100_80155808->handTouchedPlayer = 0;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            work                                   = D_actor_403100_80155808;
            work->vulnerable                       = 0;
            work->headBody.radius                  = 0x400;
            D_actor_403100_80155808->holdingPlayer = 0;
            D_actor_403100_80155808->state         = 1;
            D_actor_403100_80155808->subState      = 0;
        }
    } else if ((s16)D_actor_403100_80155808->stateFrames >= 0x1E) {
        gGameSession->suppressDeathChecks = 0;
    }
}
static void func_actor_403100_8013A4C8(Task* arg0)
{
    _Actor403100Flame*  flames;
    WorldCollisionBody* body;
    s32                 i;
    Actor403100Work*    work;

    i                                          = 0;
    flames                                     = D_actor_403100_80155814;
    body                                       = &flames->body;
    arg0->extra.tmd->otOffset                  = 0x1F;
    work                                       = *(Actor403100Work* volatile*)&D_actor_403100_80155808;
    (*(volatile s16*)&D_actor_403100_80155810) = 0;
    work->walkSpeed                            = 0x30;
    work->walkStage                            = 0;
    work->flameLifetime                        = 0x1C;
    for (; i < ARRAY_SIZE(D_actor_403100_80155814); i++) {
        if (flames[i].active != 0) {
            flames[i].active = 0;
            worldCollisionUnlinkBody(body);
        }
        body = &(PARENT_OF(body, _Actor403100Flame, body) + 1)->body;
    }
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 1);
    D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
    D_actor_403100_80155808->animationId      = 7;
    D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
    D_actor_403100_80155808->stateFrames      = 0;
    D_actor_403100_80155808->subState        += 1;
}
static void func_actor_403100_8013A5AC(Task* arg0)
{
    SVECTOR   offset;
    SVECTOR   velocity;
    s32       sound;
    s32       pan;
    s32       depth;
    GfxCoord* effectCoord;

    if (D_actor_403100_80155810 == 1) {
        D_actor_403100_80155808->subState = 3;
        return;
    }
    D_actor_403100_80155808->aimTarget.vx = -0x3110;
    D_actor_403100_80155808->stateFrames += 1;
    D_actor_403100_80155808->aimTarget.vy = -0xC80;
    D_actor_403100_80155808->aimTarget.vz = ((s32)(rsin((s16)D_actor_403100_80155808->stateFrames << 5) * 0x10) >> 6) + 0x6DB;
    if ((s16)D_actor_403100_80155808->stateFrames == 0x33) {
        D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_TRACK;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x7C) {
        D_actor_403100_80155808->aimMode = ACTOR_403100_AIM_YAW_ONLY;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x33) {
        sound = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0004;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[4]);
        depth = worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[4]);
        sndEvtRequestScriptStart(sound, pan, (s8)(depth / 2));
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x7C) {
        SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 0xA);
    }
    if ((u32)(D_actor_403100_80155808->stateFrames - 0x34) < 0x48U) {
        offset.vy   = -0x1F0;
        offset.vz   = 0x620;
        velocity.vy = -0x20;
        offset.vx   = 0;
        velocity.vx = 0;
        velocity.vz = 0xE0;
        func_actor_403100_80132064(arg0, &offset, &velocity, 0);
    }
    if ((u32)(D_actor_403100_80155808->stateFrames - 0x7D) < 0xBU) {
        effectCoord = &arg0->extra.tmd->coords[3];
        offset.vy   = -0x140;
        offset.vx   = 0;
        offset.vz   = 0x400;
        Gp_SpawnEff(EFFECT_SMOKE_PUFF, effectCoord, -0x3FFCB400, &offset);
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->stateFrames = 0;
        D_actor_403100_80155808->subState   += 1;
    }
}
static void func_actor_403100_8013A81C(Task* arg0)
{
    SVECTOR   offset;
    s32       sound;
    s32       sound2;
    s32       pan;
    GfxCoord* effectCoord;
    s32       pan2;
    u16       frame;

    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    if ((s16)frame == 0x31) {
        sound = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0002;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[4]);
        sndEvtRequestScriptStart(sound, pan, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[4]) / 2));
        sound2 = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0001;
        pan2   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(sound2, pan2, (s8)(worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[1]) / 2));
        D_actor_403100_80155808->headPitchPhase = ACTOR_403100_PITCH_PHASE_START;
        Gp_SpawnPadLerp(0x12, 0xFFU, 8U);
        D_actor_403100_80155808->shakeFrames = 0x12;
    }
    if ((u32)(D_actor_403100_80155808->stateFrames - 0x32) < 0xBU) {
        effectCoord = &arg0->extra.tmd->coords[3];
        offset.vy   = -0x140;
        offset.vx   = 0;
        offset.vz   = 0x400;
        Gp_SpawnEff(EFFECT_SMOKE_PUFF, effectCoord, -0x3FFCB400, &offset);
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->state    = 1;
        D_actor_403100_80155808->subState = 0;
    }
}
static void func_actor_403100_8013AA04(Task* arg0)
{
    s32 sound;
    s32 pan;
    u16 angle;
    u16 frame;
    s32 depth;

    angle                                = (u16)D_actor_403100_80155808->rotation.vy;
    frame                                = D_actor_403100_80155808->stateFrames;
    D_actor_403100_80155808->rotation.vy = angle + ((s32)((-0x4000 - (angle * 0x10)) << 0x10) >> 0x16);
    D_actor_403100_80155808->stateFrames = frame + 1;
    if ((u32)((frame - 0x33) & 0xFFFF) < 0x16U) {
        func_actor_403100_8013C7B4(arg0);
    } else if ((func_actor_403100_80133928() << 0x10) != 0) {
        return;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x44) {
        sound = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0008;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[7]);
        depth = worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[7]);
        sndEvtRequestScriptStart(sound, (s32)pan, (s8)(depth / 2));
    }
    if (D_actor_403100_80155808->handTouchedPlayer != 0 || D_actor_403100_80155808->forearmTouchedPlayer != 0) {
        D_actor_403100_80155808->vulnerable    = 1;
        D_actor_403100_80155808->holdingPlayer = 1;
        Gp_StateC08.flags                     |= ATTACHMENT_FLAG_EVENT_LOCK;
        func_actor_403100_8013D1B8(3, 0x3F4);
        func_actor_403100_8013D0B8(D_actor_403100_80155808->playerPosition.vx, D_actor_403100_80155808->playerPosition.vy, (s16)((u16)D_actor_403100_80155808->playerPosition.vz + 0xBB8), 0x800);
        D_actor_403100_80155808->handTouchedPlayer = 0;
        D_actor_403100_80155808->stateFrames       = 0;
        gGameSession->suppressViewTriggers         = 1;
        D_actor_403100_80155808->subState         += 1;
        return;
    }
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->handTouchedPlayer = 0;
        D_actor_403100_80155808->state             = 1;
        D_actor_403100_80155808->subState          = 0;
    }
}
static void func_actor_403100_8013AC04(Task* task)
{
    s32              state;
    s32              message;
    s32              sound;
    Task*            task;
    s32              finished;
    s32              pan;
    s32              depth;
    Task*            player;
    GameActor*       actor;
    u8               completed;
    Actor403100Work* work;

    player   = *gPlayerActorTasks;
    actor    = (GameActor*)player->work;
    finished = 0;
    if ((s16)D_actor_403100_80155808->stateFrames == 0) {
        Gp_SpawnPadLerp(0xC, 0xFF, 0x80);
        sound = (((u16)((Enemy*)player->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 7;
        pan   = (s8)worldCoordGetOriginAudioPan(&player->extra.tmd->coords[1]);
        depth = worldCoordGetOriginAudioDepth(&player->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(sound, pan, (s8)(depth / 2));
    }
    work = D_actor_403100_80155808;
    if ((s16)work->stateFrames < 5) {
        func_actor_403100_8013D0B8(work->playerPosition.vx, work->playerPosition.vy, (s16)(work->playerPosition.vz + 0x3E8), 0x800);
    }
    if (((s16)D_actor_403100_80155808->stateFrames >= 6) || (D_actor_403100_80155808->playerPosition.vz >= 0x1B58)) {
        finished = 1;
    }
    D_actor_403100_80155808->stateFrames = (s16)((u16)D_actor_403100_80155808->stateFrames + 1);
    if ((completed = finished != 0)) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x14;
        task                                                       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        if (taskMessageDispatch(task, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackPair(D_actor_403100_80147614, 2), 0) != 0) {
            gGameSession->suppressDeathChecks     = 1;
            gGameSession->deathSoundCountdown     = GAME_SESSION_DEATH_SOUND_HOLD;
            D_actor_403100_80155808->playerKilled = 1U;
        }
        func_actor_403100_8013D0B8(-0x1928, -0xC7C, 0x29D6, 0x800);
        if (D_actor_403100_80155808->playerKilled != 0) {
            actor->state = 0xA;
            state        = 7;
            message      = 0x3FF;
        } else {
            state   = 2;
            message = 0x3F4;
        }
        func_actor_403100_8013D1B8(state, message);
        D_actor_403100_80155808->stateFrames = 0;
        D_actor_403100_80155808->subState    = (u16)(D_actor_403100_80155808->subState + 1);
    }
}
static void func_actor_403100_8013AE28(Task* task)
{
    Task* player;
    s32   sound;
    s32   sound2;
    s32   sound3;
    s32   pan;
    s32   pan2;
    s32   pan3;
    s32   depth;
    u16   frame;

    player                               = *gPlayerActorTasks;
    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    if ((s16)frame == 0xA) {
        if (D_actor_403100_80155808->sectionDamaged[0] == 0) {
            func_dryfield_night_motel_balcony_8017E250(0, 1);
            D_actor_403100_80155808->sectionDamaged[0] = 1;
        } else {
            func_dryfield_night_motel_balcony_8017E250(0, 2);
        }
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0xB) {
        sound = (((u16)((Enemy*)player->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0006;
        pan   = (s8)worldCoordGetOriginAudioPan(&player->extra.tmd->coords[1]);
        depth = worldCoordGetOriginAudioDepth(&player->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(sound, pan, (s8)(depth / 2));
        Gp_SpawnPadLerp(0x12, 0xFFU, 8U);
        D_actor_403100_80155808->shakeFrames = 0x12;
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x1C) {
        sound2 = (((u16)((Enemy*)player->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0007;
        pan2   = (s8)worldCoordGetOriginAudioPan(&player->extra.tmd->coords[1]);
        depth  = worldCoordGetOriginAudioDepth(&player->extra.tmd->coords[1]);
        sndEvtRequestScriptStart(sound2, pan2, (s8)(depth / 2));
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x20) {
        if (D_actor_403100_80155808->playerKilled != 0) {
            sound3 = (((u16)((Enemy*)player->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x531D000B;
            pan3   = (s8)worldCoordGetOriginAudioPan(&player->extra.tmd->coords[1]);
            depth  = worldCoordGetOriginAudioDepth(&player->extra.tmd->coords[1]);
            sndEvtRequestScriptStart(sound3, pan3, (s8)(depth / 2));
        }
    }
    if (D_actor_403100_80155808->playerKilled == 0) {
        if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
            D_actor_403100_80155808->handTouchedPlayer = 0;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            if (D_actor_403100_8015580C->hp > 0) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 4;
            }
            gGameSession->suppressViewTriggers     = 0;
            D_actor_403100_80155808->vulnerable    = 0;
            D_actor_403100_80155808->holdingPlayer = 0;
            D_actor_403100_80155808->state         = 1;
            D_actor_403100_80155808->subState      = 0;
        }
    } else if ((s16)D_actor_403100_80155808->stateFrames >= 0x32) {
        gGameSession->suppressDeathChecks = 0;
    }
}
static void func_actor_403100_8013B128(Task* arg0)
{
    _Actor403100Flame*  flames;
    Actor403100Work*    work;
    Actor403100Work*    finalWork;
    WorldCollisionBody* body;
    s32                 sound;
    s32                 i;
    s32                 pan;
    s32                 depth;
    GfxCoord*           coords;
    TmdObject*          model;

    model                                  = arg0->extra.tmd;
    coords                                 = model->coords;
    gGameSession->suppressViewTriggers     = 0;
    model->otOffset                        = 0;
    D_actor_403100_8015580C->reactionFlags = 0;
    Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
    D_actor_403100_8015580C->node.state.parts.flags = (WORLD_TARGET_HIDE_HP | WORLD_TARGET_NOT_LOCKABLE);
    i                                               = 0;
    if (D_actor_403100_80155808->fightFramesLeft < 0) {
        arg0->state                       = 4;
        D_actor_403100_80155808->state    = 0;
        D_actor_403100_80155808->subState = 0U;
        return;
    }
    worldTargetUnlinkNode(&D_actor_403100_8015580C->node);
    flames                  = D_actor_403100_80155814;
    body                    = &flames->body;
    D_actor_403100_80155810 = 0;
    for (; i < ARRAY_SIZE(D_actor_403100_80155814); i++) {
        if (flames[i].active != 0) {
            flames[i].active = 0;
            worldCollisionUnlinkBody(body);
        }
        body = &(PARENT_OF(body, _Actor403100Flame, body) + 1)->body;
    }
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 1);
    D_actor_403100_80155808->savedView                         = gGameSession->location.loc.view;
    D_actor_403100_80155808->savedRootMatrix                   = coords->coord;
    D_actor_403100_80155808->savedRotation                     = D_actor_403100_80155808->rotation;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x18;
    work                                                       = D_actor_403100_80155808;
    work->phaseChangeDone                                      = 1;
    work->walkStage                                            = 5;
    work->animationRate                                        = ANIMATION_RATE_ONE;
    work->animationId                                          = 0x10;
    work->animationRequest                                     = ACTOR_403100_ANIMATION_REQUEST_RESET;
    work->aimMode                                              = ACTOR_403100_AIM_ANIMATED;
    D_actor_403100_80155808->armPitch                          = 0;
    D_actor_403100_80155808->armYaw                            = 0;
    D_actor_403100_80155808->forearmTurn.vx                    = 0;
    D_actor_403100_80155808->forearmTurn.vy                    = 0;
    D_actor_403100_80155808->forearmTurn.vz                    = 0;
    coords->coord.t[0]                                         = -0x44C;
    coords->coord.t[1]                                         = -0x1388;
    coords->coord.t[2]                                         = 0x2710;
    D_actor_403100_80155808->rotation.vx                       = 0;
    D_actor_403100_80155808->rotation.vy                       = 0xA00;
    D_actor_403100_80155808->rotation.vz                       = 0;
    gGameSession->hideHud                                      = 1;
    Gp_MsgPlayerWeapon(0);
    sound = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F000B;
    pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[4]);
    depth = worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[4]);
    sndEvtRequestScriptStart(sound, pan, (s8)(depth / 2));
    finalWork              = D_actor_403100_80155808;
    finalWork->stateFrames = 0;
    finalWork->vulnerable  = 1;
    finalWork->subState    = (u16)(finalWork->subState + 1);
}
static void func_actor_403100_8013B3C4(Task* arg0)
{
    s32       sound;
    s32       pan;
    s32       depth;
    GfxCoord* coords;

    coords                                                     = arg0->extra.tmd->coords;
    D_actor_403100_80155808->stateFrames                      += 1;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x18;
    if ((s16)D_actor_403100_80155808->stateFrames == 0x64) {
        Gp_LoadImages(&D_actor_403100_801555EC[0]);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x10E) {
        sound = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0005;
        pan   = (s8)worldCoordGetOriginAudioPan(&arg0->extra.tmd->coords[4]);
        depth = worldCoordGetOriginAudioDepth(&arg0->extra.tmd->coords[4]);
        sndEvtRequestScriptStart(sound, pan, (s8)(depth / 2));
    }
    coords->coord.t[0]                   = -0x44C;
    coords->coord.t[1]                   = -0x1388;
    coords->coord.t[2]                   = 0x2710;
    D_actor_403100_80155808->rotation.vx = 0;
    D_actor_403100_80155808->rotation.vy = 0xA00;
    D_actor_403100_80155808->rotation.vz = 0;
    if (Actor403100_TestFlags()) {
        D_actor_403100_80155808->vulnerable = 0;
        Gp_LinkNode(&D_actor_403100_8015580C->node);
        D_actor_403100_8015580C->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        gGameSession->hideHud                           = 0;
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
        coords->coord                                              = D_actor_403100_80155808->savedRootMatrix;
        D_actor_403100_80155808->rotation                          = D_actor_403100_80155808->savedRotation;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_actor_403100_80155808->savedView;
        D_actor_403100_80155808->state                             = 1;
        D_actor_403100_80155808->subState                          = 0;
    }
}

static void func_actor_403100_8013B5E0(Task* arg0, s16 arg1)
{
    SVECTOR                     headRotation, middleRotation, lowerRotation;
    MATRIX                      worldMatrix;
    VECTOR                      delta, local;
    _Actor403100HeadAimScratch* scratch;
    SVECTOR*                    angles;
    GfxCoord*                   coords;
    GfxCoord*                   head;
    GfxCoord*                   middle;
    GfxCoord*                   lower;
    GfxCoord*                   root;
    MATRIX*                     headLocal;
    MATRIX*                     lowerInverse;
    MATRIX*                     dest;
    s32                         sum;
    s32                         offsetY;
    s32                         part;

    coords  = arg0->extra.tmd->coords;
    head    = &coords[3];
    middle  = &coords[2];
    lower   = &coords[1];
    part    = 3;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor403100HeadAimScratch);
    angles  = &scratch->targetAngles;
    gfxSetRotIdentity(&scratch->aim);
    root = arg0->extra.tmd->coords;
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &root[part].workm, &worldMatrix);
    delta.vx = D_actor_403100_80155808->aimTarget.vx - worldMatrix.t[0];
    offsetY  = worldMatrix.t[1] + 0x600;
    delta.vy = D_actor_403100_80155808->aimTarget.vy - offsetY;
    delta.vz = D_actor_403100_80155808->aimTarget.vz - worldMatrix.t[2];
    ApplyTransposeMatrixLV(&root->coord, &delta, &local);
    angles->vx = (ratan2(-local.vy, local.vz) << 20) >> 20;
    angles->vy = (ratan2(local.vx, local.vz) << 20) >> 20;
    angles->vz = 0;
    if (arg1 == ACTOR_403100_AIM_TRACK) {
        func_actor_403100_8013CEAC((u16*)angles, 8, 0x280, -0x2C0);
        func_actor_403100_8013CF60(angles, 8, 2, 1, 4);
        func_actor_403100_8013D06C();
        RotMatrixZXY(&D_actor_403100_80155808->headAim, &scratch->aim);
    } else if (arg1 == ACTOR_403100_AIM_YAW_ONLY) {
        Gp_MtxToEuler(&coords[part].coord, &headRotation);
        D_actor_403100_80155808->headAim.vx += ((s32)(((u16)headRotation.vx - (u16)D_actor_403100_80155808->headAim.vx) << 20) >> 23);
        D_actor_403100_80155808->headAim.vz += ((s32)(((u16)headRotation.vz - (u16)D_actor_403100_80155808->headAim.vz) << 20) >> 23);
        func_actor_403100_8013CF60(angles, 8, 4, 1, 4);
        RotMatrixZXY(&D_actor_403100_80155808->headAim, &scratch->aim);
    } else if (arg1 == ACTOR_403100_AIM_ANIMATED) {
        Gp_MtxToEuler(&coords[part].coord, &headRotation);
        Gp_MtxToEuler(&coords[2].coord, &middleRotation);
        Gp_MtxToEuler(&coords[1].coord, &lowerRotation);
        sum                                  = (u16)headRotation.vx + ((u16)middleRotation.vx + (u16)lowerRotation.vx);
        headRotation.vx                      = sum;
        headRotation.vy                      = (u16)headRotation.vy + ((u16)middleRotation.vy + (u16)lowerRotation.vy);
        headRotation.vz                      = (u16)headRotation.vz + ((u16)middleRotation.vz + (u16)lowerRotation.vz);
        D_actor_403100_80155808->headAim.vx += ((s32)((sum - (u16)D_actor_403100_80155808->headAim.vx) << 20) >> 23);
        D_actor_403100_80155808->headAim.vy += ((s32)(((u16)headRotation.vy - (u16)D_actor_403100_80155808->headAim.vy) << 20) >> 23);
        D_actor_403100_80155808->headAim.vz += ((s32)(((u16)headRotation.vz - (u16)D_actor_403100_80155808->headAim.vz) << 20) >> 23);
        RotMatrixZXY(&D_actor_403100_80155808->headAim, &scratch->aim);
    } else if (arg1 == ACTOR_403100_AIM_TRACK_FAST) {
        func_actor_403100_8013CEAC((u16*)angles, 0x10, 0x280, -0x280);
        func_actor_403100_8013CF60(angles, 0x10, 4, 2, 8);
        func_actor_403100_8013D06C();
        RotMatrixZXY(&D_actor_403100_80155808->headAim, &scratch->aim);
    } else if (arg1 == ACTOR_403100_AIM_YAW_ONLY_FAST) {
        Gp_MtxToEuler(&coords[part].coord, &headRotation);
        D_actor_403100_80155808->headAim.vx += ((s32)(((u16)headRotation.vx - (u16)D_actor_403100_80155808->headAim.vx) << 20) >> 23);
        D_actor_403100_80155808->headAim.vz += ((s32)(((u16)headRotation.vz - (u16)D_actor_403100_80155808->headAim.vz) << 20) >> 23);
        func_actor_403100_8013CF60(angles, 0x10, 4, 2, 8);
        RotMatrixZXY(&D_actor_403100_80155808->headAim, &scratch->aim);
    } else if (arg1 == ACTOR_403100_AIM_TRACK_STEPPED) {
        func_actor_403100_8013CEAC((u16*)angles, 0x10, 0x280, -0x280);
        func_actor_403100_8013CF60(angles, D_actor_403100_80155808->aimYawStep, 4, 2, 8);
        func_actor_403100_8013D06C();
        RotMatrixZXY(&D_actor_403100_80155808->headAim, &scratch->aim);
    }
    // The head's own rotation is the aim brought into the frame of the two
    // trunk parts it hangs from: (lower * middle)^-1 * aim.
    headLocal = &scratch->local;
    TransposeMatrix(&middle->coord, headLocal);
    lowerInverse = &scratch->lowerInverse;
    TransposeMatrix(&lower->coord, lowerInverse);
    MulMatrix(headLocal, lowerInverse);
    MulMatrix(headLocal, &scratch->aim);
    dest          = &head->coord;
    dest->m[0][0] = headLocal->m[0][0];
    dest->m[0][1] = headLocal->m[0][1];
    dest->m[0][2] = headLocal->m[0][2];
    dest->m[1][0] = headLocal->m[1][0];
    dest->m[1][1] = headLocal->m[1][1];
    dest->m[1][2] = headLocal->m[1][2];
    dest->m[2][0] = headLocal->m[2][0];
    dest->m[2][1] = headLocal->m[2][1];
    dest->m[2][2] = headLocal->m[2][2];
    SCRATCH_STACK_RELEASE_BLOCK(_Actor403100HeadAimScratch);
    lower->composeStamp  = GRAPHICS_COORD_DIRTY;
    middle->composeStamp = GRAPHICS_COORD_DIRTY;
    head->composeStamp   = GRAPHICS_COORD_DIRTY;
}
/// Steps of the behaviour mode `func_actor_403100_8013DB48`, indexed by `subState`.
static const TaskFuncTable6 D_actor_403100_80131F84 = {
    {
        func_actor_403100_801375B8,
        func_actor_403100_801376D8,
        func_actor_403100_801379B4,
        func_actor_403100_80137CA8,
        func_actor_403100_80137DC4,
        func_actor_403100_8013F1D8,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013DC18`, indexed by `subState`.
static const TaskFuncTable5 D_actor_403100_80131F9C = {
    {
        func_actor_403100_80137F4C,
        func_actor_403100_80138048,
        func_actor_403100_8013F230,
        func_actor_403100_8013F270,
        func_actor_403100_8013842C,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013DCAC`, indexed by `subState`.
static const TaskFuncTable9 D_actor_403100_80131FB0 = {
    {
        func_actor_403100_8013F2D8,
        func_actor_403100_8013F344,
        func_actor_403100_80138610,
        func_actor_403100_801386DC,
        func_actor_403100_80138790,
        func_actor_403100_80138844,
        func_actor_403100_80138AB4,
        func_actor_403100_80138C18,
        func_actor_403100_8013F3AC,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013DD78`, indexed by `subState`.
static const TaskFuncTable11 D_actor_403100_80131FD4 = {
    {
        func_actor_403100_80138D08,
        func_actor_403100_80138DB0,
        func_actor_403100_8013F3EC,
        func_actor_403100_80138F88,
        func_actor_403100_8013922C,
        func_actor_403100_801395EC,
        func_actor_403100_80139818,
        func_actor_403100_80139E80,
        func_actor_403100_8013A064,
        func_actor_403100_8013A254,
        func_actor_403100_8013F488,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013DE0C`, indexed by `subState`.
static const TaskFuncTable5 D_actor_403100_80132000 = {
    {
        func_actor_403100_8013A4C8,
        func_actor_403100_8013A5AC,
        func_actor_403100_8013F4E0,
        func_actor_403100_8013F520,
        func_actor_403100_8013A81C,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013DEA0`, indexed by `subState`.
static const TaskFuncTable4 D_actor_403100_80132014 = {
    {
        func_actor_403100_8013F588,
        func_actor_403100_8013AA04,
        func_actor_403100_8013AC04,
        func_actor_403100_8013AE28,
    },
};

/// Steps of the behaviour mode `func_actor_403100_8013DFBC`, indexed by `subState`.
static const TaskFuncTable3 D_actor_403100_80132024 = {
    {
        func_actor_403100_8013F6B0,
        func_actor_403100_8013F6F4,
        func_actor_403100_8013F76C,
    },
};

/// Handlers `func_actor_403100_8013BA64` dispatches on `walkStage`.
static const TaskFuncTable6 D_actor_403100_80132030 = {
    {
        func_actor_403100_8013BB8C,
        func_actor_403100_8013BDE4,
        func_actor_403100_8013F7AC,
        func_actor_403100_8013F7B4,
        func_actor_403100_8013BEF0,
        func_actor_403100_8013F7BC,
    },
};

static void func_actor_403100_8013BA64(Task* arg0)
{
    GfxCoord*      coords   = arg0->extra.tmd->coords;
    TaskFuncTable6 handlers = D_actor_403100_80132030;

    D_actor_403100_80155808->previousStridePhase = (u16)D_actor_403100_80155808->stridePhase;
    handlers.funcs[D_actor_403100_80155808->walkStage](arg0);
    if (((viewGetMappedIndex() & 0xFF) == 7) || ((viewGetMappedIndex() & 0xFF) == 8)) {
        if ((s16)D_actor_403100_80155808->state == 6) {
            if (D_actor_403100_80155808->playerRegion == 2) {
                coords->coord.t[0] += (-4000 - coords->coord.t[0]) >> 4;
            } else {
                coords->coord.t[0] += (-2700 - coords->coord.t[0]) >> 4;
            }
        }
    } else {
        coords->coord.t[0] += (-1100 - coords->coord.t[0]) >> 3;
    }
}
static inline void _actor403100StepRoot(TmdObject* obj, GfxCoord* coords)
{
    s16 mode;
    s32 delta;
    s32 delta2;
    s32 delta3;

    mode = D_actor_403100_80155808->playerRegion - 1;
    switch (mode) {
        case 0:
        case 4:
            obj->otOffset = 0;
            delta         = D_actor_403100_80155808->playerPosition.vz - coords->coord.t[2];
            if (delta > 4864) {
                coords->coord.t[2]                  += D_actor_403100_80155808->walkSpeed;
                D_actor_403100_80155808->stridePhase = (D_actor_403100_80155808->stridePhase + 0x20) & 0x7FF;
            } else if (delta < -4864) {
                coords->coord.t[2]                  -= D_actor_403100_80155808->walkSpeed;
                D_actor_403100_80155808->stridePhase = (D_actor_403100_80155808->stridePhase + 0x20) & 0x7FF;
            } else {
                D_actor_403100_80155808->walkStage++;
            }
            break;
        case 1:
        case 5:
            delta2 = 1300 - coords->coord.t[2];
            if (delta2 > 48) {
                coords->coord.t[2]                  += D_actor_403100_80155808->walkSpeed;
                D_actor_403100_80155808->stridePhase = (D_actor_403100_80155808->stridePhase + 0x20) & 0x7FF;
            } else if (delta2 < -48) {
                coords->coord.t[2]                  -= D_actor_403100_80155808->walkSpeed;
                D_actor_403100_80155808->stridePhase = (D_actor_403100_80155808->stridePhase + 0x20) & 0x7FF;
            } else {
                D_actor_403100_80155808->walkStage++;
            }
            break;
        case 2:
        case 3:
            obj->otOffset = 0;
            delta3        = D_actor_403100_80155808->playerPosition.vz - coords->coord.t[2];
            if (delta3 > 640) {
                coords->coord.t[2]                  += D_actor_403100_80155808->walkSpeed;
                D_actor_403100_80155808->stridePhase = (D_actor_403100_80155808->stridePhase + 0x20) & 0x7FF;
            } else if (delta3 < -640) {
                coords->coord.t[2]                  -= D_actor_403100_80155808->walkSpeed;
                D_actor_403100_80155808->stridePhase = (D_actor_403100_80155808->stridePhase + 0x20) & 0x7FF;
            } else {
                D_actor_403100_80155808->walkStage++;
            }
            break;
    }
}

static void func_actor_403100_8013BB8C(Task* arg0)
{
    s32       sound;
    s32       pan;
    s32       depth;
    GfxCoord* coords;

    coords = arg0->extra.tmd->coords;
    _actor403100StepRoot(arg0->extra.tmd, coords);
    if (D_actor_403100_80155808->stridePhase == 0) {
        if (D_actor_403100_80155808->previousStridePhase != 0) {
            Gp_SpawnPadLerp(0x1E, 0xFF, 8);
            D_actor_403100_80155808->shakeFrames = 0x1E;
            sound                                = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0001;
            pan                                  = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords + 1);
            depth                                = worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords + 1);
            sndEvtRequestScriptStart(sound, pan, (s8)(depth / 2));
        }
    }
    coords->coord.t[1] = -((rsin(D_actor_403100_80155808->stridePhase) << 13) >> 16);
}
static void func_actor_403100_8013BDE4(Task* arg0)
{
    s16       angle;
    s32       sound;
    s32       pan;
    s32       depth;
    GfxCoord* coords;

    coords = arg0->extra.tmd->coords;
    if (D_actor_403100_80155808->stridePhase != 0) {
        angle                                = ((u16)D_actor_403100_80155808->stridePhase + 0x20) & 0x7FF;
        D_actor_403100_80155808->stridePhase = angle;
        if (angle == 0) {
            Gp_SpawnPadLerp(0x1E, 0xFF, 8);
            D_actor_403100_80155808->shakeFrames = 0x1E;
            sound                                = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0001;
            pan                                  = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords + 1);
            depth                                = worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords + 1);
            sndEvtRequestScriptStart(sound, pan, (s8)(depth / 2));
        }
    } else {
        D_actor_403100_80155808->walkStage = 0;
    }
    coords->coord.t[1] = -((rsin(D_actor_403100_80155808->stridePhase) << 13) >> 16);
}
static void func_actor_403100_8013BEF0(Task* arg0)
{
    s16       angle;
    s32       sound;
    s32       pan;
    s32       depth;
    GfxCoord* coords;

    coords = arg0->extra.tmd->coords;
    if (D_actor_403100_80155808->stridePhase != 0) {
        angle                                = ((u16)D_actor_403100_80155808->stridePhase + 0x20) & 0x7FF;
        D_actor_403100_80155808->stridePhase = angle;
        if (angle == 0) {
            Gp_SpawnPadLerp(0x1E, 0xFF, 8);
            D_actor_403100_80155808->shakeFrames = 0x1E;
            sound                                = (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401F0001;
            pan                                  = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords + 1);
            depth                                = worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords + 1);
            sndEvtRequestScriptStart(sound, pan, (s8)(depth / 2));
        }
    } else {
        D_actor_403100_80155808->walkStage++;
    }
    coords->coord.t[1] = -((rsin(D_actor_403100_80155808->stridePhase) << 13) >> 16);
}
static void func_actor_403100_8013C008(s16 arg0, s16 arg1)
{
    POLY_FT4*     poly;
    SpriteSource* entry;
    s32           i;
    u16           clut;

    for (i = 0; i < ARRAY_SIZE(D_actor_403100_801557E0); i++) {
        entry          = &D_actor_403100_801557E0[i];
        poly           = gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setPolyFT4(poly);
        poly->tpage = entry->tpage;
        clut        = entry->clut;
        setShadeTex(poly, 1);
        poly->clut = clut;
        poly->u0   = entry->uv.fields.u0;
        poly->v0   = entry->uv.fields.v0;
        poly->u1   = entry->uv.fields.u0 + (u8)entry->size.fields.w;
        poly->v1   = entry->uv.fields.v0;
        poly->u2   = entry->uv.fields.u0;
        poly->v2   = entry->uv.fields.v0 + (u8)entry->size.fields.h;
        poly->u3   = entry->uv.fields.u0 + (u8)entry->size.fields.w;
        poly->v3   = entry->uv.fields.v0 + (u8)entry->size.fields.h;
        poly->x0   = entry->x0 + arg0;
        poly->y0   = entry->y0 + arg1;
        poly->x1   = arg0 + (entry->x0 + entry->size.fields.w);
        poly->y1   = entry->y0 + arg1;
        poly->x2   = entry->x0 + arg0;
        poly->y2   = arg1 + (entry->y0 + entry->size.fields.h);
        poly->x3   = arg0 + (entry->x0 + entry->size.fields.w);
        poly->y3   = arg1 + (entry->y0 + entry->size.fields.h);
        addPrim((&gGpuCurrentOt[((((u32)(entry->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), poly);
    }
}
static void func_actor_403100_8013C214(Task* arg0)
{
    Task*            playerTask;
    GfxCoord*        soundCoords;
    Task*            task;
    s16              next;
    s16              next2;
    s32              sound;
    s32              randomSound;
    s32              phase;
    s32              x1;
    s32              nextX1;
    s32              x2;
    s32              nextX2;
    s32              x3;
    s32              nextX3;
    s32              x4;
    s32              nextX4;
    s32              sound2;
    s32              pan;
    s32              pan2;
    s32              depth;
    u32              random;
    s32              depth2;
    GfxCoord*        coords;
    Actor403100Work* work;
    s32              sound3, pan3;
    s32              depth3;

    playerTask = *gPlayerActorTasks;
    coords     = arg0->extra.tmd->coords + 6;
    if (D_actor_403100_80155808->playerKilled == 0) {
        if (D_actor_403100_80155808->repromptDelay != 0) {
            next                                   = (u16)D_actor_403100_80155808->repromptDelay - 1;
            D_actor_403100_80155808->repromptDelay = next;
            if (next == 1) {
                D_actor_403100_80155808->promptPending = 1;
            }
        } else {
            D_actor_403100_80155808->auxFrames     = (u16)D_actor_403100_80155808->auxFrames + 1;
            next2                                  = (u16)D_actor_403100_80155808->squeezeFrames + 1;
            D_actor_403100_80155808->squeezeFrames = next2;
            if (next2 >= 0xB4) {
                D_actor_403100_80155808->squeezeFrames = 0;
                task                                   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                if (taskMessageDispatch(task, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, Gp_PackPair(D_actor_403100_80147614, 3), 0) != 0) {
                    sound = (((u16)((Enemy*)(playerTask)->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x531D000B;
                    pan   = (s8)worldCoordGetOriginAudioPan(playerTask->extra.tmd->coords + 1);
                    depth = worldCoordGetOriginAudioDepth(playerTask->extra.tmd->coords + 1);
                    sndEvtRequestScriptStart(sound, (s32)pan, (s8)(depth / 2));
                    gGameSession->deathSoundCountdown = GAME_SESSION_DEATH_SOUND_HOLD;
                    work                              = D_actor_403100_80155808;
                    work->playerKilled                = 1;
                    work->playerDeathFrames           = 0;
                    gGameSession->suppressDeathChecks = 1;
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x400, 0, 0);
                    func_actor_403100_8013D1B8(6, 0x3FF);
                }
                if (D_actor_403100_80155808->playerKilled == 0) {
                    Gp_SpawnPadLerp(0xA, 0xC0U, 0x20U);
                    random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = random;
                    randomSound     = (random >> 0x10) & 3;
                    if (randomSound == 0) {
                        soundCoords = playerTask->extra.tmd->coords + 1;
                        sound2      = (((u16)((Enemy*)(playerTask)->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
                        pan2        = (s8)worldCoordGetOriginAudioPan(soundCoords);
                        depth2      = worldCoordGetOriginAudioDepth(playerTask->extra.tmd->coords + 1);
                        sndEvtRequestScriptStart(sound2, (s32)pan2, (s8)(depth2 / 2));
                    } else if (randomSound == 1) {
                        soundCoords = playerTask->extra.tmd->coords + 1;
                        sound3      = (((u16)((Enemy*)(playerTask)->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 7;
                        pan3        = (s8)worldCoordGetOriginAudioPan(soundCoords);
                        depth3      = worldCoordGetOriginAudioDepth(playerTask->extra.tmd->coords + 1);
                        sndEvtRequestScriptStart(sound3, (s32)pan3, (s8)(depth3 / 2));
                    }
                }
            }
            phase = ((u16)D_actor_403100_80155808->auxFrames >> 5) & 3;
            if (phase == 0) {
                D_actor_403100_80155808->forearmStrokeDone = 0;
                D_actor_403100_80155808->armPitch          = (u16)D_actor_403100_80155808->armPitch + ((s32)(0xD0 - D_actor_403100_80155808->armPitch) >> 1);
                D_actor_403100_80155808->armYaw            = (u16)D_actor_403100_80155808->armYaw + ((s32)(-0x350 - D_actor_403100_80155808->armYaw) >> 1);
                D_actor_403100_80155808->forearmTurn.vx    = (u16)D_actor_403100_80155808->forearmTurn.vx + ((s32)(-0x110 - D_actor_403100_80155808->forearmTurn.vx) >> 1);
                D_actor_403100_80155808->forearmTurn.vy    = (u16)D_actor_403100_80155808->forearmTurn.vy + ((s32)(0x290 - D_actor_403100_80155808->forearmTurn.vy) >> 1);
                D_actor_403100_80155808->forearmTurn.vz    = (u16)D_actor_403100_80155808->forearmTurn.vz + ((s32)(0x60 - D_actor_403100_80155808->forearmTurn.vz) >> 1);
                x1                                         = coords->coord.t[0];
                nextX1                                     = x1 + ((s32)(-0xB80 - x1) >> 3);
                coords->coord.t[0]                         = nextX1;
                if (nextX1 >= -0xBD0) {
                    D_actor_403100_80155808->forearmStrokeDone = 1;
                    return;
                }
            } else if (phase == 1) {
                D_actor_403100_80155808->forearmStrokeDone = 0;
                D_actor_403100_80155808->armPitch          = (u16)D_actor_403100_80155808->armPitch + ((s32)(0x30 - D_actor_403100_80155808->armPitch) >> 2);
                D_actor_403100_80155808->armYaw            = (u16)D_actor_403100_80155808->armYaw + ((s32)(-0xD0 - D_actor_403100_80155808->armYaw) >> 2);
                D_actor_403100_80155808->forearmTurn.vx    = (u16)D_actor_403100_80155808->forearmTurn.vx + ((s32)(-0x150 - D_actor_403100_80155808->forearmTurn.vx) >> 2);
                D_actor_403100_80155808->forearmTurn.vy    = (u16)D_actor_403100_80155808->forearmTurn.vy + ((s32)(0x270 - D_actor_403100_80155808->forearmTurn.vy) >> 2);
                D_actor_403100_80155808->forearmTurn.vz    = (u16)D_actor_403100_80155808->forearmTurn.vz + ((s32)(-0xA0 - D_actor_403100_80155808->forearmTurn.vz) >> 2);
                x2                                         = coords->coord.t[0];
                nextX2                                     = x2 + ((s32)(-0x1180 - x2) >> 2);
                coords->coord.t[0]                         = nextX2;
                if (nextX2 < -0x111F) {
                    D_actor_403100_80155808->forearmStrokeDone = 1;
                }
            } else if (phase == 2) {
                D_actor_403100_80155808->forearmStrokeDone = 0;
                D_actor_403100_80155808->armPitch          = (u16)D_actor_403100_80155808->armPitch + ((s32)(0xD0 - D_actor_403100_80155808->armPitch) >> 2);
                D_actor_403100_80155808->armYaw            = (u16)D_actor_403100_80155808->armYaw + ((s32)(-0x350 - D_actor_403100_80155808->armYaw) >> 2);
                D_actor_403100_80155808->forearmTurn.vx    = (u16)D_actor_403100_80155808->forearmTurn.vx + ((s32)(-0x110 - D_actor_403100_80155808->forearmTurn.vx) >> 2);
                D_actor_403100_80155808->forearmTurn.vy    = (u16)D_actor_403100_80155808->forearmTurn.vy + ((s32)(0x290 - D_actor_403100_80155808->forearmTurn.vy) >> 2);
                D_actor_403100_80155808->forearmTurn.vz    = (u16)D_actor_403100_80155808->forearmTurn.vz + ((s32)(0x60 - D_actor_403100_80155808->forearmTurn.vz) >> 2);
                x3                                         = coords->coord.t[0];
                nextX3                                     = x3 + ((s32)(-0xB80 - x3) >> 2);
                coords->coord.t[0]                         = nextX3;
                if (nextX3 >= -0xBD0) {
                    D_actor_403100_80155808->forearmStrokeDone = 1;
                    return;
                }
            } else if (phase == 3) {
                D_actor_403100_80155808->forearmStrokeDone = 0;
                D_actor_403100_80155808->armPitch          = (u16)D_actor_403100_80155808->armPitch + ((s32)(0x30 - D_actor_403100_80155808->armPitch) >> 1);
                D_actor_403100_80155808->armYaw            = (u16)D_actor_403100_80155808->armYaw + ((s32)(-0xD0 - D_actor_403100_80155808->armYaw) >> 1);
                D_actor_403100_80155808->forearmTurn.vx    = (u16)D_actor_403100_80155808->forearmTurn.vx + ((s32)(-0x150 - D_actor_403100_80155808->forearmTurn.vx) >> 1);
                D_actor_403100_80155808->forearmTurn.vy    = (u16)D_actor_403100_80155808->forearmTurn.vy + ((s32)(0x270 - D_actor_403100_80155808->forearmTurn.vy) >> 1);
                D_actor_403100_80155808->forearmTurn.vz    = (u16)D_actor_403100_80155808->forearmTurn.vz + ((s32)(-0xA0 - D_actor_403100_80155808->forearmTurn.vz) >> 1);
                x4                                         = coords->coord.t[0];
                nextX4                                     = x4 + ((s32)(-0x1180 - x4) >> 2);
                coords->coord.t[0]                         = nextX4;
                if (nextX4 < -0x111F) {
                    D_actor_403100_80155808->forearmStrokeDone = 1;
                }
            }
        }
    }
}
static inline s32 Actor403100CoordToViewInline(GfxCoord* coord, SVECTOR* pos)
{
    SVECTOR local;
    VECTOR  result;
    s32     flag;

    local.vx = pos->vx;
    local.vy = pos->vy;
    local.vz = pos->vz;
    while (1) {
        if (coord->parent == NULL) {
            return 0;
        }
        if (coord == &gGfxViewCoord) {
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            return 1;
        }
        gte_SetTransMatrix(&coord->coord);
        gte_SetRotMatrix(&coord->coord);
        gte_ldv0(&local);
        gte_rtv0tr();
        gte_stlvnl(&result);
        gte_stflg(&flag);
        local.vx = result.vx;
        local.vy = result.vy;
        local.vz = result.vz;
        coord    = coord->parent;
    }
}

static inline void Actor403100ResetStateInline(s16 anim, s16 angle, s16 frame)
{
    D_actor_403100_80155808->animationBlendFrames = frame;
    D_actor_403100_80155808->animationRate        = angle;
    D_actor_403100_80155808->animationId          = anim;
    D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
}

static void func_actor_403100_8013C7B4(Task* arg0)
{
    SVECTOR   pos0, pos1, delta;
    GfxCoord* playerCoord;
    GfxCoord* joint;
    s16       dx0;
    s16       dx1;
    s16       dz0;
    s16       dz1;
    u16       savedRate;
    GfxCoord* coords;
    GfxCoord* second;

    playerCoord = (*gPlayerActorTasks)->extra.tmd->coords;
    savedRate   = (u16)D_actor_403100_80155808->animationRate;
    coords      = arg0->extra.tmd->coords;
    second      = coords + 7;
    Actor403100ResetStateInline(D_actor_403100_80155808->animationId, D_actor_403100_80155808->animationRate, 0);
    func_actor_403100_801327CC();
    func_actor_403100_801328DC(arg0);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    joint                  = coords + 8;
    coords[8].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(joint);
    pos0.vx = 0x160;
    pos0.vy = 0x148;
    pos0.vz = 0x2C0;
    Actor403100CoordToViewInline(joint, &pos0);
    dx0      = (u16)playerCoord->coord.t[0] - (u16)pos0.vx;
    delta.vx = dx0;
    delta.vy = (u16)playerCoord->coord.t[1] - ((u16)pos0.vy + 0x352);
    dz0      = (u16)playerCoord->coord.t[2] - (u16)pos0.vz;
    delta.vz = dz0;
    if ((SquareRoot0((dx0 * dx0) + (dz0 * dz0)) < 0x401) && ((u32)(((u16)delta.vy + 0x351) & 0xFFFF) < 0x6A3U)) {
        D_actor_403100_80155808->handTouchedPlayer = 1;
    }
    pos1.vx = 0x160;
    pos1.vy = 0x148;
    pos1.vz = 0x180;
    Actor403100CoordToViewInline(second, &pos1);
    dx1      = (u16)playerCoord->coord.t[0] - (u16)pos1.vx;
    delta.vx = dx1;
    delta.vy = (u16)playerCoord->coord.t[1] - ((u16)pos1.vy + 0x352);
    dz1      = (u16)playerCoord->coord.t[2] - (u16)pos1.vz;
    delta.vz = dz1;
    if ((SquareRoot0((dx1 * dx1) + (dz1 * dz1)) < 0x401) && ((u32)(((u16)delta.vy + 0x351) & 0xFFFF) < 0x6A3U)) {
        D_actor_403100_80155808->forearmTouchedPlayer = 1;
    }
    D_actor_403100_80155808->animationRate = -(s16)savedRate;
    Actor403100ResetStateInline(D_actor_403100_80155808->animationId, D_actor_403100_80155808->animationRate, 0);
    func_actor_403100_801327CC(arg0);
    D_actor_403100_80155808->animationRate = (s16)savedRate;
    Actor403100ResetStateInline(D_actor_403100_80155808->animationId, D_actor_403100_80155808->animationRate, 0);
}
static void func_actor_403100_8013CBE0(Task* task)
{
    s16          next;
    s16          next2;
    s32          sound;
    register s32 soundId asm("a1");
    s32          pan;
    u32          random;
    s32          depth;
    u8           request;
    u8           state;

    state = D_actor_403100_80155808->jawPitchPhase;
    switch (state) {
        case ACTOR_403100_PITCH_PHASE_REST:
            D_actor_403100_80155808->jawPitchOffset = (s16)((u16)D_actor_403100_80155808->jawPitchOffset + ((s32) - (D_actor_403100_80155808->jawPitchOffset * 0x10) >> 7));
            return;
        case ACTOR_403100_PITCH_PHASE_START:
            request = D_actor_403100_80155808->jawKickSound;
            if (request == state) {
                soundId = 0x401F0009;
                goto play_sound;
            }
            if (request == 2) {
                random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = random;
                if ((random >> 16) & 1) {
                    soundId  = 0x401F0000;
                    soundId |= 2;
                } else {
                    soundId  = 0x401F0000;
                    soundId |= 5;
                }
            play_sound:
                sound = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundId;
                // Keep the selected sound ID in a1 for the bank merge in the call delay slot.
                pan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords + 4);
                depth = worldCoordGetOriginAudioDepth(task->extra.tmd->coords + 4);
                sndEvtRequestScriptStart(sound, pan, (s8)(depth / 2));
            }
            D_actor_403100_80155808->jawKickSound  = 0U;
            D_actor_403100_80155808->jawPitchPhase = D_actor_403100_80155808->jawPitchPhase + 1;
            return;
        case ACTOR_403100_PITCH_PHASE_SWING:
            next                                    = (u16)D_actor_403100_80155808->jawPitchOffset + ((s32)(-0x2200 - (D_actor_403100_80155808->jawPitchOffset * 0x10)) >> 7);
            D_actor_403100_80155808->jawPitchOffset = next;
            if (next < -0x1FF) {
                D_actor_403100_80155808->jawPitchPhase = D_actor_403100_80155808->jawPitchPhase + 1;
                return;
            }
            return;
        case ACTOR_403100_PITCH_PHASE_RETURN:
            next2                                   = (u16)D_actor_403100_80155808->jawPitchOffset + 0xC;
            D_actor_403100_80155808->jawPitchOffset = next2;
            if ((next2 << 16) >= 0) {
                D_actor_403100_80155808->jawPitchPhase = ACTOR_403100_PITCH_PHASE_REST;
            }
            break;
    }
}
static void func_actor_403100_8013CDC0(void)
{
    s16 next;
    s16 next2;
    u8  state;

    state = D_actor_403100_80155808->headPitchPhase;
    switch (state) { /* irregular */
        case ACTOR_403100_PITCH_PHASE_REST:
            D_actor_403100_80155808->headPitchOffset =
                (u16)D_actor_403100_80155808->headPitchOffset +
                ((s32) - (D_actor_403100_80155808->headPitchOffset * 0x10) >> 7);
            return;
        case ACTOR_403100_PITCH_PHASE_START:
            D_actor_403100_80155808->headPitchPhase = ACTOR_403100_PITCH_PHASE_SWING;
            return;
        case ACTOR_403100_PITCH_PHASE_SWING:
            next = (u16)D_actor_403100_80155808->headPitchOffset +
                   ((s32)(0x1E00 - (D_actor_403100_80155808->headPitchOffset * 0x10)) >> 7);
            D_actor_403100_80155808->headPitchOffset = next;
            if (next >= 0x1C0) {
                D_actor_403100_80155808->headPitchPhase =
                    D_actor_403100_80155808->headPitchPhase + 1;
                return;
            }
            return;
        case ACTOR_403100_PITCH_PHASE_RETURN:
            next2                                    = (u16)D_actor_403100_80155808->headPitchOffset - 0xC;
            D_actor_403100_80155808->headPitchOffset = next2;
            if ((next2 << 0x10) <= 0) {
                D_actor_403100_80155808->headPitchPhase = ACTOR_403100_PITCH_PHASE_REST;
            }
            break;
    }
}

static void func_actor_403100_8013CEAC(u16* arg0, s32 arg1, s32 arg2, s16 arg3)
{
    s16 facing;
    s16 target;
    s16 angle;
    u16 targetU;
    u16 facingU;

    angle = *arg0 - 0x140;
    *arg0 = angle;
    if ((angle < (s16)arg2) && (arg3 < (s16)angle)) {
        target  = D_actor_403100_80155808->headAim.vx;
        targetU = (u16)D_actor_403100_80155808->headAim.vx;
        if ((u32)(((s16)angle - target) + 0x20) >= 0x41U) {
            if (target < (s16)angle) {
                D_actor_403100_80155808->headAim.vx = (s16)(targetU + arg1);
                return;
            }
            D_actor_403100_80155808->headAim.vx = (s16)(targetU - arg1);
        }
    } else {
        facing  = D_actor_403100_80155808->headAim.vx;
        facingU = (u16)D_actor_403100_80155808->headAim.vx;
        if (facing >= 0x21) {
            D_actor_403100_80155808->headAim.vx = (s16)(facingU - 0x18);
            return;
        }
        if (facing < -0x20) {
            D_actor_403100_80155808->headAim.vx = (s16)(facingU + 0x18);
        }
    }
}
static void func_actor_403100_8013CF60(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    s16 facing;
    s16 target;
    u16 yaw;
    u16 targetU;
    u16 facingU;

    yaw = (u16)arg0->vy;
    if ((u32)((yaw + 0x27F) & 0xFFFF) < 0x4FFU) {
        target  = D_actor_403100_80155808->headAim.vy;
        targetU = (u16)D_actor_403100_80155808->headAim.vy;
        if ((u32)(((s16)yaw - target) + 0x20) >= 0x41U) {
            if (target < (s16)yaw) {
                D_actor_403100_80155808->headAim.vy  = targetU + arg1;
                D_actor_403100_80155808->rotation.vy = D_actor_403100_80155808->rotation.vy + arg2;
                return;
            }
            D_actor_403100_80155808->headAim.vy  = targetU - arg1;
            D_actor_403100_80155808->rotation.vy = D_actor_403100_80155808->rotation.vy - arg2;
            return;
        }
        facing  = (s16)D_actor_403100_80155808->rotation.vy;
        facingU = D_actor_403100_80155808->rotation.vy;
        if (facing < target) {
            D_actor_403100_80155808->rotation.vy = facingU + arg3;
            return;
        }
        if (target < facing) {
            D_actor_403100_80155808->rotation.vy = facingU - arg3;
        }
    } else {
        if ((s16)yaw >= 0x281) {
            D_actor_403100_80155808->rotation.vy = D_actor_403100_80155808->rotation.vy + arg4;
        }
        if (arg0->vy < -0x280) {
            D_actor_403100_80155808->rotation.vy = D_actor_403100_80155808->rotation.vy - arg4;
        }
    }
}

static void func_actor_403100_8013D06C(void)
{
    if (D_actor_403100_80155808->headAim.vz >= 0x11) {
        D_actor_403100_80155808->headAim.vz = (u16)D_actor_403100_80155808->headAim.vz - 8;
    }
    if (D_actor_403100_80155808->headAim.vz < -0x10) {
        D_actor_403100_80155808->headAim.vz = (u16)D_actor_403100_80155808->headAim.vz + 8;
    }
}

static void func_actor_403100_8013D0B8(s16 arg0, s16 arg1, s16 arg2, s16 arg3)
{
    ActorTransform msg;

    msg.pos.vx = arg0;
    msg.pos.vy = arg1;
    msg.pos.vz = arg2;
    msg.rot.vx = 0;
    msg.rot.vy = arg3;
    msg.rot.vz = 0;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3E9, &msg, 0);
}
static void func_actor_403100_8013D11C(Task* arg0)
{
    GfxCoord*                      coords;
    WorldCoordTransientPointLight* slot;
    WorldCoordPointLight*          light;
    s16                            value;
    u32                            random;

    coords                                        = arg0->extra.tmd->coords;
    slot                                          = &gWorldCoordTransientPointLights[2];
    slot->framesLeft                              = 8;
    light                                         = &slot->light;
    light->inner                                  = 0x300;
    random                                        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    light->outer                                  = 0x3000;
    value                                         = ((random >> 16) & 0x700) + 0x800;
    light->head.color.r                           = value;
    light->head.color.g                           = value >> 3;
    light->head.color.b                           = value >> 4;
    coords                                       += 3;
    light->head.transform.lighting.local.t[0]     = coords->coord.t[0];
    light->head.transform.lighting.local.t[1]     = coords->coord.t[1];
    light->head.transform.lighting.local.t[2]     = coords->coord.t[2];
    gRandomLcgState                               = random;
    slot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
}
static void func_actor_403100_8013D1B8(s16 arg0, s16 arg1)
{
    AnimationPlayRequest msg;

    msg.source.sets                            = _gActor403100PlayerAnimationSets;
    msg.animationId                            = (s32)arg0;
    msg.blend                                  = ANIMATION_BLEND_RESET;
    msg.blendFrames                            = 0;
    msg.enableWorldCollision                   = ANIMATION_WORLD_COLLISION_DISABLE;
    D_actor_403100_80155808->playerAnimationId = (s8)arg0;
    if (arg1 == 0x3FF) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &msg, 0);
    } else if (arg1 == 0x3F4) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
    }
}
static void func_actor_403100_8013D24C(void)
{
    s32 state;

    state = D_actor_403100_80155808->hitTaken;
    if ((state == 1) && (D_actor_403100_80155808->jawPitchPhase == ACTOR_403100_PITCH_PHASE_REST) && (D_actor_403100_80155808->headPitchPhase == ACTOR_403100_PITCH_PHASE_REST)) {
        D_actor_403100_80155808->jawPitchPhase  = state;
        D_actor_403100_80155808->headPitchPhase = state;
        D_actor_403100_80155808->jawKickSound   = state;
    }
}
static void func_actor_403100_8013D2A0(s16 arg0)
{
    if ((D_actor_403100_80155808->jawPitchPhase == ACTOR_403100_PITCH_PHASE_REST) && (D_actor_403100_80155808->headPitchPhase == ACTOR_403100_PITCH_PHASE_REST)) {
        if (arg0 == 1) {
            D_actor_403100_80155808->jawKickSound = 2;
        }
        D_actor_403100_80155808->jawPitchPhase  = ACTOR_403100_PITCH_PHASE_START;
        D_actor_403100_80155808->headPitchPhase = ACTOR_403100_PITCH_PHASE_START;
    }
}

static s32 func_actor_403100_8013D2F4(GfxCoord* coord, MATRIX* matrix)
{
    MATRIX    result;
    MATRIX    parent;
    GfxCoord* current;

    current = coord->parent;
    *matrix = coord->coord;
    while (1) {
        if (current == NULL) {
            return 0;
        }
        if (current == &gGfxViewCoord) {
            return 1;
        }
        parent = current->coord;
        MatrixNormal(&parent, &parent);
        gte_SetRotMatrix(&parent);
        MulRotMatrix(matrix);
        MatrixNormal(matrix, &result);
        *matrix = result;
        current = current->parent;
    }
}
#include "../../shared/coord_math_local_to_world.inc.c"

s32 func_actor_403100_8013D564(Task* arg0, s32 arg1, u16* arg2, s32 arg3)
{
    u16 value;

    switch (arg2[1]) {
        case 10:
            arg0->state                       = 3;
            D_actor_403100_80155808->state    = 0;
            D_actor_403100_80155808->subState = 0;
            break;
        case 0xFFFF:
            D_actor_403100_80155808->state    = 0;
            D_actor_403100_80155808->subState = 0;
            arg0->state                       = 2;
            break;
        default:
            value = arg2[1];
            if (value < 9U) {
                D_actor_403100_80155808->state    = value;
                D_actor_403100_80155808->subState = 0;
                arg0->state                       = 1;
            }
            break;
    }
    D_actor_403100_80155808->subState = 0;
}
s32 func_actor_403100_8013D5F4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    D_actor_403100_80155808->releaseRequested = 1;
}

void func_actor_403100_8013D608(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u16        flags;
    TmdObject* object;

    object = arg0->extra.tmd;
    switch (arg2) {
        case 0:
            object->flags = (object->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW) & (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            return;
        case 1:
            object->flags = object->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
        case 2:
            object->flags                               = object->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
            D_actor_403100_80155808->bufferReleaseDelay = arg2;
            flags                                       = object->flags | TMD_OBJECT_SKIP_AUTO_BUFFER;
            object->flags                               = flags;
            return;
        case 3:
            flags         = (object->flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW) | TMD_OBJECT_SKIP_AUTO_BUFFER;
            object->flags = flags;
            return;
    }
}
static void func_actor_403100_8013D6B4(Task* arg0)
{
    GfxCoord* coord;

    coord                                        = arg0->extra.tmd->coords;
    D_actor_403100_80155808->savedArmPitch       = D_actor_403100_80155808->armPitch;
    D_actor_403100_80155808->savedArmYaw         = D_actor_403100_80155808->armYaw;
    D_actor_403100_80155808->savedForearmTurn.vx = D_actor_403100_80155808->forearmTurn.vx;
    D_actor_403100_80155808->savedForearmTurn.vy = D_actor_403100_80155808->forearmTurn.vy;
    D_actor_403100_80155808->savedForearmTurn.vz = D_actor_403100_80155808->forearmTurn.vz;
    D_actor_403100_80155808->savedForearmX       = coord[6].coord.t[0];
    D_actor_403100_80155808->savedAuxFrames      = D_actor_403100_80155808->auxFrames;
}
static void func_actor_403100_8013D700(Task* arg0)
{
    GfxCoord* coord;

    coord                                   = arg0->extra.tmd->coords;
    D_actor_403100_80155808->armPitch       = D_actor_403100_80155808->savedArmPitch;
    D_actor_403100_80155808->armYaw         = D_actor_403100_80155808->savedArmYaw;
    D_actor_403100_80155808->forearmTurn.vx = D_actor_403100_80155808->savedForearmTurn.vx;
    D_actor_403100_80155808->forearmTurn.vy = D_actor_403100_80155808->savedForearmTurn.vy;
    D_actor_403100_80155808->forearmTurn.vz = D_actor_403100_80155808->savedForearmTurn.vz;
    coord[6].coord.t[0]                     = D_actor_403100_80155808->savedForearmX;
    D_actor_403100_80155808->auxFrames      = D_actor_403100_80155808->savedAuxFrames;
}
static void func_actor_403100_8013D74C(Task* arg0)
{
    D_actor_403100_80155808->auxFrames         = 0;
    D_actor_403100_80155808->forearmStrokeDone = 0;
    D_actor_403100_80155808->repromptDelay     = 0;
    D_actor_403100_80155808->squeezeFrames     = 0;
}
static void func_actor_403100_8013D770(Task* arg0)
{
    _actor403100TurnPart6(arg0);
}
static void func_actor_403100_8013D88C(Task* arg0)
{
    worldCollisionUnlinkBody(&D_actor_403100_80155808->headBody);
    worldCollisionUnlinkBody(&D_actor_403100_80155808->trunkBody);
    worldCollisionUnlinkBody(&D_actor_403100_80155808->handAttack);
    worldCollisionUnlinkBody(&D_actor_403100_80155808->forearmAttack);
    enemyDestroy(arg0->spawnArg2.pointer, arg0);
}

static void func_actor_403100_8013D8F4(Task* arg0)
{
    D_actor_403100_80155808->bufferReleaseDelay = -1;
    D_actor_403100_80155808->sceneScale         = 0x1400;
    Gp_StateC08.flags                           = Gp_StateC08.flags | ATTACHMENT_FLAG_EVENT_LOCK;
    gGameSession->suppressViewTriggers          = 0;
    D_actor_403100_8015580C->reactionFlags      = 0;
    Gp_SetLightMode(arg0->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 0xA);
    worldTargetUnlinkNode(&D_actor_403100_8015580C->node);
    func_800E8614(D_actor_335800_80165FC0, 0);
    arg0->state                       = 1;
    D_actor_403100_80155808->state    = 0;
    D_actor_403100_80155808->subState = 0;
    D_actor_403100_80155808->state    = 9;
    D_actor_403100_80155808->subState = 0;
}
static s32 func_actor_403100_8013D9C4(s16 x, s16 y, _Actor403100Zone* zone)
{
    while (zone->id != ACTOR_403100_ZONE_END) {
        if (x >= zone->x && zone->x + zone->width >= x &&
            y >= zone->z && zone->z + zone->depth >= y) {
            return zone->id;
        }
        zone++;
    }
    return 0;
}
static void func_actor_403100_8013DA6C(Task* task)
{
    TaskFunc fns[2] = { func_actor_403100_8013712C, func_actor_403100_8013F12C };

    fns[(s16)D_actor_403100_80155808->subState](task);
}

static void func_actor_403100_8013DAC4(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80131F60;
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        sp.funcs[(s16)D_actor_403100_80155808->subState](arg0);
    }
}
static void func_actor_403100_8013DB48(Task* arg0)
{
    TaskFuncTable6 sp;

    sp = D_actor_403100_80131F84;
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        sp.funcs[(s16)D_actor_403100_80155808->subState](arg0);
        func_actor_403100_80132C3C(arg0, 6, 7, 0x400, -0xC80);
        func_actor_403100_80132C3C(arg0, 7, 8, 0x400, -0xC80);
    }
}
static void func_actor_403100_8013DC18(Task* arg0)
{
    TaskFuncTable5 sp;

    sp = D_actor_403100_80131F9C;
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        sp.funcs[(s16)D_actor_403100_80155808->subState](arg0);
    }
}
static void func_actor_403100_8013DCAC(Task* arg0)
{
    TaskFuncTable9 sp;

    sp = D_actor_403100_80131FB0;
    sp.funcs[(s16)D_actor_403100_80155808->subState](arg0);
    func_actor_403100_80132C3C(arg0, 6, 7, 0x400, -0xC80);
    func_actor_403100_80132C3C(arg0, 7, 8, 0x400, -0xC80);
}
static void func_actor_403100_8013DD78(Task* arg0)
{
    TaskFuncTable11 sp;

    sp = D_actor_403100_80131FD4;
    sp.funcs[(s16)D_actor_403100_80155808->subState](arg0);
}
static void func_actor_403100_8013DE0C(Task* arg0)
{
    TaskFuncTable5 sp;

    sp = D_actor_403100_80132000;
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        sp.funcs[(s16)D_actor_403100_80155808->subState](arg0);
    }
}
static void func_actor_403100_8013DEA0(Task* arg0)
{
    TaskFuncTable4 handlers;

    handlers = D_actor_403100_80132014;
    handlers.funcs[(s16)D_actor_403100_80155808->subState](arg0);
}
static void func_actor_403100_8013DF0C(Task* task)
{
    void (*fns[2])(void) = { func_actor_403100_8013F610, func_actor_403100_8013F658 };

    fns[(s16)D_actor_403100_80155808->subState]();
}
static void func_actor_403100_8013DF64(Task* task)
{
    TaskFunc fns[2] = { func_actor_403100_8013B128, func_actor_403100_8013B3C4 };

    fns[(s16)D_actor_403100_80155808->subState](task);
}
static void func_actor_403100_8013DFBC(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80132024;
    func_actor_403100_8013D24C();
    sp.funcs[(s16)D_actor_403100_80155808->subState](arg0);
}
static void func_actor_403100_8013E02C(s16 arg0, s16 arg1, s16 arg2)
{
    D_actor_403100_80155808->animationBlendFrames = arg2;
    D_actor_403100_80155808->animationRate        = arg1;
    D_actor_403100_80155808->animationId          = arg0;
    D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
}

void func_actor_403100_8013E04C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80131E70;
    sp.funcs[task->state](task);
}

void func_actor_403100_8013E0A4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80131E7C;
    sp.funcs[task->state](task);
}

/// Runs the handler for the task's current top-level state.
void func_actor_403100_8013E0FC(Task* arg0)
{
    TaskFuncTable6 sp;

    sp = D_actor_403100_80131F1C;
    sp.funcs[arg0->state](arg0);
}

static void func_actor_403100_8013E16C(void)
{
}

static void func_actor_403100_8013E174(void)
{
    s16 timer;

    if (gPlayerStatus.hp > 0) {
        timer                                         = (u16)D_actor_403100_80155808->playerReactionFrames - 1;
        D_actor_403100_80155808->playerReactionFrames = timer;
        if (timer < 0) {
            func_actor_403100_8013D1B8(5, 0x3F4);
            D_actor_403100_80155808->playerReactionStage = ACTOR_403100_PLAYER_REACTION_HIT_ENDING;
            return;
        }
        func_actor_403100_8013D1B8(5, 0x3F4);
    }
}
static void func_actor_403100_8013E1E4(void)
{
    AnimationPlayRequest sp;

    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        _gActor403100PlayerAnimationSets[4] = (Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])->table.sets[7];
        sp.source.sets                      = _gActor403100PlayerAnimationSets;
        sp.blend                            = ANIMATION_BLEND_INTERPOLATE;
        sp.blendFrames                      = 3;
        sp.enableWorldCollision             = ANIMATION_WORLD_COLLISION_DISABLE;
        sp.animationId                      = 4;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, &sp, 0);
        D_actor_403100_80155808->playerReactionStage = ACTOR_403100_PLAYER_REACTION_RECOVERING;
    }
}
static void func_actor_403100_8013E2BC(void)
{
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 2, 0);
        D_actor_403100_80155808->playerReactionFrames = 0;
        D_actor_403100_80155808->playerAnimationId    = 0;
        D_actor_403100_80155808->handTouchedPlayer    = 0;
        D_actor_403100_80155808->playerReactionStage  = ACTOR_403100_PLAYER_REACTION_NONE;
    }
}
static s32 func_actor_403100_8013E33C(GfxCoord* arg0, MATRIX* arg1, GfxCoord* arg2)
{
    MATRIX    matrix;
    GfxCoord* coord;

    coord = arg0->parent;
    *arg1 = arg0->coord;
    while (1) {
        if (coord == NULL) {
            return 0;
        }
        if (coord == arg2) {
            return 1;
        }
        gte_SetRotMatrix(&coord->coord);
        MulRotMatrix(arg1);
        MatrixNormal(arg1, &matrix);
        *arg1 = matrix;
        coord = coord->parent;
    }
}
static s32 func_actor_403100_8013E450(GfxCoord* arg0, MATRIX* arg1, GfxCoord* arg2)
{
    MATRIX    matrix;
    MATRIX    normal;
    MATRIX    transposed;
    GfxCoord* coord;

    coord = arg0->parent;
    if (coord == &gGfxViewCoord) {
        return 0;
    }
    matrix = coord->coord;
    while (1) {
        coord = coord->parent;
        if (coord == NULL) {
            return 0;
        }
        if (coord == arg2) {
            break;
        }
        gte_SetRotMatrix(&coord->coord);
        MulRotMatrix(&matrix);
        MatrixNormal(&matrix, &normal);
        matrix = normal;
    }
    gte_TransposeMatrix(&matrix, &transposed);
    gte_SetRotMatrix(&transposed);
    MulRotMatrix(arg1);
    return 1;
}
static void func_actor_403100_8013E5FC(Task* task)
{
    D_actor_403100_8015580C->recs        = 0;
    D_actor_403100_80155808->stateFrames = 0;
    D_actor_403100_80155808->state      += 1;
}
static void func_actor_403100_8013E624(Task* arg0)
{
    u16 timer;

    timer                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = timer;
    if ((s16)timer == 0x12) {
        Gp_ReleaseStateF0Add(arg0, 0);
    }
    if ((s16)D_actor_403100_80155808->stateFrames == 0x168) {
        arg0->state                       = 5;
        D_actor_403100_80155808->state    = 0;
        D_actor_403100_80155808->subState = 0;
    }
}
static void func_actor_403100_8013E6A0(Task* arg0)
{
    GfxCoord* coord;

    coord               = arg0->extra.tmd->coords;
    arg0->killCountdown = 0x5A;
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    arg0->state         = arg0->state + 1;
    func_actor_403100_8013E6F0(arg0);
}

static void func_actor_403100_8013E6F0(Task* arg0)
{
    GfxCoord* coord;
    u16       countdown;

    coord = arg0->extra.tmd->coords;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        if (!(arg0->killCountdown & 7)) {
            Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, 0x80020400, NULL);
        }
        countdown           = arg0->killCountdown - 1;
        arg0->killCountdown = countdown;
        if ((countdown << 0x10) <= 0) {
            arg0->killCountdown = 0;
            arg0->state         = arg0->state + 1;
            taskKill(arg0);
        }
    }
}

static void func_actor_403100_8013E784(Task* arg0)
{
    u16 temp_v0;

    temp_v0             = arg0->killCountdown + 1;
    arg0->killCountdown = temp_v0;
    if ((s16)temp_v0 >= 0x1E) {
        taskKill(arg0);
    }
}

static void func_actor_403100_8013E7C8(Task* arg0)
{
    GfxCoord* coord;
    GfxCoord* coord2;
    u16       countdown;

    coord2               = arg0->extra.tmd->coords;
    arg0->killCountdown  = 0x5A;
    coord2->parent       = &gGfxViewCoord;
    coord2->composeStamp = GRAPHICS_COORD_DIRTY;
    arg0->state         += 1;
    coord                = arg0->extra.tmd->coords;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        if (!(arg0->killCountdown & 7)) {
            Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, 0x80020400, NULL);
        }
        countdown           = arg0->killCountdown - 1;
        arg0->killCountdown = countdown;
        if ((countdown << 0x10) <= 0) {
            arg0->killCountdown = 0;
            arg0->state         = arg0->state + 1;
            taskKill(arg0);
        }
    }
}

static void func_actor_403100_8013E88C(Task* arg0)
{
    GfxCoord* coord;
    u16       countdown;

    coord = arg0->extra.tmd->coords;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        if (!(arg0->killCountdown & 7)) {
            Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, 0x20400, NULL);
        }
        countdown           = arg0->killCountdown - 1;
        arg0->killCountdown = countdown;
        if ((countdown << 0x10) <= 0) {
            arg0->killCountdown = 0;
            arg0->state         = arg0->state + 1;
            taskKill(arg0);
        }
    }
}

static void func_actor_403100_8013E920(Task* arg0)
{
    u16 temp_v0;

    temp_v0             = arg0->killCountdown + 1;
    arg0->killCountdown = temp_v0;
    if ((s16)temp_v0 >= 0x1E) {
        taskKill(arg0);
    }
}
static void func_actor_403100_8013E964(Task* task)
{
}

static void func_actor_403100_8013E96C(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80131EB0;
    sp.funcs[(s16)D_actor_403100_80155808->subState](arg0);
    func_actor_403100_801327CC(arg0);
}
static void func_actor_403100_8013E9D8(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80131EBC;
    sp.funcs[(s16)D_actor_403100_80155808->subState](arg0);
    func_actor_403100_801327CC(arg0);
    func_actor_403100_8013B5E0(arg0, D_actor_403100_80155808->aimMode);
}
static void func_actor_403100_8013EA60(Task* arg0)
{
    TaskFuncTable4 handlers;
    TmdObject*     obj;

    obj        = arg0->extra.tmd;
    handlers   = D_actor_403100_80131EC8;
    obj->flags = 0;
    handlers.funcs[(s16)D_actor_403100_80155808->subState](arg0);
}
static void func_actor_403100_8013EAD4(Task* arg0)
{
    TaskFuncTable3 sp;
    TmdObject*     obj;

    obj        = arg0->extra.tmd;
    sp         = D_actor_403100_80131ED8;
    obj->flags = 0;
    sp.funcs[(s16)D_actor_403100_80155808->subState](arg0);
    func_actor_403100_801327CC(arg0);
    func_actor_403100_8013B5E0(arg0, D_actor_403100_80155808->aimMode);
}
static void func_actor_403100_8013EB68(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80131EE4;
    sp.funcs[(s16)D_actor_403100_80155808->subState](arg0);
}
static void func_actor_403100_8013EBC8(Task* arg0)
{
    TaskFuncTable4 handlers;
    TmdObject*     obj;

    obj        = arg0->extra.tmd;
    handlers   = D_actor_403100_80131EF0;
    obj->flags = 0;
    handlers.funcs[(s16)D_actor_403100_80155808->subState](arg0);
    func_actor_403100_801327CC(arg0);
}
static void func_actor_403100_8013EC4C(Task* arg0)
{
    TaskFuncTable4 handlers;
    TmdObject*     obj;

    obj        = arg0->extra.tmd;
    handlers   = D_actor_403100_80131F00;
    obj->flags = 0;
    handlers.funcs[(s16)D_actor_403100_80155808->subState](arg0);
    func_actor_403100_801327CC(arg0);
}
static void func_actor_403100_8013ECD0(Task* arg0)
{
    TaskFuncTable3 sp;
    TmdObject*     obj;

    obj        = arg0->extra.tmd;
    sp         = D_actor_403100_80131F10;
    obj->flags = 0;
    sp.funcs[(s16)D_actor_403100_80155808->subState](arg0);
    func_actor_403100_801327CC(arg0);
}
static void func_actor_403100_8013ED48(Task* task)
{
}

static void func_actor_403100_8013ED50(Task* arg0)
{
    GfxCoord* coord;

    coord                                     = arg0->extra.tmd->coords;
    D_actor_403100_80155808->sceneScale       = 0x1400;
    D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
    D_actor_403100_80155808->animationId      = 3;
    D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
    func_actor_403100_801327CC();
    D_actor_403100_80155808->rotation.vy = 0;
    coord->coord.t[0]                    = -0x2710;
    coord->coord.t[1]                    = -0x258;
    coord->coord.t[2]                    = -0x2328;
    D_actor_403100_80155808->stridePhase = 0;
    D_actor_403100_80155808->stateFrames = 0;
    D_actor_403100_80155808->subState    = D_actor_403100_80155808->subState + 1;
}

static void func_actor_403100_8013EDDC(Task* task)
{
    u16 frame;

    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    if ((s16)frame == 0x1E) {
        func_dryfield_night_motel_balcony_8017E128(1);
    }
}

static void func_actor_403100_8013EE28(Task* arg0)
{
    GfxCoord* coord;

    coord                                     = arg0->extra.tmd->coords;
    D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
    D_actor_403100_80155808->aimMode          = ACTOR_403100_AIM_ANIMATED;
    D_actor_403100_80155808->animationId      = 3;
    D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
    func_actor_403100_801327CC(arg0);
    D_actor_403100_80155808->rotation.vy = 0;
    coord->coord.t[0]                    = -0x1710;
    coord->coord.t[1]                    = 0;
    coord->coord.t[2]                    = -0x2846;
    D_actor_403100_80155808->sceneScale  = 0x1910;
    D_actor_403100_80155808->stateFrames = 0;
    D_actor_403100_80155808->subState   += 1;
}
static void func_actor_403100_8013EEB0(Task* task)
{
}

static void func_actor_403100_8013EEB8(Task* arg0)
{
    GfxCoord* coord;

    coord                                 = arg0->extra.tmd->coords;
    D_actor_403100_80155808->stateFrames += 1;
    coord->coord.t[2]                    += 0x64;
    if ((s16)D_actor_403100_80155808->stateFrames == 0x20) {
        D_actor_403100_80155808->stateFrames = 0;
        D_actor_403100_80155808->subState   += 1;
    }
    func_actor_403100_801327CC();
}
static void func_actor_403100_8013EF24(Task* task)
{
}

static void func_actor_403100_8013EF2C(Task* task)
{
}

static void func_actor_403100_8013EF34(Task* task)
{
    D_actor_403100_80155808->stateFrames = 0;
    D_actor_403100_80155808->auxFrames   = 0;
    D_actor_403100_80155808->subState    = D_actor_403100_80155808->subState + 1;
}

static void func_actor_403100_8013EF58(Task* task)
{
}

static void func_actor_403100_8013EF60(Task* task)
{
    if (Actor403100_TestFlags12C()) {
        D_actor_403100_80155808->animationBlendFrames = 0xB4;
        D_actor_403100_80155808->animationRate        = ANIMATION_RATE_ONE;
        D_actor_403100_80155808->animationId          = 1;
        D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
        D_actor_403100_80155808->subState            += 1;
    }
}
static void func_actor_403100_8013EFC0(Task* task)
{
}

static void func_actor_403100_8013EFC8(Task* arg0)
{
    TmdObject* obj;
    GfxCoord*  coord;

    obj               = arg0->extra.tmd;
    obj->otOffset     = 0x10;
    coord             = obj->coords;
    coord->coord.t[0] = -0x49C;
    coord->coord.t[2] = 0x1130;
    coord->coord.t[1] = 0;

    D_actor_403100_80155808->rotation.vy      = 0xC00;
    D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
    D_actor_403100_80155808->animationId      = 0x14;
    D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
    D_actor_403100_80155808->sceneScale       = 0x1400;
    D_actor_403100_80155808->stateFrames      = 0;
    D_actor_403100_80155808->subState         = D_actor_403100_80155808->subState + 1;
}

static void func_actor_403100_8013F034(Task* arg0)
{
    GfxCoord* coord;

    coord             = arg0->extra.tmd->coords;
    coord->coord.t[0] = -0xAF0;
    coord->coord.t[1] = 0x300;
    coord->coord.t[2] = -0xE74;

    D_actor_403100_80155808->rotation.vy      = 0x800;
    D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
    D_actor_403100_80155808->animationId      = 0x18;
    D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
    D_actor_403100_80155808->sceneScale       = 0x1400;
    D_actor_403100_80155808->stridePhase      = 0;
    D_actor_403100_80155808->stateFrames      = 0;
    D_actor_403100_80155808->auxFrames        = 0;
    D_actor_403100_80155808->subState         = D_actor_403100_80155808->subState + 1;
}

static void func_actor_403100_8013F0A8(Task* arg0)
{
    TmdObject* obj;

    obj                                = arg0->extra.tmd;
    obj->flags                        |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    D_actor_403100_8014762C.bp         = 0;
    D_actor_403100_8014762C.mp         = 0;
    D_actor_403100_8014762C.exp      >>= 1;
    gGameSession->location.loc.variant = 4;
    Gp_ReleaseStateF0Add(arg0, 0);
    arg0->state                       = 5;
    D_actor_403100_80155808->state    = 0;
    D_actor_403100_80155808->subState = 0;
}
static void func_actor_403100_8013F12C(Task* task)
{
    u16 frame;

    if ((func_actor_403100_80133928() << 0x10) == 0) {
        frame                                = D_actor_403100_80155808->stateFrames + 1;
        D_actor_403100_80155808->stateFrames = frame;
        if ((s16)frame >= 0x1F) {
            D_actor_403100_80155808->state    = 1;
            D_actor_403100_80155808->subState = 0;
        }
    }
}

static void func_actor_403100_8013F18C(Task* task)
{
    D_actor_403100_80155808->animationBlendFrames = 4;
    D_actor_403100_80155808->animationRate        = ANIMATION_RATE_ONE;
    D_actor_403100_80155808->animationId          = 1;
    D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
    D_actor_403100_80155808->walkSpeed            = 0x20;
    D_actor_403100_80155808->walkStage            = 0;
    D_actor_403100_80155808->aimMode              = ACTOR_403100_AIM_TRACK;
    D_actor_403100_80155808->stateFrames          = 0;
    D_actor_403100_80155808->subState             = D_actor_403100_80155808->subState + 1;
}

static void func_actor_403100_8013F1D8(Task* task)
{
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->state    = 1;
        D_actor_403100_80155808->subState = 0;
    }
}
static void func_actor_403100_8013F230(Task* task)
{
    u16 frame;

    frame                                = D_actor_403100_80155808->stateFrames;
    D_actor_403100_80155808->stateFrames = frame + 1;
    if ((s16)frame >= 0xD) {
        D_actor_403100_80155808->state    = 1;
        D_actor_403100_80155808->subState = 0;
    }
}

static void func_actor_403100_8013F270(Task* task)
{
    D_actor_403100_80155808->stateFrames = 0;
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 0xA);
    D_actor_403100_80155808->animationBlendFrames = 0x14;
    D_actor_403100_80155808->animationRate        = 0x1C;
    D_actor_403100_80155808->animationId          = 3;
    D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
    D_actor_403100_80155808->subState             = D_actor_403100_80155808->subState + 1;
}

static void func_actor_403100_8013F2D8(Task* task)
{
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        D_actor_403100_80155808->handTouchedPlayer    = 0;
        D_actor_403100_80155808->forearmTouchedPlayer = 0;
        D_actor_403100_80155808->aimMode              = ACTOR_403100_AIM_YAW_ONLY_FAST;
        D_actor_403100_80155808->walkStage            = 4;
        D_actor_403100_80155808->armPitch             = 0;
        D_actor_403100_80155808->armYaw               = 0;
        D_actor_403100_80155808->subState             = D_actor_403100_80155808->subState + 1;
    }
}

static void func_actor_403100_8013F344(Task* task)
{
    if (((func_actor_403100_80133928() << 0x10) == 0) &&
        (D_actor_403100_80155808->walkStage == 5)) {
        D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
        D_actor_403100_80155808->animationId      = 4;
        D_actor_403100_80155808->stateFrames      = 0;
        D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
        D_actor_403100_80155808->subState         = D_actor_403100_80155808->subState + 1;
    }
}

static void func_actor_403100_8013F3AC(Task* task)
{
    u16 frame;

    frame                                = D_actor_403100_80155808->stateFrames + 1;
    D_actor_403100_80155808->stateFrames = frame;
    if ((s16)frame >= 0x5A) {
        D_actor_403100_80155808->state    = 1;
        D_actor_403100_80155808->subState = 0;
    }
}

static void func_actor_403100_8013F3EC(Task* arg0)
{
    GfxCoord* coord;

    coord                                     = arg0->extra.tmd->coords;
    D_actor_403100_80155808->walkStage        = 5;
    coord->coord.t[0]                         = -0x44C;
    coord->coord.t[1]                         = -0x1388;
    coord->coord.t[2]                         = 0x2710;
    D_actor_403100_80155808->rotation.vy      = 0xA00;
    D_actor_403100_80155808->animationRate    = ANIMATION_RATE_ONE;
    D_actor_403100_80155808->animationId      = 0xB;
    D_actor_403100_80155808->rotation.vx      = 0;
    D_actor_403100_80155808->rotation.vz      = 0;
    D_actor_403100_80155808->animationRequest = ACTOR_403100_ANIMATION_REQUEST_RESET;
    func_actor_403100_80132528(arg0);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x18;
    D_actor_403100_80155808->overlayX                          = 0;
    D_actor_403100_80155808->overlayY                          = 0;
    D_actor_403100_80155808->subState                         += 1;
}
static void func_actor_403100_8013F488(Task* task)
{
    if (Actor403100_TestFlags()) {
        D_actor_403100_80155808->state    = 1;
        D_actor_403100_80155808->subState = 0;
    }
}
static void func_actor_403100_8013F4E0(Task* task)
{
    u16 frame;

    frame                                = D_actor_403100_80155808->stateFrames;
    D_actor_403100_80155808->stateFrames = frame + 1;
    if ((s16)frame >= 0xD) {
        D_actor_403100_80155808->state    = 1;
        D_actor_403100_80155808->subState = 0;
    }
}
static void func_actor_403100_8013F520(Task* task)
{
    D_actor_403100_80155808->stateFrames = 0;
    SndEvt_EnqueueType7(SOUND_CHARACTER(SOUND_BANK_BURNER, 4), 0xA);
    D_actor_403100_80155808->animationBlendFrames = 0x14;
    D_actor_403100_80155808->animationRate        = 0x1C;
    D_actor_403100_80155808->animationId          = 3;
    D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
    D_actor_403100_80155808->subState            += 1;
}
static void func_actor_403100_8013F588(Task* task)
{
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        D_actor_403100_80155808->walkStage            = 4;
        D_actor_403100_80155808->animationRate        = 0xC;
        D_actor_403100_80155808->aimMode              = ACTOR_403100_AIM_ANIMATED;
        D_actor_403100_80155808->animationId          = 8;
        D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_RESET;
        D_actor_403100_80155808->stateFrames          = 0;
        D_actor_403100_80155808->rotation.vy          = (u16)D_actor_403100_80155808->rotation.vy & 0xFFF;
        D_actor_403100_80155808->handTouchedPlayer    = 0;
        D_actor_403100_80155808->forearmTouchedPlayer = 0;
        D_actor_403100_80155808->subState            += 1;
    }
}
static void func_actor_403100_8013F610(void)
{
    D_actor_403100_80155808->aimMode              = ACTOR_403100_AIM_ANIMATED;
    D_actor_403100_80155808->walkStage            = 4;
    D_actor_403100_80155808->animationBlendFrames = 8;
    D_actor_403100_80155808->animationRate        = ANIMATION_RATE_ONE;
    D_actor_403100_80155808->animationId          = 0xA;
    D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
    D_actor_403100_80155808->subState             = D_actor_403100_80155808->subState + 1;
}

static void func_actor_403100_8013F658(void)
{
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->state    = 1;
        D_actor_403100_80155808->subState = 0;
    }
}
static void func_actor_403100_8013F6B0(Task* task)
{
    D_actor_403100_80155808->aimMode              = ACTOR_403100_AIM_ANIMATED;
    D_actor_403100_80155808->walkStage            = 4;
    D_actor_403100_80155808->animationBlendFrames = 0xA;
    D_actor_403100_80155808->animationRate        = 8;
    D_actor_403100_80155808->animationId          = 0xA;
    D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
    D_actor_403100_80155808->subState            += 1;
}
static void func_actor_403100_8013F6F4(Task* task)
{
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->animationBlendFrames = 4;
        D_actor_403100_80155808->animationRate        = ANIMATION_RATE_ONE;
        D_actor_403100_80155808->stateFrames          = 0;
        D_actor_403100_80155808->animationId          = 1;
        D_actor_403100_80155808->animationRequest     = ACTOR_403100_ANIMATION_REQUEST_BLEND;
        D_actor_403100_80155808->subState            += 1;
    }
}
static void func_actor_403100_8013F76C(Task* task)
{
    u16 frame;

    frame                                = D_actor_403100_80155808->stateFrames;
    D_actor_403100_80155808->stateFrames = frame + 1;
    if ((s16)frame >= 0x3D) {
        D_actor_403100_80155808->state    = 1;
        D_actor_403100_80155808->subState = 0;
    }
}

static void func_actor_403100_8013F7AC(Task* task)
{
}

static void func_actor_403100_8013F7B4(Task* task)
{
}

static void func_actor_403100_8013F7BC(Task* task)
{
}
