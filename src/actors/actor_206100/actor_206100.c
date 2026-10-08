#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/memory.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
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

#include "overlay.h"

#include "rooms/neo_ark_submarine_gallery.h"
#include "../../shared/screen_wave.h"
#include "../../shared/coord_math.h"
#include "../../shared/diver.h"

extern TaskDesc D_actor_206100_80158B0C[];

/// The `worldCollisionLinkBody` record `_actor206100UnlinkForDeath` unlinks when it
/// retires the actor, plus the area-record list that handler applies.

/// The attack a shot's sphere carries at `WorldCollisionBody.key`.
/// `power` is 0x1A and `reaction` is 5.
extern DamageAttack D_actor_206100_80155194;

/// Enemy parameters `_actor206100InitEnemy` parks in `Enemy::param`.
/// `attacks` is `D_actor_206100_80155194` above and `hpMax` is the actor's
/// maximum hit points (2000), seeded into `field_40` / `field_42` at spawn.
extern EnemyParams D_actor_206100_80155198;

/// Animation bank handed to `animationInitContext` by `_actor206100InitEnemy`.
extern AnimationSet* D_actor_206100_80158B24[];

/// Placement records `_actor206100SpawnBogDiver` parks at `Enemy::place`
/// -- the same slot `areaSpawnPlacements` fills from a room's own place list, so this
/// is a local six-entry copy of one: `field_0` is 4 on the five live entries
/// and 0xFF on the sixth, the value `areaSpawnPlacements` stops its walk on.  The
/// overlay indexes it with the variant it was spawned for rather than walking
/// it, so the tail entry is reachable.
extern AreaPlacement D_actor_206100_80155134[];

/// The eight positions of the ring the actor circles, walked by the index at
/// `_Actor206100Work::waypointIndex`.
///
/// Each is a point in the root coordinate's translation units: radius 7600 in
/// the XZ plane, one 45-degree step per entry, at a constant height of 3000.
extern SVECTOR D_actor_206100_80158B68[8];

/// Frames a Bog Diver slot stays empty after the diver in it has died.
#define ACTOR_206100_BOG_DIVER_SUMMON_COOLDOWN 0xB4

/// One of the two Bog Divers the Sea Diver keeps summoned while it circles.
///
/// A slot holds one summoned diver at a time. While it is empty, and fewer
/// than five divers have been summoned in all, it summons the next one as soon
/// as its cooldown has run out. When the diver in it dies the slot empties
/// and waits `ACTOR_206100_BOG_DIVER_SUMMON_COOLDOWN` frames.
typedef struct {
    Enemy* enemy;          // the summoned Bog Diver's enemy record; NULL while the slot is empty
    s32    summonCooldown; // frames before the empty slot summons again, counted down a frame while it is empty
} _Actor206100BogDiverSlot;
STATIC_ASSERT_SIZEOF(_Actor206100BogDiverSlot, 0x8);

/// The two Bog Diver slots, emptied by the spawn state
/// `_actor206100Spawn`.
extern _Actor206100BogDiverSlot D_actor_206100_80158CBC[2];

/// Farthest from the origin, in the XZ plane, that a step of the fight may
/// leave the root.
#define ACTOR_206100_FIGHT_AREA_RADIUS 0x190

/// Scratch-stack block of the fight's bounded step: where the root stands
/// from the origin once the step has been taken.
///
/// The step reserves one block and releases it before it returns.
typedef struct {
    SVECTOR toOrigin; // from the root to the origin in the XZ plane; `vy` and `pad` are never written
    s32     distance; // length of `toOrigin`; past `ACTOR_206100_FIGHT_AREA_RADIUS` the step's XZ is undone
} _Actor206100OriginDistanceScratch;
STATIC_ASSERT_SIZEOF(_Actor206100OriginDistanceScratch, 0xC);

/// Values of `_Actor206100Work::hitReaction`.
enum {
    ACTOR_206100_HIT_REACTION_NONE   = 0,
    ACTOR_206100_HIT_REACTION_LIGHT  = 1, // light recoil of the neck and head
    ACTOR_206100_HIT_REACTION_HEAVY  = 2, // heavy recoil of the neck and head
    ACTOR_206100_HIT_REACTION_STATUS = 3, // held until the status buildup runs out
    ACTOR_206100_HIT_REACTION_BLAST  = 4  // heavy recoil, then the recoil state
};

/// Values of `_Actor206100Work::neckPhase`.
enum {
    ACTOR_206100_NECK_FREE       = 0, // the animation poses the neck
    ACTOR_206100_NECK_STRAIGHTEN = 1, // the captured neck angles ease to zero
    ACTOR_206100_NECK_RETRACTED  = 2  // the neck is straight and `neckScale` shortens it
};

/// Values of `_Actor206100Work::state` during the fight (task state 2). 4, 5
/// and 6 have empty handlers and are never selected.
enum {
    ACTOR_206100_FIGHT_STATE_ENTRANCE    = 0, // scripted: whites the screen out, moves with the player to the fight's place and comes up
    ACTOR_206100_FIGHT_STATE_ATTACK      = 1, // discharges, then fires six shots at the target
    ACTOR_206100_FIGHT_STATE_DIVE        = 2, // goes under with a splash and waits
    ACTOR_206100_FIGHT_STATE_SURFACE     = 3, // comes back up; one time in four dives again, otherwise attacks
    ACTOR_206100_FIGHT_STATE_RECOIL      = 7, // plays the recoil animation out, then dives
    ACTOR_206100_FIGHT_STATE_STATUS_HOLD = 8  // floats at the water level until the status buildup runs out
};

/// Body clip and normal-rate blend duration shared by dive and surface entry.
enum { ACTOR_206100_DIVE_SURFACE_CLIP         = 1,
       ACTOR_206100_DIVE_SURFACE_BLEND_FRAMES = 10 };

/// Work block of the Sea Diver task.
///
/// The spawn state allocates it zeroed and keeps it at `Task::work`. It holds
/// the animation playback and its storage, the two collision spheres that take
/// the hits with their contact records, storage for the model's matrices, the
/// ring the diver circles before the fight, and the state machine.
///
/// The task state picks a table of states, `state` an entry of that table and
/// `subState` a step of that entry's own table. The task states are the spawn
/// (0), the circling of the waypoint ring while Bog Divers are summoned (1),
/// the fight (2), the death (3) and the despawn (4). While circling, `state`
/// is 0 for the placement on the ring and 1 from then on; the death counts it
/// up through its four steps.
///
/// Angles are 4096ths of a turn, and positions are in the root coordinate's
/// parent space, which is the view's, with Y growing downward. Model parts are
/// numbered as the skeleton's coordinates, which is the Bog Diver's: 0 the
/// root, 1 the trunk, 2 and 3 the neck, 4 the head and 5 the head's one child.
typedef struct {
    ActorAnimRig15        rig;                // animation playback of the model, one slot per part; 1..14 play `animClip`, and slot 1's status tells when it ended
    u16                   lookPitch;          // signed pitch of the head's look toward the target; an attack eases it to 0x400 while it charges
    u16                   lookYaw;            // signed yaw of the look toward the target, spread over parts 2, 3 and 4 a third each
    u16                   lookRoll;           // third angle of the look; only ever rewritten with its own value and never applied
    byte                  field_362[0x2];     // never accessed
    WorldCollisionBody    trunkBody;          // sphere on part 1 that other bodies touch; its contacts carry the hits
    WorldCollisionContact hitContacts[6];     // contacts of `trunkBody` and `headBody`; also the enemy's hit records
    WorldCollisionBody    headBody;           // smaller sphere on part 4; shares `hitContacts`
    SVECTOR               prevRootPos;        // root position at the start of the frame; its XZ is restored when a step ends more than 400 from the origin
    SVECTOR               rotation;           // root rotation: `vy` heading, `vz` roll; `vx` is only ever cleared and never applied
    byte                  field_444[0x1C];    // never accessed
    MATRIX                colorMtx;           // storage for the model's `TmdObject::colorMtx`
    MATRIX                lightMtx;           // storage for the model's `TmdObject::lightMtx`
    byte                  field_4A0[0x20];    // never accessed
    EffectSpawnArg        effectArg;          // argument record of the hit and discharge effects, hung off part 1
    byte                  field_4C8[0x8];     // never accessed
    SVECTOR               targetPos;          // root position of the nearer of the player and the companion
    SVECTOR               lowerNeckAngles;    // Euler angles of part 2 as the neck retracted, eased to zero
    SVECTOR               upperNeckAngles;    // the same for part 3
    byte                  field_4E8[0xC];     // never accessed
    SVECTOR*              waypoints;          // ring of eight points circled while Bog Divers are summoned and into the entrance; `vy` is the height swum at
    Task*                 waveTask;           // screen-wave task covering the entrance's change of place, kept to be killed; NULL when its spawn failed
    byte                  field_4FC[0x8];     // never accessed
    s16                   hitCooldown;        // frames before another hit is taken; set from the hit's id parameter 2
    byte                  field_506[0x2];     // never accessed
    s16                   field_508;          // 0x1000 from the spawn; never read, role unproven
    s16                   field_50A;          // 0x1000 from the spawn; never read, role unproven
    s16                   animRequest;        // `DIVER_ANIM_REQUEST_*`
    s16                   animPlaying;        // animation last applied to the slots
    s16                   animClip;           // requested animation: index into the package's animation bank
    s16                   animFrames;         // frames since `animClip` was applied; rescaled to the new rate when a blend re-requests the playing animation
    u16                   animStatus;         // slot 1's ANIMATION_SLOT_* results from the latest frame's ticks; the states test this copy to learn their clip ended
    u16                   bobPhase;           // counts the frames run before the death, wrapping; never read here - the Bog Diver's bob counter sits at this place in its block
    u16                   frameCount;         // counts the same frames beside `bobPhase`; never read here either
    s16                   animStep;           // playback rate of slots 1..14; `ANIMATION_RATE_ONE` is normal speed
    s16                   playerBearing;      // heading from the root to the player relative to `rotation.vy`, 0..0xFFF; never read
    s16                   stateFrames;        // frames spent in the current state or step; the entrance's white-out counts it up by 6 as its level
    s16                   state;              // index into the state table of the current task state; `ACTOR_206100_FIGHT_STATE_*` during the fight
    s16                   subState;           // index into the step table of the current state
    s16                   animBlend;          // frames a blend request takes; cleared once a different animation has been blended into
    s16                   goalY;              // Y the root eases a sixteenth of the way to each frame from the fight on; follows the waypoint's while circling
    s16                   targetDistance;     // horizontal distance from the root to `targetPos`
    s16                   hitTaken;           // 1 when a hit or a status tick dealt damage this frame; lets `hitReaction` be consumed
    s16                   hitReaction;        // `ACTOR_206100_HIT_REACTION_*` awaiting the state machine
    s16                   attackFrames;       // frames left of the spark discharge that opens an attack, 24 from its start; paces its sound and sparks
    byte                  field_530[0x4];     // never accessed
    s16                   field_534;          // counted down a frame during the fight while nonzero, but never set; role unproven
    s16                   waterLevel;         // Y of the room's water surface: the float height of the status hold and the death, and 400 under it the diver cannot be locked onto
    byte                  field_538[0x2];     // never accessed
    s16                   neckPhase;          // `ACTOR_206100_NECK_*`
    s16                   neckScale;          // Z scale of part 2, 0x1000 = 1.0: eased to 0x2AA while retracted and back on release; the head takes the inverse
    s16                   modelScale;         // uniform scale of the root, 0x1000 = 1.0; 0x1EAA from the spawn
    s16                   part5Pitch;         // pitch added to part 5 by a recoil: swung to -0x300, then back to 0
    s16                   recoilPitch;        // pitch a recoil throws the neck and head back by: a third each on parts 2 and 3, all of it on the head
    s16                   recoilPeak;         // `recoilPitch` the running recoil rises to: 0x135 light, 0x3A0 heavy
    byte                  field_546[0x2];     // never accessed
    u8                    waypointIndex;      // entry of `waypoints` being swum to, 0..7
    byte                  field_549[0x2];     // never accessed
    u8                    wasHit;             // set by every hit that deals damage; never read
    byte                  field_54C[0x1];     // never accessed
    u8                    neckRetracted;      // 1 asks for the neck drawn in, as it is while circling; 0 releases it and lets the head look at the target
    byte                  field_54E[0x1];     // never accessed
    u8                    waypointsSinceRoll; // waypoints reached since the count last wrapped, 0..5; while it is 0 the diver rolls
    u8                    rolling;            // 1 while `rotation.vz` turns 0x20 a frame, until it completes a turn
    u8                    bogDiversSpawned;   // Bog Divers summoned so far, at most 5; also the index of the next one's placement
    u8                    bogDiversKilled;    // summoned Bog Divers that have died; the fifth starts the fight
    u8                    savedView;          // the session's view index as the entrance's white-out replaced it; never read
    u8                    recoilPhase;        // of `recoilPitch` (0 settles to zero, 1 voices the hit, 2 rises to `recoilPeak`, 3 falls back)
    u8                    shotRequested;      // 1 has the frame's tail spawn a shot from the head; set on each of an attack's six cue frames
    u8                    part5Phase;         // of `part5Pitch` (0 settles to zero, 1 starts, 2 swings to -0x300, 3 swings back)
    u8                    targetPart;         // part the enemy's target point and hit effects hang off: 4, or 1 during the status hold
} _Actor206100Work;
STATIC_ASSERT_SIZEOF(_Actor206100Work, 0x558);

/// The Diver library's name for this package's work block (see diver.h).
typedef _Actor206100Work DiverWork;

/// Distance ahead of the head, along its forward axis, at which a shot appears.
#define ACTOR_206100_SHOT_MUZZLE_DISTANCE 0x15E

/// Distance a shot moves along the head's forward axis every frame.
#define ACTOR_206100_SHOT_SPEED 0x5A

/// Added to a shot's vertical speed every frame.
#define ACTOR_206100_SHOT_GRAVITY 2

/// Frame count at which a shot still flying bursts by itself.
#define ACTOR_206100_SHOT_LIFETIME 0x5B

/// `_Actor206100ShotWork::burstSize` at launch, and what it grows by a frame.
#define ACTOR_206100_SHOT_BURST_SIZE_STEP 0x100

/// Value `_Actor206100ShotWork::burstSize` stops growing at.
#define ACTOR_206100_SHOT_BURST_SIZE_MAX 0x600

/// Added to `_Actor206100ShotWork::burstSize` to make the size word handed to
/// `_diverImpactBurst`: a room-particle size bias of 2 in bits 12..15. Bit 28
/// is set as well; the burst ignores bits 16..31.
#define ACTOR_206100_SHOT_BURST_VARIANT 0x10002000

/// Work block of a shot of the Sea Diver's attack.
///
/// A shot is a task of its own with a coordinate for a body, parented to the
/// view coordinate. The fight spawns one on each of an attack's six cue
/// frames, `ACTOR_206100_SHOT_MUZZLE_DISTANCE` ahead of the head and aimed
/// along it; from then on the shot moves by `velocity` every frame, falling
/// under `ACTOR_206100_SHOT_GRAVITY`, and throws sparks and spray that grow
/// with `burstSize`. It bursts when its sphere touches a body or the room, or
/// when it has flown `ACTOR_206100_SHOT_LIFETIME` frames, and the Diver
/// library's teardown then unlinks the sphere and ends the task.
typedef struct {
    DiverStrikeWork       strike;      // head the Diver library's teardown reads: the sphere carrying the attack, linked while the shot flies
    WorldCollisionContact contacts[2]; // contacts of the sphere: what the shot touched this frame
    SVECTOR               velocity;    // movement a frame in the view coordinate's space: the head's forward axis times `ACTOR_206100_SHOT_SPEED`; `pad` is never accessed
    s32                   burstPhase;  // phase handed to the burst, picking the spark's frame and pacing its puffs; cleared at launch and never advanced, where the Bog Diver's shot counts its frames here
    s32                   burstSize;   // size of the burst's spark and spray: `ACTOR_206100_SHOT_BURST_SIZE_STEP` at launch, growing by as much a frame up to `ACTOR_206100_SHOT_BURST_SIZE_MAX`
} _Actor206100ShotWork;
STATIC_ASSERT_SIZEOF(_Actor206100ShotWork, 0x68);

/// The wave `_actor206100EntranceRelocateTick` arms: pale cyan modulation with a
/// one-frame ramp.
extern ScreenWaveCtx D_actor_206100_80158CCC;

/// Child task `_actor206100EntranceRelocateTick` starts with the tint above as its
/// spawn arg.  Its callback is `_screenWaveTask`.
extern TaskDesc D_actor_206100_80158AF0[];

static void _actor206100InitEnemy(Task* task);

static void _actor206100InitHitBodies(Task* task);

static void _actor206100LaunchShot(Task* task);

/// Steps the actor's model coordinate `arg1` along the heading `arg2`, in the
/// XZ plane, and marks it dirty.
///
/// `task->extra` is the actor's `TmdObject`, so `coords` is the root
/// `GfxCoord` of its part array: `coord.t[0]` gains `rsin(arg2) * arg1`
/// and `coord.t[2]` `rcos(arg2) * arg1`. The `<< 4` on the `rsin` / `rcos`
/// result and the `>> 16` after the multiply are one `>> 12` split in two, the
/// unit circle the rest of the overlay's rotation code uses. Clearing `composeStamp` is
/// what makes the composition pass rebuild the matrix from `coord`, so the caller never
/// writes `workm` itself. The `task->extra` chain is walked again for each of
/// the three statements because `rsin` / `rcos` sit between them.
///
/// Every call site in this overlay takes `arg2` from the actor's heading and
/// `arg1` from a step distance, either a constant (`0x30`, `0x40`) or an
/// `s16` the caller narrows itself.

static void _actor206100EntranceDepartureTick(Task* task);

static void _actor206100EntranceArrivalTick(Task* task);

static void _actor206100EnterAttack(Task* task);

static void _actor206100AttackTick(Task* task);

static void _actor206100SurfaceSplashTick(Task* task);

static void _actor206100SummonBogDiversTick(Task* task);

/// Transforms `pos` from `coord`'s space up the parent chain into the view
/// coordinate's space.  Returns 1 with `pos` rewritten once the walk reaches
/// `gGfxViewCoord`, or 0 with `pos` untouched if the chain ends first.

static void _actor206100Despawn(Task* task);

static void _actor206100CopyRotation(const MATRIX* source, MATRIX* destination);

static void _actor206100UpdateNeckRetraction(Task* task, u8 unusedNeckRetracted);
static void _actor206100RecoilPitchTick(Task* task);
static void _actor206100ApplyPart5Recoil(Task* task);
static void _actor206100Part5RecoilTick(Task* task);
static void _actor206100SwimWaypointRing(Task* task);
static void _actor206100PlaceOnWaypointRing(Task* task);

static Enemy* _actor206100SpawnBogDiver(s32 placementIndex);

static void _actor206100DiveSplashTick(Task* task);
static void _actor206100StartRecoil(Task* task, s16 peakPitch);
static void _actor206100StepWithinFightArea(Task* task, s16 distance);
static void _actor206100BlendRequestedClip(Task* task);
static s16  _actor206100ScaleFramesForAnimRate(Task* task, s16 frames);
static void _actor206100EnterDive(Task* task);
static void _actor206100WaitUnderwater(Task* task);
static void _actor206100EnterSurface(Task* task);
static void _actor206100WaitSurface(Task* task);
static void _actor206100DeathPlaybackTick(Task* task);
static void _actor206100UnlinkForDeath(Task* task);
static void _actor206100EnterDeathPlayback(Task* task);
static void _actor206100DeathSinkTick(Task* task);
static void _actor206100AimHead(Task* task);

/// Distortion amplitude of the screen wave: `frame * scale / span` of the
/// running spawn argument, recomputed every frame.
extern s32 gScreenWaveRamp;

/// The spawn argument of the running wave task, parked at spawn so the tick
/// reads the ramp through it.
extern ScreenWaveCtx* gScreenWaveCtx;

/// Per-column and per-row phase records: each is seeded with a random offset
/// and speed at spawn and advanced by its speed every frame.
extern ScreenWaveOscillator gScreenWaveColumns[13];
extern ScreenWaveOscillator gScreenWaveRows[32];

static void _actor206100WaitRecoilBoundary(Task* task);
static void _actor206100EnterStatusHold(Task* task);
static void _actor206100StatusHoldTick(Task* task);

static void _actor206100FlyShot(Task* task);

static const TaskFuncTable3 D_actor_206100_80149E24 = {
    {
        _actor206100LaunchShot,
        _actor206100FlyShot,
        _diverStrikeTeardown,
    },
};

