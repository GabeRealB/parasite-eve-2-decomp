#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// 0x18-byte block the watcher's routines take from the scratch stack when they
/// need a vector or two to work in: one of each width a GTE rotation works
/// between.
///
/// The block is reserved and released within one routine and carries nothing
/// from one use to the next, so each use gives the members its own meaning. The
/// routines that rebuild the beam's end point only fill `shortVec`.
typedef struct {
    VECTOR  vec;      // 32-bit vector: a view-space point (the muzzle, the target), or an offset whose length is taken (to the attacking player, from the muzzle to the strike contact); an enemy target's lock position is received in its three components
    SVECTOR shortVec; // 16-bit vector: a GTE rotation's input (the muzzle offset, the beam length along the forward axis, an enemy's lock position), or an effect's spawn offset from the watcher's coordinate
} _Actor02100VectorScratch;
STATIC_ASSERT_SIZEOF(_Actor02100VectorScratch, 0x18);

/// 0x40-byte block the scan for an enemy target takes from the scratch stack
/// and reuses for every enemy it measures.
///
/// The range and facing tests work in world space, where the watcher's
/// coordinate and an enemy's lock position both are; the sight test works in
/// view space, where the room's occluders are, so the lock position is rotated
/// by the view first.
typedef struct {
    VECTOR  viewPos;       // enemy's lock position in view space
    VECTOR  delta;         // offset from the watcher to the enemy's lock position, in world units
    VECTOR3 lockPos;       // enemy's lock position in the world
    byte    unknown_2C[4]; // never accessed; role unproven
    SVECTOR from;          // sight segment's enemy end: the world lock position as the rotation's input, then `viewPos` narrowed
    SVECTOR to;            // sight segment's other end: the watcher's origin in view space
} _Actor02100EnemyScanScratch;
STATIC_ASSERT_SIZEOF(_Actor02100EnemyScanScratch, 0x40);

/// 0x20-byte block the scan for the player takes from the scratch stack.
///
/// The watcher's coordinate and the player's are both composed into view
/// space, where the room's occluders are, so the facing, range and sight tests
/// all work there and the player's position needs no rotation of its own. The
/// player is measured at its model's root coordinate by a watcher carrying the
/// gun, and at the model's fourth coordinate by every other.
typedef struct {
    VECTOR  delta; // offset from the watcher's origin to the player's, in view space
    SVECTOR from;  // sight segment's player end: the player's origin in view space
    SVECTOR to;    // sight segment's other end: the watcher's origin in view space
} _Actor02100PlayerScanScratch;
STATIC_ASSERT_SIZEOF(_Actor02100PlayerScanScratch, 0x20);

/// 8-byte block the projection of the beam's two points takes from the scratch
/// stack: where the GTE's results for one point are stored before they are
/// copied into `_Actor02100Work`.
typedef struct {
    DVECTOR sxy;   // screen position of the point
    s32     depth; // ordering depth of the point: a quarter of its projected z
} _Actor02100BeamProjectScratch;
STATIC_ASSERT_SIZEOF(_Actor02100BeamProjectScratch, 0x8);

/// 0x28-byte block taken from the scratch stack to turn the line of fire onto
/// the target.
///
/// The line starts at the muzzle, not at the watcher's origin, so the muzzle is
/// brought into the frame `_Actor02100Work::targetPos` is kept in before the
/// two are subtracted. What remains is the direction `_Actor02100Work::aim` is
/// built along.
typedef struct {
    VECTOR  aimVector;    // working vector: the muzzle in view space, then the offset from `muzzle` to the target the aim matrix is built along
    VECTOR  muzzle;       // the muzzle rotated from view space into the watcher's axes, the frame of `_Actor02100Work::targetPos`
    SVECTOR muzzleOffset; // rotation's input: the muzzle in the watcher's own frame, `ACTOR_02100_MUZZLE_OFFSET` along the forward axis
} _Actor02100AimScratch;
STATIC_ASSERT_SIZEOF(_Actor02100AimScratch, 0x28);

/// 0x48-byte block the gun attack holds on the scratch stack for the length of
/// one tick.
///
/// A shot's flare is the only thing built in it, in its last eight bytes. The
/// blocks the attack's aiming and beam steps work in are reserved below this
/// one, so nothing reaches the bytes before them.
typedef struct {
    byte    unknown_0[0x40]; // never accessed; role unproven
    SVECTOR muzzleOffset;    // the muzzle in the watcher's own frame, `ACTOR_02100_MUZZLE_OFFSET` along the forward axis: the offset from the watcher's coordinate a shot's flare effects are spawned at
} _Actor02100GunAttackScratch;
STATIC_ASSERT_SIZEOF(_Actor02100GunAttackScratch, 0x48);

/// The two ways the watcher's beam is drawn. The value selects a colour in a
/// weapon's `_Actor02100WeaponParams` and a pair in its row of edge offsets.
enum {
    ACTOR_02100_BEAM_STYLE_SIGHT = 0, // sight line an attack follows the target with while it aims and locks
    ACTOR_02100_BEAM_STYLE_FIRE  = 1, // firing beam of the beam attack, whose centre line is drawn grey
    ACTOR_02100_BEAM_STYLE_COUNT = 2
};

/// Positions in `_Actor02100WeaponParams::values`.
///
/// The colours are one triplet per `ACTOR_02100_BEAM_STYLE_*`. The three colour
/// positions are those of the first style, and each further style's
/// components lie three positions on.
enum {
    ACTOR_02100_WEAPON_PARAM_AIM_TICKS     = 0, // ticks the aim step follows the target for before the attack locks
    ACTOR_02100_WEAPON_PARAM_RECOVER_TICKS = 1, // ticks the recovery step lasts before the watcher resumes
    ACTOR_02100_WEAPON_PARAM_RED           = 2, // red of the sight line, 0 to 255
    ACTOR_02100_WEAPON_PARAM_GREEN         = 3, // green of the sight line
    ACTOR_02100_WEAPON_PARAM_BLUE          = 4, // blue of the sight line
    ACTOR_02100_WEAPON_PARAM_COUNT         = 2 + (ACTOR_02100_BEAM_STYLE_COUNT * 3)
};

/// One weapon's attack timing and beam colours, selected by
/// `_Actor02100Work::weapon`.
///
/// Both attacks open with an aim step and end with a recovery step, and both
/// take those steps' lengths from here. A colour is the one the beam has on its
/// centre line, from which it fades to black at both edges. The gun never
/// draws a firing beam, so its second colour is unused.
///
/// The record is a row of shorts read by position, not a set of members: the
/// drawing code reaches a colour component by an index counted from the start
/// of the row.
typedef struct {
    s16 values[ACTOR_02100_WEAPON_PARAM_COUNT]; // indexed by `ACTOR_02100_WEAPON_PARAM_*`
} _Actor02100WeaponParams;
STATIC_ASSERT_SIZEOF(_Actor02100WeaponParams, 0x10);

/// 0x3C-byte block taken from the scratch stack while one beam is drawn
/// between the two screen points in `_Actor02100Work`.
///
/// The beam is drawn as eight segments of equal length. Each is a centre line
/// and, on either side of it, a quad reaching out to one edge of the beam. An
/// edge lies off the centre line across the beam, by an offset that shrinks
/// with the segment's depth.
typedef struct {
    VECTOR  span;      // offset from the first screen point to the second; z is 0
    SVECTOR direction; // `span` as a unit vector, 4096 for 1.0, with y negated: (vy, vx) is then the direction across the beam
    s32     depth;     // ordering depth of the segment being drawn, taken at its end towards the second point
    s32     depthStep; // depth added per segment, an eighth of the difference between the two points
    s16     x[6];      // screen x of the segment's corners: [0] and [1] the ends of its centre line, [2] and [3] those ends on the first edge, [4] and [5] on the second
    s16     y[6];      // screen y of the same corners
    s16     stepX;     // screen x added per segment, an eighth of `span`
    s16     stepY;     // screen y added per segment, an eighth of `span`
} _Actor02100BeamDrawScratch;
STATIC_ASSERT_SIZEOF(_Actor02100BeamDrawScratch, 0x3C);

/// Where the two edges of a beam lie, for one weapon and one beam style.
///
/// Each value is an offset across the beam from its centre line. It is scaled
/// by 0x300 over a segment's depth to give the offset on screen, so the beam
/// narrows with distance. Every stored pair is a negative and a positive
/// offset of the same size, which makes the beam symmetric about its centre.
typedef struct {
    s16 first;  // offset of the edge the first quad of a segment reaches
    s16 second; // offset of the edge the second quad reaches
} _Actor02100BeamEdges;
STATIC_ASSERT_SIZEOF(_Actor02100BeamEdges, 0x4);

/// The corners of one of the two quads a beam segment is built from.
///
/// Both quads name the two ends of the centre line first and the same ends on
/// one edge after them. A quad is shaded from the beam's colour on its first
/// two corners to black on the other two, so each fades outwards from the
/// centre line.
typedef struct {
    s16 index[4]; // corners in drawing order, as indices into `_Actor02100BeamDrawScratch::x` and `y`
} _Actor02100BeamQuadCorners;
STATIC_ASSERT_SIZEOF(_Actor02100BeamQuadCorners, 0x8);

/// Values of `_Actor02100Work::mode`: what the per-frame tick runs.
enum {
    ACTOR_02100_MODE_WATCH     = 0, // stands still and scans for a target
    ACTOR_02100_MODE_PATROL    = 1, // runs the patrol cycle and scans for a target
    ACTOR_02100_MODE_BEAM      = 2, // beam attack of weapons 0 to 3
    ACTOR_02100_MODE_GUN       = 3, // burst attack of `ACTOR_02100_WEAPON_GUN`
    ACTOR_02100_MODE_DESTROYED = 4  // nothing runs; the task's death state finishes the watcher
};

/// Values of `_Actor02100Work::patrolStep`.
enum {
    ACTOR_02100_PATROL_STEP_OUT        = 0, // moves at `patrolVelocity` for the patrol range
    ACTOR_02100_PATROL_STEP_OUT_PAUSE  = 1, // rests 60 ticks at the far end
    ACTOR_02100_PATROL_STEP_BACK       = 2, // moves back at the negated velocity for the same time
    ACTOR_02100_PATROL_STEP_BACK_PAUSE = 3  // rests 60 ticks where it started
};

/// Values of `_Actor02100Work::step` in `ACTOR_02100_MODE_BEAM`.
enum {
    ACTOR_02100_BEAM_STEP_AIM     = 0, // follows the target with the sight line for the weapon's aim time
    ACTOR_02100_BEAM_STEP_LOCK    = 1, // holds the line still for 15 ticks
    ACTOR_02100_BEAM_STEP_FIRE    = 2, // draws the firing beam for 4 ticks, the strike keys armed during the second
    ACTOR_02100_BEAM_STEP_RECOVER = 3  // waits out the weapon's recovery time, then resumes watching or patrolling
};

/// Values of `_Actor02100Work::step` in `ACTOR_02100_MODE_GUN`.
enum {
    ACTOR_02100_GUN_STEP_AIM     = 0, // follows the target with the sight line for the weapon's aim time
    ACTOR_02100_GUN_STEP_LOCK    = 1, // holds for 4 ticks
    ACTOR_02100_GUN_STEP_SHOT    = 2, // muzzle flare and report, then one re-aim
    ACTOR_02100_GUN_STEP_ARM     = 3, // arms the strike keys and counts the shot
    ACTOR_02100_GUN_STEP_DISARM  = 4, // clears the strike keys
    ACTOR_02100_GUN_STEP_NEXT    = 5, // next shot, or recovery after the tenth
    ACTOR_02100_GUN_STEP_RECOVER = 6  // waits out the weapon's recovery time, then resumes watching or patrolling
};