static u32     _gActor206100DiverEnergyBallPartVerts[1];
static SVECTOR _gActor206100DiverEnergyBallVerts[24];
static SVECTOR _gActor206100DiverEnergyBallNormals[25];
static TmdBone _gActor206100DiverEnergyBallSkeleton[1];
static u32     _gActor206100DiverEnergyBallStream[270];

static TmdSource _gActor206100DiverBody;
static void      _actor206100ShotTask(Task* task);
static void      _actor206100Task(Task* task);

static TmdBone _gActor206100DiverBodySkeleton[15] = {
#include "assets/diver_body_skeleton.inc"
};

static u32 _gActor206100DiverBodyPartVerts[15] = {
#include "assets/diver_body_partVerts.inc"
};

static SVECTOR _gActor206100DiverBodyVerts[145] = {
#include "assets/diver_body_verts.inc"
};

static SVECTOR _gActor206100DiverBodyNormals[142] = {
#include "assets/diver_body_normals.inc"
};

static u32 _gActor206100DiverBodyStream[2494] = {
#include "assets/diver_body_stream.inc"
};

static TmdSource _gActor206100DiverBody = {
    0,
    11652,
    5104,
    15,
    _gActor206100DiverBodyPartVerts,
    _gActor206100DiverBodyVerts,
    _gActor206100DiverBodyNormals,
    _gActor206100DiverBodySkeleton,
    _gActor206100DiverBodyStream,
};

static TmdBone _gActor206100DiverBurstHeadSkeleton[1] = {
#include "assets/diver_burst_head_skeleton.inc"
};

static u32 _gActor206100DiverBurstHeadPartVerts[1] = {
#include "assets/diver_burst_head_partVerts.inc"
};

static SVECTOR _gActor206100DiverBurstHeadVerts[35] = {
#include "assets/diver_burst_head_verts.inc"
};

static SVECTOR _gActor206100DiverBurstHeadNormals[44] = {
#include "assets/diver_burst_head_normals.inc"
};

static u32 _gActor206100DiverBurstHeadStream[360] = {
#include "assets/diver_burst_head_stream.inc"
};

static TmdSource _gActor206100DiverBurstHead = {
    0,
    2388,
    0,
    1,
    _gActor206100DiverBurstHeadPartVerts,
    _gActor206100DiverBurstHeadVerts,
    _gActor206100DiverBurstHeadNormals,
    _gActor206100DiverBurstHeadSkeleton,
    _gActor206100DiverBurstHeadStream,
};

static TmdBone _gActor206100DiverBurstArmRightSkeleton[1] = {
#include "assets/diver_burst_arm_right_skeleton.inc"
};

static u32 _gActor206100DiverBurstArmRightPartVerts[1] = {
#include "assets/diver_burst_arm_right_partVerts.inc"
};

static SVECTOR _gActor206100DiverBurstArmRightVerts[14] = {
#include "assets/diver_burst_arm_right_verts.inc"
};

static SVECTOR _gActor206100DiverBurstArmRightNormals[24] = {
#include "assets/diver_burst_arm_right_normals.inc"
};

static u32 _gActor206100DiverBurstArmRightStream[143] = {
#include "assets/diver_burst_arm_right_stream.inc"
};

static TmdSource _gActor206100DiverBurstArmRight = {
    0,
    904,
    0,
    1,
    _gActor206100DiverBurstArmRightPartVerts,
    _gActor206100DiverBurstArmRightVerts,
    _gActor206100DiverBurstArmRightNormals,
    _gActor206100DiverBurstArmRightSkeleton,
    _gActor206100DiverBurstArmRightStream,
};

static TmdBone _gActor206100DiverBurstArmLeft1Skeleton[1] = {
#include "assets/diver_burst_arm_left_1_skeleton.inc"
};

static u32 _gActor206100DiverBurstArmLeft1PartVerts[1] = {
#include "assets/diver_burst_arm_left_1_partVerts.inc"
};

static SVECTOR _gActor206100DiverBurstArmLeft1Verts[14] = {
#include "assets/diver_burst_arm_left_1_verts.inc"
};

static SVECTOR _gActor206100DiverBurstArmLeft1Normals[24] = {
#include "assets/diver_burst_arm_left_1_normals.inc"
};

static u32 _gActor206100DiverBurstArmLeft1Stream[143] = {
#include "assets/diver_burst_arm_left_1_stream.inc"
};

static TmdSource _gActor206100DiverBurstArmLeft1 = {
    0,
    904,
    0,
    1,
    _gActor206100DiverBurstArmLeft1PartVerts,
    _gActor206100DiverBurstArmLeft1Verts,
    _gActor206100DiverBurstArmLeft1Normals,
    _gActor206100DiverBurstArmLeft1Skeleton,
    _gActor206100DiverBurstArmLeft1Stream,
};

static TmdBone _gActor206100DiverBurstLegRightSkeleton[1] = {
#include "assets/diver_burst_leg_right_skeleton.inc"
};

static u32 _gActor206100DiverBurstLegRightPartVerts[1] = {
#include "assets/diver_burst_leg_right_partVerts.inc"
};

static SVECTOR _gActor206100DiverBurstLegRightVerts[19] = {
#include "assets/diver_burst_leg_right_verts.inc"
};

static SVECTOR _gActor206100DiverBurstLegRightNormals[33] = {
#include "assets/diver_burst_leg_right_normals.inc"
};

static u32 _gActor206100DiverBurstLegRightStream[210] = {
#include "assets/diver_burst_leg_right_stream.inc"
};

static TmdSource _gActor206100DiverBurstLegRight = {
    0,
    1360,
    0,
    1,
    _gActor206100DiverBurstLegRightPartVerts,
    _gActor206100DiverBurstLegRightVerts,
    _gActor206100DiverBurstLegRightNormals,
    _gActor206100DiverBurstLegRightSkeleton,
    _gActor206100DiverBurstLegRightStream,
};

static TmdBone _gActor206100DiverBurstArmLeft2Skeleton[1] = {
#include "assets/diver_burst_arm_left_2_skeleton.inc"
};

static u32 _gActor206100DiverBurstArmLeft2PartVerts[1] = {
#include "assets/diver_burst_arm_left_2_partVerts.inc"
};

static SVECTOR _gActor206100DiverBurstArmLeft2Verts[19] = {
#include "assets/diver_burst_arm_left_2_verts.inc"
};

static SVECTOR _gActor206100DiverBurstArmLeft2Normals[33] = {
#include "assets/diver_burst_arm_left_2_normals.inc"
};

static u32 _gActor206100DiverBurstArmLeft2Stream[210] = {
#include "assets/diver_burst_arm_left_2_stream.inc"
};

static TmdSource _gActor206100DiverBurstArmLeft2 = {
    0,
    1360,
    0,
    1,
    _gActor206100DiverBurstArmLeft2PartVerts,
    _gActor206100DiverBurstArmLeft2Verts,
    _gActor206100DiverBurstArmLeft2Normals,
    _gActor206100DiverBurstArmLeft2Skeleton,
    _gActor206100DiverBurstArmLeft2Stream,
};

static TmdBone _gActor206100DiverEnergyBallSkeleton[1] = {
#include "assets/diver_energy_ball_skeleton.inc"
};

static u32 _gActor206100DiverEnergyBallPartVerts[1] = {
#include "assets/diver_energy_ball_partVerts.inc"
};

static SVECTOR _gActor206100DiverEnergyBallVerts[24] = {
#include "assets/diver_energy_ball_verts.inc"
};

static SVECTOR _gActor206100DiverEnergyBallNormals[25] = {
#include "assets/diver_energy_ball_normals.inc"
};

static u32 _gActor206100DiverEnergyBallStream[270] = {
#include "assets/diver_energy_ball_stream.inc"
};

static TmdSource _gActor206100DiverEnergyBall = {
    0,
    1760,
    0,
    1,
    _gActor206100DiverEnergyBallPartVerts,
    _gActor206100DiverEnergyBallVerts,
    _gActor206100DiverEnergyBallNormals,
    _gActor206100DiverEnergyBallSkeleton,
    _gActor206100DiverEnergyBallStream,
};