/// Values of `_Actor02100Work::step` in the task's death state.
enum {
    ACTOR_02100_DEATH_STEP_RELEASE = 0, // hides the model and unlinks the target node and the three bodies
    ACTOR_02100_DEATH_STEP_WAIT    = 1  // counts `stepFrames` down, then destroys the enemy
};

/// Values of `_Actor02100Work::targetKind`: what `target` is.
enum {
    ACTOR_02100_TARGET_NONE   = 0,
    ACTOR_02100_TARGET_PLAYER = 1, // the player's task
    ACTOR_02100_TARGET_ENEMY  = 2  // another enemy's task under the scene task
};

/// Values of `_Actor02100Work::loopSoundKind`: which looping sound `loopSound` is.
enum {
    ACTOR_02100_LOOP_SOUND_NONE   = 0,
    ACTOR_02100_LOOP_SOUND_PATROL = 1, // movement sound of a patrol leg
    ACTOR_02100_LOOP_SOUND_CHARGE = 2  // charge sound of an attack's aim step
};

/// Bounds of the two digits a placement's variant holds, and the one weapon
/// with a mode of its own.
enum {
    ACTOR_02100_WEAPON_GUN         = 4, // `_Actor02100Work::weapon` that fires the ten-shot burst
    ACTOR_02100_WEAPON_COUNT       = 5, // weapons, each with a row in the package's attack, timing and width tables
    ACTOR_02100_PATROL_RANGE_COUNT = 5  // values of `_Actor02100Work::patrolRange`
};

/// Distances along the watcher's line of fire, in world units.
enum {
    ACTOR_02100_MUZZLE_OFFSET = 0x12C, // from the model's origin to the muzzle, along the forward axis
    ACTOR_02100_STRIKE_REACH  = 0x2710 // length of the strike capsules before a contact clips them
};

/// Work block of a Watcher, the Shelter's security turret.
///
/// The actor's task allocates it zeroed when it starts and keeps it at
/// `Task::work`. The placement's variant is two decimal digits: the tens set
/// `patrolRange` and the units `weapon`.
///
/// A watcher stands still or patrols a straight track, and scans for a target
/// in front of it with a clear line of sight: the player within the sight
/// range of its placement, or a nearer enemy of the room. A player attack that
/// damages it reveals the player at any range. Finding a target stops the
/// movement and starts the weapon's attack, which follows the target with a
/// sight line, holds, fires along that line and recovers before the watcher
/// resumes.
///
/// The line of fire starts at the muzzle and follows `aim`. Two capsules lie
/// along it and share one contact: `playerStrikeBody` strikes the player and
/// `enemyStrikeBody` the room's other enemies. Both take part in the collision
/// passes from the aim step on, with a zero key until the attack strikes, so
/// the contact also measures where the line is obstructed. `beamLength` is
/// that distance, and the beam is drawn between the two `beamPoints`.
///
/// Timers count ticks. Distances and positions are world units.
typedef struct {
    MATRIX                color;              // storage for the model's `TmdObject::colorMtx`
    MATRIX                light;              // storage for the model's `TmdObject::lightMtx`
    WorldCollisionBody    hitBody;            // sphere around the model's origin that receives the attacks made on the watcher
    WorldCollisionContact hitContacts[1];     // contact table of `hitBody`, also installed as `Enemy::recs`
    WorldCollisionBody    playerStrikeBody;   // capsule along the line of fire whose key carries the weapon's `DamageAttack` to the player
    WorldCollisionContact strikeContacts[1];  // contact table both strike capsules share: what the line of fire touches first
    WorldCollisionCapsule playerStrikeShape;  // segment of `playerStrikeBody`: [0] the far end, `ACTOR_02100_STRIKE_REACH` along `aim`, [1] the muzzle; both radii 20
    WorldCollisionBody    enemyStrikeBody;    // capsule along the same line whose key is an attack of the category enemies take damage from, numbered `weapon` + 0x26
    WorldCollisionCapsule enemyStrikeShape;   // segment of `enemyStrikeBody`, the same as `playerStrikeShape`: its far end is copied from that one
    EffectSpawnArg        hitEffect;          // argument record of the hit effect a damaging attack spawns on the watcher
    VECTOR                targetPos;          // target's world position rotated into the watcher's axes
    SVECTOR               velocity;           // translation added to the watcher's coordinate each tick; zero while it pauses or attacks
    SVECTOR               resumeVelocity;     // `velocity` at the moment a target was found, restored when the attack ends
    SVECTOR               beamPoints[2];      // visible beam in the watcher's frame: [0] the muzzle, [1] the point `beamLength` along `aim`
    SVECTOR               patrolVelocity;     // velocity of the outward patrol leg, 25 a tick along the watcher's X axis (zero when stationary); the return leg negates it
    Task*                 target;             // task the last scan found, NULL for none; see `targetKind`
    MATRIX                aim;                // rotation from the watcher's forward axis onto the line of fire
    s32                   targetDistance;     // distance to `target` in world units, 0 with none; 1 stands in when a hit revealed the player
    s32                   loopSound;          // sound event of the looping sound in progress, as queued; used to retrigger and stop it
    s16                   patrolStep;         // `ACTOR_02100_PATROL_STEP_*`
    s16                   patrolFrames;       // ticks spent in `patrolStep`
    s16                   hitCooldown;        // ticks left during which attacks on the watcher are ignored; set from the attack that damaged it
    s16                   mode;               // `ACTOR_02100_MODE_*`
    s16                   step;               // stage of `mode`: `ACTOR_02100_BEAM_STEP_*`, `ACTOR_02100_GUN_STEP_*` or `ACTOR_02100_DEATH_STEP_*`
    s16                   patrolRange;        // tens digit of the placement variant, 0 to 4: a patrol leg lasts 40 ticks per unit (0 stationary)
    s16                   weapon;             // units digit of the placement variant, 0 to 4: 0 to 3 beams, `ACTOR_02100_WEAPON_GUN`
    s16                   stepFrames;         // ticks spent in `step`; the death wait counts it down from 60 instead
    s16                   shotsFired;         // shots of the running gun burst, which ends at 10
    s16                   hitThisTick;        // 1 when a player attack damaged the watcher this tick (0 otherwise); the next scan then finds the player at any range
    s16                   targetKind;         // `ACTOR_02100_TARGET_*`
    u16                   beamLength;         // distance from the muzzle to the strike capsules' contact, in world units
    s16                   beamBlocked;        // 1 when that contact is a room surface that blocks probes (0 otherwise)
    s16                   scanDelay;          // ticks before the next scan: held at 5 while the session's `viewReady` is 1
    s16                   loopSoundKind;      // `ACTOR_02100_LOOP_SOUND_*`
    s16                   hitEffectCooldown;  // ticks before a damaging attack may spawn the hit effect again, 10 after each
    s16                   beamScreenX[2];     // screen x of `beamPoints`
    s16                   beamScreenY[2];     // screen y of `beamPoints`
    s32                   beamScreenDepth[2]; // ordering depth of `beamPoints`, a quarter of the projected z
} _Actor02100Work;
STATIC_ASSERT_SIZEOF(_Actor02100Work, 0x19C);

static TmdBone _gActor02100WatcherBodySkeleton[3];
static u32     _gActor02100WatcherBodyPartVerts[3];
static SVECTOR _gActor02100WatcherBodyVerts[40];
static SVECTOR _gActor02100WatcherBodyNormals[32];
static u32     _gActor02100WatcherBodyStream[243];

extern DamageAttack               Actor02100_D03D64[5];
extern EnemyParams                Actor02100_D03D78;
extern _Actor02100WeaponParams    Actor02100_D03D88[];
extern _Actor02100BeamEdges       Actor02100_D03DD8[][ACTOR_02100_BEAM_STYLE_COUNT];
extern s16                        Actor02100_D03E00[];
extern _Actor02100BeamQuadCorners Actor02100_D03E1C[];
extern s16                        Actor02100_D03E2C[];

static void Actor02100_Fn03168(Task* arg0);
static void Actor02100_Fn031C4(Enemy* arg0, Task* arg1);
static void Actor02100_Fn032E4(Task* arg0);
static void _actor02100DestroyState(Enemy* enemy, Task* task);
static s32  _actor02100UpdateTargetPosition(Task* task);

static void _actor02100Initialize(Enemy* enemy, Task* task);

static const EnemyTaskFuncTable3 Actor02100_D00004 = { {
    _actor02100Initialize,
    Actor02100_Fn031C4,
    _actor02100DestroyState,
} };

static TmdBone _gActor02100WatcherBodySkeleton[3] = {
#include "assets/watcher_body_skeleton.inc"
};

static u32 _gActor02100WatcherBodyPartVerts[3] = {
#include "assets/watcher_body_partVerts.inc"
};

static SVECTOR _gActor02100WatcherBodyVerts[40] = {
#include "assets/watcher_body_verts.inc"
};

static SVECTOR _gActor02100WatcherBodyNormals[32] = {
#include "assets/watcher_body_normals.inc"
};

static u32 _gActor02100WatcherBodyStream[243] = {
#include "assets/watcher_body_stream.inc"
};

static TmdSource _gActor02100WatcherBody = {
    0,
    0x6A8,
    0,
    3,
    _gActor02100WatcherBodyPartVerts,
    _gActor02100WatcherBodyVerts,
    _gActor02100WatcherBodyNormals,
    _gActor02100WatcherBodySkeleton,
    _gActor02100WatcherBodyStream,
};

DamageAttack Actor02100_D03D64[5] = {
    { 15, 7 },
    { 25, 7 },
    { 12, 2 },
    { 12, 10 },
    { 15, 7 },
};

EnemyParams Actor02100_D03D78 = { Actor02100_D03D64, 70, 15, 0, 0, 100, 0, 0, 0 };

_Actor02100WeaponParams Actor02100_D03D88[5] = {
    { { 30, 90, 48, 0, 0, 255, 0, 0 } },
    { { 90, 90, 48, 0, 0, 255, 0, 0 } },
    { { 30, 180, 8, 16, 32, 32, 64, 128 } },
    { { 30, 180, 24, 24, 0, 32, 32, 0 } },
    { { 30, 180, 48, 48, 48, 0, 64, 16 } },
};

_Actor02100BeamEdges Actor02100_D03DD8[5][ACTOR_02100_BEAM_STYLE_COUNT] = {
    { { -4, 4 }, { -6, 6 } },
    { { -10, 10 }, { -14, 14 } },
    { { -10, 10 }, { -10, 10 } },
    { { -10, 10 }, { -10, 10 } },
    { { -4, 4 }, { -6, 6 } },
};

s16 Actor02100_D03E00[8] = { 3000, 3500, 4000, 4500, 5000, 6000, 7000, 8000 };

TaskDesc Actor02100_D03E10 = { { { TASK_BODY_TMD, 0x60 } }, Actor02100_Fn03168, { .model = &_gActor02100WatcherBody } };

_Actor02100BeamQuadCorners Actor02100_D03E1C[2] = {
    { { 0, 1, 2, 3 } },
    { { 0, 1, 4, 5 } },
};

s16 Actor02100_D03E2C[80] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

static s32 _actor02100SegmentOccluded(const SVECTOR* segmentStart, const SVECTOR* segmentEnd);

static void _actor02100ScanEnemyTargets(Task* task);

static void _actor02100DrawBeam(Task* task, s32 beamStyle);

static void _actor02100ProjectBeamPoints(Task* task);

static void Actor02100_Fn004C4(Task* arg0);

static void Actor02100_Fn03488(Task* arg0);

static void _actor02100TickPatrol(Task* task);

static void Actor02100_Fn016EC(Task* arg0);

static void Actor02100_Fn01FF0(Task* arg0);

static void _actor02100AcquireTarget(Task* task);