AreaPlacement D_actor_206100_80155134[6] = {
    { 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2 },
    { 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5 },
    { 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7 },
    { 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

DamageAttack D_actor_206100_80155194 = { 26, 5 };

EnemyParams D_actor_206100_80155198 = { &D_actor_206100_80155194, 2000, 400, 1000, 15, 200, 3, 100, 5 };

static AnimationPackedPose _gActor206100Animation0B8F8Bank1[8] = {
#include "assets/actor_206100_animation_0B8F8_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0B8F8Bank4[133] = {
#include "assets/actor_206100_animation_0B8F8_bank4.inc"
};

static AnimationRecord _gActor206100Animation0B8F8Records[183] = {
#include "assets/actor_206100_animation_0B8F8_records.inc"
};

static u16 _gActor206100Animation0B8F8Indices[16] = {
#include "assets/actor_206100_animation_0B8F8_indices.inc"
};

static AnimationSet _gActor206100Animation0B8F8 = {
    _gActor206100Animation0B8F8Records,
    _gActor206100Animation0B8F8Indices,
    { NULL, _gActor206100Animation0B8F8Bank1, NULL, NULL, _gActor206100Animation0B8F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0BC80Bank1[3] = {
#include "assets/actor_206100_animation_0BC80_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0BC80Bank4[78] = {
#include "assets/actor_206100_animation_0BC80_bank4.inc"
};

static AnimationRecord _gActor206100Animation0BC80Records[121] = {
#include "assets/actor_206100_animation_0BC80_records.inc"
};

static u16 _gActor206100Animation0BC80Indices[16] = {
#include "assets/actor_206100_animation_0BC80_indices.inc"
};

static AnimationSet _gActor206100Animation0BC80 = {
    _gActor206100Animation0BC80Records,
    _gActor206100Animation0BC80Indices,
    { NULL, _gActor206100Animation0BC80Bank1, NULL, NULL, _gActor206100Animation0BC80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0C544Bank1[8] = {
#include "assets/actor_206100_animation_0C544_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0C544Bank4[174] = {
#include "assets/actor_206100_animation_0C544_bank4.inc"
};

static AnimationRecord _gActor206100Animation0C544Records[345] = {
#include "assets/actor_206100_animation_0C544_records.inc"
};

static u16 _gActor206100Animation0C544Indices[16] = {
#include "assets/actor_206100_animation_0C544_indices.inc"
};

static AnimationSet _gActor206100Animation0C544 = {
    _gActor206100Animation0C544Records,
    _gActor206100Animation0C544Indices,
    { NULL, _gActor206100Animation0C544Bank1, NULL, NULL, _gActor206100Animation0C544Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0CC4CBank1[10] = {
#include "assets/actor_206100_animation_0CC4C_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0CC4CBank4[167] = {
#include "assets/actor_206100_animation_0CC4C_bank4.inc"
};

static AnimationRecord _gActor206100Animation0CC4CRecords[235] = {
#include "assets/actor_206100_animation_0CC4C_records.inc"
};

static u16 _gActor206100Animation0CC4CIndices[16] = {
#include "assets/actor_206100_animation_0CC4C_indices.inc"
};

static AnimationSet _gActor206100Animation0CC4C = {
    _gActor206100Animation0CC4CRecords,
    _gActor206100Animation0CC4CIndices,
    { NULL, _gActor206100Animation0CC4CBank1, NULL, NULL, _gActor206100Animation0CC4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0D530Bank1[10] = {
#include "assets/actor_206100_animation_0D530_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0D530Bank4[213] = {
#include "assets/actor_206100_animation_0D530_bank4.inc"
};

static AnimationRecord _gActor206100Animation0D530Records[308] = {
#include "assets/actor_206100_animation_0D530_records.inc"
};

static u16 _gActor206100Animation0D530Indices[16] = {
#include "assets/actor_206100_animation_0D530_indices.inc"
};

static AnimationSet _gActor206100Animation0D530 = {
    _gActor206100Animation0D530Records,
    _gActor206100Animation0D530Indices,
    { NULL, _gActor206100Animation0D530Bank1, NULL, NULL, _gActor206100Animation0D530Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0D968Bank1[6] = {
#include "assets/actor_206100_animation_0D968_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0D968Bank4[103] = {
#include "assets/actor_206100_animation_0D968_bank4.inc"
};

static AnimationRecord _gActor206100Animation0D968Records[131] = {
#include "assets/actor_206100_animation_0D968_records.inc"
};

static u16 _gActor206100Animation0D968Indices[16] = {
#include "assets/actor_206100_animation_0D968_indices.inc"
};

static AnimationSet _gActor206100Animation0D968 = {
    _gActor206100Animation0D968Records,
    _gActor206100Animation0D968Indices,
    { NULL, _gActor206100Animation0D968Bank1, NULL, NULL, _gActor206100Animation0D968Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0DEA8Bank1[15] = {
#include "assets/actor_206100_animation_0DEA8_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0DEA8Bank4[109] = {
#include "assets/actor_206100_animation_0DEA8_bank4.inc"
};

static AnimationRecord _gActor206100Animation0DEA8Records[164] = {
#include "assets/actor_206100_animation_0DEA8_records.inc"
};

static u16 _gActor206100Animation0DEA8Indices[16] = {
#include "assets/actor_206100_animation_0DEA8_indices.inc"
};

static AnimationSet _gActor206100Animation0DEA8 = {
    _gActor206100Animation0DEA8Records,
    _gActor206100Animation0DEA8Indices,
    { NULL, _gActor206100Animation0DEA8Bank1, NULL, NULL, _gActor206100Animation0DEA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0E4A8Bank1[20] = {
#include "assets/actor_206100_animation_0E4A8_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0E4A8Bank4[126] = {
#include "assets/actor_206100_animation_0E4A8_bank4.inc"
};

static AnimationRecord _gActor206100Animation0E4A8Records[180] = {
#include "assets/actor_206100_animation_0E4A8_records.inc"
};

static u16 _gActor206100Animation0E4A8Indices[16] = {
#include "assets/actor_206100_animation_0E4A8_indices.inc"
};

static AnimationSet _gActor206100Animation0E4A8 = {
    _gActor206100Animation0E4A8Records,
    _gActor206100Animation0E4A8Indices,
    { NULL, _gActor206100Animation0E4A8Bank1, NULL, NULL, _gActor206100Animation0E4A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0E798Bank1[5] = {
#include "assets/actor_206100_animation_0E798_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0E798Bank4[43] = {
#include "assets/actor_206100_animation_0E798_bank4.inc"
};

static AnimationRecord _gActor206100Animation0E798Records[112] = {
#include "assets/actor_206100_animation_0E798_records.inc"
};

static u16 _gActor206100Animation0E798Indices[16] = {
#include "assets/actor_206100_animation_0E798_indices.inc"
};

static AnimationSet _gActor206100Animation0E798 = {
    _gActor206100Animation0E798Records,
    _gActor206100Animation0E798Indices,
    { NULL, _gActor206100Animation0E798Bank1, NULL, NULL, _gActor206100Animation0E798Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor206100Animation0ECA8Bank1[11] = {
#include "assets/actor_206100_animation_0ECA8_bank1.inc"
};

static AnimationPackedRotation _gActor206100Animation0ECA8Bank4[108] = {
#include "assets/actor_206100_animation_0ECA8_bank4.inc"
};

static AnimationRecord _gActor206100Animation0ECA8Records[165] = {
#include "assets/actor_206100_animation_0ECA8_records.inc"
};

static u16 _gActor206100Animation0ECA8Indices[16] = {
#include "assets/actor_206100_animation_0ECA8_indices.inc"
};

static AnimationSet _gActor206100Animation0ECA8 = {
    _gActor206100Animation0ECA8Records,
    _gActor206100Animation0ECA8Indices,
    { NULL, _gActor206100Animation0ECA8Bank1, NULL, NULL, _gActor206100Animation0ECA8Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_206100_80158AF0[2] = {
    { { { TASK_BODY_NONE, 192 } }, _screenWaveTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

TaskDesc D_actor_206100_80158B0C[2] = {
    { { { TASK_BODY_TMD, 96 } }, _actor206100Task, { .model = &_gActor206100DiverBody } },
    { { { TASK_BODY_COORD, 96 } }, _actor206100ShotTask, { .value = 0 } },
};

AnimationSet* D_actor_206100_80158B24[17] = {
    NULL,
    &_gActor206100Animation0B8F8,
    NULL,
    &_gActor206100Animation0BC80,
    NULL,
    &_gActor206100Animation0C544,
    NULL,
    &_gActor206100Animation0CC4C,
    &_gActor206100Animation0D530,
    NULL,
    &_gActor206100Animation0D968,
    &_gActor206100Animation0DEA8,
    NULL,
    NULL,
    &_gActor206100Animation0E4A8,
    &_gActor206100Animation0E798,
    &_gActor206100Animation0ECA8,
};

SVECTOR D_actor_206100_80158B68[8] = {
    { 0, 3000, 7600, 0 },
    { 5373, 3000, 5373, 0 },
    { 7600, 3000, 0, 0 },
    { 5373, 3000, -5373, 0 },
    { 0, 3000, -7600, 0 },
    { -5373, 3000, -5373, 0 },
    { -7600, 3000, 0, 0 },
    { -5373, 3000, 5373, 0 },
};

ScreenWaveCtx* gScreenWaveCtx = NULL;

ScreenWaveOscillator gScreenWaveColumns[13] = { 0 };

ScreenWaveOscillator gScreenWaveRows[32] = { 0 };

_Actor206100BogDiverSlot D_actor_206100_80158CBC[2] = { 0 };

ScreenWaveCtx D_actor_206100_80158CCC = { 0 };

static void _actor206100CirclingTick(Task* task);

static void _actor206100FightTick(Task* task);

static void _actor206100DeathTick(Task* task);

static void _actor206100DispatchEntrance(Task* task);

static void _actor206100DispatchAttack(Task* task);

static void _actor206100DispatchDive(Task* task);

static void _actor206100DispatchSurface(Task* task);

static void _actor206100FightState4(Task* task);

static void _actor206100FightState5(Task* task);

static void _actor206100FightState6(Task* task);

static void _actor206100DispatchRecoil(Task* task);

static void _actor206100DispatchStatusHold(Task* task);

static void _actor206100EntranceHoldPlayer(Task* task);

static void _actor206100EntranceLeadInTick(Task* task);

extern TaskDesc D_actor_100400_80147E48;

static void           _actor206100TrackTarget(Task* task);
static void           _actor206100TakeHits(Task* task);
static inline void    _actor206100AnimUpdate(Task* task);
static void           _actor206100Spawn(Task* task);
static void           _actor206100EntranceRelocateTick(Task* task);
static __inline__ s16 _actor206100ConsumeHitReaction(Task* task);

#include "../../shared/screen_wave.inc.c"

#include "../../shared/diver_impact_burst.inc.c"
#include "../../shared/diver_draw_spark.inc.c"

/// Copies the nine rotation coefficients without replacing matrix translation.
///
/// Borrows live source and destination matrices, which may be identical. Leaves
/// the destination alignment halfword intact; no GTE or coordinate-stamp changes.
static inline void _actor206100CopyRotationElements(const MATRIX* source, MATRIX* destination)
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

/// Initializes the Sea Diver's live model, enemy, animation rig and hit bodies.
///
/// Requires the zeroed work block, loaded body model and Enemy in
/// `spawnArg2.pointer`. Binds model lighting to work-owned matrices and enemy
/// contacts to the work-owned table, links the target node initially un-lockable,
/// seeds HP from the enemy parameters, and records the placed root heading.
/// The work and model must remain live until the bodies and target are unlinked.
static void _actor206100InitEnemy(Task* task)
{
    enum { ACTOR_206100_MODEL_OT_OFFSET     = 10,
           ACTOR_206100_HIT_EFFECT_SPAWN_LO = 0x580,
           ACTOR_206100_HIT_EFFECT_SPAWN_HI = 3 };
    _Actor206100Work* work;
    TmdObject*        model;
    Enemy*            enemy;
    GfxCoord*         rootCoord;
    u16               initialHp;

    model                      = task->extra.tmd;
    work                       = task->work;
    enemy                      = task->spawnArg2.pointer;
    model->otOffset            = ACTOR_206100_MODEL_OT_OFFSET;
    model->lightMtx            = &work->lightMtx;
    model->flags               = 0;
    model->colorMtx            = &work->colorMtx;
    rootCoord                  = model->coords;
    work->effectArg.coord      = &task->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = ACTOR_206100_HIT_EFFECT_SPAWN_LO;
    work->effectArg.spawnArgHi = ACTOR_206100_HIT_EFFECT_SPAWN_HI;
    enemy->field_4             = &rootCoord->coord;
    enemy->field_48            = 0;
    enemy->bodyPos.vx          = 0;
    enemy->bodyPos.vy          = 0;
    enemy->bodyPos.vz          = 0;
    enemy->coord               = &task->extra.tmd->coords[4];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->recs                   = work->hitContacts;
    enemy->param                  = &D_actor_206100_80155198;
    initialHp                     = D_actor_206100_80155198.hpMax;
    enemy->hpMax                  = initialHp;
    enemy->hp                     = initialHp;
    rootCoord->parent             = &gGfxViewCoord;
    animationInitContext(&work->rig.anim, D_actor_206100_80158B24, model, work->rig.poses, work->rig.slots);
    _actor206100InitHitBodies(task);
    work->rotation.vy = ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]);
    work->targetPart  = 4;
}

/// Straightens and retracts the neck, or restores its length and target aim.
///
/// Requires live work and model parts 0..4. The work block's `neckRetracted`
/// selects the behavior; `unusedNeckRetracted` is ignored. Retraction eases the
/// captured neck angles to within 48 units of zero, then shortens the lower
/// neck's Z basis toward 0x2AA. Release restores Q12 unit scale and resumes
/// head aim once scale is at least 0xF80. The head keeps its angles and receives
/// reciprocal Z scale while the neck is shortened. Scale must remain positive; angles
/// use 4096 units per turn. Retains translations and recomposes the hierarchy.
static void _actor206100UpdateNeckRetraction(Task* task, u8 unusedNeckRetracted)
{
    enum { ACTOR_206100_NECK_RELEASE_REQUEST          = 0,
           ACTOR_206100_NECK_RETRACT_REQUEST          = 1,
           ACTOR_206100_NECK_STRAIGHT_ANGLE_THRESHOLD = 48,
           ACTOR_206100_NECK_RETRACTED_SCALE          = 0x2AA,
           ACTOR_206100_NECK_RELEASE_SCALE_THRESHOLD  = 0xF80 };
    VECTOR            axisScale;
    MATRIX            rotationMatrix;
    MATRIX            lowerNeckBasis;
    MATRIX            upperNeckBasis;
    SVECTOR           headAngles;
    MATRIX            headBasis;
    _Actor206100Work* work;
    GfxCoord*         coords;
    GfxCoord*         lowerNeckCoord;
    GfxCoord*         upperNeckCoord;
    GfxCoord*         headCoord;
    s32               inverseNeckScale;

    /// Rebuilds both neck bases and inverse-scales the head, retaining translations.
    ///
    /// Captures work, lowerNeckCoord, upperNeckCoord, headCoord, headAngles,
    /// axisScale, the three basis matrices, rotationMatrix and inverseNeckScale.
    /// Requires positive Q12 neckScale and initialized headAngles; expands as
    /// statements at braced call sites, with no arguments or retained pointers.
#define ACTOR_206100_APPLY_NECK_SCALE()                                \
    gfxSetRotIdentity(&lowerNeckBasis);                                \
    axisScale.vx = ONE;                                                \
    axisScale.vy = ONE;                                                \
    axisScale.vz = work->neckScale;                                    \
    ScaleMatrix(&lowerNeckBasis, &axisScale);                          \
    _actor206100CopyRotation(&lowerNeckBasis, &lowerNeckCoord->coord); \
    gfxSetRotIdentity(&upperNeckBasis);                                \
    axisScale.vx = ONE;                                                \
    axisScale.vy = ONE;                                                \
    axisScale.vz = ONE;                                                \
    ScaleMatrix(&upperNeckBasis, &axisScale);                          \
    _actor206100CopyRotation(&upperNeckBasis, &upperNeckCoord->coord); \
    gfxSetRotIdentity(&headBasis);                                     \
    axisScale.vx     = ONE;                                            \
    axisScale.vy     = ONE;                                            \
    inverseNeckScale = (ONE * ONE) / work->neckScale;                  \
    axisScale.vz     = inverseNeckScale;                               \
    ScaleMatrix(&headBasis, &axisScale);                               \
    gfxSetRotIdentity(&rotationMatrix);                                \
    RotMatrix(&headAngles, &rotationMatrix);                           \
    MulMatrix(&headBasis, &rotationMatrix);                            \
    _actor206100CopyRotation(&headBasis, &headCoord->coord);

    coords         = task->extra.tmd->coords;
    work           = task->work;
    lowerNeckCoord = &coords[2];
    upperNeckCoord = &coords[3];
    headCoord      = &coords[4];

    switch (work->neckRetracted) {
        case ACTOR_206100_NECK_RETRACT_REQUEST:
            switch (work->neckPhase) {
                case ACTOR_206100_NECK_FREE:
                    // Capture the animated neck before straightening it.
                    coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
                    coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
                    coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
                    coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
                    coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(headCoord);
                    gfxExtractEulerAngles(&lowerNeckCoord->coord, &work->lowerNeckAngles);
                    gfxExtractEulerAngles(&upperNeckCoord->coord, &work->upperNeckAngles);
                    work->neckPhase = ACTOR_206100_NECK_STRAIGHTEN;
                    work->neckScale = ONE;
                    /* fallthrough */
                case ACTOR_206100_NECK_STRAIGHTEN: {
                    work->lowerNeckAngles.vx = (u16)work->lowerNeckAngles.vx + ((s32) - (work->lowerNeckAngles.vx * 0x10) >> 7);
                    work->lowerNeckAngles.vy = (u16)work->lowerNeckAngles.vy + ((s32) - (work->lowerNeckAngles.vy * 0x10) >> 7);
                    work->lowerNeckAngles.vz = (u16)work->lowerNeckAngles.vz + ((s32) - (work->lowerNeckAngles.vz * 0x10) >> 7);
                    work->upperNeckAngles.vx = (u16)work->upperNeckAngles.vx + ((s32) - (work->upperNeckAngles.vx * 0x10) >> 7);
                    work->upperNeckAngles.vy = (u16)work->upperNeckAngles.vy + ((s32) - (work->upperNeckAngles.vy * 0x10) >> 7);
                    work->upperNeckAngles.vz = (u16)work->upperNeckAngles.vz + ((s32) - (work->upperNeckAngles.vz * 0x10) >> 7);
                    gfxSetRotIdentity(&rotationMatrix);
                    RotMatrix(&work->lowerNeckAngles, &rotationMatrix);
                    _actor206100CopyRotation(&rotationMatrix, &lowerNeckCoord->coord);
                    gfxSetRotIdentity(&rotationMatrix);
                    RotMatrix(&work->upperNeckAngles, &rotationMatrix);
                    _actor206100CopyRotation(&rotationMatrix, &upperNeckCoord->coord);
                    if ((abs(work->lowerNeckAngles.vx) < ACTOR_206100_NECK_STRAIGHT_ANGLE_THRESHOLD) && (abs(work->lowerNeckAngles.vy) < ACTOR_206100_NECK_STRAIGHT_ANGLE_THRESHOLD) && (abs(work->lowerNeckAngles.vz) < ACTOR_206100_NECK_STRAIGHT_ANGLE_THRESHOLD) &&
                        (abs(work->upperNeckAngles.vx) < ACTOR_206100_NECK_STRAIGHT_ANGLE_THRESHOLD) && (abs(work->upperNeckAngles.vy) < ACTOR_206100_NECK_STRAIGHT_ANGLE_THRESHOLD) && (abs(work->upperNeckAngles.vz) < ACTOR_206100_NECK_STRAIGHT_ANGLE_THRESHOLD)) {
                        work->neckPhase = ACTOR_206100_NECK_RETRACTED;
                    }
                    lowerNeckCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                    upperNeckCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                    headCoord->composeStamp      = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(headCoord);
                    break;
                }
                case ACTOR_206100_NECK_RETRACTED: {
                    // Shorten the neck and compensate the head's Z scale.
                    gfxExtractEulerAngles(&headCoord->coord, &headAngles);
                    work->neckScale = (u16)work->neckScale + ((ACTOR_206100_NECK_RETRACTED_SCALE - work->neckScale) >> 3);
                    ACTOR_206100_APPLY_NECK_SCALE();
                    coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
                    coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
                    coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(headCoord);
                    break;
                }
            }
            break;
        case ACTOR_206100_NECK_RELEASE_REQUEST:
            coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
            coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
            coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
            coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(headCoord);
            // Restore neck length before handing the head back to target aim.
            if (work->neckScale < ACTOR_206100_NECK_RELEASE_SCALE_THRESHOLD) {
                gfxExtractEulerAngles(&headCoord->coord, &headAngles);
                work->neckScale = (u16)work->neckScale + ((ONE - work->neckScale) >> 2);
                ACTOR_206100_APPLY_NECK_SCALE();
                coords[2].composeStamp = GRAPHICS_COORD_DIRTY;
                coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
                coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(headCoord);
                work->neckPhase = ACTOR_206100_NECK_FREE;
            } else {
                _actor206100AimHead(task);
                work->neckPhase = ACTOR_206100_NECK_FREE;
            }
            break;
    }
#undef ACTOR_206100_APPLY_NECK_SCALE
}
/// Captures the frame-start root position and tracks the nearer player or companion.
///
/// Requires live work and model roots in the same parent frame. Stores XYZ as
/// signed halfwords and measures horizontal distance from those narrowed offsets;
/// ties choose the player. With no player, preserves the previous target and
/// bearing. The bearing always follows the player, relative to root yaw in
/// 0..4095 angle units; normalization changes GTE state.
static void _actor206100TrackTarget(Task* task)
{
    enum { ACTOR_206100_PLAYER_BEARING_MASK = 0xFFF };
    _Actor206100Work* work;
    GfxCoord*         rootCoord;
    GfxCoord*         playerCoord;
    GfxCoord*         companionCoord;
    Task*             player;
    SVECTOR           playerOffset;
    SVECTOR           companionOffset;
    s32               nearestDistance;
    s32               companionDistance;

    work                 = task->work;
    rootCoord            = task->extra.tmd->coords;
    player               = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    work->prevRootPos.vx = rootCoord->coord.t[0];
    work->prevRootPos.vy = rootCoord->coord.t[1];
    work->prevRootPos.vz = rootCoord->coord.t[2];
    // Keep the old target when no player task is present.
    if (player != NULL) {
        playerCoord     = player->extra.tmd->coords;
        playerOffset.vx = (u16)playerCoord->coord.t[0] - (u16)rootCoord->coord.t[0];
        playerOffset.vy = (u16)playerCoord->coord.t[1] - (u16)rootCoord->coord.t[1];
        playerOffset.vz = (u16)playerCoord->coord.t[2] - (u16)rootCoord->coord.t[2];
        nearestDistance = SquareRoot0(playerOffset.vx * playerOffset.vx + playerOffset.vz * playerOffset.vz);
        if (gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] == NULL) {
            work->targetPos.vx   = playerCoord->coord.t[0];
            work->targetPos.vy   = playerCoord->coord.t[1];
            work->targetPos.vz   = playerCoord->coord.t[2];
            work->targetDistance = nearestDistance;
        } else {
            companionCoord     = gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION]->extra.tmd->coords;
            companionOffset.vx = (u16)companionCoord->coord.t[0] - (u16)rootCoord->coord.t[0];
            companionOffset.vy = (u16)companionCoord->coord.t[1] - (u16)rootCoord->coord.t[1];
            companionOffset.vz = (u16)companionCoord->coord.t[2] - (u16)rootCoord->coord.t[2];
            companionDistance  = SquareRoot0(companionOffset.vx * companionOffset.vx + companionOffset.vz * companionOffset.vz);
            if (companionDistance < nearestDistance) {
                work->targetPos.vx = companionCoord->coord.t[0];
                work->targetPos.vy = companionCoord->coord.t[1];
                work->targetPos.vz = companionCoord->coord.t[2];
                nearestDistance    = companionDistance;
            } else {
                work->targetPos.vx = playerCoord->coord.t[0];
                work->targetPos.vy = playerCoord->coord.t[1];
                work->targetPos.vz = playerCoord->coord.t[2];
            }
            work->targetDistance = nearestDistance;
        }
        // Bearing always follows the player, even when the companion is nearer.
        VectorNormalSS(&playerOffset, &playerOffset);
        work->playerBearing = (ratan2(playerOffset.vx, playerOffset.vz) - work->rotation.vy) & ACTOR_206100_PLAYER_BEARING_MASK;
    }
}
/// Moves a Sea Diver shot and switches it to burst teardown on impact or expiry.
///
/// Requires live shot work, its linked attack sphere and two initialized contacts.
/// Running updates apply gravity and parent-space velocity, consume contacts and
/// age the shot; other actor-control modes leave it unchanged. Player, enemy and
/// hazard bodies trigger a burst. Grid hits trigger one unless class 3 is present.
/// The burst disables grid/pair tests before the twelve-tick linger state unlinks
/// the sphere; growth of the packed burst size retains its clamped store/reload.
static void _actor206100FlyShot(Task* task)
{
    enum {
        ACTOR_206100_SHOT_CONTACT_HAZARD    = 0x50000,
        ACTOR_206100_SHOT_PASS_SURFACE_MASK = 1 << 3
    };
    _Actor206100ShotWork* shot;
    GfxCoord*             shotCoord;
    WorldCollisionDelta   unusedPushback;
    s32                   surfaceMask;
    s32                   shouldBurst;
    s32                   burstKind;
    s32                   contactIndex;
    s32                   gridResult;
    s32                   burstSize;

    shouldBurst = 0;
    shot        = task->work;
    shotCoord   = task->extra.tmd->coords;
    burstKind   = DIVER_BURST_TRAIL;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        shot->velocity.vy      += ACTOR_206100_SHOT_GRAVITY;
        shotCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        shotCoord->coord.t[0]  += shot->velocity.vx;
        shotCoord->coord.t[1]  += shot->velocity.vy;
        shotCoord->coord.t[2]  += shot->velocity.vz;
        // Body and hazard contacts burst immediately; grid class 3 suppresses a grid burst.
        if (worldCollisionFindContactIndex(shot->contacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
            for (contactIndex = 0; contactIndex < ARRAY_SIZE(shot->contacts); contactIndex++) {
                switch (shot->contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) {
                    case WORLD_COLLISION_CONTACT_PLAYER_BODY:
                    case WORLD_COLLISION_CONTACT_ENEMY_BODY:
                    case ACTOR_206100_SHOT_CONTACT_HAZARD:
                        shouldBurst = 1;
                        break;
                }
            }
        }
        gridResult = worldCollisionResolvePushback(shot->contacts, &unusedPushback, ARRAY_SIZE(shot->contacts), &surfaceMask);
        if (gridResult <= WORLD_COLLISION_PUSHBACK_OPPOSED) {
            if (gridResult > WORLD_COLLISION_PUSHBACK_NO_GRID_HIT) {
                if ((surfaceMask & ACTOR_206100_SHOT_PASS_SURFACE_MASK) == 0) {
                    shouldBurst = 1;
                }
            }
        }
        worldCollisionClearContacts(shot->contacts);
        // Disable collision for the burst, leaving unlinking to the linger state.
        if ((++task->killCountdown >= ACTOR_206100_SHOT_LIFETIME) || (shouldBurst != 0)) {
            task->killCountdown            = 0;
            shot->strike.attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            burstKind                      = DIVER_BURST_IMPACT;
            task->state                   += 1;
        }
        burstSize = shot->burstSize;
        if (burstSize < ACTOR_206100_SHOT_BURST_SIZE_MAX) {
            shot->burstSize = burstSize + ACTOR_206100_SHOT_BURST_SIZE_STEP;
        } else {
            shot->burstSize = ACTOR_206100_SHOT_BURST_SIZE_MAX;
        }
        _diverImpactBurst(shotCoord, shot->burstPhase, burstKind, shot->burstSize + ACTOR_206100_SHOT_BURST_VARIANT);
    }
}
/// Applies player-attack contacts and pending status damage to the Sea Diver.
///
/// Requires the live Enemy, initialized six-contact hit table and current target
/// distance/part. Cooldown gates direct damage; the selected reaction can clear
/// hitTaken and allow another contact to be considered. Damage narrows to s16,
/// critical hits multiply by four and incendiary hits double that narrowed amount.
/// Credits Life Drain before subtracting HP, preserves signed-halfword HP clamping,
/// then handles stagger, buildup and poison. Clears contacts and ticks cooldown.
static void _actor206100TakeHits(Task* task)
{
    enum {
        ACTOR_206100_HEAVY_HIT_DAMAGE          = 180,
        ACTOR_206100_HIT_ATTRIBUTE_BLAST       = 4,
        ACTOR_206100_HIT_ATTRIBUTE_HEAVY       = 5,
        ACTOR_206100_HIT_ATTRIBUTE_NO_REACTION = 8,
        ACTOR_206100_HIT_ATTRIBUTE_LIGHT       = 9,
        ACTOR_206100_HIT_EFFECT_NONE           = 0,
        ACTOR_206100_HIT_EFFECT_CRITICAL       = 1,
        ACTOR_206100_HIT_EFFECT_INCENDIARY     = 2,
        ACTOR_206100_PLAYER_ATTACK_ROW_MASK    = 0x7F,
        ACTOR_206100_PLAYER_ATTACK_ATTACHMENT  = 0x8000,
        ACTOR_206100_STAGGER_EXEMPT_WEAPON_ROW = 28
    };
    _Actor206100Work* work;
    Enemy*            enemy;
    s32               extraEffectKind;
    s16               hpDamage;
    s32               baseDamage;
    s32               effectOrStatusResult;
    s32               statusDamageHalf;
    s32               contactIndex;
    s32               lightReaction;
    s32               heavyReaction;

    extraEffectKind = ACTOR_206100_HIT_EFFECT_NONE;
    lightReaction   = ACTOR_206100_HIT_REACTION_LIGHT;
    heavyReaction   = ACTOR_206100_HIT_REACTION_HEAVY;
    work            = task->work;
    enemy           = task->spawnArg2.pointer;
    work->hitTaken  = 0;
    // A reaction-clearing hit can let the scan continue to another contact.
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->hitContacts); contactIndex++) {
        if ((work->hitContacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_ATTACK) {
            if (work->hitCooldown == 0) {
                work->hitTaken    = lightReaction;
                work->wasHit      = lightReaction;
                baseDamage        = damageComputePlayerAttack(work->hitContacts[contactIndex].key.value, work->targetDistance, 0, 0);
                hpDamage          = baseDamage;
                work->hitCooldown = damageGetPlayerAttackHitCooldown(work->hitContacts[contactIndex].key.value);
                if (damageRollCriticalHit(enemy, work->hitContacts[contactIndex].key.value, 0) != 0) {
                    hpDamage        = ((u32)baseDamage << 16) >> 14;
                    extraEffectKind = ACTOR_206100_HIT_EFFECT_CRITICAL;
                }
                effectSpawnHit(damageGetPlayerAttackEffectId(work->hitContacts[contactIndex].key.value),
                               &task->extra.tmd->coords[work->targetPart], 0, &work->effectArg);
                if (hpDamage >= ACTOR_206100_HEAVY_HIT_DAMAGE) {
                    work->hitReaction = heavyReaction;
                } else {
                    work->hitReaction = lightReaction;
                }
                switch (damageGetPlayerAttackReaction(work->hitContacts[contactIndex].key.value) & 0xFFFF) {
                    case DAMAGE_PLAYER_REACTION_NONE:
                        break;
                    case DAMAGE_PLAYER_REACTION_STAGGER:
                        damageStartEnemyStagger(enemy);
                        break;
                    case DAMAGE_PLAYER_REACTION_BUILDUP:
                        damageStartEnemyBuildup(enemy, work->hitContacts[contactIndex].key.value, 0);
                        break;
                    case DAMAGE_PLAYER_REACTION_POISON:
                        damageTryStartEnemyDamageOverTime(enemy, work->hitContacts[contactIndex].key.value, 0);
                        break;
                    case ACTOR_206100_HIT_ATTRIBUTE_BLAST:
                        work->hitReaction = ACTOR_206100_HIT_REACTION_BLAST;
                        break;
                    case ACTOR_206100_HIT_ATTRIBUTE_HEAVY:
                    case DAMAGE_PLAYER_REACTION_EXPLOSION:
                        work->hitReaction = heavyReaction;
                        break;
                    case DAMAGE_PLAYER_REACTION_INCENDIARY:
                        extraEffectKind   = ACTOR_206100_HIT_EFFECT_INCENDIARY;
                        work->hitReaction = ACTOR_206100_HIT_REACTION_HEAVY;
                        hpDamage         += hpDamage;
                        break;
                    case ACTOR_206100_HIT_ATTRIBUTE_NO_REACTION:
                        work->hitReaction = ACTOR_206100_HIT_REACTION_NONE;
                        work->hitTaken    = 0;
                        break;
                    case ACTOR_206100_HIT_ATTRIBUTE_LIGHT:
                        work->hitReaction = lightReaction;
                        break;
                }
                if ((work->hitContacts[contactIndex].key.value & ACTOR_206100_PLAYER_ATTACK_ROW_MASK) == ACTOR_206100_STAGGER_EXEMPT_WEAPON_ROW && (work->hitContacts[contactIndex].key.value & ACTOR_206100_PLAYER_ATTACK_ATTACHMENT) == 0) {
                    enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
                    work->hitReaction     = lightReaction;
                }
                effectOrStatusResult = extraEffectKind;
                switch (effectOrStatusResult) {
                    case ACTOR_206100_HIT_EFFECT_CRITICAL:
                        effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[work->targetPart], 0, 0);
                        break;
                    case ACTOR_206100_HIT_EFFECT_INCENDIARY:
                        effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[work->targetPart], 2, 0);
                        break;
                }
                damageAccumulateLifeDrainHp(enemy, work->hitContacts[contactIndex].key.value, hpDamage, 0);
                worldTargetAddReadoutAmount(&enemy->node, hpDamage, 0);
                enemy->hp -= hpDamage;
                if ((s16)enemy->hp < 0) {
                    enemy->hp = 0;
                }
            } else if ((damageGetPlayerAttackEffectId(work->hitContacts[contactIndex].key.value)) == EFFECT_HIT_KIND_LIFE_DRAIN_MOTES) {
                effectSpawnHit(EFFECT_HIT_KIND_LIFE_DRAIN_MOTES, &task->extra.tmd->coords[1], 0, &work->effectArg);
            }
        }
        if (work->hitTaken != 0) {
            break;
        }
    }
    // Consume status requests after contact damage, then apply any poison pulse.
    if (enemy->reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags &= ENEMY_REACTION_STAGGER_CLEAR;
        work->hitReaction     = ACTOR_206100_HIT_REACTION_HEAVY;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
        enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->hitReaction     = ACTOR_206100_HIT_REACTION_STATUS;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        effectOrStatusResult = damageTickEnemyDamageOverTime(enemy);
        statusDamageHalf     = (s16)effectOrStatusResult;
        if (statusDamageHalf != 0) {
            enemy->hp -= effectOrStatusResult;
            worldTargetAddReadoutAmount(&enemy->node, statusDamageHalf, 0);
            if ((s16)enemy->hp < 0) {
                enemy->hp = 0;
            }
            work->hitTaken    = 1;
            work->hitReaction = ACTOR_206100_HIT_REACTION_LIGHT;
        }
        if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
    worldCollisionClearContacts(work->hitContacts);
    if (work->hitCooldown > 0) {
        work->hitCooldown--;
    } else {
        work->hitCooldown = 0;
    }
}
#include "../../shared/diver_inlines.inc.c"

#include "../../shared/diver_turn_joint.inc.c"

/// Applies the pending body-animation request and ticks slots 1 through 14.
///
/// Requires an initialized live rig and a loaded clip supporting all body
/// tracks. Blend requests reset the frame count for a changed clip or rescale
/// it for the playing clip; reset requests restart it at zero. Both acknowledge
/// the request as playing. Subsequent playing updates count one tick. Slot 0
/// and the cached `animStatus` are preserved; callers capture slot 1 afterward.
static inline void _actor206100AnimUpdate(Task* task)
{
    _Actor206100Work* work;
    s16               requestKind;
    s32               slotIndex;

    work        = task->work;
    requestKind = work->animRequest;
    if (requestKind == DIVER_ANIM_REQUEST_BLEND) {
        if (work->animPlaying != work->animClip) {
            work->animFrames = 0;
        } else {
            work->animFrames = _actor206100ScaleFramesForAnimRate(task, work->animFrames);
        }
        _actor206100BlendRequestedClip(task);
        work->animRequest = DIVER_ANIM_REQUEST_PLAYING;
    } else if (requestKind == DIVER_ANIM_REQUEST_RESET) {
        _diverRestartClip(task);
        work->animRequest = DIVER_ANIM_REQUEST_PLAYING;
        work->animFrames  = 0;
    } else if (requestKind == DIVER_ANIM_REQUEST_PLAYING) {
        work->animFrames = work->animFrames + 1;
    }
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
}

/// Allocates and initializes the Sea Diver before its waypoint-ring phase.
///
/// Requires the loaded model and Enemy in spawnArg2.pointer. Allocates zeroed
/// work from the primary heap, destroys the enemy/task on failure, and clears
/// both summoned-diver slots on success. Starts the swim clip, seeds root
/// height and Q12 model scale, retracts the neck, acquires the battle reference
/// and selects ring placement. Task teardown owns release of the work block.
static void _actor206100Spawn(Task* task)
{
    enum { ACTOR_206100_SPAWN_SWIM_CLIP     = 3,
           ACTOR_206100_SPAWN_MODEL_SCALE   = 0x1EAA,
           ACTOR_206100_SPAWN_ROOT_Y        = 10000,
           ACTOR_206100_TASK_STATE_CIRCLING = 1,
           ACTOR_206100_RING_STATE_PLACE    = 0 };
    _Actor206100Work* work;
    _Actor206100Work* requestWork;
    Enemy*            enemy;
    GfxCoord*         rootCoord;
    s32               slotIndex;

    // Establish owned work before linking the model and hit bodies.
    enemy      = task->spawnArg2.pointer;
    rootCoord  = task->extra.tmd->coords;
    task->work = memCalloc(sizeof(_Actor206100Work), 0);
    work       = task->work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    D_neo_ark_submarine_gallery_801818B8 = 1;
    work->waterLevel                     = D_neo_ark_submarine_gallery_80181A48;
    for (slotIndex = 0; slotIndex < ARRAY_SIZE(D_actor_206100_80158CBC); slotIndex++) {
        D_actor_206100_80158CBC[slotIndex].enemy          = NULL;
        D_actor_206100_80158CBC[slotIndex].summonCooldown = 0;
    }
    _actor206100InitEnemy(task);
    // Apply the initial swimming pose before entering the ring phase.
    requestWork              = task->work;
    requestWork->animStep    = ANIMATION_RATE_ONE;
    requestWork->animClip    = ACTOR_206100_SPAWN_SWIM_CLIP;
    requestWork->animRequest = DIVER_ANIM_REQUEST_RESET;
    _actor206100AnimUpdate(task);
    work->field_508       = 0x1000;
    work->field_50A       = 0x1000;
    work->modelScale      = ACTOR_206100_SPAWN_MODEL_SCALE;
    work->neckRetracted   = 1;
    rootCoord->coord.t[0] = 0;
    work->goalY           = ACTOR_206100_SPAWN_ROOT_Y;
    rootCoord->coord.t[1] = ACTOR_206100_SPAWN_ROOT_Y;
    rootCoord->coord.t[2] = 0;
    sceneAcquireBattleRef(0);
    task->state = ACTOR_206100_TASK_STATE_CIRCLING;
    _diverSetState(task, ACTOR_206100_RING_STATE_PLACE);
    // Ring entry repeats the nested-state reset made by the task transition.
    _diverSetState(task, ACTOR_206100_RING_STATE_PLACE);
}
/// The actor's five top-level states, dispatched on `Task::state` by its task
/// callback `_actor206100Task`: `_actor206100Spawn` (which
/// builds the work block), `_actor206100CirclingTick`,
/// `_actor206100FightTick`, `_actor206100DeathTick` and the exit
/// `_actor206100Despawn`.
static const TaskFuncTable5 D_actor_206100_80149E5C = {
    {
        _actor206100Spawn,
        _actor206100CirclingTick,
        _actor206100FightTick,
        _actor206100DeathTick,
        _actor206100Despawn,
    },
};

static const TaskFuncTable9 D_actor_206100_80149E70 = {
    {
        _actor206100DispatchEntrance,
        _actor206100DispatchAttack,
        _actor206100DispatchDive,
        _actor206100DispatchSurface,
        _actor206100FightState4,
        _actor206100FightState5,
        _actor206100FightState6,
        _actor206100DispatchRecoil,
        _actor206100DispatchStatusHold,
    },
};

/// Applies per-axis local scale to a coordinate's rotation basis.
///
/// Borrows a writable coordinate and axis factors in signed Q12 (ONE = 1.0);
/// the vector's fourth word is unused. Preserves translation and does not dirty
/// the composition stamp. Changes GTE rotation and arithmetic state.
static inline void _actor206100ScaleCoord(GfxCoord* coord, VECTOR* factors)
{
    MATRIX scaling;

    gfxSetRotIdentity(&scaling);
    ScaleMatrix(&scaling, factors);
    MulMatrix(&coord->coord, &scaling);
}

/// Applies a uniform local scale to the coordinate's rotation basis.
///
/// Borrows a writable coordinate; scale is signed Q12, ONE = 1.0. Preserves
/// translation and the composition stamp, so the caller must dirty the coordinate
/// when needed. Changes GTE rotation and arithmetic state.
static inline void _actor206100ScaleCoordUniform(GfxCoord* coord, s16 scale)
{
    VECTOR factors;

    factors.vx = scale;
    factors.vy = factors.vx;
    factors.vz = factors.vx;
    _actor206100ScaleCoord(coord, &factors);
}

/// Rebuilds the Sea Diver root's roll and heading before model scaling.
///
/// Requires live work and model. Applies Z roll then Y heading in 4096ths of a
/// turn, replacing the rotation basis while preserving translation and alignment
/// bytes. Dirties the root for later composition; changes GTE state.
static inline void _actor206100ApplyRootRotation(Task* task)
{
    _Actor206100Work* work;
    GfxCoord*         rootCoord;
    MATRIX            rootBasis;
    MATRIX*           rootMatrix;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;
    gfxSetRotIdentity(&rootBasis);
    RotMatrixZ(work->rotation.vz, &rootBasis);
    RotMatrixY(work->rotation.vy, &rootBasis);
    rootMatrix = &rootCoord->coord;
    _actor206100CopyRotationElements(&rootBasis, rootMatrix);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Spawns a Sea Diver shot along the head's forward axis.
///
/// Requires the loaded shot descriptor and a live model with head coordinate 4
/// whose parent chain reaches the view. World points narrow to signed halfwords
/// at each parent. The shot starts 350 local units along +Z; its initial velocity
/// is the transformed 90-unit +Z span, so head scale affects both distances.
/// The new task owns zeroed primary-heap work. Spawn failure drops the shot;
/// work-allocation failure kills the new task. No parent or source pointer is retained.
static inline void _actor206100SpawnShot(Task* task)
{
    enum { ACTOR_206100_SHOT_HEAD_PART  = 4,
           ACTOR_206100_SHOT_TASK_INDEX = 1 };
    _Actor206100ShotWork* shotWork;
    GfxCoord*             headCoord;
    GfxCoord*             shotCoord;
    Task*                 shotTask;
    SVECTOR               muzzlePosition;
    SVECTOR               headOrigin;
    SVECTOR               velocityTip;

    headCoord = &task->extra.tmd->coords[ACTOR_206100_SHOT_HEAD_PART];
    shotTask  = taskSpawnFromTable(D_actor_206100_80158B0C, ACTOR_206100_SHOT_TASK_INDEX, 0, 0);
    if (shotTask != NULL) {
        shotWork = memCalloc(sizeof(*shotWork), false);
        if (shotWork == NULL) {
            taskKill(shotTask);
        } else {
            headOrigin.vx  = 0;
            headOrigin.vy  = 0;
            headOrigin.vz  = 0;
            velocityTip.vx = 0;
            velocityTip.vy = 0;
            velocityTip.vz = ACTOR_206100_SHOT_SPEED;
            _actorRenderTransformPointToWorld(headCoord, &headOrigin);
            _actorRenderTransformPointToWorld(headCoord, &velocityTip);
            shotTask->work    = shotWork;
            shotCoord         = shotTask->extra.tmd->coords;
            muzzlePosition.vx = 0;
            muzzlePosition.vy = 0;
            muzzlePosition.vz = ACTOR_206100_SHOT_MUZZLE_DISTANCE;
            _actorRenderTransformPointToWorld(headCoord, &muzzlePosition);
            shotCoord->coord.t[0] = muzzlePosition.vx;
            shotCoord->coord.t[1] = muzzlePosition.vy;
            shotCoord->coord.t[2] = muzzlePosition.vz;
            shotWork->velocity.vx = velocityTip.vx - headOrigin.vx;
            shotWork->velocity.vy = velocityTip.vy - headOrigin.vy;
            shotWork->velocity.vz = velocityTip.vz - headOrigin.vz;
        }
    }
}

/// Depth under the water surface past which the Sea Diver cannot be locked
/// onto.
#define ACTOR_206100_LOCKABLE_DEPTH 0x190

/// Transforms a lock point into its parent frame, narrowing the result to halfwords.
///
/// Borrows a Q12 local-to-parent matrix and signed-halfword game coordinates.
/// localPoint and parentPoint may alias; widePoint and gteFlags are separate
/// writable scratch. Writes XYZ only, preserving the fourth halfword. Captures
/// the GTE overflow flags without interpreting them and overwrites GTE state.
static inline void _actor206100TransformLockPointToParent(const MATRIX* localToParent, const SVECTOR* localPoint, SVECTOR* parentPoint, VECTOR* widePoint, s32* gteFlags)
{
    gte_SetTransMatrix(localToParent);
    gte_SetRotMatrix(localToParent);
    gte_ldv0(localPoint);
    gte_rtv0tr();
    gte_stlvnl(widePoint);
    gte_stflg(gteFlags);
    parentPoint->vx = widePoint->vx;
    parentPoint->vy = widePoint->vy;
    parentPoint->vz = widePoint->vz;
}

/// Sets lock eligibility from a selected model part's depth below the water.
///
/// Requires live work, Enemy and a valid model partIndex (the fight uses 1 or 4).
/// Walks the live acyclic parent chain, excluding the view and narrowing XYZ to
/// signed halfwords after each transform. An incomplete chain leaves world origin
/// zero. Assigns the whole target-flag byte: NOT_LOCKABLE strictly beyond 400
/// units below waterLevel, zero otherwise. Changes GTE state; flags are ignored.
static inline void _actor206100UpdateLockable(Task* task, u8 partIndex)
{
    _Actor206100Work* work;
    Enemy*            enemy;
    GfxCoord*         currentCoord;
    SVECTOR           worldOrigin;
    SVECTOR           parentOrigin;
    VECTOR            transformedOrigin;
    s32               gteFlags;
    SVECTOR*          inputOrigin;
    SVECTOR*          outputOrigin;

    inputOrigin     = &parentOrigin;
    outputOrigin    = &worldOrigin;
    work            = task->work;
    currentCoord    = &task->extra.tmd->coords[partIndex];
    enemy           = task->spawnArg2.pointer;
    worldOrigin.vx  = 0;
    worldOrigin.vy  = 0;
    worldOrigin.vz  = 0;
    parentOrigin.vx = 0;
    parentOrigin.vy = 0;
    parentOrigin.vz = 0;
    // Stage each parent transform; commit only after reaching the view.
    while (1) {
        if (currentCoord->parent == NULL) {
            break;
        }
        if (currentCoord != &gGfxViewCoord) {
            _actor206100TransformLockPointToParent(&currentCoord->coord, inputOrigin, &parentOrigin, &transformedOrigin, &gteFlags);
            currentCoord = currentCoord->parent;
            continue;
        }
        outputOrigin->vx = parentOrigin.vx;
        outputOrigin->vy = parentOrigin.vy;
        outputOrigin->vz = parentOrigin.vz;
        break;
    }
    if (work->waterLevel + ACTOR_206100_LOCKABLE_DEPTH < worldOrigin.vy) {
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    } else {
        enemy->node.state.parts.flags = 0;
    }
}

/// Advances the attack discharge's sound, spark cues and signed-frame countdown.
///
/// Requires live task work, Enemy and trunk coordinate 1. A nonzero countdown
/// voices each multiple of eight ticks and requests sparks at 24 or 48 ticks
/// remaining, then decrements once. Effect allocation does not gate that decrement.
/// Sound pan/depth narrow to signed bytes; the Enemy placement selects the voice.
static inline void _actor206100TickDischargeEffects(Task* task)
{
    enum {
        ACTOR_206100_DISCHARGE_SOUND_INTERVAL_MASK  = 7,
        ACTOR_206100_DISCHARGE_SPARK_INTERVAL_TICKS = 24,
        ACTOR_206100_DISCHARGE_EFFECT_PART          = 1,
        ACTOR_206100_DISCHARGE_SOUND                = SOUND_CHARACTER(4, 11),
    };
    _Actor206100Work* work;
    s32               soundRequestId;
    s32               audioPan;
    Enemy*            soundEnemy;

    work = task->work;
    if (work->attackFrames != 0) {
        if ((work->attackFrames & ACTOR_206100_DISCHARGE_SOUND_INTERVAL_MASK) == 0) {
            soundEnemy     = task->spawnArg2.pointer;
            soundRequestId = ((soundEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_206100_DISCHARGE_SOUND;
            audioPan       = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundRequestId, audioPan,
                                     (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }
        if (work->attackFrames == ACTOR_206100_DISCHARGE_SPARK_INTERVAL_TICKS ||
            work->attackFrames == 2 * ACTOR_206100_DISCHARGE_SPARK_INTERVAL_TICKS) {
            effectSpawnHit(EFFECT_HIT_KIND_SPARK_BURST, task->extra.tmd->coords + ACTOR_206100_DISCHARGE_EFFECT_PART, NULL, &work->effectArg);
        }
        work->attackFrames--;
    }
}

/// Advances the Sea Diver fight, its discharge effects, pose, shots and lock point.
///
/// Requires initialized work/model/Enemy, player resources and a populated fight
/// state (0..3, 7 or 8); entries 4..6 are inert. targetPart must be a live model
/// coordinate, selected as trunk 1 or head 4. Only running combat advances state,
/// counters, slots 1..14, damage and root height. Paused combat refreshes colour
/// and permits drawing; hidden combat suppresses drawing. Every mode refreshes
/// lock eligibility from depth below the water, excluding the view transform.
/// Root rotation uses 4096 units per turn and scale uses twelve fractional bits.
/// Death is selected after the requested shot, preserving that tick's effects.
static void _actor206100FightTick(Task* task)
{
    enum {
        ACTOR_206100_TASK_STATE_DEATH         = 3,
        ACTOR_206100_ROOT_HEIGHT_EASING_SHIFT = 4,
    };
    _Actor206100Work* work        = task->work;
    GfxCoord*         rootCoord   = task->extra.tmd->coords;
    TmdObject*        model       = task->extra.tmd;
    Enemy*            enemy       = task->spawnArg2.pointer;
    TaskFuncTable9    fightStates = D_actor_206100_80149E70;
    _Actor206100Work* counterWork;
    _Actor206100Work* deathWork;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->bobPhase   = work->bobPhase + 1;
            work->frameCount = work->frameCount + 1;
            _actor206100TrackTarget(task);
            fightStates.funcs[work->state](task);
            _actor206100TickDischargeEffects(task);
            counterWork = task->work;
            if (counterWork->field_534 != 0) {
                counterWork->field_534--;
            }
            // Consume animation requests before applying the independent neck and recoil pose.
            _actor206100AnimUpdate(task);
            work->animStatus = work->rig.slots[1].status.fields.flags;
            _actor206100UpdateNeckRetraction(task, work->neckRetracted);
            _actor206100RecoilPitchTick(task);
            _actor206100Part5RecoilTick(task);
            _actor206100ApplyPart5Recoil(task);
            _actor206100ApplyRootRotation(task);
            _actor206100ScaleCoordUniform(task->extra.tmd->coords, work->modelScale);
            _actor206100TakeHits(task);
            if (work->shotRequested != 0) {
                _actor206100SpawnShot(task);
                work->shotRequested = 0;
            }
            if (enemy->hp <= 0) {
                deathWork           = task->work;
                task->state         = ACTOR_206100_TASK_STATE_DEATH;
                deathWork->state    = 0;
                deathWork->subState = 0;
            }
            rootCoord->coord.t[1] =
                rootCoord->coord.t[1] + ((work->goalY - rootCoord->coord.t[1]) >> ACTOR_206100_ROOT_HEIGHT_EASING_SHIFT);
            enemy->coord = &task->extra.tmd->coords[work->targetPart];
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actorRenderUpdateModelColor(task);
            model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
    _actor206100UpdateLockable(task, work->targetPart);
}

/// Fades the Sea Diver entrance to black, then relocates and re-enables its bodies.
///
/// Requires live work/model, the player and loaded entrance wave resources.
/// Each tick swims the waypoint ring and raises subtractive darkness by six to
/// 255. At full darkness it places the player, resets normal-rate clip 3, moves
/// the diver to (5500,3500,5500), saves the view and selects view 7. It advances
/// the entrance substate and retains the cyan screen-wave task, whose ramp
/// reaches full strength in one tick.
static void _actor206100EntranceRelocateTick(Task* task)
{
    enum {
        ACTOR_206100_ENTRANCE_DARKEN_STEP     = 6,
        ACTOR_206100_ENTRANCE_DARKNESS_LIMIT  = 256,
        ACTOR_206100_ENTRANCE_FULL_DARKNESS   = 255,
        ACTOR_206100_ENTRANCE_PLAYER_X        = 1680,
        ACTOR_206100_ENTRANCE_PLAYER_Y        = 5000,
        ACTOR_206100_ENTRANCE_PLAYER_Z        = 2200,
        ACTOR_206100_ENTRANCE_PLAYER_YAW      = 512,
        ACTOR_206100_ENTRANCE_RELOCATE_CLIP   = 3,
        ACTOR_206100_ENTRANCE_RELOCATE_Y      = 3500,
        ACTOR_206100_ENTRANCE_RELOCATE_XZ     = 5500,
        ACTOR_206100_ENTRANCE_RELOCATE_YAW    = 2560,
        ACTOR_206100_ENTRANCE_RELOCATE_VIEW   = 7,
        ACTOR_206100_ENTRANCE_WAVE_FRAMES     = 1,
        ACTOR_206100_ENTRANCE_WAVE_SCALE      = 96,
        ACTOR_206100_ENTRANCE_WAVE_RED        = 64,
        ACTOR_206100_ENTRANCE_WAVE_GREEN_BLUE = 128,
    };

    _Actor206100Work* work;
    _Actor206100Work* requestWork;
    TmdObject*        model;
    GfxCoord*         rootCoord;
    ActorTransform    playerTransform;

    work      = task->work;
    model     = task->extra.tmd;
    rootCoord = model->coords;
    _actor206100SwimWaypointRing(task);
    work->stateFrames = work->stateFrames + ACTOR_206100_ENTRANCE_DARKEN_STEP;
    if (work->stateFrames >= ACTOR_206100_ENTRANCE_DARKNESS_LIMIT) {
        work->stateFrames = ACTOR_206100_ENTRANCE_FULL_DARKNESS;
    }
    fadeDrawOverlay(work->stateFrames, work->stateFrames, work->stateFrames, GPU_BLEND_SUBTRACT);
    // Change placement and view only after subtractive fading covers the scene.
    if (work->stateFrames == ACTOR_206100_ENTRANCE_FULL_DARKNESS) {
        work->trunkBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->headBody.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        playerTransform.pos.vx = ACTOR_206100_ENTRANCE_PLAYER_X;
        playerTransform.pos.vy = ACTOR_206100_ENTRANCE_PLAYER_Y;
        playerTransform.pos.vz = ACTOR_206100_ENTRANCE_PLAYER_Z;
        playerTransform.rot.vx = 0;
        playerTransform.rot.vy = ACTOR_206100_ENTRANCE_PLAYER_YAW;
        playerTransform.rot.vz = 0;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &playerTransform, 0);
        requestWork                                                = task->work;
        requestWork->animStep                                      = ANIMATION_RATE_ONE;
        requestWork->animClip                                      = ACTOR_206100_ENTRANCE_RELOCATE_CLIP;
        requestWork->animRequest                                   = DIVER_ANIM_REQUEST_RESET;
        rootCoord->coord.t[1]                                      = ACTOR_206100_ENTRANCE_RELOCATE_Y;
        work->goalY                                                = ACTOR_206100_ENTRANCE_RELOCATE_Y;
        rootCoord->coord.t[0]                                      = ACTOR_206100_ENTRANCE_RELOCATE_XZ;
        rootCoord->coord.t[2]                                      = ACTOR_206100_ENTRANCE_RELOCATE_XZ;
        work->rotation.vx                                          = 0;
        work->rotation.vy                                          = ACTOR_206100_ENTRANCE_RELOCATE_YAW;
        work->rotation.vz                                          = 0;
        work->savedView                                            = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTOR_206100_ENTRANCE_RELOCATE_VIEW;
        work->stateFrames                                          = 0;
        work->neckRetracted                                        = 0;
        work->subState                                             = work->subState + 1;
        // The cyan distortion masks the following entrance movement.
        D_actor_206100_80158CCC.span            = ACTOR_206100_ENTRANCE_WAVE_FRAMES;
        D_actor_206100_80158CCC.scale           = ACTOR_206100_ENTRANCE_WAVE_SCALE;
        D_actor_206100_80158CCC.r               = ACTOR_206100_ENTRANCE_WAVE_RED;
        D_actor_206100_80158CCC.modulateTexture = SCREEN_WAVE_MODULATE_TEXTURE;
        D_actor_206100_80158CCC.g               = ACTOR_206100_ENTRANCE_WAVE_GREEN_BLUE;
        D_actor_206100_80158CCC.b               = ACTOR_206100_ENTRANCE_WAVE_GREEN_BLUE;
        work->waveTask                          = taskSpawnFromTable(D_actor_206100_80158AF0, 0, 0, &D_actor_206100_80158CCC);
    }
}
/// Moves the Sea Diver out of the entrance view and stages its reappearance.
///
/// Requires live work/model and the player task. Active ticks descend toward
/// Y 7500 and advance 48 parent-coordinate units along the heading. Tick 3
/// voices departure; tick 34 switches to view 6, places the diver at Y 7000
/// with goal Y 5000, hides the player model and synchronously places the
/// player. Queues the arrival clip at normal rate and advances the entrance
/// substate; playback applies that request later.
static void _actor206100EntranceDepartureTick(Task* task)
{
    enum { ACTOR_206100_ENTRANCE_DEPART_SOUND_FRAME = 3,
           ACTOR_206100_ENTRANCE_DEPART_FRAMES      = 34,
           ACTOR_206100_ENTRANCE_DEPART_GOAL_Y      = 7500,
           ACTOR_206100_ENTRANCE_DEPART_STEP        = 48,
           ACTOR_206100_ENTRANCE_ARRIVAL_ROOT_Y     = 7000,
           ACTOR_206100_ENTRANCE_ARRIVAL_GOAL_Y     = 5000,
           ACTOR_206100_ENTRANCE_ARRIVAL_VIEW       = 6,
           ACTOR_206100_ENTRANCE_ARRIVAL_CLIP       = 1 };
    _Actor206100Work* work;
    _Actor206100Work* requestWork;
    GfxCoord*         rootCoord;
    ActorTransform    playerTransform;

    work              = task->work;
    rootCoord         = task->extra.tmd->coords;
    work->stateFrames = work->stateFrames + 1;
    if (work->stateFrames == ACTOR_206100_ENTRANCE_DEPART_SOUND_FRAME) {
        sndEvtRequestScriptStart(SOUND_NEO_ARK_SUB_GALLERY_DIVER_DEPART, 0, 0);
    }
    // Stage both actors for reappearance in the next entrance view.
    if (work->stateFrames == ACTOR_206100_ENTRANCE_DEPART_FRAMES) {
        rootCoord->coord.t[0]                                      = 0;
        rootCoord->coord.t[2]                                      = 0;
        work->goalY                                                = ACTOR_206100_ENTRANCE_ARRIVAL_GOAL_Y;
        work->stateFrames                                          = 0U;
        rootCoord->coord.t[1]                                      = ACTOR_206100_ENTRANCE_ARRIVAL_ROOT_Y;
        work->rotation.vy                                          = 0;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTOR_206100_ENTRANCE_ARRIVAL_VIEW;
        worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
        playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
        playerTransform.pos.vx = 0x690;
        playerTransform.pos.vy = 0x1388;
        playerTransform.pos.vz = 0x898;
        playerTransform.rot.vx = 0;
        playerTransform.rot.vy = 0xA00;
        playerTransform.rot.vz = 0;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &playerTransform, 0);
        requestWork              = task->work;
        requestWork->animStep    = ANIMATION_RATE_ONE;
        requestWork->animClip    = ACTOR_206100_ENTRANCE_ARRIVAL_CLIP;
        requestWork->animRequest = DIVER_ANIM_REQUEST_RESET;
        work->subState           = work->subState + 1;
        return;
    }
    work->goalY = work->goalY + ((ACTOR_206100_ENTRANCE_DEPART_GOAL_Y - work->goalY) >> 2);
    _diverStepForward(task, ACTOR_206100_ENTRANCE_DEPART_STEP, work->rotation.vy);
}
/// Emits 32 rotating water-spray particles around the model root at a local height.
///
/// Requires a live model root and the loaded room spray handler. Offsets use a
/// 512-unit XZ radius in root-local game coordinates; localY is signed and Y
/// increases downward. Each particle has size 328, two updates per animation
/// cell and upward-burst speed 32. Unsigned trig shifts retain low signed
/// halfwords, including the negative XZ offsets. Spawn failures are ignored.
/// The effect spawner copies XYZ during the call;
/// the spray task never follows the retained pointer to this temporary offset.
static inline void _actor206100SpawnSplashRing(Task* task, s16 localY)
{
    enum { ACTOR_206100_SPLASH_PARTICLE_COUNT   = 32,
           ACTOR_206100_SPLASH_ANGLE_STEP_SHIFT = 7,
           ACTOR_206100_SPLASH_RADIUS_SHIFT     = 3,
           ACTOR_206100_SPLASH_PARTICLE_RECIPE  = 0x01202148 };
    GfxCoord* splashCoord;
    SVECTOR   splashOffset;
    s32       particleIndex;

    particleIndex = 0;
    splashCoord   = task->extra.tmd->coords;
    do {
        splashOffset.vx = (u32)rsin(particleIndex << ACTOR_206100_SPLASH_ANGLE_STEP_SHIFT) >> ACTOR_206100_SPLASH_RADIUS_SHIFT;
        splashOffset.vy = localY;
        splashOffset.vz = (u32)rcos(particleIndex << ACTOR_206100_SPLASH_ANGLE_STEP_SHIFT) >> ACTOR_206100_SPLASH_RADIUS_SHIFT;
        effectSpawn(gRoomEffectWaterSprayId, splashCoord, ACTOR_206100_SPLASH_PARTICLE_RECIPE, &splashOffset);
        particleIndex++;
    } while (particleIndex < ACTOR_206100_SPLASH_PARTICLE_COUNT);
}

/// Finishes the Sea Diver entrance and returns the player to the fight.
///
/// Runs at entrance substate 4 with live work/model and a nullable wave task.
/// Tick 1 ends the wave ramp; tick 3 kills its task and voices reappearance if
/// the timer survived that call. Tick 12 emits water spray at local Y -660.
/// Tick 70 resumes/shows the player, selects view 2, centers root XZ and selects
/// the attack at substate zero. The enclosing fight tick applies the queued pose.
static void _actor206100EntranceArrivalTick(Task* task)
{
    enum {
        ACTOR_206100_ENTRANCE_STOP_WAVE_TICK    = 1,
        ACTOR_206100_ENTRANCE_RELEASE_WAVE_TICK = 3,
        ACTOR_206100_ENTRANCE_SPLASH_TICK       = 12,
        ACTOR_206100_ENTRANCE_RESUME_TICK       = 70,
        ACTOR_206100_ENTRANCE_FIGHT_VIEW        = 2,
        ACTOR_206100_ENTRANCE_SPLASH_Y          = -660
    };
    _Actor206100Work* work;
    _Actor206100Work* transitionWork;
    GfxCoord*         rootCoord;
    s16               splashY;
    s16               soundFrame;

    work                                 = task->work;
    D_neo_ark_submarine_gallery_801818B8 = 0;
    rootCoord                            = task->extra.tmd->coords;
    work->stateFrames                    = work->stateFrames + 1;
    if (work->stateFrames == ACTOR_206100_ENTRANCE_STOP_WAVE_TICK) {
        D_actor_206100_80158CCC.state = SCREEN_WAVE_RAMP_FINISHED;
    }
    soundFrame = work->stateFrames;
    if (soundFrame == ACTOR_206100_ENTRANCE_RELEASE_WAVE_TICK) {
        if (work->waveTask != NULL) {
            taskKill(work->waveTask);
        }
        if (work->stateFrames == soundFrame) {
            sndEvtRequestScriptStart(SOUND_NEO_ARK_SUB_GALLERY_DIVER_REAPPEAR, 0, 0);
        }
    }
    if (work->stateFrames == ACTOR_206100_ENTRANCE_SPLASH_TICK) {
        splashY = ACTOR_206100_ENTRANCE_SPLASH_Y;
        _actor206100SpawnSplashRing(task, splashY);
    }
    if (work->stateFrames == ACTOR_206100_ENTRANCE_RESUME_TICK) {
        // Release player control only after the arrival splash has played.
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
        playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACTOR_206100_ENTRANCE_FIGHT_VIEW;
        rootCoord->coord.t[0]                                      = 0;
        rootCoord->coord.t[2]                                      = 0;
        transitionWork                                             = task->work;
        transitionWork->state                                      = ACTOR_206100_FIGHT_STATE_ATTACK;
        transitionWork->subState                                   = 0;
    }
}

/// Consumes this frame's pending hit reaction during the fight.
///
/// Only handles reactions when `hitTaken` is exactly 1. Light and heavy hits
/// start neck/head recoil and return 0, allowing the current substate to run.
/// Status and blast reactions stop the attack-loop sound and select status
/// hold or recoil at substate 0, returning 1 to skip the current handler.
/// Clears the pending reaction when handled, but preserves the hit flag.
static __inline__ s16 _actor206100ConsumeHitReaction(Task* task)
{
    enum { ACTOR_206100_LIGHT_RECOIL_PEAK = 0x135,
           ACTOR_206100_HEAVY_RECOIL_PEAK = 0x3A0 };
    _Actor206100Work* work = task->work;

    if (work->hitTaken == 1) {
        switch (work->hitReaction) {
            case ACTOR_206100_HIT_REACTION_LIGHT:
                work->hitReaction = ACTOR_206100_HIT_REACTION_NONE;
                _actor206100StartRecoil(task, ACTOR_206100_LIGHT_RECOIL_PEAK);
                break;
            case ACTOR_206100_HIT_REACTION_HEAVY:
                work->hitReaction = ACTOR_206100_HIT_REACTION_NONE;
                _actor206100StartRecoil(task, ACTOR_206100_HEAVY_RECOIL_PEAK);
                break;
            case ACTOR_206100_HIT_REACTION_STATUS:
                sndEvtRequestScriptStop(SOUND_NEO_ARK_SUB_GALLERY_DIVER_ATTACK_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                work->hitReaction = ACTOR_206100_HIT_REACTION_NONE;
                _diverSetState(task, ACTOR_206100_FIGHT_STATE_STATUS_HOLD);
                return 1;
            case ACTOR_206100_HIT_REACTION_BLAST:
                sndEvtRequestScriptStop(SOUND_NEO_ARK_SUB_GALLERY_DIVER_ATTACK_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                work->hitReaction = ACTOR_206100_HIT_REACTION_NONE;
                _actor206100StartRecoil(task, ACTOR_206100_HEAVY_RECOIL_PEAK);
                _diverSetState(task, ACTOR_206100_FIGHT_STATE_RECOIL);
                return 1;
            default:
                work->hitReaction = ACTOR_206100_HIT_REACTION_NONE;
                break;
        }
    }
    return 0;
}

/// Dispatches the attack's entry or active step after consuming hit reactions.
///
/// Requires live work with subState 0 or 1. A status or blast reaction that
/// selects another fight state skips this tick's attack handler; light and
/// heavy recoil allow it to run. The fight driver advances animation afterward.
static void _actor206100DispatchAttack(Task* task)
{
    _Actor206100Work* work                = task->work;
    TaskFunc          substateHandlers[2] = {
        _actor206100EnterAttack,
        _actor206100AttackTick,
    };

    if (_actor206100ConsumeHitReaction(task) == 0) {
        substateHandlers[work->subState](task);
    }
}
/// Turns the root heading toward a point outside a signed yaw deadband.
///
/// Borrows live task work, a model root and a target in the root's parent frame.
/// Only XZ is read, narrowed to signed halfwords before normalization. Angles
/// use 4096 units per turn; the difference wraps to -2048..2047. Positive
/// yawStep is a fixed increment and may overshoot; deadband is nonnegative.
/// Dirties the root and changes GTE state without translating it.
static inline void _actor206100TurnTowardPoint(Task* task, const SVECTOR* target, s32 yawStep, s32 deadband)
{
    _Actor206100Work* work;
    GfxCoord*         rootCoord;
    SVECTOR           targetDirection;
    s32               targetYaw;
    s32               currentYaw;
    s32               yawDifference;

    work                    = task->work;
    rootCoord               = task->extra.tmd->coords;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    targetDirection.vx      = target->vx - rootCoord->coord.t[0];
    targetDirection.vy      = 0;
    targetDirection.vz      = target->vz - rootCoord->coord.t[2];
    VectorNormalSS(&targetDirection, &targetDirection);
    targetYaw     = ratan2(targetDirection.vx, targetDirection.vz);
    currentYaw    = (u16)work->rotation.vy;
    yawDifference = ((currentYaw - targetYaw) << 20) >> 20;
    if (yawDifference > deadband) {
        work->rotation.vy = currentYaw - yawStep;
    } else if (yawDifference < -deadband) {
        work->rotation.vy = currentYaw + yawStep;
    }
}

/// Runs the attack's discharge and six shot cues while approaching the target.
///
/// Requires live work, model and cached target position. Counts active ticks,
/// starts a 24-tick discharge, raises the head during the charge and requests
/// shots on ticks 84, 91, 98, 105, 112 and 119. A slot boundary, settled slot
/// or control jump stops the sound and selects the dive. Still turns and takes
/// this tick's bounded 16-unit step after that state change.
static void _actor206100AttackTick(Task* task)
{
    enum { ACTOR_206100_ATTACK_DISCHARGE_FRAMES  = 24,
           ACTOR_206100_ATTACK_CHARGE_FIRST_TICK = 41,
           ACTOR_206100_ATTACK_CHARGE_TICK_COUNT = 37U,
           ACTOR_206100_ATTACK_CHARGE_PITCH      = 0x400,
           ACTOR_206100_ATTACK_SHOT_FIRST_TICK   = 84,
           ACTOR_206100_ATTACK_SHOT_TICK_SPACING = 7,
           ACTOR_206100_ATTACK_SHOT_LAST_TICK    = 119,
           ACTOR_206100_ATTACK_YAW_STEP          = 12,
           ACTOR_206100_ATTACK_YAW_DEADBAND      = 24,
           ACTOR_206100_ATTACK_STEP_DISTANCE     = 16 };
    _Actor206100Work* attackWork = task->work;
    u16               pitchBits;
    s32               panOffset;

    attackWork->stateFrames = attackWork->stateFrames + 1;
    // Charge first, then schedule shots and keep their looping sound in step.
    if (attackWork->stateFrames == 1) {
        attackWork->attackFrames = ACTOR_206100_ATTACK_DISCHARGE_FRAMES;
    }
    if ((u32)((u16)attackWork->stateFrames - ACTOR_206100_ATTACK_CHARGE_FIRST_TICK) < ACTOR_206100_ATTACK_CHARGE_TICK_COUNT) {
        pitchBits             = attackWork->lookPitch;
        attackWork->lookPitch = pitchBits + ((s32)((ACTOR_206100_ATTACK_CHARGE_PITCH * 16 - (pitchBits * 16)) << 0x10) >> 0x16);
    }
    if (attackWork->stateFrames == ACTOR_206100_ATTACK_SHOT_FIRST_TICK) {
        panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(SOUND_NEO_ARK_SUB_GALLERY_DIVER_ATTACK_LOOP, panOffset,
                                 (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if (attackWork->stateFrames == ACTOR_206100_ATTACK_SHOT_LAST_TICK) {
        sndEvtRequestScriptStop(SOUND_NEO_ARK_SUB_GALLERY_DIVER_ATTACK_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    }
    if (attackWork->stateFrames == ACTOR_206100_ATTACK_SHOT_FIRST_TICK ||
        attackWork->stateFrames == ACTOR_206100_ATTACK_SHOT_FIRST_TICK + ACTOR_206100_ATTACK_SHOT_TICK_SPACING ||
        attackWork->stateFrames == ACTOR_206100_ATTACK_SHOT_FIRST_TICK + 2 * ACTOR_206100_ATTACK_SHOT_TICK_SPACING ||
        attackWork->stateFrames == ACTOR_206100_ATTACK_SHOT_FIRST_TICK + 3 * ACTOR_206100_ATTACK_SHOT_TICK_SPACING ||
        attackWork->stateFrames == ACTOR_206100_ATTACK_SHOT_FIRST_TICK + 4 * ACTOR_206100_ATTACK_SHOT_TICK_SPACING ||
        attackWork->stateFrames == ACTOR_206100_ATTACK_SHOT_LAST_TICK) {
        attackWork->shotRequested = 1;
    }
    if (_diverClipHasBoundaryOrJump(task)) {
        sndEvtRequestScriptStop(SOUND_NEO_ARK_SUB_GALLERY_DIVER_ATTACK_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        _diverSetState(task, ACTOR_206100_FIGHT_STATE_DIVE);
    }
    // Finish this tick's approach even when the clip selected the dive.
    _actor206100TurnTowardPoint(task, &attackWork->targetPos, ACTOR_206100_ATTACK_YAW_STEP, ACTOR_206100_ATTACK_YAW_DEADBAND);
    _actor206100StepWithinFightArea(task, ACTOR_206100_ATTACK_STEP_DISTANCE);
}

/// The five sub-state handlers `_actor206100DispatchEntrance` picks between: it
/// copies the table onto its stack and calls `funcs[subState]`.
static const TaskFuncTable5 D_actor_206100_80149E94 = {
    {
        _actor206100EntranceHoldPlayer,
        _actor206100EntranceLeadInTick,
        _actor206100EntranceRelocateTick,
        _actor206100EntranceDepartureTick,
        _actor206100EntranceArrivalTick,
    },
};

/// The state-0 dispatcher's sub-state table, a table in its own right rather
/// than the local array `_actor206100DispatchAttack` builds -- the dispatcher
/// copies it whole, which is why the copy is a three-word block move out of
/// `.rodata`.  `_actor206100DispatchSurface` has the same body over the sibling
/// table `D_actor_206100_80149EB4`.
static const TaskFuncTable3 D_actor_206100_80149EA8 = {
    {
        _actor206100EnterDive,
        _actor206100DiveSplashTick,
        _actor206100WaitUnderwater,
    },
};

/// The sibling table, immediately after `D_actor_206100_80149EA8` in
/// `.rodata`: the three sub-state handlers `_actor206100DispatchSurface`
/// dispatches between, the first `_actor206100EnterSurface` and the third
/// `_actor206100WaitSurface` bracketing the ring of debris
/// `_actor206100SurfaceSplashTick` throws.
static const TaskFuncTable3 D_actor_206100_80149EB4 = {
    {
        _actor206100EnterSurface,
        _actor206100SurfaceSplashTick,
        _actor206100WaitSurface,
    },
};

/// The last object in this unit's `.rodata`, one object after
/// `D_actor_206100_80149EB4` and flush against the unit's first code address:
/// the four steps of the death `_actor206100DeathTick` dispatches on
/// `_Actor206100Work::state`.
static const TaskFuncTable4 D_actor_206100_80149EC0 = {
    {
        _actor206100UnlinkForDeath,
        _actor206100EnterDeathPlayback,
        _actor206100DeathPlaybackTick,
        _actor206100DeathSinkTick,
    },
};

/// Dispatches the dive's entry, splash or underwater wait after hit reactions.
///
/// Requires live work/model and subState 0..2. A reaction selecting another fight
/// state skips the dive handler. Otherwise turns toward the cached target by
/// 24 angle units outside a 40-unit deadband and takes a bounded 20-unit step.
/// The fight driver applies animation requests afterward.
static void _actor206100DispatchDive(Task* task)
{
    enum { ACTOR_206100_DIVE_YAW_STEP      = 24,
           ACTOR_206100_DIVE_YAW_DEADBAND  = 40,
           ACTOR_206100_DIVE_STEP_DISTANCE = 20 };
    _Actor206100Work* work             = task->work;
    TaskFuncTable3    substateHandlers = D_actor_206100_80149EA8;

    if (_actor206100ConsumeHitReaction(task) == 0) {
        substateHandlers.funcs[work->subState](task);
        // Complete this tick's approach even if a substate selected surfacing.
        _actor206100TurnTowardPoint(task, &work->targetPos, ACTOR_206100_DIVE_YAW_STEP, ACTOR_206100_DIVE_YAW_DEADBAND);
        _actor206100StepWithinFightArea(task, ACTOR_206100_DIVE_STEP_DISTANCE);
    }
}
/// Settles the head look and emits the dive splash before the underwater wait.
///
/// Requires live work/model and Enemy. The first 29 active ticks ease the packed
/// look angles toward zero. Tick 30 emits 32 water particles at local Y -100,
/// voices the dive with the enemy instance tag, sets parent-space goal Y 7800
/// and advances the substate with a cleared timer. Y increases downward.
static void _actor206100DiveSplashTick(Task* task)
{
    enum {
        ACTOR_206100_DIVE_SPLASH_TICK          = 30,
        ACTOR_206100_DIVE_SPLASH_Y             = -100,
        ACTOR_206100_DIVE_GOAL_Y               = 7800,
        ACTOR_206100_DIVE_SOUND                = 0x551E0006,
        ACTOR_206100_DIVE_SOUND_INSTANCE_SHIFT = 8
    };
    _Actor206100Work* work;
    s32               soundId;
    s32               panOffset;
    s16               splashY;

    work              = task->work;
    work->stateFrames = work->stateFrames + 1;
    // Preserve the unsigned-angle shifts used to settle the look before diving.
    if (work->stateFrames < ACTOR_206100_DIVE_SPLASH_TICK) {
        work->lookPitch = work->lookPitch + ((s32) - (work->lookPitch << 0x14) >> 0x15);
        work->lookYaw   = work->lookYaw + ((s32) - (work->lookYaw << 0x14) >> 0x15);
    }
    splashY = ACTOR_206100_DIVE_SPLASH_Y;
    if (work->stateFrames == ACTOR_206100_DIVE_SPLASH_TICK) {
        _actor206100SpawnSplashRing(task, splashY);
        soundId   = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_206100_DIVE_SOUND_INSTANCE_SHIFT) | ACTOR_206100_DIVE_SOUND;
        panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        // The underwater wait owns the timer after the splash.
        work->stateFrames = 0;
        work->goalY       = ACTOR_206100_DIVE_GOAL_Y;
        work->subState    = work->subState + 1;
    }
}
/// Dispatches surface entry, splash or completion after consuming hit reactions.
///
/// Requires live work/model and subState 0..2. Reactions selecting another fight
/// state skip the surface handler. Otherwise turns toward the cached target by
/// 24 angle units outside a 40-unit deadband and takes a bounded 20-unit step,
/// even when the handler changes state. The fight driver applies playback later.
static void _actor206100DispatchSurface(Task* task)
{
    enum { ACTOR_206100_SURFACE_YAW_STEP      = 24,
           ACTOR_206100_SURFACE_YAW_DEADBAND  = 40,
           ACTOR_206100_SURFACE_STEP_DISTANCE = 20 };
    _Actor206100Work* work             = task->work;
    TaskFuncTable3    substateHandlers = D_actor_206100_80149EB4;

    if (_actor206100ConsumeHitReaction(task) == 0) {
        substateHandlers.funcs[work->subState](task);
        // Complete this tick's approach even if the splash selected another dive.
        _actor206100TurnTowardPoint(task, &work->targetPos, ACTOR_206100_SURFACE_YAW_STEP, ACTOR_206100_SURFACE_YAW_DEADBAND);
        _actor206100StepWithinFightArea(task, ACTOR_206100_SURFACE_STEP_DISTANCE);
    }
}
/// Emits the surface splash and chooses another dive or surface completion.
///
/// Requires the initialized surface substate with a cleared timer. Tick 7 emits
/// 32 water particles at local Y -1000. At tick 31, a fresh LCG draw chooses
/// another dive with probability one in four; otherwise clears the timer and
/// advances to the wait for the surface clip boundary.
static void _actor206100SurfaceSplashTick(Task* task)
{
    enum {
        ACTOR_206100_SURFACE_SPLASH_TICK      = 7,
        ACTOR_206100_SURFACE_DECISION_TICK    = 31,
        ACTOR_206100_SURFACE_SPLASH_Y         = -1000,
        ACTOR_206100_SURFACE_REDIVE_DRAW_MASK = 3
    };
    _Actor206100Work* work;
    s16               splashY;

    work              = task->work;
    work->stateFrames = work->stateFrames + 1;
    if (work->stateFrames == ACTOR_206100_SURFACE_SPLASH_TICK) {
        splashY = ACTOR_206100_SURFACE_SPLASH_Y;
        _actor206100SpawnSplashRing(task, splashY);
    }
    // One draw in four restarts the dive; the rest advance to surface completion.
    if (work->stateFrames >= ACTOR_206100_SURFACE_DECISION_TICK) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (((gRandomLcgState >> 0x10) & ACTOR_206100_SURFACE_REDIVE_DRAW_MASK) == 0) {
            _diverSetState(task, ACTOR_206100_FIGHT_STATE_DIVE);
            return;
        }
        work->stateFrames = 0;
        work->subState    = work->subState + 1;
    }
}
/// Advances the Sea Diver's waypoint-ring phase and its body animation.
///
/// Requires initialized work/model and ring state 0 (place) or 1 (swim and
/// summon); dispatch is unchecked. Running combat advances the wrapping counters,
/// dispatches the ring state and ticks slots 1..14 before rebuilding root roll,
/// heading and signed Q12 scale. Paused combat refreshes colour and allows drawing
/// without advancing motion or playback. Hidden combat suppresses drawing and
/// returns. The loaded waypoint and animation resources stay borrowed by work.
static void _actor206100CirclingTick(Task* task)
{
    _Actor206100Work* work          = task->work;
    TmdObject*        model         = task->extra.tmd;
    TaskFunc          ringStates[2] = {
        _actor206100PlaceOnWaypointRing,
        _actor206100SummonBogDiversTick,
    };

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->bobPhase   = work->bobPhase + 1;
            work->frameCount = work->frameCount + 1;
            ringStates[work->state](task);
            _actor206100AnimUpdate(task);
            work->animStatus = work->rig.slots[1].status.fields.flags;
            _actor206100UpdateNeckRetraction(task, work->neckRetracted);
            _actor206100ApplyRootRotation(task);
            _actor206100ScaleCoordUniform(task->extra.tmd->coords, work->modelScale);
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actorRenderUpdateModelColor(task);
            model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
}
/// Maintains two summoned Bog Divers while swimming the waypoint ring.
///
/// Requires live work and both borrowed slot records. Holds the timer at tick 30
/// and re-engages battle, then fills empty slots until five divers have spawned.
/// An empty slot waits out its cooldown. Retiring a dead diver arms a 180-tick
/// cooldown; the fifth retirement starts the entrance in task fight state 2.
/// Slot pointers are borrowed Enemy records and are cleared as soon as HP is zero.
static void _actor206100SummonBogDiversTick(Task* task)
{
    enum {
        ACTOR_206100_RING_ENGAGE_TICK       = 30,
        ACTOR_206100_TASK_STATE_FIGHT       = 2,
        ACTOR_206100_BOG_DIVER_SUMMON_COUNT = 5
    };
    _Actor206100Work* work;
    _Actor206100Work* transitionWork;
    Enemy*            enemy;
    s32               summonSlotIndex;
    s32               retireSlotIndex;
    u8                killedCount;

    work = task->work;
    if (work->stateFrames == ACTOR_206100_RING_ENGAGE_TICK) {
        sceneEngageBattle(1);
    } else {
        work->stateFrames = work->stateFrames + 1;
    }
    _actor206100SwimWaypointRing(task);
    // Fill empty slots before retiring this tick's dead enemies.
    summonSlotIndex = 0;
    do {
        if (work->bogDiversSpawned < ACTOR_206100_BOG_DIVER_SUMMON_COUNT && D_actor_206100_80158CBC[summonSlotIndex].enemy == NULL) {
            if (D_actor_206100_80158CBC[summonSlotIndex].summonCooldown == 0) {
                enemy = _actor206100SpawnBogDiver(work->bogDiversSpawned);
                if (enemy != NULL) {
                    D_actor_206100_80158CBC[summonSlotIndex].enemy = enemy;
                    enemy->hp                                      = 1; // Liveness marker before the Bog Diver spawn callback initializes HP.
                    work->bogDiversSpawned                         = work->bogDiversSpawned + 1;
                }
            } else {
                D_actor_206100_80158CBC[summonSlotIndex].summonCooldown = D_actor_206100_80158CBC[summonSlotIndex].summonCooldown - 1;
            }
        }
        summonSlotIndex++;
    } while (summonSlotIndex < ARRAY_SIZE(D_actor_206100_80158CBC));
    retireSlotIndex = 0;
    do {
        if (D_actor_206100_80158CBC[retireSlotIndex].enemy != NULL &&
            D_actor_206100_80158CBC[retireSlotIndex].enemy->hp <= 0) {
            D_actor_206100_80158CBC[retireSlotIndex].enemy          = NULL;
            D_actor_206100_80158CBC[retireSlotIndex].summonCooldown = ACTOR_206100_BOG_DIVER_SUMMON_COOLDOWN;
            killedCount                                             = work->bogDiversKilled + 1;
            work->bogDiversKilled                                   = killedCount;
            if (killedCount >= ACTOR_206100_BOG_DIVER_SUMMON_COUNT) {
                transitionWork           = task->work;
                task->state              = ACTOR_206100_TASK_STATE_FIGHT;
                transitionWork->state    = ACTOR_206100_FIGHT_STATE_ENTRANCE;
                transitionWork->subState = 0;
            }
        }
        retireSlotIndex++;
    } while (retireSlotIndex < ARRAY_SIZE(D_actor_206100_80158CBC));
}
/// Steers and swims around the Sea Diver's eight-point waypoint ring.
///
/// Requires the live model and borrowed ring, with `waypointIndex` in 0..7.
/// Sets goal Y from the waypoint. A signed-halfword horizontal distance below
/// 1000 advances the index and ends movement for this tick; otherwise turns
/// by 44 angle units outside a 256-unit deadband and steps 64 coordinate units.
/// Every six arrivals re-arms a roll, advanced 32 angle units per active tick
/// until it reaches a whole-turn boundary. Angles use 4096 units per turn.
static void _actor206100SwimWaypointRing(Task* task)
{
    enum { ACTOR_206100_RING_ARRIVAL_RADIUS         = 1000,
           ACTOR_206100_RING_ROLL_STEP              = 32,
           ACTOR_206100_RING_ANGLE_MASK             = 0xFFF,
           ACTOR_206100_RING_ROLL_WAYPOINT_INTERVAL = 6,
           ACTOR_206100_RING_YAW_STEP               = 44,
           ACTOR_206100_RING_YAW_DEADBAND           = 256,
           ACTOR_206100_RING_STEP_DISTANCE          = 64 };
    _Actor206100Work* ringWork = task->work;
    GfxCoord*         rootCoord;
    SVECTOR*          waypoints;
    u32               waypointIndex;
    SVECTOR           waypointOffset;
    u16               nextRoll;
    u8                reachedCount;

    rootCoord = task->extra.tmd->coords;
    if (ringWork->waypointsSinceRoll == 0) {
        ringWork->rolling = 1;
    }
    if (ringWork->rolling == 1) {
        nextRoll              = ringWork->rotation.vz + ACTOR_206100_RING_ROLL_STEP;
        ringWork->rotation.vz = nextRoll;
        if ((nextRoll & ACTOR_206100_RING_ANGLE_MASK) == 0) {
            ringWork->rolling = 0;
        }
    }
    // Advance only after entering the waypoint's horizontal arrival radius.
    waypointOffset.vx = ringWork->waypoints[ringWork->waypointIndex].vx - rootCoord->coord.t[0];
    waypointOffset.vy = ringWork->waypoints[ringWork->waypointIndex].vy - rootCoord->coord.t[1];
    waypointOffset.vz = ringWork->waypoints[ringWork->waypointIndex].vz - rootCoord->coord.t[2];
    ringWork->goalY   = ringWork->waypoints[ringWork->waypointIndex].vy;
    if ((s16)SquareRoot0(waypointOffset.vx * waypointOffset.vx + waypointOffset.vz * waypointOffset.vz) < ACTOR_206100_RING_ARRIVAL_RADIUS) {
        ringWork->waypointIndex      = (ringWork->waypointIndex + 1) & (ARRAY_SIZE(D_actor_206100_80158B68) - 1);
        reachedCount                 = ringWork->waypointsSinceRoll + 1;
        ringWork->waypointsSinceRoll = reachedCount;
        if (reachedCount >= ACTOR_206100_RING_ROLL_WAYPOINT_INTERVAL) {
            ringWork->waypointsSinceRoll = 0;
        }
    } else {
        waypoints     = ringWork->waypoints;
        waypointIndex = ringWork->waypointIndex;
        _actor206100TurnTowardPoint(task, &waypoints[waypointIndex], ACTOR_206100_RING_YAW_STEP, ACTOR_206100_RING_YAW_DEADBAND);
        _diverStepForward(task, ACTOR_206100_RING_STEP_DISTANCE, ringWork->rotation.vy);
    }
}
/// Advances the neck/head recoil pitch through voice, rise and return phases.
///
/// Requires live work and Enemy; a hit re-arms the reaction without clearing pitch.
/// Angles use 4096 units per turn. Settling decays by an eighth, rising eases
/// halfway toward recoilPeak until within 32 units, and return subtracts 16 units
/// per tick before snapping to zero. Neck/head posing has already consumed the
/// current pitch, so this update affects its next pose. The rise keeps unsigned
/// halfword operands and signed shifted subtraction, preserving their narrowing.
static void _actor206100RecoilPitchTick(Task* task)
{
    enum { ACTOR_206100_RECOIL_SETTLE               = 0,
           ACTOR_206100_RECOIL_VOICE                = 1,
           ACTOR_206100_RECOIL_RISE                 = 2,
           ACTOR_206100_RECOIL_FALL                 = 3,
           ACTOR_206100_RECOIL_PEAK_TOLERANCE       = 32,
           ACTOR_206100_RECOIL_RETURN_STEP          = 16,
           ACTOR_206100_RECOIL_SOUND                = 0x40040006,
           ACTOR_206100_RECOIL_SOUND_INSTANCE_SHIFT = 8 };
    _Actor206100Work* work;
    s32               soundId;
    s32               panOffset;
    s16               nextPitch;

    work = task->work;
    switch (work->recoilPhase) {
        case ACTOR_206100_RECOIL_SETTLE:
            work->recoilPitch = work->recoilPitch + ((s32) - (work->recoilPitch * 0x10) >> 7);
            break;
        case ACTOR_206100_RECOIL_VOICE:
            soundId   = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_206100_RECOIL_SOUND_INSTANCE_SHIFT) | ACTOR_206100_RECOIL_SOUND;
            panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            work->recoilPhase = work->recoilPhase + 1;
            break;
        case ACTOR_206100_RECOIL_RISE:
            nextPitch         = (u16)work->recoilPitch + ((s32)(((u16)work->recoilPeak - (u16)work->recoilPitch) << 0x14) >> 0x15);
            work->recoilPitch = nextPitch;
            if (nextPitch < work->recoilPeak - ACTOR_206100_RECOIL_PEAK_TOLERANCE) {
                break;
            }
            work->recoilPhase = work->recoilPhase + 1;
            break;
        case ACTOR_206100_RECOIL_FALL:
            nextPitch         = (u16)work->recoilPitch - ACTOR_206100_RECOIL_RETURN_STEP;
            work->recoilPitch = nextPitch;
            if ((nextPitch << 0x10) <= 0) {
                work->recoilPhase = ACTOR_206100_RECOIL_SETTLE;
                work->recoilPitch = 0;
            }
            break;
    }
}
/// Poses the released neck and head to look toward the cached target.
///
/// Requires the live model, current composed head/view matrices and target
/// position in the root's parent frame. Measures from 256 units below the
/// head, eases the stored look angles inside the forward cone and returns them
/// toward zero outside it. Spreads yaw over neck parts 2, 3 and head part 4;
/// adds recoil pitch and clamps the head pitch to -512..768 angle units.
/// Angles use 4096 units per turn. Preserves translations and updates the
/// head's composition; changes GTE state while composing and rotating joints.
static void _actor206100AimHead(Task* task)
{
    enum { ACTOR_206100_LOOK_ORIGIN_Y_OFFSET = 256,
           ACTOR_206100_LOOK_FORWARD_LIMIT   = 1023,
           ACTOR_206100_LOOK_DEADBAND        = 32,
           ACTOR_206100_LOOK_YAW_STEP        = 24,
           ACTOR_206100_HEAD_PITCH_MAX       = 768,
           ACTOR_206100_HEAD_PITCH_MIN       = -512 };
    SVECTOR           desiredAngles;
    SVECTOR*          desiredAnglesPtr;
    SVECTOR           lowerNeckAngles;
    SVECTOR           upperNeckAngles;
    MATRIX            inverseTrunkBasis;
    MATRIX            inverseLowerNeckBasis;
    MATRIX            inverseUpperNeckBasis;
    MATRIX            lowerNeckBasis;
    MATRIX            upperNeckBasis;
    MATRIX            headBasis;
    MATRIX            headToWorld;
    VECTOR            targetOffset;
    VECTOR            localTargetOffset;
    GfxCoord*         trunkCoord;
    GfxCoord*         upperNeckCoord;
    GfxCoord*         headCoord;
    _Actor206100Work* work;
    GfxCoord*         coords;
    GfxCoord*         lowerNeckCoord;
    MATRIX*           lowerNeckMatrix;
    MATRIX*           upperNeckMatrix;
    MATRIX*           headMatrix;
    s32               signedPitch;
    s32               signedYaw;
    s32               signedRoll;
    s32               combinedPitch;
    u16               desiredPitchBits;
    u16               currentPitchBits;
    u32               pitchDifference;
    s16               clampedPitch;

    coords         = task->extra.tmd->coords;
    work           = task->work;
    trunkCoord     = &coords[1];
    lowerNeckCoord = &coords[2];
    upperNeckCoord = &coords[3];
    headCoord      = &coords[4];
    // Measure the target in the root's axes, then ease the stored look.
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &headCoord->workm, &headToWorld);
    targetOffset.vx = work->targetPos.vx - headToWorld.t[0];
    targetOffset.vy = work->targetPos.vy - (headToWorld.t[1] + ACTOR_206100_LOOK_ORIGIN_Y_OFFSET);
    targetOffset.vz = work->targetPos.vz - headToWorld.t[2];
    ApplyTransposeMatrixLV(&coords->coord, &targetOffset, &localTargetOffset);
    desiredAnglesPtr     = &desiredAngles;
    desiredAnglesPtr->vx = (ratan2(-localTargetOffset.vy, localTargetOffset.vz) << 20) >> 20;
    desiredAnglesPtr->vy = (ratan2(localTargetOffset.vx, localTargetOffset.vz) << 20) >> 20;
    desiredAnglesPtr->vz = 0;
    signedPitch          = (s16)work->lookPitch;
    signedYaw            = (s16)work->lookYaw;
    signedRoll           = (s16)work->lookRoll;
    work->lookPitch      = signedPitch;
    work->lookYaw        = signedYaw;
    work->lookRoll       = signedRoll;
    if ((u16)(desiredAngles.vy + ACTOR_206100_LOOK_FORWARD_LIMIT) < 2 * ACTOR_206100_LOOK_FORWARD_LIMIT + 1) {
        if ((u32)(((s16)desiredAngles.vy - (s16)work->lookYaw) + ACTOR_206100_LOOK_DEADBAND) >= 2 * ACTOR_206100_LOOK_DEADBAND + 1) {
            work->lookYaw = ((s16)work->lookYaw < desiredAngles.vy) ? work->lookYaw + ACTOR_206100_LOOK_YAW_STEP
                                                                    : work->lookYaw - ACTOR_206100_LOOK_YAW_STEP;
        }
        desiredPitchBits = desiredAngles.vx;
        if ((u16)(desiredPitchBits + ACTOR_206100_LOOK_FORWARD_LIMIT) < 2 * ACTOR_206100_LOOK_FORWARD_LIMIT + 1) {
            pitchDifference  = ((s16)desiredPitchBits - (s16)work->lookPitch) + ACTOR_206100_LOOK_DEADBAND;
            currentPitchBits = work->lookPitch;
            if (pitchDifference >= 2 * ACTOR_206100_LOOK_DEADBAND + 1) {
                work->lookPitch = currentPitchBits + ((s32)(((u16)desiredPitchBits - currentPitchBits) << 20) >> 23);
            }
        }
    } else {
        work->lookYaw   = work->lookYaw + ((s32) - (s16)(work->lookYaw * 0x10) >> 8);
        work->lookPitch = work->lookPitch + ((s32) - (s16)(work->lookPitch * 0x10) >> 8);
    }
    lowerNeckMatrix = &lowerNeckCoord->coord;

    gfxSetRotIdentity(&lowerNeckBasis);
    gfxSetRotIdentity(&upperNeckBasis);
    gfxSetRotIdentity(&headBasis);

    // Rebuild neck pitch before spreading the look yaw over the three joints.
    gfxExtractEulerAngles(lowerNeckMatrix, &lowerNeckAngles);
    upperNeckMatrix = &upperNeckCoord->coord;
    gfxExtractEulerAngles(&upperNeckCoord->coord, &upperNeckAngles);
    RotMatrixX(lowerNeckAngles.vx + (s16)(work->recoilPitch / 3), &lowerNeckBasis);
    RotMatrixX(upperNeckAngles.vx + (s16)(work->recoilPitch / 3), &upperNeckBasis);
    _actor206100CopyRotationElements(&lowerNeckBasis, lowerNeckMatrix);
    _actor206100CopyRotationElements(&upperNeckBasis, upperNeckMatrix);
    actorRenderComposeCoord(trunkCoord);
    actorRenderComposeCoord(lowerNeckCoord);
    actorRenderComposeCoord(upperNeckCoord);
    _diverTurnJoint(lowerNeckCoord, (s16)work->lookYaw / 3);
    _diverTurnJoint(upperNeckCoord, (s16)work->lookYaw / 3);

    gfxSetRotIdentity(&headBasis);

    combinedPitch = (s16)work->lookPitch + work->recoilPitch;
    clampedPitch  = combinedPitch;
    if (clampedPitch >= ACTOR_206100_HEAD_PITCH_MAX) {
        clampedPitch = ACTOR_206100_HEAD_PITCH_MAX;
    } else if (clampedPitch < ACTOR_206100_HEAD_PITCH_MIN) {
        clampedPitch = ACTOR_206100_HEAD_PITCH_MIN;
    }
    RotMatrixX((s32)clampedPitch, &headBasis);
    RotMatrixY((s16)((s16)work->lookYaw / 3), &headBasis);
    // Undo the posed parent rotations to install the desired head basis.
    TransposeMatrix(&trunkCoord->coord, &inverseTrunkBasis);
    TransposeMatrix(&lowerNeckCoord->coord, &inverseLowerNeckBasis);
    TransposeMatrix(&upperNeckCoord->coord, &inverseUpperNeckBasis);
    MulMatrix(&inverseTrunkBasis, &inverseLowerNeckBasis);
    MulMatrix(&inverseTrunkBasis, &inverseUpperNeckBasis);
    MulMatrix(&inverseTrunkBasis, &headBasis);
    headMatrix = &headCoord->coord;
    _actor206100CopyRotationElements(&inverseTrunkBasis, headMatrix);
    headCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(headCoord);
}

/// Runs the Sea Diver death stages and eases its root toward the death goal.
///
/// Requires live work/model and state 0..3, selecting unlink, playback entry,
/// playback or sinking. Running control dispatches the stage, captures slot-1
/// status and eases XYZ by one sixteenth in parent-space game units. Paused
/// control only refreshes cached-position lighting; hidden control excludes
/// the model from active drawing. The current tick finishes after a stage change.
static void _actor206100DeathTick(Task* task)
{
    _Actor206100Work* work;
    TmdObject*        model;
    GfxCoord*         rootCoord;
    TaskFuncTable4    deathHandlers;

    work          = task->work;
    model         = task->extra.tmd;
    rootCoord     = model->coords;
    deathHandlers = D_actor_206100_80149EC0;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            deathHandlers.funcs[work->state](task);
            work->animStatus      = work->rig.slots[1].status.fields.flags;
            rootCoord->coord.t[0] = rootCoord->coord.t[0] + (-rootCoord->coord.t[0] >> 4);
            rootCoord->coord.t[2] = rootCoord->coord.t[2] + (-rootCoord->coord.t[2] >> 4);
            rootCoord->coord.t[1] =
                rootCoord->coord.t[1] + ((work->goalY - rootCoord->coord.t[1]) >> 4);
            /* fallthrough */
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actorRenderUpdateModelColor(task);
            return;
    }
}
/// Advances death animation for fifty active ticks before the sinking step.
///
/// Requires live work, model and initialized body slots. Applies pending
/// animation requests and ticks slots 1..14 without refreshing cached status;
/// the death driver captures it afterward. Dirties the root each tick, then
/// clears the state timer and advances the death-state index at tick 50.
static void _actor206100DeathPlaybackTick(Task* task)
{
    enum { ACTOR_206100_DEATH_PLAYBACK_FRAMES = 50 };
    _Actor206100Work* work;
    GfxCoord*         rootCoord;

    work              = task->work;
    rootCoord         = task->extra.tmd->coords;
    work->stateFrames = work->stateFrames + 1;
    _actor206100AnimUpdate(task);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->stateFrames >= ACTOR_206100_DEATH_PLAYBACK_FRAMES) {
        work->stateFrames = 0;
        work->state       = work->state + 1;
    }
}
#include "../../shared/diver_step_forward.inc.c"

/// Starts the neck/head recoil and the head-child pitch reaction.
///
/// `peakPitch` is the recoil's positive target in 4096ths of a turn (309 for
/// light hits, 928 for heavy hits). Re-arms both reaction phases without
/// resetting their current pitch, so a new hit can interrupt a running recoil.
static void _actor206100StartRecoil(Task* task, s16 peakPitch)
{
    enum { ACTOR_206100_PART5_PHASE_START  = 1,
           ACTOR_206100_RECOIL_PHASE_START = 1 };
    _Actor206100Work* work = task->work;

    work->part5Phase  = ACTOR_206100_PART5_PHASE_START;
    work->recoilPhase = ACTOR_206100_RECOIL_PHASE_START;
    work->recoilPeak  = peakPitch;
}

/// Adds the recoil pitch to the animated head-child coordinate, part 5.
///
/// Requires a freshly animated live model pose. Extracts Euler angles, adds
/// part5Pitch in 4096ths of a turn and rebuilds only the nine rotation coefficients.
/// Preserves translation, alignment bytes and the composition stamp; applying it
/// repeatedly without restoring animation would accumulate the pitch.
static void _actor206100ApplyPart5Recoil(Task* task)
{
    enum { ACTOR_206100_RECOIL_CHILD_PART = 5 };
    _Actor206100Work* work;
    GfxCoord*         coords;
    SVECTOR           partAngles;
    MATRIX            partBasis;
    MATRIX*           partMatrix;

    work       = task->work;
    coords     = task->extra.tmd->coords;
    partMatrix = &coords[ACTOR_206100_RECOIL_CHILD_PART].coord;

    gfxSetRotIdentity(&partBasis);

    gfxExtractEulerAngles(partMatrix, &partAngles);
    partAngles.vx += work->part5Pitch;
    RotMatrix(&partAngles, &partBasis);
    _actor206100CopyRotationElements(&partBasis, partMatrix);
}

/// Advances the recoil pitch offset of the head's child, part 5.
///
/// Requires live work. Angles use 4096 units per turn: entry arms the swing,
/// which eases one quarter toward -768 until below -735. Return adds 28 per tick
/// until nonnegative, then settling decays the retained overshoot by an eighth.
/// Keeps signed halfword results and unsigned addends; it does not snap to zero.
static void _actor206100Part5RecoilTick(Task* task)
{
    enum { ACTOR_206100_PART5_SETTLE           = 0,
           ACTOR_206100_PART5_START            = 1,
           ACTOR_206100_PART5_SWING            = 2,
           ACTOR_206100_PART5_RETURN           = 3,
           ACTOR_206100_PART5_PEAK_PITCH       = -768,
           ACTOR_206100_PART5_RETURN_THRESHOLD = -735,
           ACTOR_206100_PART5_RETURN_STEP      = 28 };
    _Actor206100Work* work = task->work;
    s16               swingPitch;
    s16               returnPitch;

    switch (work->part5Phase) {
        case ACTOR_206100_PART5_SETTLE:
            work->part5Pitch = (u16)work->part5Pitch + ((-(work->part5Pitch * 0x10)) >> 7);
            return;
        case ACTOR_206100_PART5_START:
            work->part5Phase = ACTOR_206100_PART5_SWING;
            return;
        case ACTOR_206100_PART5_SWING:
            swingPitch       = (u16)work->part5Pitch + ((ACTOR_206100_PART5_PEAK_PITCH * 16 - work->part5Pitch * 0x10) >> 6);
            work->part5Pitch = swingPitch;
            if (swingPitch < ACTOR_206100_PART5_RETURN_THRESHOLD) {
                work->part5Phase++;
                return;
            }
            return;
        case ACTOR_206100_PART5_RETURN:
            returnPitch      = (u16)work->part5Pitch + ACTOR_206100_PART5_RETURN_STEP;
            work->part5Pitch = returnPitch;
            if (returnPitch >= 0) {
                work->part5Phase = ACTOR_206100_PART5_SETTLE;
            }
            break;
    }
}
/// Steps along the heading and restores this frame's XZ if outside the fight area.
///
/// `distance` is a signed step in root-parent coordinate units. Requires the
/// live model/work and `prevRootPos` captured at the start of the frame.
/// After stepping, measures signed-halfword XZ distance from the origin; beyond
/// `ACTOR_206100_FIGHT_AREA_RADIUS` restores the frame-start XZ, preserving Y.
/// Borrows and releases one scratch-stack block and dirties the root.
static void _actor206100StepWithinFightArea(Task* task, s16 distance)
{
    _Actor206100Work*                  work;
    GfxCoord*                          rootCoord;
    _Actor206100OriginDistanceScratch* scratch;

    scratch   = SCRATCH_STACK_RESERVE_BLOCK(_Actor206100OriginDistanceScratch);
    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    _diverStepForward(task, distance, work->rotation.vy);
    scratch->toOrigin.vx = -rootCoord->coord.t[0];
    scratch->toOrigin.vz = -rootCoord->coord.t[2];
    scratch->distance    = SquareRoot0(scratch->toOrigin.vx * scratch->toOrigin.vx +
                                       scratch->toOrigin.vz * scratch->toOrigin.vz);
    // Restore the frame-start position, rather than only undoing this step.
    if (scratch->distance > ACTOR_206100_FIGHT_AREA_RADIUS) {
        rootCoord->coord.t[0] = work->prevRootPos.vx;
        rootCoord->coord.t[2] = work->prevRootPos.vz;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor206100OriginDistanceScratch);
}

/// Spawns one Bog Diver and assigns its Sea Diver summon placement and palette.
///
/// placementIndex is 0..4; it also supplies the four-bit enemy instance key.
/// Requires the Bog Diver package and its model resources in the other live slot.
/// Returns the spawned Enemy or NULL on failure. Borrows the placement record
/// for that enemy's lifetime, so this overlay must remain loaded. Uses variant 3,
/// texture-page offset zero and CLUT-row offset two, rebuilding both buffer halves.
static Enemy* _actor206100SpawnBogDiver(s32 placementIndex)
{
    enum {
        ACTOR_206100_BOG_DIVER_VARIANT         = 3,
        ACTOR_206100_BOG_DIVER_CLUT_ROW_OFFSET = 2
    };
    Enemy*     enemy;
    TmdObject* model;

    enemy = enemySpawnFromTable(&D_actor_100400_80147E48, 0, ACTOR_206100_BOG_DIVER_VARIANT, NULL);
    if (enemy != NULL) {
        enemy->placeKey          = placementIndex << ENEMY_PLACE_INDEX_SHIFT;
        enemy->place             = &D_actor_206100_80155134[(s16)placementIndex];
        model                    = enemy->task->extra.tmd;
        model->texturePageOffset = 0;
        model->clutRowOffset     = ACTOR_206100_BOG_DIVER_CLUT_ROW_OFFSET;
        // Rebuild both primitive-buffer halves after applying the summon palette.
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
        return enemy;
    }
    return NULL;
}

/// Links the Sea Diver shot attack sphere and emits its launch burst.
///
/// Requires the task-owned zeroed shot work and a coordinate body placed by
/// the head-shot spawner. Parents it to the view, initializes two contacts and
/// a 320-unit sphere carrying attack entry zero, then enables grid/pair tests.
/// Composes the shot coordinate before its launch effect and enters flight.
/// The teardown stage owns unlinking; task teardown releases the work.
static void _actor206100LaunchShot(Task* task)
{
    enum {
        ACTOR_206100_SHOT_RADIUS = 320
    };
    _Actor206100ShotWork*  shot;
    WorldCollisionContact* contacts;
    GfxCoord*              shotCoord;

    shot                    = task->work;
    shotCoord               = task->extra.tmd->coords;
    task->killCountdown     = 0;
    shot->burstSize         = ACTOR_206100_SHOT_BURST_SIZE_STEP;
    shot->burstPhase        = 0;
    shotCoord->parent       = &gGfxViewCoord;
    shotCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    // Link the sphere before enabling grid and pair tests against its live contacts.
    shot->strike.attackBody.key              = damagePackAttackKey(&D_actor_206100_80155194, 0);
    shot->strike.attackBody.coord            = task->extra.tmd->coords;
    contacts                                 = shot->contacts;
    shot->strike.attackBody.context.contacts = contacts;
    shot->strike.attackBody.pos.vx           = 0;
    shot->strike.attackBody.pos.vy           = 0;
    shot->strike.attackBody.pos.vz           = 0;
    shot->strike.attackBody.radius           = ACTOR_206100_SHOT_RADIUS;
    shot->strike.attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &shot->strike.attackBody);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(shot->contacts), 0);
    shot->strike.attackBody.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    actorRenderComposeCoord(shotCoord);
    _diverImpactBurst(shotCoord, (u16)shot->burstPhase, DIVER_BURST_LAUNCH, shot->burstSize + ACTOR_206100_SHOT_BURST_VARIANT);
    task->state++;
}

#include "../../shared/diver_strike_teardown.inc.c"

#include "../../shared/coord_math_local_to_world.inc.c"

/// Dispatches a Sea Diver shot through launch, flight and burst teardown.
///
/// Requires Task::state in 0..2 and a live coordinate body/shot work. The shot
/// spawner initializes state zero; launch and flight advance it, and teardown
/// ends the task after its linger interval.
static void _actor206100ShotTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_206100_80149E24;
    stateHandlers.funcs[task->state](task);
}

/// Links the Sea Diver's trunk and head spheres to its shared hit-contact table.
///
/// Requires zeroed live work and model coordinates 1 and 4. Both spheres have
/// the same category-3 identity; the trunk radius is 1024 and the head radius
/// is 512 in coordinate units. Clears all six contacts once, links both bodies
/// to the enemy-body list, and leaves pair testing disabled. Teardown must
/// unlink both bodies before the model or work is released.
static void _actor206100InitHitBodies(Task* task)
{
    enum { ACTOR_206100_HIT_BODY_IDENTITY = 0x3D,
           ACTOR_206100_TRUNK_HIT_RADIUS  = 1024,
           ACTOR_206100_HEAD_HIT_RADIUS   = 512 };
    _Actor206100Work* work;

    work = task->work;

    work->trunkBody.coord            = &task->extra.tmd->coords[1];
    work->trunkBody.context.contacts = work->hitContacts;
    work->trunkBody.pos.vx           = 0;
    work->trunkBody.pos.vy           = 0;
    work->trunkBody.pos.vz           = 0;
    work->trunkBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_206100_HIT_BODY_IDENTITY;
    work->trunkBody.radius           = ACTOR_206100_TRUNK_HIT_RADIUS;
    work->trunkBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->trunkBody);
    worldCollisionInitContacts(work->hitContacts, ARRAY_SIZE(work->hitContacts), 0);
    work->trunkBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->headBody.coord            = &task->extra.tmd->coords[4];
    work->headBody.context.contacts = work->hitContacts;
    work->headBody.pos.vx           = 0;
    work->headBody.pos.vy           = 0;
    work->headBody.pos.vz           = 0;
    work->headBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_206100_HIT_BODY_IDENTITY;
    work->headBody.radius           = ACTOR_206100_HEAD_HIT_RADIUS;
    work->headBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->headBody);
    work->headBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

#include "../../shared/diver_restart_clip.inc.c"

/// Captures body-slot poses and blends them toward the requested clip start.
///
/// Requires a live initialized rig and a loaded `animClip` supporting slots 1..14.
/// `animStep` narrows to each slot's signed-byte rate in sixteenths of a frame.
/// `animBlend` counts whole normal-rate frames; 0..2047 keeps blend time
/// nonnegative. Leaves slot 0 and request fields intact. Pose buffers and clip
/// data must remain live throughout the blend; pose capture changes GTE state.
static inline void _actor206100SeekBodySlotsWithBlend(_Actor206100Work* work)
{
    s32 slotIndex;

    slotIndex = 1;
    do {
        work->rig.slots[slotIndex].rate = work->animStep;
        animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animClip, 0, work->animBlend);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->rig.slots));
}

/// Seeks body slots 1..14 to the requested clip with the pending blend.
///
/// Requires the initialized live rig and a loaded clip supporting every body
/// track. Each slot takes `animStep` narrowed to a signed-byte rate in
/// sixteenths of a frame, then captures and blends toward track record 0.
/// `animBlend` counts normal-rate frames (0..2047 keeps the remaining blend
/// nonnegative). Clears it after a changed clip, preserves it for a repeated
/// clip, and latches `animPlaying`. Leaves request acknowledgement to the
/// animation driver; changes GTE state during pose capture.
static void _actor206100BlendRequestedClip(Task* task)
{
    _Actor206100Work* work;

    work = task->work;
    if (work->animPlaying == work->animClip) {
        _actor206100SeekBodySlotsWithBlend(work);
    } else {
        _actor206100SeekBodySlotsWithBlend(work);
        work->animBlend = 0;
    }
    work->animPlaying = work->animClip;
}
/// Converts a frame count to the requested animation playback rate.
///
/// `animStep` is in sixteenths of a frame per tick: normal rate preserves the
/// count, double rate halves it, and zero returns 0. Keeps the signed divide
/// and shifted halfword narrowing, including their rounding and wrap behavior.
static s16 _actor206100ScaleFramesForAnimRate(Task* task, s16 frames)
{
    _Actor206100Work* work = task->work;

    if (work->animStep == 0) {
        return 0;
    }
    return ((frames << 8) / work->animStep << 12) >> 16;
}

/// Dispatches the Sea Diver through spawn, circling, fight, death and despawn.
///
/// Requires Task::state in 0..4, a live model and Enemy in spawnArg2.pointer.
/// State zero allocates owned work; later states require it. The final state
/// destroys the Enemy and lets task teardown release the model and work.
static void _actor206100Task(Task* task)
{
    TaskFuncTable5 stateHandlers;

    stateHandlers = D_actor_206100_80149E5C;
    stateHandlers.funcs[task->state](task);
}

/// Destroys the Sea Diver enemy and its task in the final task state.
///
/// Requires the live Enemy in spawnArg2.pointer. Death has already unlinked
/// the hit bodies; enemy destruction tears down the model, task and work.
static void _actor206100Despawn(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// Copies the nine rotation coefficients, preserving destination translation.
///
/// Borrows live MATRIX objects and also leaves the alignment halfword intact.
/// Source and destination may be the same object.
static void _actor206100CopyRotation(const MATRIX* source, MATRIX* destination)
{
    _actor206100CopyRotationElements(source, destination);
}

/// Dispatches the five scripted entrance substates with Sea Diver lock-on disabled.
///
/// Requires live work/model and Enemy, with subState 0..4. Assigns the complete
/// target-flag byte to NOT_LOCKABLE before dispatching. The fight tick supplies
/// animation playback, root motion and lighting after the entrance handler.
static void _actor206100DispatchEntrance(Task* task)
{
    _Actor206100Work* work;
    Enemy*            enemy;
    TaskFuncTable5    substateHandlers;

    work                          = task->work;
    enemy                         = task->spawnArg2.pointer;
    substateHandlers              = D_actor_206100_80149E94;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    substateHandlers.funcs[work->subState](task);
}

/// Empty handler for unused Sea Diver fight-state slot 4.
///
/// No state transition selects this slot; dispatch leaves the task unchanged.
static void _actor206100FightState4(Task* task)
{
}

/// Empty handler for unused Sea Diver fight-state slot 5.
///
/// No state transition selects this slot; dispatch leaves the task unchanged.
static void _actor206100FightState5(Task* task)
{
}

/// Empty handler for unused Sea Diver fight-state slot 6.
///
/// No state transition selects this slot; dispatch leaves the task unchanged.
static void _actor206100FightState6(Task* task)
{
}

/// Dispatches recoil entry or the wait for a clip boundary or control jump.
///
/// Requires live work with subState 0 or 1. Entry queues recoil playback and
/// voice; the waiting step selects the dive when cached slot status permits it.
static void _actor206100DispatchRecoil(Task* task)
{
    _Actor206100Work* work                = task->work;
    TaskFunc          substateHandlers[2] = {
        _diverEnterRecoil,
        _actor206100WaitRecoilBoundary,
    };

    substateHandlers[work->subState](task);
}

/// Dispatches entry or the active wait for the Sea Diver's status buildup.
///
/// Requires live work with subState 0 or 1. Entry sets water-level goal Y and
/// queues playback; the active step selects the attack when buildup expires.
static void _actor206100DispatchStatusHold(Task* task)
{
    _Actor206100Work* work                = task->work;
    TaskFunc          substateHandlers[2] = {
        _actor206100EnterStatusHold,
        _actor206100StatusHoldTick,
    };

    substateHandlers[work->subState](task);
}

/// Holds the player for the entrance while continuing the waypoint swim.
///
/// Requires live waypoint, animation and model state. Requests scripted player
/// hold, clears the entrance timer and advances to its lead-in substate.
static void _actor206100EntranceHoldPlayer(Task* task)
{
    _Actor206100Work* work = task->work;

    _actor206100SwimWaypointRing(task);
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    work->stateFrames = 0;
    work->subState    = work->subState + 1;
}

/// Continues the entrance swim for ninety active ticks before the white-out.
///
/// Requires live waypoint work/model. Counts through a u16 temporary and tests
/// its signed halfword view; clears the timer and advances the entrance substate
/// at tick 90. The top-level fight driver controls pause behavior.
static void _actor206100EntranceLeadInTick(Task* task)
{
    enum { ACTOR_206100_ENTRANCE_LEAD_IN_FRAMES = 90 };
    u16               stateFrames;
    _Actor206100Work* work = task->work;

    _actor206100SwimWaypointRing(task);
    stateFrames       = work->stateFrames + 1;
    work->stateFrames = stateFrames;
    if ((s16)stateFrames >= ACTOR_206100_ENTRANCE_LEAD_IN_FRAMES) {
        work->stateFrames = 0;
        work->subState    = work->subState + 1;
    }
}

/// Enters the attack's charging step with an eight-frame half-rate clip blend.
///
/// Requires live work and the loaded attack clip. Clears the state timer,
/// queues clip 7 and advances the substate; playback applies the request later.
static void _actor206100EnterAttack(Task* task)
{
    enum { ACTOR_206100_ATTACK_CLIP         = 7,
           ACTOR_206100_ATTACK_BLEND_FRAMES = 8 };
    _Actor206100Work* work;
    _Actor206100Work* requestWork;

    work              = task->work;
    work->stateFrames = 0;
    requestWork       = task->work;
    _diverRequestClipBlend(requestWork, ACTOR_206100_ATTACK_CLIP, ANIMATION_RATE_ONE / 2, ACTOR_206100_ATTACK_BLEND_FRAMES);
    work->subState = work->subState + 1;
}

/// Queues the dive's ten-frame normal-rate blend into body clip 1.
///
/// Requires live initialized animation work and loaded body tracks. Clears the
/// state timer and advances to the splash substate; playback applies the request
/// later in the fight tick.
static void _actor206100EnterDive(Task* task)
{
    _Actor206100Work* work = task->work;

    _diverRequestClipBlend(work, ACTOR_206100_DIVE_SURFACE_CLIP, ANIMATION_RATE_ONE, ACTOR_206100_DIVE_SURFACE_BLEND_FRAMES);
    work->stateFrames = 0;
    work->subState    = work->subState + 1;
}

/// Selects the surface state after ninety-one underwater waiting ticks.
///
/// Requires live work after the dive splash reset the timer. Counts through a
/// u16 temporary and tests its signed halfword view. Restarts surface at substate
/// zero without clearing the timer; surface entry clears it on the next tick.
static void _actor206100WaitUnderwater(Task* task)
{
    enum { ACTOR_206100_UNDERWATER_WAIT_FRAMES = 91 };
    u16               stateFrames;
    _Actor206100Work* work;

    work              = task->work;
    stateFrames       = work->stateFrames + 1;
    work->stateFrames = stateFrames;
    if ((s16)stateFrames >= ACTOR_206100_UNDERWATER_WAIT_FRAMES) {
        _diverSetState(task, ACTOR_206100_FIGHT_STATE_SURFACE);
    }
}

/// Starts surfacing with a voice cue, body blend and a height goal of 5000.
///
/// Requires live work/model and Enemy. Height is in root-parent game units with
/// Y downward. Queues clip 1 at normal rate over ten frames, clears the timer and
/// advances to the splash substate; the fight driver applies playback and height
/// easing afterward.
static void _actor206100EnterSurface(Task* task)
{
    enum { ACTOR_206100_SURFACE_SOUND                = 0x551E0005,
           ACTOR_206100_SURFACE_SOUND_INSTANCE_SHIFT = 8,
           ACTOR_206100_SURFACE_GOAL_Y               = 5000 };
    _Actor206100Work* work;
    _Actor206100Work* requestWork;
    s32               soundId;
    s32               panOffset;

    work      = task->work;
    soundId   = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_206100_SURFACE_SOUND_INSTANCE_SHIFT) | ACTOR_206100_SURFACE_SOUND;
    panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, panOffset,
                             (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    requestWork = task->work;
    _diverRequestClipBlend(requestWork, ACTOR_206100_DIVE_SURFACE_CLIP, ANIMATION_RATE_ONE, ACTOR_206100_DIVE_SURFACE_BLEND_FRAMES);
    work->goalY       = ACTOR_206100_SURFACE_GOAL_Y;
    work->stateFrames = 0;
    work->subState    = work->subState + 1;
}

/// Selects the attack after sixty-one ticks of the surface waiting substate.
///
/// Requires live work after the surface splash reset the timer. Counts through a
/// u16 temporary and tests its signed halfword view. Restarts attack at substate
/// zero; its entry clears the timer and queues the attack clip on the next tick.
static void _actor206100WaitSurface(Task* task)
{
    enum { ACTOR_206100_SURFACE_WAIT_FRAMES = 61 };
    u16               stateFrames;
    _Actor206100Work* work;

    work              = task->work;
    stateFrames       = work->stateFrames + 1;
    work->stateFrames = stateFrames;
    if ((s16)stateFrames >= ACTOR_206100_SURFACE_WAIT_FRAMES) {
        _diverSetState(task, ACTOR_206100_FIGHT_STATE_ATTACK);
    }
}

#include "../../shared/diver_state7_enter.inc.c"

/// Selects the dive when the recoil slot reaches a boundary or control jump.
///
/// Reads cached slot-1 status without ticking or consuming it. A true status
/// selects the dive at substate 0; otherwise leaves the state unchanged.
static void _actor206100WaitRecoilBoundary(Task* task)
{
    if (_diverClipHasBoundaryOrJump(task)) {
        _diverSetState(task, ACTOR_206100_FIGHT_STATE_DIVE);
    }
}

/// Enters the status hold at water level and queues its first animation.
///
/// Requires live work and loaded clip 14. Queues an eight-frame normal-rate
/// blend, clears the state timer, sets goal Y to the stored water level and
/// advances to the hold's waiting substate. Playback applies the request later.
static void _actor206100EnterStatusHold(Task* task)
{
    enum { ACTOR_206100_STATUS_HOLD_CLIP         = 14,
           ACTOR_206100_STATUS_HOLD_BLEND_FRAMES = 8 };
    _Actor206100Work* work = task->work;

    _diverRequestClipBlend(work, ACTOR_206100_STATUS_HOLD_CLIP, ANIMATION_RATE_ONE, ACTOR_206100_STATUS_HOLD_BLEND_FRAMES);
    work->stateFrames = 0;
    work->goalY       = work->waterLevel;
    work->subState    = work->subState + 1;
}

/// Keeps the Sea Diver in its status hold until enemy buildup expires.
///
/// Requires live work, loaded hold clips and a started Enemy buildup reaction.
/// From tick 31 targets the trunk. A cached clip boundary or control jump
/// queues the looping hold clip at half rate with an eight-frame blend.
/// Buildup completion restores the head target and restarts the attack at
/// substate zero; animation playback applies any queued request afterward.
static void _actor206100StatusHoldTick(Task* task)
{
    enum { ACTOR_206100_STATUS_HOLD_TRUNK_TARGET_FRAME = 31,
           ACTOR_206100_STATUS_HOLD_LOOP_CLIP          = 16,
           ACTOR_206100_STATUS_HOLD_LOOP_BLEND_FRAMES  = 8,
           ACTOR_206100_TRUNK_PART                     = 1,
           ACTOR_206100_HEAD_PART                      = 4 };
    u16               stateFrames;
    _Actor206100Work* work;
    _Actor206100Work* requestWork;

    work              = task->work;
    stateFrames       = work->stateFrames + 1;
    work->stateFrames = stateFrames;
    if ((s16)stateFrames >= ACTOR_206100_STATUS_HOLD_TRUNK_TARGET_FRAME) {
        work->targetPart = ACTOR_206100_TRUNK_PART;
    }
    if (_diverClipHasBoundaryOrJump(task)) {
        requestWork = task->work;
        _diverRequestClipBlend(requestWork, ACTOR_206100_STATUS_HOLD_LOOP_CLIP,
                               ANIMATION_RATE_ONE / 2, ACTOR_206100_STATUS_HOLD_LOOP_BLEND_FRAMES);
    }
    if (damageTickEnemyBuildup(task->spawnArg2.pointer) != 0) {
        work->targetPart = ACTOR_206100_HEAD_PART;
        _diverSetState(task, ACTOR_206100_FIGHT_STATE_ATTACK);
    }
}
/// Places the Sea Diver at the first vertex of its eight-point waypoint ring.
///
/// Requires live model/work and Enemy. Borrows the overlay's waypoint array for
/// the task lifetime, retracts the neck, disables lock-on and queues a normal-rate
/// restart of swim clip 3. Sets root XYZ in parent-space game units and a quarter-
/// turn heading, advances the waypoint modulo the ring size and enters summoning.
/// Sets black actor colour; the enclosing circling tick dirties/rebuilds the root.
static void _actor206100PlaceOnWaypointRing(Task* task)
{
    enum { ACTOR_206100_RING_SWIM_CLIP    = 3,
           ACTOR_206100_RING_START_YAW    = 0x400,
           ACTOR_206100_RING_STATE_SUMMON = 1 };
    GfxCoord*         rootCoord;
    _Actor206100Work* work;
    _Actor206100Work* requestWork;
    Enemy*            enemy;

    work                          = task->work;
    enemy                         = task->spawnArg2.pointer;
    rootCoord                     = task->extra.tmd->coords;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    work->neckRetracted           = 1;
    work->waypointIndex           = 0;
    work->waypoints               = D_actor_206100_80158B68;
    requestWork                   = task->work;
    requestWork->animStep         = ANIMATION_RATE_ONE;
    requestWork->animClip         = ACTOR_206100_RING_SWIM_CLIP;
    requestWork->animRequest      = DIVER_ANIM_REQUEST_RESET;
    work->rotation.vy             = ACTOR_206100_RING_START_YAW;
    rootCoord->coord.t[0]         = work->waypoints[work->waypointIndex].vx;
    rootCoord->coord.t[1]         = work->waypoints[work->waypointIndex].vy;
    rootCoord->coord.t[2]         = work->waypoints[work->waypointIndex].vz;
    work->waypointIndex           = (work->waypointIndex + 1) & (ARRAY_SIZE(D_actor_206100_80158B68) - 1);
    // Begin swimming toward the next vertex; the summon state drives the ring.
    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    work->stateFrames = 0;
    _diverSetState(task, ACTOR_206100_RING_STATE_SUMMON);
}

/// Removes Sea Diver targeting and hit bodies, grants rewards and starts death playback.
///
/// Requires the live Enemy and its linked trunk/head bodies. Stops the attack
/// loop, applies the saved area updates, sets water-level goal Y, releases the
/// battle reference with rewards and marks the progression nibble. Clears the
/// Enemy contact alias, unlinks both bodies, resets the death timer and advances
/// to playback entry. Keeps model/work live and voices the instance-tagged death.
static void _actor206100UnlinkForDeath(Task* task)
{
    enum {
        ACTOR_206100_DEATH_SOUND                = 0x40040006,
        ACTOR_206100_DEATH_SOUND_INSTANCE_SHIFT = 8
    };
    _Actor206100Work* work;
    Enemy*            enemy;
    s32               soundId;
    s32               panOffset;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    sndEvtRequestScriptStop(SOUND_NEO_ARK_SUB_GALLERY_DIVER_ATTACK_LOOP, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    areaApplySavedUpdates(D_neo_ark_submarine_gallery_8018590C);
    work->goalY = work->waterLevel;
    // End targeting and combat rewards before removing the two contact bodies.
    worldTargetUnlinkNode(&enemy->node);
    sceneReleaseBattleRefWithRewards(task, 0);
    gameFlagSetNibble(GAME_FLAG_0F3, 1);
    enemy->recs = 0;
    worldCollisionUnlinkBody(&work->trunkBody);
    worldCollisionUnlinkBody(&work->headBody);
    work->stateFrames = 0;
    work->state       = work->state + 1;
    // Resolve the voice's instance tag after reward callbacks.
    soundId   = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_206100_DEATH_SOUND_INSTANCE_SHIFT) | ACTOR_206100_DEATH_SOUND;
    panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, panOffset,
                             (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Starts death playback with a four-frame normal-rate blend into clip 14.
///
/// Requires live initialized work and loaded body tracks after death unlinking.
/// Applies the request and ticks slots 1..14 immediately, then advances to the
/// fifty-tick death playback state. Preserves the timer and cached animStatus;
/// the enclosing death driver captures slot 1's status afterward.
static void _actor206100EnterDeathPlayback(Task* task)
{
    enum { ACTOR_206100_DEATH_CLIP         = 14,
           ACTOR_206100_DEATH_BLEND_FRAMES = 4 };
    _Actor206100Work* work;

    work = task->work;
    _diverRequestClipBlend(work, ACTOR_206100_DEATH_CLIP, ANIMATION_RATE_ONE, ACTOR_206100_DEATH_BLEND_FRAMES);
    _actor206100AnimUpdate(task);
    work->state = work->state + 1;
}
/// Sinks the dying Sea Diver and selects despawn after 270 ticks.
///
/// Requires initialized work/model in the final death stage. Raises goal Y by
/// sixteen parent-coordinate units per tick (positive Y is downward); the death
/// driver eases the root toward it. Tick 90 starts the room red disc. At tick 270
/// selects task state 4 and clears its state/substate, retaining the goal and timer.
static void _actor206100DeathSinkTick(Task* task)
{
    enum {
        ACTOR_206100_DEATH_SINK_UNITS_PER_TICK = 16,
        ACTOR_206100_DEATH_ROOM_EFFECT_TICK    = 90,
        ACTOR_206100_DEATH_DESPAWN_TICK        = 270,
        ACTOR_206100_TASK_STATE_DESPAWN        = 4,
    };

    _Actor206100Work* work;
    _Actor206100Work* despawnWork;
    GfxCoord*         rootCoord;

    rootCoord               = task->extra.tmd->coords;
    work                    = task->work;
    work->stateFrames       = work->stateFrames + 1;
    work->goalY             = work->goalY + ACTOR_206100_DEATH_SINK_UNITS_PER_TICK;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->stateFrames == ACTOR_206100_DEATH_ROOM_EFFECT_TICK) {
        taskSpawnFromTable(D_neo_ark_submarine_gallery_801818BC, 0, 0, 0);
    }
    if (work->stateFrames >= ACTOR_206100_DEATH_DESPAWN_TICK) {
        task->state           = ACTOR_206100_TASK_STATE_DESPAWN;
        despawnWork           = task->work;
        despawnWork->state    = 0;
        despawnWork->subState = 0;
    }
}