/// Allocates and initializes a Watcher, then enters its active task state.
///
/// The placement variant selects patrol range (tens digit, 0..4) and weapon
/// (units digit, 0..4); invalid values or allocation failure destroy the enemy.
/// Applies the placement roll in 4096 units per turn, binds model lighting and
/// target tracking, and links one receiving sphere and two inactive strike
/// capsules. The task owns the zeroed work block until enemy teardown. Requires
/// the model and enemy placement to be live and an initialized scratch stack.
static void _actor02100Initialize(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_02100_PATROL_SPEED           = 25,
        ACTOR_02100_BODY_TARGET_OFFSET     = 150,
        ACTOR_02100_BODY_RADIUS            = 400,
        ACTOR_02100_STRIKE_RADIUS          = 20,
        ACTOR_02100_HIT_EFFECT_MAGNITUDE   = 512,
        ACTOR_02100_BODY_ID                = 0x15,
        ACTOR_02100_TASK_ACTIVE            = 1,
        ACTOR_02100_ROTATION_FRACTION_BITS = 12
    };
    WorldCollisionContact* hitContacts;
    WorldCollisionContact* strikeContacts;
    SVECTOR*               placementAngles;
    ActorEulerTurnScratch* rotationCursor;
    s16                    weaponVariant;
    s16                    patrolSpeed;
    TmdObject*             model;
    GfxCoord*              coord;
    _Actor02100Work*       work;
    s16*                   localColumn1;
    s16*                   localColumn2;
    MATRIX*                localMatrix;

    model = task->extra.tmd;
    coord = model->coords;
    work  = memCalloc(sizeof(_Actor02100Work), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work        = work;
    work->patrolRange = (s16)((enemy->place->variant / 10) & 0xFF);
    weaponVariant     = (enemy->place->variant % 10) & 0xFF;
    work->weapon      = weaponVariant;
    if ((work->patrolRange >= ACTOR_02100_PATROL_RANGE_COUNT) || (weaponVariant >= ACTOR_02100_WEAPON_COUNT)) {
        enemyDestroy(enemy, task);
        return;
    }
    // Compose the placement roll before deriving the patrol track.
    model->flags                  = 0;
    coord->composeStamp           = GRAPHICS_COORD_DIRTY;
    rotationCursor                = SCRATCH_STACK_CURSOR(ActorEulerTurnScratch);
    model->lightMtx               = &work->light;
    model->colorMtx               = &work->color;
    placementAngles               = &rotationCursor[-1].angles;
    placementAngles->vx           = 0;
    placementAngles->vy           = 0;
    SCRATCH_STACK_CURSOR(SVECTOR) = placementAngles;
    placementAngles->vz           = enemy->place->mode;
    RotMatrix(placementAngles, &rotationCursor[-1].rotation);
    localMatrix = &coord->coord;
    gte_SetRotMatrix(localMatrix);
    gte_ldclmv(&rotationCursor[-1].rotation);
    gte_rtir();
    gte_stclmv(localMatrix);
    gte_ldclmv(&rotationCursor[-1].rotation.m[0][1]);
    gte_rtir();
    localColumn1 = &coord->coord.m[0][1];
    gte_stclmv(localColumn1);
    gte_ldclmv(&rotationCursor[-1].rotation.m[0][2]);
    gte_rtir();
    localColumn2 = &coord->coord.m[0][2];
    gte_stclmv(localColumn2);
    enemy->field_4  = localMatrix;
    enemy->field_48 = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->bodyPos.vz          = ACTOR_02100_BODY_TARGET_OFFSET;
    enemy->coord               = coord;
    enemy->bodyPos.vx          = 0;
    enemy->bodyPos.vy          = 0;
    enemy->param               = &Actor02100_D03D78;
    enemy->recs                = work->hitContacts;
    enemy->hp                  = (u16)Actor02100_D03D78.hpMax;
    work->hitEffect.coord      = coord;
    work->hitEffect.spawnArgLo = ACTOR_02100_HIT_EFFECT_MAGNITUDE;
    work->hitEffect.spawnArgHi = 1;
    sceneAcquireBattleRef(0);
    patrolSpeed = ACTOR_02100_PATROL_SPEED;
    if (work->patrolRange == 0) {
        work->mode  = ACTOR_02100_MODE_WATCH;
        patrolSpeed = 0;
    } else {
        work->mode = ACTOR_02100_MODE_PATROL;
    }
    work->velocity.vx       = (coord->coord.m[0][0] * patrolSpeed) >> ACTOR_02100_ROTATION_FRACTION_BITS;
    work->velocity.vy       = (coord->coord.m[1][0] * patrolSpeed) >> ACTOR_02100_ROTATION_FRACTION_BITS;
    work->velocity.vz       = (coord->coord.m[2][0] * patrolSpeed) >> ACTOR_02100_ROTATION_FRACTION_BITS;
    work->patrolVelocity.vx = work->velocity.vx;
    work->patrolVelocity.vy = work->velocity.vy;
    work->patrolVelocity.vz = work->velocity.vz;
    // The receiving sphere and two strike capsules keep their storage in work.
    hitContacts                    = work->hitContacts;
    work->hitBody.coord            = coord;
    work->hitBody.context.contacts = hitContacts;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vy           = 0;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = (WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_02100_BODY_ID);
    work->hitBody.radius           = ACTOR_02100_BODY_RADIUS;
    work->hitBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hitBody);
    worldCollisionInitContacts(hitContacts, ARRAY_SIZE(work->hitContacts), 0);
    strikeContacts = work->strikeContacts;
    /// Initializes a strike capsule's local line of fire and borrowed contacts.
    ///
    /// `shape` is a side-effect-free capsule lvalue, evaluated once per member;
    /// `contactTable` is evaluated once. Invoke only as a standalone statement.
    /// Uses this function's strike-radius constant and the package's muzzle
    /// offset and strike reach; captures no caller variables.
#define ACTOR_02100_INIT_STRIKE_SHAPE(shape, contactTable) \
    {                                                      \
        (shape).ends[0].vx = 0;                            \
        (shape).ends[0].vy = 0;                            \
        (shape).ends[0].vz = ACTOR_02100_STRIKE_REACH;     \
        (shape).ends[1].vx = 0;                            \
        (shape).ends[1].vy = 0;                            \
        (shape).ends[1].vz = ACTOR_02100_MUZZLE_OFFSET;    \
        (shape).end0Radius = ACTOR_02100_STRIKE_RADIUS;    \
        (shape).end1Radius = ACTOR_02100_STRIKE_RADIUS;    \
        (shape).contacts   = (contactTable);               \
    }
    ACTOR_02100_INIT_STRIKE_SHAPE(work->playerStrikeShape, strikeContacts);
    work->playerStrikeBody.context.capsule = &work->playerStrikeShape;
    work->playerStrikeBody.coord           = coord;
    work->playerStrikeBody.pos.vx          = 0;
    work->playerStrikeBody.pos.vy          = 0;
    work->playerStrikeBody.pos.vz          = 0;
    work->playerStrikeBody.key             = 0;
    work->playerStrikeBody.radius          = 0;
    work->playerStrikeBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->hitBody.flags                    = (u16)(work->hitBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->playerStrikeBody);
    worldCollisionInitContacts(strikeContacts, ARRAY_SIZE(work->strikeContacts), 0);
    ACTOR_02100_INIT_STRIKE_SHAPE(work->enemyStrikeShape, strikeContacts);
#undef ACTOR_02100_INIT_STRIKE_SHAPE
    work->enemyStrikeBody.coord           = coord;
    work->enemyStrikeBody.context.capsule = &work->enemyStrikeShape;
    work->enemyStrikeBody.pos.vx          = 0;
    work->enemyStrikeBody.pos.vy          = 0;
    work->enemyStrikeBody.pos.vz          = 0;
    work->enemyStrikeBody.key             = 0;
    work->enemyStrikeBody.radius          = 0;
    work->enemyStrikeBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->playerStrikeBody.flags          = (u16)((work->playerStrikeBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED))) | (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT));
    worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &work->enemyStrikeBody);
    work->enemyStrikeBody.flags                 = (u16)((work->enemyStrikeBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED))) | WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT);
    task->state                                 = ACTOR_02100_TASK_ACTIVE;
    SCRATCH_STACK_CURSOR(ActorEulerTurnScratch) = SCRATCH_STACK_CURSOR(ActorEulerTurnScratch) + 1;
}

static void Actor02100_Fn004C4(Task* arg0)
{
    _Actor02100VectorScratch*        scratch;
    _Actor02100Work*                 work;
    Enemy*                           enemy;
    GfxCoord*                        coord;
    GfxCoord*                        src;
    WorldCollisionSurfaceProperties* surface;
    s32                              damage;
    s32                              stun;
    s32                              sound;
    s32                              pan;
    s32                              depth;
    s32                              index;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor02100VectorScratch);
    coord   = arg0->extra.tmd->coords;
    work    = arg0->work;
    enemy   = arg0->spawnArg2.pointer;

    if (work->hitCooldown != 0) {
        work->hitCooldown--;
        if (work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    work->hitThisTick = 0;
    if (work->hitEffectCooldown != 0) {
        work->hitEffectCooldown--;
    }

    if (work->hitCooldown == 0) {
        if ((work->hitContacts[0].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == 0x20000) {
            if (work->hitContacts[0].key.value & 0x8000) {
                worldTargetAddReadoutAmount(&enemy->node, 0, 0);
            } else if ((((u32)work->hitContacts[0].key.value >> 8) & 0x3F) < 0x21U) {
                src             = gPlayerActorTasks[((u32)work->hitContacts[0].key.value >> 7) & 1]->extra.tmd->coords;
                scratch->vec.vx = src->coord.t[0] - coord->coord.t[0];
                scratch->vec.vy = src->coord.t[1] - coord->coord.t[1];
                scratch->vec.vz = src->coord.t[2] - coord->coord.t[2];
                damage          = damageComputePlayerAttack(work->hitContacts[0].key.value,
                                                            SquareRoot0(scratch->vec.vx * scratch->vec.vx +
                                                                        scratch->vec.vy * scratch->vec.vy +
                                                                        scratch->vec.vz * scratch->vec.vz),
                                                            0, 0);
                if (damageRollCriticalHit(arg0->spawnArg2.pointer,
                                          work->hitContacts[0].key.value, 0) != 0) {
                    damage *= 4;
                    effectSpawn(EFFECT_CRITICAL_HIT, coord, 0, 0);
                }
                enemy->hp -= damage;
                worldTargetAddReadoutAmount(&enemy->node, damage, 0);
                work->hitThisTick = 1;
                if (enemy->hp <= 0) {
                    effectSpawn(EFFECT_EXPLOSION, coord, 0x10002400, 0);
                    effectSpawn(EFFECT_SMOKE_PUFF, coord, 0x32FF1400, 0);
                    work->mode                    = ACTOR_02100_MODE_DESTROYED;
                    work->step                    = ACTOR_02100_DEATH_STEP_RELEASE;
                    work->playerStrikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->enemyStrikeBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->playerStrikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
                    work->enemyStrikeBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
                    arg0->state                   = 2;
                    sound                         = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4015000A;
                    sndEvtRequestScriptStart(sound, (s8)worldCoordGetOriginAudioPan(coord),
                                             (s8)worldCoordGetOriginAudioDepth(coord));
                } else if (damage > 0) {
                    if (work->hitEffectCooldown == 0) {
                        scratch->shortVec.vx = 0;
                        scratch->shortVec.vy = 0;
                        scratch->shortVec.vz = 0xC8;
                        if ((damageGetPlayerAttackReaction(work->hitContacts[0].key.value) & 0xFFFF) == DAMAGE_PLAYER_REACTION_INCENDIARY) {
                            effectSpawn(EFFECT_HIT_BLAST, coord,
                                        work->hitEffect.spawnArgLo | (work->hitEffect.spawnArgHi << 16),
                                        &scratch->shortVec);
                        }
                        effectSpawnHit(EFFECT_HIT_KIND_SPARK_BURST, coord, &scratch->shortVec, &work->hitEffect);
                        work->hitEffectCooldown = 10;
                    }
                    sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40150009;
                    pan   = (s8)worldCoordGetOriginAudioPan(coord);
                    depth = (s8)worldCoordGetOriginAudioDepth(coord);
                    sndEvtRequestScriptStart(sound, pan, depth);
                    stun = damageGetPlayerAttackHitCooldown(work->hitContacts[0].key.value);
                    if (stun > 0) {
                        work->hitCooldown = stun;
                    }
                }
            }
        }
    }

    worldCollisionClearContacts(work->hitContacts);
    work->beamBlocked = 0;
    if (worldCollisionCountContactsByKind(work->strikeContacts, WORLD_COLLISION_CONTACT_GRID) != 0) {
        index   = worldCollisionSurfaceClassFromKey(work->strikeContacts[0].key.value);
        surface = Gp_RoomParamTables[gGameSession->location.loc.stage - 1]
                                    [gGameSession->location.loc.area - 1][index];
        if (surface->probePassThrough == WORLD_COLLISION_SURFACE_BLOCK_PROBES) {
            work->beamBlocked = 1;
        }
    }

    if ((work->strikeContacts[0].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == 0x10000 ||
        (work->strikeContacts[0].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == 0x30000 || work->beamBlocked == 1) {
        scratch->shortVec.vx = 0;
        scratch->shortVec.vy = 0;
        scratch->shortVec.vz = ACTOR_02100_MUZZLE_OFFSET;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->shortVec);
        gte_rtv0();
        gte_stlvnl(&scratch->vec);
        scratch->vec.vx += coord->workm.t[0];
        scratch->vec.vy += coord->workm.t[1];
        scratch->vec.vz += coord->workm.t[2];
        scratch->vec.vx  = work->strikeContacts[0].point.vx - scratch->vec.vx;
        scratch->vec.vy  = work->strikeContacts[0].point.vy - scratch->vec.vy;
        scratch->vec.vz  = work->strikeContacts[0].point.vz - scratch->vec.vz;
        work->beamLength = SquareRoot0(scratch->vec.vx * scratch->vec.vx +
                                       scratch->vec.vy * scratch->vec.vy +
                                       scratch->vec.vz * scratch->vec.vz);
        // Sparks fly from the contact once either attack strikes: the beam's
        // fire step and the gun's first shot share the value.
        if (work->step >= ACTOR_02100_BEAM_STEP_FIRE) {
            scratch->shortVec.vx = 0;
            scratch->shortVec.vy = 0;
            scratch->shortVec.vz = work->beamLength;
            gte_SetRotMatrix(&work->aim);
            gte_ldv0(&scratch->shortVec);
            gte_rtv0();
            gte_stsv(&scratch->shortVec);
            scratch->shortVec.vz += ACTOR_02100_MUZZLE_OFFSET;
            effectSpawn(EFFECT_IMPACT_SPARK, coord, 0, &scratch->shortVec);
        }
    }

    worldCollisionClearContacts(work->strikeContacts);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor02100VectorScratch);
}

/// Stops the looping effect, clears the offset vector and starts the tail
/// sound. Expanded at the end of both active steps, which the compiler emits
/// as one shared tail; `tailSound` is deliberately a caller-scope variable, since
/// both expansions must name the same object.
#define STOP_SOUND                                                                                                        \
    work->velocity.vx = 0;                                                                                                \
    work->velocity.vy = 0;                                                                                                \
    work->velocity.vz = 0;                                                                                                \
    sndEvtRequestScriptStop(work->loopSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);                                             \
    work->loopSoundKind = ACTOR_02100_LOOP_SOUND_NONE;                                                                    \
    tailSound           = (((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40150008); \
    {                                                                                                                     \
        s32 pan;                                                                                                          \
        s32 depth;                                                                                                        \
                                                                                                                          \
        pan   = (s8)worldCoordGetOriginAudioPan(coord);                                                                   \
        depth = (s8)worldCoordGetOriginAudioDepth(coord);                                                                 \
        sndEvtRequestScriptStart(tailSound, pan, depth);                                                                  \
    }

/// Advances the Watcher's out-and-back patrol and its movement sound.
///
/// Each leg lasts 40 ticks per patrol-range unit, followed by a 60-tick pause.
/// This updates velocity; the active task applies the translation separately.
/// The return leg negates the stored outward velocity. A zeroed patrol timer
/// starts the instance-specific looping sound on the next tick.
static void _actor02100TickPatrol(Task* task)
{
    enum {
        ACTOR_02100_PATROL_TICKS_PER_RANGE = 40,
        ACTOR_02100_PATROL_PAUSE_TICKS     = 60,
        ACTOR_02100_PATROL_LOOP_SOUND      = 0x40150007
    };
    _Actor02100Work* work;
    GfxCoord*        coord;
    s32              negX;
    s32              negY;
    s32              negZ;
    s16              patrolStep;
    s32              tailSound;

    work  = task->work;
    coord = task->extra.tmd->coords;
    work->patrolFrames++;
    patrolStep = work->patrolStep;

    switch (patrolStep) {
        case ACTOR_02100_PATROL_STEP_OUT:
            if (work->patrolFrames == 1) {
                work->loopSound = (((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_02100_PATROL_LOOP_SOUND);
                {
                    s32 pan;
                    s32 depth;

                    pan   = (s8)worldCoordGetOriginAudioPan(coord);
                    depth = (s8)worldCoordGetOriginAudioDepth(coord);
                    sndEvtRequestScriptStart(work->loopSound, pan, depth);
                }
                work->loopSoundKind = ACTOR_02100_LOOP_SOUND_PATROL;
            }
            {
                s32 pan;
                s32 depth;

                pan   = (s8)worldCoordGetOriginAudioPan(coord);
                depth = (s8)worldCoordGetOriginAudioDepth(coord);
                sndEvtRequestScriptMix(work->loopSound, pan, depth);
            }
            if (work->patrolFrames < (work->patrolRange * ACTOR_02100_PATROL_TICKS_PER_RANGE)) {
                break;
            }
            work->patrolFrames = 0;
            work->patrolStep   = ACTOR_02100_PATROL_STEP_OUT_PAUSE;
            STOP_SOUND;
            break;

        case ACTOR_02100_PATROL_STEP_OUT_PAUSE:
            if (work->patrolFrames < ACTOR_02100_PATROL_PAUSE_TICKS) {
                break;
            }
            work->patrolStep   = (patrolStep = ACTOR_02100_PATROL_STEP_BACK);
            work->patrolFrames = 0;
            work->velocity.vx  = (negX = -work->patrolVelocity.vx);
            work->velocity.vy  = (negY = -work->patrolVelocity.vy);
            work->velocity.vz  = (negZ = -work->patrolVelocity.vz);
            break;

        case ACTOR_02100_PATROL_STEP_BACK: {
            s32 patrolTicks;

            patrolTicks = work->patrolFrames;
            if (patrolTicks == 1) {
                work->loopSound = (((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_02100_PATROL_LOOP_SOUND);
                {
                    s32 pan;
                    s32 depth;

                    pan   = (s8)worldCoordGetOriginAudioPan(coord);
                    depth = (s8)worldCoordGetOriginAudioDepth(coord);
                    sndEvtRequestScriptStart(work->loopSound, pan, depth);
                }
                work->loopSoundKind = ACTOR_02100_LOOP_SOUND_PATROL;
            }
        }
            {
                s32 pan;
                s32 depth;

                pan   = (s8)worldCoordGetOriginAudioPan(coord);
                depth = (s8)worldCoordGetOriginAudioDepth(coord);
                sndEvtRequestScriptMix(work->loopSound, pan, depth);
            }
            if (work->patrolFrames < (work->patrolRange * ACTOR_02100_PATROL_TICKS_PER_RANGE)) {
                break;
            }
            work->patrolFrames = 0;
            work->patrolStep   = ACTOR_02100_PATROL_STEP_BACK_PAUSE;
            STOP_SOUND;
            break;

        case ACTOR_02100_PATROL_STEP_BACK_PAUSE:
            if (work->patrolFrames < ACTOR_02100_PATROL_PAUSE_TICKS) {
                break;
            }
            work->patrolFrames = 0;
            work->patrolStep   = ACTOR_02100_PATROL_STEP_OUT;
            work->velocity.vx  = work->patrolVelocity.vx;
            work->velocity.vy  = work->patrolVelocity.vy;
            work->velocity.vz  = work->patrolVelocity.vz;
            break;
    }
}

#undef STOP_SOUND

/// Acquires a visible target and stops movement to begin the weapon's attack.
///
/// Tests the player in front of the Watcher and within the placement's sight
/// range, then allows a nearer eligible enemy to replace it. A hit this tick
/// restricts the scan to the player and bypasses range, retaining facing and
/// visibility tests. Player coordinates 0 (gun) or 3 (beam) must be live.
/// A view-ready value of 1 reloads the five-tick scan delay. Uses composed view
/// coordinates and an initialized scratch stack with 240 bytes for nested
/// scans; no target task is owned or retained beyond its existing lifetime.
static void _actor02100AcquireTarget(Task* task)
{
    enum { ACTOR_02100_SCAN_DELAY_TICKS = 5 };
    _Actor02100Work*              work;
    _Actor02100PlayerScanScratch* scratch;
    GfxCoord*                     coord;
    GfxCoord*                     playerCoord;
    u32                           distance;
    s32                           attackMode;

    coord = task->extra.tmd->coords;
    work  = task->work;

    if (gGameSession->viewReady == 1) {
        work->scanDelay = ACTOR_02100_SCAN_DELAY_TICKS;
    }
    if (work->scanDelay != 0) {
        work->scanDelay--;
        return;
    }

    work->target         = NULL;
    work->targetDistance = 0;
    work->targetKind     = ACTOR_02100_TARGET_NONE;
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor02100PlayerScanScratch);

    if (work->weapon == ACTOR_02100_WEAPON_GUN) {
        playerCoord = &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[0];
    } else {
        playerCoord = &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[3];
    }
    playerCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(playerCoord);

    // A received hit reveals the player at any range, but not through occluders.
    if (work->hitThisTick == 0) {
        scratch->delta.vx = playerCoord->workm.t[0] - coord->workm.t[0];
        scratch->delta.vy = playerCoord->workm.t[1] - coord->workm.t[1];
        scratch->delta.vz = playerCoord->workm.t[2] - coord->workm.t[2];

        if (((scratch->delta.vx * coord->workm.m[0][2]) + (scratch->delta.vy * coord->workm.m[1][2]) +
             (scratch->delta.vz * coord->workm.m[2][2])) > 0) {
            distance = SquareRoot0((scratch->delta.vx * scratch->delta.vx) + (scratch->delta.vy * scratch->delta.vy) +
                                   (scratch->delta.vz * scratch->delta.vz));
            if (distance < Actor02100_D03E00[((Enemy*)task->spawnArg2.pointer)->place->rowIndex & (ARRAY_SIZE(Actor02100_D03E00) - 1)]) {
                scratch->from.vx = playerCoord->workm.t[0];
                scratch->from.vy = playerCoord->workm.t[1];
                scratch->from.vz = playerCoord->workm.t[2];
                scratch->to.vx   = coord->workm.t[0];
                scratch->to.vy   = coord->workm.t[1];
                scratch->to.vz   = coord->workm.t[2];
                if (_actor02100SegmentOccluded(&scratch->from, &scratch->to) == 0) {
                    work->target         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                    work->targetDistance = distance;
                    work->targetKind     = ACTOR_02100_TARGET_PLAYER;
                }
            }
        }
        _actor02100ScanEnemyTargets(task);
    } else {
        scratch->delta.vx = playerCoord->workm.t[0] - coord->workm.t[0];
        scratch->delta.vy = playerCoord->workm.t[1] - coord->workm.t[1];
        scratch->delta.vz = playerCoord->workm.t[2] - coord->workm.t[2];

        if (((scratch->delta.vx * coord->workm.m[0][2]) + (scratch->delta.vy * coord->workm.m[1][2]) +
             (scratch->delta.vz * coord->workm.m[2][2])) > 0) {
            scratch->from.vx = playerCoord->workm.t[0];
            scratch->from.vy = playerCoord->workm.t[1];
            scratch->from.vz = playerCoord->workm.t[2];
            scratch->to.vx   = coord->workm.t[0];
            scratch->to.vy   = coord->workm.t[1];
            scratch->to.vz   = coord->workm.t[2];
            if (_actor02100SegmentOccluded(&scratch->from, &scratch->to) == 0) {
                work->target         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                work->targetDistance = 1;
                work->targetKind     = ACTOR_02100_TARGET_PLAYER;
            }
        }
    }

    if (work->targetKind != ACTOR_02100_TARGET_NONE) {
        sceneEngageBattle(1);
        if (work->weapon == ACTOR_02100_WEAPON_GUN) {
            attackMode = ACTOR_02100_MODE_GUN;
        } else {
            attackMode = ACTOR_02100_MODE_BEAM;
        }
        work->resumeVelocity.vx = work->velocity.vx;
        work->resumeVelocity.vy = work->velocity.vy;
        work->resumeVelocity.vz = work->velocity.vz;
        // Either attack opens with its aim step; the two share the value.
        work->mode        = attackMode;
        work->step        = ACTOR_02100_BEAM_STEP_AIM;
        work->stepFrames  = 0;
        work->shotsFired  = 0;
        work->velocity.vx = 0;
        work->velocity.vy = 0;
        work->velocity.vz = 0;
        if (work->loopSoundKind == ACTOR_02100_LOOP_SOUND_PATROL) {
            sndEvtRequestScriptStop(work->loopSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            work->loopSoundKind = ACTOR_02100_LOOP_SOUND_NONE;
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(_Actor02100PlayerScanScratch);
}

/// Transforms a signed-halfword world point into the current view frame.
///
/// Uses the composed view rotation, then adds its translation in game units.
/// Input and output are borrowed, word-aligned storage; changes GTE state.
static __inline__ void _actor02100TransformTargetToView(const SVECTOR* worldPoint, VECTOR* viewPosition)
{
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtv0();
    gte_stlvnl(viewPosition);
    viewPosition->vx += gGfxViewCoord.workm.t[0];
    viewPosition->vy += gGfxViewCoord.workm.t[1];
    viewPosition->vz += gGfxViewCoord.workm.t[2];
}

/// Selects the nearest eligible visible enemy closer than the current target.
///
/// Walks the scene task's circular child ring, excluding dead enemies and
/// placement entry IDs marked in the exclusion table. Out-of-table IDs use
/// entry zero. Range and forward-facing tests use world coordinates; occlusion
/// uses the current composed view. The scene ring and all enemy placements
/// must be live. Borrows target tasks and releases its 64-byte scratch block;
/// nested visibility queries require 208 bytes of available scratch in total.
static void _actor02100ScanEnemyTargets(Task* task)
{
    _Actor02100EnemyScanScratch* scratch;
    Task*                        firstEnemyTask;
    Task*                        enemyTask;
    Enemy*                       enemy;
    _Actor02100Work*             work;
    GfxCoord*                    coord;
    u32                          entryId;
    u32                          distance;

    firstEnemyTask = gameGetTaskSlot(GAME_TASK_SLOT_SCENE);
    coord          = task->extra.tmd->coords;
    firstEnemyTask = firstEnemyTask->firstChild;
    work           = task->work;
    if (firstEnemyTask != NULL) {
        scratch   = SCRATCH_STACK_RESERVE_BLOCK(_Actor02100EnemyScanScratch);
        enemyTask = firstEnemyTask;
        do {
            enemy   = enemyTask->spawnArg2.pointer;
            entryId = enemy->place->entryId;
            if (entryId >= ARRAY_SIZE(Actor02100_D03E2C)) {
                entryId = 0;
            }
            if (Actor02100_D03E2C[entryId] == 0 && enemy->hp > 0) {
                worldTargetGetBodyPosition(&enemy->node, &scratch->lockPos);
                scratch->delta.vx = scratch->lockPos.vx - coord->coord.t[0];
                scratch->delta.vy = scratch->lockPos.vy - coord->coord.t[1];
                scratch->delta.vz = scratch->lockPos.vz - coord->coord.t[2];
                if ((scratch->delta.vx * coord->coord.m[0][2]) +
                        (scratch->delta.vy * coord->coord.m[1][2]) +
                        (scratch->delta.vz * coord->coord.m[2][2]) >
                    0) {
                    distance = SquareRoot0((scratch->delta.vx * scratch->delta.vx) +
                                           (scratch->delta.vy * scratch->delta.vy) +
                                           (scratch->delta.vz * scratch->delta.vz));
                    if ((work->targetDistance == 0 || distance < work->targetDistance) &&
                        distance < Actor02100_D03E00[((Enemy*)task->spawnArg2.pointer)->place->rowIndex & (ARRAY_SIZE(Actor02100_D03E00) - 1)]) {
                        // Bring the world-space lock point into the occluders' view frame.
                        scratch->from.vx = scratch->lockPos.vx;
                        scratch->from.vy = scratch->lockPos.vy;
                        scratch->from.vz = scratch->lockPos.vz;
                        _actor02100TransformTargetToView(&scratch->from, &scratch->viewPos);
                        scratch->from.vx = scratch->viewPos.vx;
                        scratch->from.vy = scratch->viewPos.vy;
                        scratch->from.vz = scratch->viewPos.vz;
                        scratch->to.vx   = coord->workm.t[0];
                        scratch->to.vy   = coord->workm.t[1];
                        scratch->to.vz   = coord->workm.t[2];
                        if (_actor02100SegmentOccluded(&scratch->from, &scratch->to) == 0) {
                            work->target         = enemyTask;
                            work->targetDistance = distance;
                            work->targetKind     = ACTOR_02100_TARGET_ENEMY;
                        }
                    }
                }
            }
            enemyTask = enemyTask->nextSibling;
        } while (enemyTask != firstEnemyTask);
        SCRATCH_STACK_RELEASE_BLOCK(_Actor02100EnemyScanScratch);
    }
}

/// Refreshes the tracked target position in the Watcher's rotated view axes.
///
/// Returns 1 for the player or a living enemy and 0 for no supported target
/// or a dead enemy; failure leaves the previous position and task reference.
/// Uses player coordinate 0 for the gun and 3 for a beam. The target and
/// composed view matrices must remain live. The transpose removes rotation,
/// without subtracting the Watcher's translation. Borrows and releases scratch
/// storage, and changes GTE state.
static s32 _actor02100UpdateTargetPosition(Task* task)
{
    _Actor02100VectorScratch* scratch;
    _Actor02100Work*          work;
    GfxCoord*                 coord;
    GfxCoord*                 targetCoord;
    VECTOR*                   viewPosition;
    WorldTargetNode*          targetNode;
    s32                       targetValid;

    work        = task->work;
    coord       = task->extra.tmd->coords;
    targetValid = 0;
    if (work->target == NULL) {
        return targetValid;
    }

    scratch      = SCRATCH_STACK_RESERVE_BLOCK(_Actor02100VectorScratch);
    viewPosition = &scratch->vec;
    switch (work->targetKind) {
        case ACTOR_02100_TARGET_PLAYER:
            if (work->weapon == ACTOR_02100_WEAPON_GUN) {
                targetCoord = work->target->extra.tmd->coords;
            } else {
                targetCoord = &work->target->extra.tmd->coords[3];
            }
            viewPosition->vx = targetCoord->workm.t[0];
            viewPosition->vy = targetCoord->workm.t[1];
            viewPosition->vz = targetCoord->workm.t[2];
            ApplyTransposeMatrixLV(&coord->workm, viewPosition, &work->targetPos);
            targetValid = 1;
            break;

        case ACTOR_02100_TARGET_ENEMY:
            if (((Enemy*)work->target->spawnArg2.pointer)->hp <= 0) {
                break;
            }
            targetNode = &((Enemy*)work->target->spawnArg2.pointer)->node;
            // The targetNode position is written over the three components of `viewPosition`, then
            // narrowed for the GTE and rotated back into `viewPosition` in view space.
            worldTargetGetBodyPosition(targetNode, (VECTOR3*)&scratch->vec);
            scratch->shortVec.vx = scratch->vec.vx;
            scratch->shortVec.vy = scratch->vec.vy;
            scratch->shortVec.vz = scratch->vec.vz;
            gte_SetRotMatrix(&gGfxViewCoord.workm);
            gte_ldv0(&scratch->shortVec);
            gte_rtv0();
            gte_stlvnl(viewPosition);
            scratch->vec.vx += gGfxViewCoord.workm.t[0];
            scratch->vec.vy += gGfxViewCoord.workm.t[1];
            scratch->vec.vz += gGfxViewCoord.workm.t[2];
            ApplyTransposeMatrixLV(&coord->workm, &scratch->vec, &work->targetPos);
            targetValid = 1;
            break;

        case ACTOR_02100_TARGET_NONE:
        default:
            break;
    }

    SCRATCH_STACK_RELEASE_BLOCK(_Actor02100VectorScratch);
    return targetValid;
}

/// Rebuilds the two strike capsules' far endpoints along the current aim.
///
/// Rotates a local +Z offset of `ACTOR_02100_STRIKE_REACH` game units and copies
/// the signed-halfword result to both capsules. Borrows and releases one
/// `SVECTOR` on the initialized scratch stack and changes GTE state.
static __inline__ void _actor02100BuildStrikeEndpoints(Task* task)
{
    _Actor02100Work* work;
    SVECTOR*         strikeOffset;

    work             = task->work;
    strikeOffset     = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    strikeOffset->vx = 0;
    strikeOffset->vy = 0;
    strikeOffset->vz = ACTOR_02100_STRIKE_REACH;
    gte_SetRotMatrix(&work->aim);
    gte_ldv0(strikeOffset);
    gte_rtv0();
    gte_stsv(&work->playerStrikeShape.ends[0]);
    work->enemyStrikeShape.ends[0] = work->playerStrikeShape.ends[0];
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Builds the aim rotation from the muzzle towards the stored target position.
///
/// Both positions use the Watcher's rotated view axes, including translation
/// in those axes. Requires live model/work and a nonzero direction suitable
/// for SDK normalization. Borrows and releases an `_Actor02100AimScratch` on
/// the initialized scratch stack. Changes GTE state.
static __inline__ void _actor02100AimAtTarget(Task* task)
{
    _Actor02100AimScratch* scratch;
    _Actor02100Work*       work;
    GfxCoord*              coord;

    coord   = task->extra.tmd->coords;
    work    = task->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor02100AimScratch);

    scratch->muzzleOffset.vx = 0;
    scratch->muzzleOffset.vy = 0;
    scratch->muzzleOffset.vz = ACTOR_02100_MUZZLE_OFFSET;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&scratch->muzzleOffset);
    gte_rtv0();
    gte_stlvnl(&scratch->aimVector);
    scratch->aimVector.vx += coord->workm.t[0];
    scratch->aimVector.vy += coord->workm.t[1];
    scratch->aimVector.vz += coord->workm.t[2];
    ApplyTransposeMatrixLV(&coord->workm, &scratch->aimVector, &scratch->muzzle);
    scratch->aimVector.vx = work->targetPos.vx - scratch->muzzle.vx;
    scratch->aimVector.vy = work->targetPos.vy - scratch->muzzle.vy;
    scratch->aimVector.vz = work->targetPos.vz - scratch->muzzle.vz;
    gfxBuildDirectionRotation(&scratch->aimVector, &work->aim, 0);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor02100AimScratch);
}

/// Rebuilds the muzzle, visible beam endpoint and both strike endpoints.
///
/// Uses the existing aim rotation and beam length in game units. Results are
/// Watcher-local signed-halfword offsets. Borrows and releases a vector block
/// and a strike input on the initialized scratch stack; changes GTE state.
static __inline__ void _actor02100BuildBeamAndStrikePoints(Task* task)
{
    _Actor02100VectorScratch* scratch;
    _Actor02100Work*          work;

    work                   = task->work;
    scratch                = SCRATCH_STACK_RESERVE_BLOCK(_Actor02100VectorScratch);
    work->beamPoints[0].vx = 0;
    work->beamPoints[0].vy = 0;
    work->beamPoints[0].vz = ACTOR_02100_MUZZLE_OFFSET;
    scratch->shortVec.vx   = 0;
    scratch->shortVec.vy   = 0;
    scratch->shortVec.vz   = work->beamLength;
    gte_SetRotMatrix(&work->aim);
    gte_ldv0(&scratch->shortVec);
    gte_rtv0();
    gte_stsv(&work->beamPoints[1]);
    work->beamPoints[1].vz += ACTOR_02100_MUZZLE_OFFSET;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor02100VectorScratch);
    _actor02100BuildStrikeEndpoints(task);
}

/// Aims at the tracked target and rebuilds the beam and both strike endpoints.
///
/// The muzzle and stored target position are compared in the Watcher's rotated
/// view axes. The resulting rotation sends local +Z towards the target. Beam
/// and strike points are local offsets in game units, narrowed to signed
/// halfwords by the GTE. Requires live model/work and an initialized scratch
/// stack; all reservations are released before returning.
static __inline__ void _actor02100AimBeamAndStrikePoints(Task* task)
{
    _actor02100AimAtTarget(task);
    _actor02100BuildBeamAndStrikePoints(task);
}

/// Stores the rotated beam endpoint, adds the muzzle offset and releases its input block.
///
/// Requires the GTE to hold the current beam-length rotation and the top
/// scratch reservation to be an `_Actor02100VectorScratch`. The signed-halfword
/// endpoint stays in the Watcher's local frame; adding the muzzle offset narrows
/// without clamping.
static __inline__ void _actor02100StoreBeamEndpoint(_Actor02100Work* work)
{
    gte_stsv(&work->beamPoints[1]);
    work->beamPoints[1].vz += ACTOR_02100_MUZZLE_OFFSET;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor02100VectorScratch);
}

/// Rebuilds the locked beam and strike points without changing aim.
///
/// `currentWork` is the task's existing work block. The retained timer branch
/// performs identical operations in both arms. Beam length is in game units;
/// GTE outputs narrow to local signed-halfword offsets. Releases all scratch
/// reservations and changes GTE state.
static __inline__ void _actor02100BuildLockedBeamAndStrikePoints(Task* task, _Actor02100Work* currentWork)
{
    // The lock lasts fifteen ticks; this distinguishes its final zero-based tick.
    enum { ACTOR_02100_BEAM_LOCK_LAST_TICK = 14 };
    _Actor02100VectorScratch* scratch;
    _Actor02100VectorScratch* lockedScratch;
    _Actor02100Work*          work;
    SVECTOR*                  inputCursor;

    if (currentWork->stepFrames == ACTOR_02100_BEAM_LOCK_LAST_TICK) {
        work                       = task->work;
        work->beamPoints[0].vz     = ACTOR_02100_MUZZLE_OFFSET;
        inputCursor                = SCRATCH_STACK_CURSOR(SVECTOR);
        scratch                    = (_Actor02100VectorScratch*)((u8*)inputCursor - sizeof(_Actor02100VectorScratch));
        work->beamPoints[0].vx     = 0;
        work->beamPoints[0].vy     = 0;
        scratch->shortVec.vx       = 0;
        scratch->shortVec.vy       = 0;
        SCRATCH_STACK_CURSOR(void) = scratch;
        scratch->shortVec.vz       = work->beamLength;
        gte_SetRotMatrix(&work->aim);
        inputCursor = &scratch->shortVec;
        gte_ldv0(inputCursor);
        gte_rtv0();
        _actor02100StoreBeamEndpoint(work);
    } else {
        work                       = task->work;
        work->beamPoints[0].vz     = ACTOR_02100_MUZZLE_OFFSET;
        inputCursor                = SCRATCH_STACK_CURSOR(SVECTOR);
        lockedScratch              = (_Actor02100VectorScratch*)((u8*)inputCursor - sizeof(_Actor02100VectorScratch));
        work->beamPoints[0].vx     = 0;
        work->beamPoints[0].vy     = 0;
        lockedScratch->shortVec.vx = 0;
        lockedScratch->shortVec.vy = 0;
        SCRATCH_STACK_CURSOR(void) = lockedScratch;
        lockedScratch->shortVec.vz = work->beamLength;
        gte_SetRotMatrix(&work->aim);
        inputCursor = &lockedScratch->shortVec;
        gte_ldv0(inputCursor);
        gte_rtv0();
        _actor02100StoreBeamEndpoint(work);
    }
    _actor02100BuildStrikeEndpoints(task);
}

/// Four-state sweep with a charge-up, a strike and a recovery wait. State 0 aims
/// at the target every frame until `_actor02100UpdateTargetPosition` loses it - which drops
/// straight to the recovery state - starts the loop sound on the first frame and
/// draws the beam from the second, and advances to state 1 once the frame count
/// reaches the per-variant limit in `Actor02100_D03D88`. State 1 stops the loop
/// sound on its first frame, rebuilds the vectors without re-aiming and draws
/// for fifteen frames, then fires the impact sound and enters state 2. State 2
/// shows the two hit objects for one frame, picks the impact sound from the
/// variant index, hides them again after four frames and recovers. State 3 waits
/// out the per-variant recovery count, restores the actor's stored position and
/// returns to state 0.
static void Actor02100_Fn016EC(Task* arg0)
{
    _Actor02100Work* work;
    GfxCoord*        coord;
    s32              sound;
    s32              soundId;
    s32              packed;
    s32              flagBit;
    s16              state;
    s16              frame;

    SCRATCH_STACK_RESERVE_BYTES(0x48);
    work    = arg0->work;
    state   = work->step;
    coord   = arg0->extra.tmd->coords;
    flagBit = 0x20000;
    sound   = 0;

    switch (state) {
        case ACTOR_02100_BEAM_STEP_AIM:
            if (work->stepFrames != 0) {
                if (_actor02100UpdateTargetPosition(arg0) == 0) {
                    work->step       = ACTOR_02100_BEAM_STEP_RECOVER;
                    work->stepFrames = 0;
                    if (work->loopSoundKind == ACTOR_02100_LOOP_SOUND_CHARGE) {
                        sndEvtRequestScriptStop(work->loopSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                        work->loopSoundKind = ACTOR_02100_LOOP_SOUND_NONE;
                    }
                    break;
                }

                _actor02100AimBeamAndStrikePoints(arg0);
            }

            if (work->stepFrames == 1) {
                work->loopSound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40150001;
                sndEvtRequestScriptStart(work->loopSound, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
                work->loopSoundKind = ACTOR_02100_LOOP_SOUND_CHARGE;
            }
            if (work->stepFrames >= 2) {
                _actor02100ProjectBeamPoints(arg0);
                _actor02100DrawBeam(arg0, ACTOR_02100_BEAM_STYLE_SIGHT);
            }
            work->playerStrikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            frame                         = (u16)work->stepFrames + 1;
            work->stepFrames              = frame;
            work->enemyStrikeBody.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->playerStrikeBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work->enemyStrikeBody.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
            if (frame >= Actor02100_D03D88[work->weapon].values[ACTOR_02100_WEAPON_PARAM_AIM_TICKS]) {
                work->stepFrames = 0;
                work->step       = ACTOR_02100_BEAM_STEP_LOCK;
            }
            break;

        case ACTOR_02100_BEAM_STEP_LOCK:
            if (work->stepFrames == state) {
                sndEvtRequestScriptStop(work->loopSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                work->loopSoundKind = ACTOR_02100_LOOP_SOUND_NONE;
            }
            _actor02100BuildLockedBeamAndStrikePoints(arg0, work);
            _actor02100ProjectBeamPoints(arg0);
            _actor02100DrawBeam(arg0, ACTOR_02100_BEAM_STYLE_SIGHT);
            frame            = (u16)work->stepFrames + 1;
            work->stepFrames = frame;
            if (frame >= 0xF) {
                work->stepFrames = 0;
                work->step       = ACTOR_02100_BEAM_STEP_FIRE;
                soundId          = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40150002;
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;

        case ACTOR_02100_BEAM_STEP_FIRE:
            _actor02100ProjectBeamPoints(arg0);
            _actor02100DrawBeam(arg0, ACTOR_02100_BEAM_STYLE_FIRE);
            frame = work->stepFrames;
            if (frame == 1) {
                work->playerStrikeBody.key = damagePackAttackKey(Actor02100_D03D64, work->weapon);
                packed                     = work->weapon + 0x26;
                work->enemyStrikeBody.key  = flagBit;
                work->enemyStrikeBody.key  = (packed << 8) | (packed | work->enemyStrikeBody.key);
                switch (work->weapon) {
                    case 0:
                        sound = 0x40150003;
                        break;
                    case 1:
                        sound = 0x40150004;
                        break;
                    case 2:
                        sound = 0x40150005;
                        break;
                    case 3:
                        sound = 0x40150006;
                        break;
                }
                soundId = sound | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord),
                                         (s8)worldCoordGetOriginAudioDepth(coord));
            } else if (frame == state) {
                work->playerStrikeBody.key = 0;
                work->enemyStrikeBody.key  = 0;
            }
            frame            = (u16)work->stepFrames + 1;
            work->stepFrames = frame;
            if (frame >= 4) {
                work->step                    = ACTOR_02100_BEAM_STEP_RECOVER;
                work->stepFrames              = 0;
                work->playerStrikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->enemyStrikeBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->playerStrikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
                work->enemyStrikeBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            }
            break;

        case ACTOR_02100_BEAM_STEP_RECOVER:
            frame            = (u16)work->stepFrames + 1;
            work->stepFrames = frame;
            if (frame >= Actor02100_D03D88[work->weapon].values[ACTOR_02100_WEAPON_PARAM_RECOVER_TICKS]) {
                work->stepFrames  = 0;
                work->step        = ACTOR_02100_BEAM_STEP_AIM;
                work->mode        = work->patrolRange != 0;
                work->velocity.vx = work->resumeVelocity.vx;
                work->velocity.vy = work->resumeVelocity.vy;
                work->velocity.vz = work->resumeVelocity.vz;
            }
            break;
    }

    SCRATCH_STACK_RELEASE_BYTES(0x48);
}

/// Seven-state attack cycle, run from `Actor02100_Fn031C4`. State 0 holds the
/// wind-up: it re-aims each frame, refreshes both direction vectors, starts the
/// looping sound on the second frame and shows the two objects, until the
/// wind-up frame count in `Actor02100_D03D88` runs out. State 1 stops that
/// sound and waits four frames; state 2 emits the two effects with a randomised
/// parameter, plays the strike sound and re-aims once; states 3 and 4 set and
/// clear the pair table entry and the colour word that make the strike hit,
/// counting one hit in `shotsFired`; state 5 loops back to state 2 until ten
/// hits, then hides both objects; state 6 waits out the recovery frame count
/// and returns to state 0. `_actor02100UpdateTargetPosition` failing at any aim point drops
/// straight to state 6.
static void Actor02100_Fn01FF0(Task* arg0)
{
    _Actor02100GunAttackScratch* scratch;
    _Actor02100Work*             work;
    GfxCoord*                    coord;
    s32                          pan0;
    s32                          pan2;
    s32                          sound2;
    u32                          random;
    s32                          packed2;
    s16                          state;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor02100GunAttackScratch);
    work    = arg0->work;
    state   = work->step;
    coord   = arg0->extra.tmd->coords;

    switch (state) {
        case ACTOR_02100_GUN_STEP_AIM:
            if (work->stepFrames != 0) {
                if (_actor02100UpdateTargetPosition(arg0) == 0) {
                    work->step       = ACTOR_02100_GUN_STEP_RECOVER;
                    work->stepFrames = 0;
                    if (work->loopSoundKind == ACTOR_02100_LOOP_SOUND_CHARGE) {
                        sndEvtRequestScriptStop(work->loopSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                        work->loopSoundKind = ACTOR_02100_LOOP_SOUND_NONE;
                    }
                    break;
                }

                _actor02100AimAtTarget(arg0);
                _actor02100BuildBeamAndStrikePoints(arg0);
            }

            if (work->stepFrames == 1) {
                work->loopSound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40150001;
                pan0            = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(work->loopSound, pan0, (s8)worldCoordGetOriginAudioDepth(coord));
                work->loopSoundKind = ACTOR_02100_LOOP_SOUND_CHARGE;
            }
            if (work->stepFrames >= 2) {
                _actor02100ProjectBeamPoints(arg0);
                _actor02100DrawBeam(arg0, ACTOR_02100_BEAM_STYLE_SIGHT);
            }
            work->playerStrikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->enemyStrikeBody.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->playerStrikeBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work->enemyStrikeBody.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
            if (++work->stepFrames >= Actor02100_D03D88[work->weapon].values[ACTOR_02100_WEAPON_PARAM_AIM_TICKS]) {
                work->stepFrames = 0;
                work->step       = ACTOR_02100_GUN_STEP_LOCK;
            }
            break;

        case ACTOR_02100_GUN_STEP_LOCK:
            if (work->stepFrames == 1) {
                sndEvtRequestScriptStop(work->loopSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                work->loopSoundKind = ACTOR_02100_LOOP_SOUND_NONE;
            }
            if (++work->stepFrames >= 4) {
                work->stepFrames = 0;
                _actor02100BuildStrikeEndpoints(arg0);
                work->step = ACTOR_02100_GUN_STEP_SHOT;
            }
            break;

        case ACTOR_02100_GUN_STEP_SHOT:
            scratch->muzzleOffset.vx = 0;
            scratch->muzzleOffset.vy = 0;
            scratch->muzzleOffset.vz = ACTOR_02100_MUZZLE_OFFSET;
            random                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            packed2                  = ((random >> 16) & 0x1FF) | 0x200;
            gRandomLcgState          = random;
            effectSpawn(EFFECT_MUZZLE_FLARE, coord, packed2, &scratch->muzzleOffset);
            effectSpawn(EFFECT_MUZZLE_FLARE_ADDITIVE, coord, packed2, &scratch->muzzleOffset);
            sound2 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4015000B;
            pan2   = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(coord));
            work->step = ACTOR_02100_GUN_STEP_ARM;
            if (_actor02100UpdateTargetPosition(arg0) == 0) {
                work->step       = ACTOR_02100_GUN_STEP_RECOVER;
                work->stepFrames = 0;
            } else {
                _actor02100AimAtTarget(arg0);
            }
            break;

        case ACTOR_02100_GUN_STEP_ARM:
            work->playerStrikeBody.key = damagePackAttackKey(Actor02100_D03D64, work->weapon);
            work->step                 = ACTOR_02100_GUN_STEP_DISARM;
            work->enemyStrikeBody.key  = ((work->weapon + 0x26) << 8) | 0x20000 | (work->weapon + 0x26);
            work->shotsFired++;
            break;

        case ACTOR_02100_GUN_STEP_DISARM:
            work->playerStrikeBody.key = 0;
            work->enemyStrikeBody.key  = 0;
            work->step                 = ACTOR_02100_GUN_STEP_NEXT;
            break;

        case ACTOR_02100_GUN_STEP_NEXT:
            if (work->shotsFired < 10) {
                work->step = ACTOR_02100_GUN_STEP_SHOT;
                _actor02100BuildStrikeEndpoints(arg0);
                break;
            }
            work->step                    = ACTOR_02100_GUN_STEP_RECOVER;
            work->stepFrames              = 0;
            work->playerStrikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->enemyStrikeBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->playerStrikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            work->enemyStrikeBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            break;

        case ACTOR_02100_GUN_STEP_RECOVER:
            if (++work->stepFrames >= Actor02100_D03D88[work->weapon].values[ACTOR_02100_WEAPON_PARAM_RECOVER_TICKS]) {
                work->stepFrames  = 0;
                work->step        = ACTOR_02100_GUN_STEP_AIM;
                work->mode        = work->patrolRange != 0;
                work->velocity.vx = work->resumeVelocity.vx;
                work->velocity.vy = work->resumeVelocity.vy;
                work->velocity.vz = work->resumeVelocity.vz;
            }
            break;
    }

    SCRATCH_STACK_RELEASE_BLOCK(_Actor02100GunAttackScratch);
}

/// Fades a beam quad from its selected centre colour to black on its edge.
///
/// Requires a validated weapon and beam-style index. Colour values narrow to
/// bytes; the packet's command byte is retained and no storage is owned.
static __inline__ void _actor02100ShadeBeamQuad(POLY_G4* quad, _Actor02100Work* work, s32 beamStyle)
{
    enum { ACTOR_02100_COLOR_COMPONENT_COUNT = 3 };
    u8 blue;
    quad->r0 = (u8)Actor02100_D03D88[work->weapon].values[(beamStyle * ACTOR_02100_COLOR_COMPONENT_COUNT) + ACTOR_02100_WEAPON_PARAM_RED];
    quad->g0 = (u8)Actor02100_D03D88[work->weapon].values[(beamStyle * ACTOR_02100_COLOR_COMPONENT_COUNT) + ACTOR_02100_WEAPON_PARAM_GREEN];
    quad->b0 = (u8)Actor02100_D03D88[work->weapon].values[(beamStyle * ACTOR_02100_COLOR_COMPONENT_COUNT) + ACTOR_02100_WEAPON_PARAM_BLUE];
    quad->r1 = (u8)Actor02100_D03D88[work->weapon].values[(beamStyle * ACTOR_02100_COLOR_COMPONENT_COUNT) + ACTOR_02100_WEAPON_PARAM_RED];
    quad->g1 = (u8)Actor02100_D03D88[work->weapon].values[(beamStyle * ACTOR_02100_COLOR_COMPONENT_COUNT) + ACTOR_02100_WEAPON_PARAM_GREEN];
    blue     = (u8)Actor02100_D03D88[work->weapon].values[(beamStyle * ACTOR_02100_COLOR_COMPONENT_COUNT) + ACTOR_02100_WEAPON_PARAM_BLUE];
    quad->r2 = 0;
    quad->g2 = 0;
    quad->b2 = 0;
    quad->r3 = 0;
    quad->g3 = 0;
    quad->b3 = 0;
    quad->b1 = blue;
}

/// Queues an eight-segment additive beam between the projected beam points.
///
/// `beamStyle` is `ACTOR_02100_BEAM_STYLE_SIGHT` or `ACTOR_02100_BEAM_STYLE_FIRE`;
/// work must hold a validated weapon index. Each segment uses two Gouraud
/// quads fading to black at the edges and one centre line. Fire uses a grey
/// centre line. Segments with SZ3/4 depth below 30 are skipped. Screen endpoints
/// must have a nonzero delta suitable for SDK normalization. Requires the
/// current 1024-tag ordering table and primitive space for up to 24 packets
/// plus eight draw-mode packets. Releases its 60-byte scratch reservation;
/// the queued GPU packets live until the current draw buffer is consumed.
static void _actor02100DrawBeam(Task* task, s32 beamStyle)
{
    enum {
        ACTOR_02100_BEAM_SEGMENT_SHIFT      = 3,
        ACTOR_02100_BEAM_SEGMENT_COUNT      = 1 << ACTOR_02100_BEAM_SEGMENT_SHIFT,
        ACTOR_02100_BEAM_MIN_DEPTH          = 30,
        ACTOR_02100_BEAM_EDGE_DEPTH_SCALE   = 0x300,
        ACTOR_02100_DIRECTION_FRACTION_BITS = 12,
        ACTOR_02100_COLOR_COMPONENT_COUNT   = 3
    };
    _Actor02100BeamQuadCorners* corners;
    POLY_G4*                    quad;
    DR_TPAGE*                   drawMode;
    LINE_F2*                    line;
    _Actor02100BeamDrawScratch* scratch;
    s32                         nextSegment;
    s32                         offsetX0;
    s32                         offsetX1;
    s32                         negatedY;
    s32                         offsetY0;
    s32                         offsetY1;
    s32                         depth;
    s32                         edge;
    s32                         segment;
    s32                         spanX;
    s32                         spanY;
    s32                         depthSpan;
    _Actor02100Work*            work;

    work    = task->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor02100BeamDrawScratch);

    scratch->span.vx = work->beamScreenX[1] - work->beamScreenX[0];
    scratch->span.vy = work->beamScreenY[1] - work->beamScreenY[0];
    scratch->span.vz = 0;
    VectorNormalS(&scratch->span, &scratch->direction);
    negatedY              = -scratch->direction.vy;
    scratch->direction.vy = negatedY;

    // Signed division by eight truncates towards zero, including negative spans.
    spanX = work->beamScreenX[1] - work->beamScreenX[0];
    if (spanX < 0) {
        spanX += ACTOR_02100_BEAM_SEGMENT_COUNT - 1;
    }
    scratch->stepX = (s16)(spanX >> ACTOR_02100_BEAM_SEGMENT_SHIFT);
    spanY          = work->beamScreenY[1] - work->beamScreenY[0];
    if (spanY < 0) {
        spanY += ACTOR_02100_BEAM_SEGMENT_COUNT - 1;
    }
    scratch->stepY = (s16)(spanY >> ACTOR_02100_BEAM_SEGMENT_SHIFT);
    depthSpan      = work->beamScreenDepth[1] - work->beamScreenDepth[0];
    if (depthSpan < 0) {
        depthSpan += ACTOR_02100_BEAM_SEGMENT_COUNT - 1;
    }
    scratch->depthStep = depthSpan >> ACTOR_02100_BEAM_SEGMENT_SHIFT;
    segment            = 0;

    do {
        nextSegment    = segment + 1;
        depth          = (scratch->depthStep * nextSegment) + work->beamScreenDepth[0];
        scratch->depth = depth;
        if (depth >= ACTOR_02100_BEAM_MIN_DEPTH) {
            scratch->x[0] = (u16)((u16)work->beamScreenX[0] + (scratch->stepX * segment));
            scratch->x[1] = (u16)((u16)work->beamScreenX[0] + (scratch->stepX * nextSegment));
            edge          = 0;
            offsetX0 =
                (s32)((s32)(scratch->direction.vy * Actor02100_D03DD8[work->weapon][beamStyle].first * ACTOR_02100_BEAM_EDGE_DEPTH_SCALE) >>
                      ACTOR_02100_DIRECTION_FRACTION_BITS) /
                (s32)scratch->depth;
            scratch->x[2] = (s16)(scratch->x[0] + offsetX0);
            scratch->x[3] = (s16)(scratch->x[1] + offsetX0);
            offsetX1 =
                (s32)((s32)(scratch->direction.vy * Actor02100_D03DD8[work->weapon][beamStyle].second * ACTOR_02100_BEAM_EDGE_DEPTH_SCALE) >>
                      ACTOR_02100_DIRECTION_FRACTION_BITS) /
                (s32)scratch->depth;
            scratch->x[4] = (s16)(scratch->x[0] + offsetX1);
            scratch->x[5] = (s16)(scratch->x[1] + offsetX1);
            scratch->y[0] = (u16)((u16)work->beamScreenY[0] + (scratch->stepY * segment));
            scratch->y[1] = (u16)((u16)work->beamScreenY[0] + (scratch->stepY * nextSegment));
            offsetY0 =
                (s32)((s32)(scratch->direction.vx * Actor02100_D03DD8[work->weapon][beamStyle].first * ACTOR_02100_BEAM_EDGE_DEPTH_SCALE) >>
                      ACTOR_02100_DIRECTION_FRACTION_BITS) /
                (s32)scratch->depth;
            scratch->y[2] = (s16)(scratch->y[0] + offsetY0);
            scratch->y[3] = (s16)(scratch->y[1] + offsetY0);
            offsetY1 =
                (s32)((s32)(scratch->direction.vx * Actor02100_D03DD8[work->weapon][beamStyle].second * ACTOR_02100_BEAM_EDGE_DEPTH_SCALE) >>
                      ACTOR_02100_DIRECTION_FRACTION_BITS) /
                (s32)scratch->depth;
            scratch->y[4] = (s16)(scratch->y[0] + offsetY1);
            scratch->y[5] = (s16)(scratch->y[1] + offsetY1);

            // Fade each edge quad from the centre colour to black.
            do {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                setPolyG4(quad);
                setSemiTrans(quad, 1);
                corners  = &Actor02100_D03E1C[edge];
                quad->x0 = (u16)scratch->x[corners->index[0]];
                quad->y0 = (u16)scratch->y[corners->index[0]];
                quad->x1 = (u16)scratch->x[corners->index[1]];
                quad->y1 = (u16)scratch->y[corners->index[1]];
                quad->x2 = (u16)scratch->x[corners->index[2]];
                quad->y2 = (u16)scratch->y[corners->index[2]];
                quad->x3 = (u16)scratch->x[corners->index[3]];
                quad->y3 = (u16)scratch->y[corners->index[3]];
                _actor02100ShadeBeamQuad(quad, work, beamStyle);
                edge += 1;
                addPrim(&gGpuCurrentOt[((u32)(scratch->depth << gDisplayState.otDepthShift) >> 4) & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*gGpuCurrentOt))], quad);
            } while (edge < ARRAY_SIZE(Actor02100_D03E1C));

            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineF2(line);
            setSemiTrans(line, 1);
            line->x0 = (u16)scratch->x[0];
            line->y0 = (u16)scratch->y[0];
            line->x1 = (u16)scratch->x[1];
            line->y1 = (u16)scratch->y[1];
            if (beamStyle == ACTOR_02100_BEAM_STYLE_FIRE) {
                line->r0 = 0x80U;
                line->g0 = 0x80U;
                line->b0 = 0x80U;
            } else {
                line->r0 = (u8)Actor02100_D03D88[work->weapon].values[(beamStyle * ACTOR_02100_COLOR_COMPONENT_COUNT) + ACTOR_02100_WEAPON_PARAM_RED];
                line->g0 = (u8)Actor02100_D03D88[work->weapon].values[(beamStyle * ACTOR_02100_COLOR_COMPONENT_COUNT) + ACTOR_02100_WEAPON_PARAM_GREEN];
                line->b0 = (u8)Actor02100_D03D88[work->weapon].values[(beamStyle * ACTOR_02100_COLOR_COMPONENT_COUNT) + ACTOR_02100_WEAPON_PARAM_BLUE];
            }
            setaddr(line, getaddr(&gGpuCurrentOt[((u32)(scratch->depth << gDisplayState.otDepthShift) >> 4) & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*gGpuCurrentOt))]));
            drawMode       = gGpuPrimCursor;
            gGpuPrimCursor = drawMode + 1;
            setaddr(&gGpuCurrentOt[((u32)(scratch->depth << gDisplayState.otDepthShift) >> 4) & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*gGpuCurrentOt))], line);
            // Additive blending, with dithering and drawing into the display area.
            setlen(drawMode, 1);
            drawMode->code[0] = _get_mode(1, 1, getTPage(0, GPU_BLEND_ADD, 0, 0));
            addPrim(&gGpuCurrentOt[((u32)(scratch->depth << gDisplayState.otDepthShift) >> 4) & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*gGpuCurrentOt))], drawMode);
        }
        segment += 1;
    } while (segment < ACTOR_02100_BEAM_SEGMENT_COUNT);

    SCRATCH_STACK_RELEASE_BLOCK(_Actor02100BeamDrawScratch);
}

static void Actor02100_Fn03168(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor02100_D00004;
    sp.funcs[arg0->state]((Enemy*)arg0->spawnArg2.pointer, arg0);
}

/// Per-frame tick, entry 1 of `Actor02100_D00004`. `gSceneCombatState.actorControl` is the global
/// gameplay mode: mode 1 only refreshes the actor colour, mode 2 parks the
/// actor (`field_C` 0x80, node not lockable) and returns, and mode 0 re-shows it
/// (`field_C` 0, node HP hidden) before falling into the normal body. The body
/// adds the per-tick translation at `velocity` to the actor's
/// coordinate, runs the state machine, and switches to state 4 - handing the
/// task over to `_actor02100DestroyState` - once `gSceneCombatState.generatorDeathStarted` reports the kill.
static void Actor02100_Fn031C4(Enemy* arg0, Task* arg1)
{
    TmdObject*       obj;
    _Actor02100Work* work;
    GfxCoord*        coord;
    s32              mode;

    obj   = arg1->extra.tmd;
    mode  = gSceneCombatState.actorControl;
    work  = arg1->work;
    coord = obj->coords;
    switch (mode) {
        case 0:
            obj->flags                   = 0;
            arg0->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
            break;
        case 1:
            Actor02100_Fn03488(arg1);
            return;
        case 2:
            obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = 1;
            return;
    }
    Actor02100_Fn004C4(arg1);
    coord->coord.t[0]  += work->velocity.vx;
    coord->coord.t[1]  += work->velocity.vy;
    coord->coord.t[2]  += work->velocity.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    Actor02100_Fn032E4(arg1);
    Actor02100_Fn03488(arg1);
    if (gSceneCombatState.generatorDeathStarted == 1) {
        work->mode  = ACTOR_02100_MODE_DESTROYED;
        work->step  = ACTOR_02100_DEATH_STEP_RELEASE;
        arg1->state = 2;
    }
}

static void Actor02100_Fn032E4(Task* arg0)
{
    _Actor02100Work* work;
    s16              state;

    work  = arg0->work;
    state = work->mode;
    switch (state) {
        case ACTOR_02100_MODE_PATROL:
            _actor02100TickPatrol(arg0);
        case ACTOR_02100_MODE_WATCH:
            if (gameFlagGetNibble(GAME_FLAG_SHELTER_WATCHERS_DISABLED) == 0) {
                _actor02100AcquireTarget(arg0);
            }
            break;
        case ACTOR_02100_MODE_BEAM:
            Actor02100_Fn016EC(arg0);
            break;
        case ACTOR_02100_MODE_GUN:
            Actor02100_Fn01FF0(arg0);
            break;
        case ACTOR_02100_MODE_DESTROYED:
            break;
    }
}

/// Writes the end-minus-start sight direction with twelve fractional bits.
///
/// The delta must fit signed halfwords with squared length in 1..0x7FFFFFFF.
/// Endpoints share a frame; their storage is borrowed and remains unchanged.
/// The output's fourth word is untouched. Changes GTE arithmetic state.
static __inline__ void _actor02100NormalizeSightSegment(const SVECTOR* segmentStart, const SVECTOR* segmentEnd, VECTOR* direction)
{
    direction->vx = segmentEnd->vx - segmentStart->vx;
    direction->vy = segmentEnd->vy - segmentStart->vy;
    direction->vz = segmentEnd->vz - segmentStart->vz;
    VectorNormal(direction, direction);
}

/// Returns 1 when an enabled room sight occluder crosses the segment, else 0.
///
/// Endpoints share the current composed view frame and use game units. Quad
/// edges and crossings in either direction count; endpoints and parallel
/// intersections do not. Stops at the first enabled hit. End minus start must
/// fit signed halfwords and have squared length in 1..0x7FFFFFFF for SDK
/// normalization to 4096 per unit. Inputs are unchanged and not retained.
/// Requires a live occluder list and an initialized scratch stack with 144
/// bytes available for nested queries, clear of the inputs. Releases scratch
/// before return and changes GTE state.
static s32 _actor02100SegmentOccluded(const SVECTOR* segmentStart, const SVECTOR* segmentEnd)
{
    VECTOR*                 direction;
    WorldCollisionOccluder* occluder;
    s32                     occluded;

    occluded  = 0;
    occluder  = D_80115550;
    direction = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    // Normalize once for the complete enabled-occluder scan.
    _actor02100NormalizeSightSegment(segmentStart, segmentEnd, direction);
    for (; occluder != NULL; occluder = occluder->next) {
        if (occluder->flags & WORLD_COLLISION_OCCLUDER_ENABLED) {
            occluded = worldCollisionTestOccluderSegment(occluder, segmentStart, segmentEnd, direction);
            if (occluded == 1) {
                break;
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
    return occluded;
}

static void Actor02100_Fn03488(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

/// Projects the local beam endpoints to screen pixels and SZ3/4 sorting depths.
///
/// The model's composed view matrix and both local beam points must be current.
/// Stores signed screen halfwords and the GTE depth without testing projection
/// flags. Borrows and releases an eight-byte scratch block and changes GTE state.
static void _actor02100ProjectBeamPoints(Task* task)
{
    _Actor02100BeamProjectScratch* scratch;
    GfxCoord*                      coord;
    _Actor02100Work*               work;
    s32                            pointIndex;
    s32                            screenY;

    work    = task->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor02100BeamProjectScratch);
    coord   = task->extra.tmd->coords;
    for (pointIndex = 0; pointIndex < ARRAY_SIZE(work->beamPoints); pointIndex++) {
        gte_SetRotMatrix(&coord->workm);
        gte_SetTransMatrix(&coord->workm);
        gte_ldv0(&work->beamPoints[pointIndex]);
        gte_rtps();
        gte_stsxy(&scratch->sxy);
        gte_stszotz(&scratch->depth);
        work->beamScreenX[pointIndex]     = scratch->sxy.vx;
        screenY                           = scratch->sxy.vy;
        work->beamScreenY[pointIndex]     = screenY;
        work->beamScreenDepth[pointIndex] = scratch->depth;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor02100BeamProjectScratch);
}

/// Unlinks a destroyed Watcher, awards its rewards and waits before enemy teardown.
///
/// The release step hides the model, removes target tracking and all three
/// collision bodies, clears contact ownership, balances the battle reference
/// and stops any loop sound. The wait step destroys the enemy after 60 ticks.
/// The enemy, task and work remain live until that final call.
static void _actor02100DestroyState(Enemy* enemy, Task* task)
{
    enum { ACTOR_02100_DEATH_WAIT_TICKS = 60 };
    _Actor02100Work* work;

    work = task->work;
    switch (work->step) {
        case ACTOR_02100_DEATH_STEP_RELEASE:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            worldTargetUnlinkNode(&enemy->node);
            worldCollisionUnlinkBody(&work->hitBody);
            worldCollisionUnlinkBody(&work->playerStrikeBody);
            worldCollisionUnlinkBody(&work->enemyStrikeBody);
            enemy->recs = NULL;
            sceneReleaseBattleRefWithRewards(task, 0x15);
            work->step       = ACTOR_02100_DEATH_STEP_WAIT;
            work->stepFrames = ACTOR_02100_DEATH_WAIT_TICKS;
            if (work->loopSoundKind != ACTOR_02100_LOOP_SOUND_NONE) {
                sndEvtRequestScriptStop(work->loopSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            }
            break;
        case ACTOR_02100_DEATH_STEP_WAIT:
            if (--work->stepFrames <= 0) {
                enemyDestroy(enemy, task);
            }
            break;
    }
}
