#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/loading.h"
#include "gameplay/object_fields.h"
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

typedef struct Actor02100Fn00048Scratch {
    /* 0x00 */ SVECTOR rotation;
    /* 0x08 */ MATRIX  matrix;
} Actor02100Fn00048Scratch;
STATIC_ASSERT_SIZEOF(Actor02100Fn00048Scratch, 0x28);

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

typedef struct Actor02100Fn011C4Scratch {
    /* 0x00 */ VECTOR  transformed;
    /* 0x10 */ VECTOR  delta;
    /* 0x20 */ VECTOR3 lock;
    /* 0x2C */ s32     pad_2C;
    /* 0x30 */ SVECTOR from;
    /* 0x38 */ SVECTOR to;
} Actor02100Fn011C4Scratch;
STATIC_ASSERT_SIZEOF(Actor02100Fn011C4Scratch, 0x40);

/// 0x20-byte scratch block taken from the scratch stack by
/// `Actor02100_Fn00DCC`: the world-space delta between the two coordinates,
/// then the two endpoints as `SVECTOR`s for the line-of-sight test.
typedef struct Actor02100Sight {
    /* 0x00 */ VECTOR  delta;
    /* 0x10 */ SVECTOR from;
    /* 0x18 */ SVECTOR to;
} Actor02100Sight;
STATIC_ASSERT_SIZEOF(Actor02100Sight, 0x20);

/// 8-byte block taken from the scratch stack by `Actor02100_Fn034E0`: the
/// projected screen position (`stsxy`) and the quartered depth (`stszotz`).
typedef struct Actor02100Screen {
    /* 0x0 */ DVECTOR sxy;
    /* 0x4 */ s32     sz;
} Actor02100Screen;
STATIC_ASSERT_SIZEOF(Actor02100Screen, 8);

/// 0x28-byte scratch `Actor02100_Fn01FF0` takes from the scratch stack while it
/// re-aims the actor. `shortVec` is the local offset fed to the GTE,
/// `transformed` that offset rotated by the coordinate's matrix and translated
/// by its position, and `delta` the same point mapped back into the
/// coordinate's own frame; the difference between `delta` and the stored target
/// position is what `Gp_OrientAlong` turns into the actor's facing matrix.
typedef struct Actor02100Fn01FF0Scratch {
    /* 0x00 */ VECTOR  transformed;
    /* 0x10 */ VECTOR  delta;
    /* 0x20 */ SVECTOR shortVec;
} Actor02100Fn01FF0Scratch;
STATIC_ASSERT_SIZEOF(Actor02100Fn01FF0Scratch, 0x28);

/// 0x48-byte block `Actor02100_Fn01FF0` reserves on entry and releases on
/// return. Only `shortVec` is read - it is the spawn offset passed to the two
/// effects state 2 emits; the leading bytes are never touched, and the helpers
/// the function inlines allocate their own scratch below this block.
typedef struct Actor02100Fn01FF0Block {
    /* 0x00 */ byte    pad_0[0x40];
    /* 0x40 */ SVECTOR shortVec;
} Actor02100Fn01FF0Block;
STATIC_ASSERT_SIZEOF(Actor02100Fn01FF0Block, 0x48);

/// Entry of `Actor02100_D03D88`, selected by `_Actor02100Work::weapon`, read
/// through two views of the same sixteen bytes. `bounds` holds the frame
/// counts that drive the state machine: `stepFrames` advances state 0 to state 1
/// once it reaches `field_0`, and returns state 6 to state 0 once it reaches
/// `field_2`. The remaining twelve bytes are two RGB triplets, one component
/// per short; `Actor02100_Fn02924` selects a triplet with its style argument
/// and reads each component's low byte, so it indexes the whole entry as
/// `shorts` and reaches the triplets at indices 2..7.
typedef union Actor02100Fn01FF0Timing {
    struct Actor02100Fn01FF0TimingBounds {
        /* 0x0 */ s16  field_0;
        /* 0x2 */ s16  field_2;
        /* 0x4 */ byte pad_4[0xC];
    } bounds;
    s16 shorts[8];
} Actor02100Fn01FF0Timing;
STATIC_ASSERT_SIZEOF(Actor02100Fn01FF0Timing, 0x10);

/// 0x3C-byte block `Actor02100_Fn02924` takes from the scratch stack while it
/// draws one beam between the two screen points in `_Actor02100Work`. `delta` is
/// the span between those points, which `VectorNormalS` turns into `normal`;
/// the y component of `normal` is then negated, so that scaling the pair by a
/// width gives the sideways offset of a beam edge. The beam is drawn as eight
/// segments: `depth` is the current segment's depth, `depthStep` the depth
/// added per segment, and `stepX`/`stepY` the per-segment step of the centre
/// line. `x` and `y` hold one segment's six corners - the two centre-line
/// points, then the two points of each edge.
typedef struct Actor02100Fn02924Scratch {
    /* 0x00 */ VECTOR  delta;
    /* 0x10 */ SVECTOR normal;
    /* 0x18 */ s32     depth;
    /* 0x1C */ s32     depthStep;
    /* 0x20 */ s16     x[6];
    /* 0x2C */ s16     y[6];
    /* 0x38 */ s16     stepX;
    /* 0x3A */ s16     stepY;
} Actor02100Fn02924Scratch;
STATIC_ASSERT_SIZEOF(Actor02100Fn02924Scratch, 0x3C);

/// One beam style in an `Actor02100_D03DD8` row. Each value is scaled by the
/// beam normal and added to the centre line, so the pair places the beam's two
/// edges; every stored row holds a negative and a positive offset of the same
/// size, which makes the beam symmetric about its centre.
typedef struct Actor02100Fn02924Edges {
    /* 0x0 */ s16 first;
    /* 0x2 */ s16 second;
} Actor02100Fn02924Edges;

/// Row of `Actor02100_D03DD8`, selected by `_Actor02100Work::weapon`. The
/// style argument of `Actor02100_Fn02924` picks one of the two entries, and
/// picks the matching colour triplet out of `Actor02100_D03D88`.
typedef struct Actor02100Fn02924Widths {
    /* 0x0 */ Actor02100Fn02924Edges styles[2];
} Actor02100Fn02924Widths;

/// Entry of `Actor02100_D03E1C`: the corners of one of the two quads a segment
/// is built from, as indices into `Actor02100Fn02924Scratch::x` and `y`. Both
/// entries name the centre-line pair first and one edge second, and the quad is
/// shaded from the beam colour on those first two corners to black on the
/// other two, so each quad fades outwards from the centre line.
typedef struct Actor02100Fn02924Corners {
    /* 0x0 */ s16 corners[4];
} Actor02100Fn02924Corners;

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

extern DamageAttack             Actor02100_D03D64[5];
extern EnemyParams              Actor02100_D03D78;
extern Actor02100Fn01FF0Timing  Actor02100_D03D88[];
extern Actor02100Fn02924Widths  Actor02100_D03DD8[];
extern s16                      Actor02100_D03E00[];
extern Actor02100Fn02924Corners Actor02100_D03E1C[];
extern s16                      Actor02100_D03E2C[];

static void Actor02100_Fn03168(Task* arg0);
static void Actor02100_Fn031C4(Enemy* arg0, Task* arg1);
static void Actor02100_Fn032E4(Task* arg0);
static void Actor02100_Fn035D4(Enemy* arg0, Task* arg1);
static s32  Actor02100_Fn014E4(Task* arg0);

static void Actor02100_Fn00048(Enemy* arg0, Task* arg1);

static const EnemyTaskFuncTable3 Actor02100_D00004 = { {
    Actor02100_Fn00048,
    Actor02100_Fn031C4,
    Actor02100_Fn035D4,
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

Actor02100Fn01FF0Timing Actor02100_D03D88[5] = {
    { .shorts = { 30, 90, 48, 0, 0, 255, 0, 0 } },
    { .shorts = { 90, 90, 48, 0, 0, 255, 0, 0 } },
    { .shorts = { 30, 180, 8, 16, 32, 32, 64, 128 } },
    { .shorts = { 30, 180, 24, 24, 0, 32, 32, 0 } },
    { .shorts = { 30, 180, 48, 48, 48, 0, 64, 16 } },
};

Actor02100Fn02924Widths Actor02100_D03DD8[5] = {
    { { { -4, 4 }, { -6, 6 } } },
    { { { -10, 10 }, { -14, 14 } } },
    { { { -10, 10 }, { -10, 10 } } },
    { { { -10, 10 }, { -10, 10 } } },
    { { { -4, 4 }, { -6, 6 } } },
};

s16 Actor02100_D03E00[8] = { 3000, 3500, 4000, 4500, 5000, 6000, 7000, 8000 };

TaskDesc Actor02100_D03E10 = { { { TASK_BODY_TMD, 0x60 } }, Actor02100_Fn03168, { .model = &_gActor02100WatcherBody } };

Actor02100Fn02924Corners Actor02100_D03E1C[2] = {
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

static s32 Actor02100_Fn0337C(SVECTOR* arg0, SVECTOR* arg1);

static void Actor02100_Fn011C4(Task* arg0);

static void Actor02100_Fn02924(Task* arg0, s32 arg1);

static void Actor02100_Fn034E0(Task* arg0);

static void Actor02100_Fn004C4(Task* arg0);

static void Actor02100_Fn03488(Task* arg0);

static void Actor02100_Fn00ADC(Task* arg0);

static void Actor02100_Fn016EC(Task* arg0);

static void Actor02100_Fn01FF0(Task* arg0);

static void            Actor02100_Fn00DCC(Task* arg0);
static __inline__ void Actor02100_AimAndBuildVectors(Task* arg0);
static __inline__ void Actor02100_SetVector(Task* arg0);
static __inline__ void _actor02100StoreNearVector(_Actor02100Work* work);
static __inline__ void Actor02100_BuildVectors(Task* arg0, _Actor02100Work* currentWork);
static __inline__ void Actor02100_OrientScratch(Task* arg0);
static __inline__ void Actor02100_UpdateVectors(Task* arg0);
static __inline__ void Actor02100_ReleaseScratch28(void);

static void Actor02100_Fn00048(Enemy* arg0, Task* arg1)
{
    WorldCollisionContact*    table;
    WorldCollisionContact*    contacts;
    SVECTOR*                  rotation;
    Actor02100Fn00048Scratch* head;
    s16                       variant;
    s16                       scale;
    TmdObject*                extra;
    GfxCoord*                 coord;
    _Actor02100Work*          work;
    s16*                      column1;
    s16*                      column2;
    MATRIX*                   matrix;

    extra = arg1->extra.tmd;
    coord = extra->coords;
    work  = memCalloc(sizeof(_Actor02100Work), false);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work        = work;
    work->patrolRange = (s16)((arg0->place->variant / 10) & 0xFF);
    variant           = (arg0->place->variant % 10) & 0xFF;
    work->weapon      = variant;
    if ((work->patrolRange >= ACTOR_02100_PATROL_RANGE_COUNT) || (variant >= ACTOR_02100_WEAPON_COUNT)) {
        enemyDestroy(arg0, arg1);
        return;
    }
    extra->flags                  = 0;
    coord->composeStamp           = GRAPHICS_COORD_DIRTY;
    head                          = SCRATCH_STACK_CURSOR(Actor02100Fn00048Scratch);
    extra->lightMtx               = &work->light;
    extra->colorMtx               = &work->color;
    rotation                      = &head[-1].rotation;
    rotation->vx                  = 0;
    rotation->vy                  = 0;
    SCRATCH_STACK_CURSOR(SVECTOR) = rotation;
    rotation->vz                  = arg0->place->mode;
    RotMatrix(rotation, &head[-1].matrix);
    matrix = &coord->coord;
    gte_SetRotMatrix(matrix);
    gte_ldclmv(&head[-1].matrix);
    gte_rtir();
    gte_stclmv(matrix);
    gte_ldclmv(&head[-1].matrix.m[0][1]);
    gte_rtir();
    column1 = &coord->coord.m[0][1];
    gte_stclmv(column1);
    gte_ldclmv(&head[-1].matrix.m[0][2]);
    gte_rtir();
    column2 = &coord->coord.m[0][2];
    gte_stclmv(column2);
    arg0->field_4  = matrix;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->bodyPos.vz           = 0x96;
    arg0->coord                = coord;
    arg0->bodyPos.vx           = 0;
    arg0->bodyPos.vy           = 0;
    arg0->param                = &Actor02100_D03D78;
    arg0->recs                 = work->hitContacts;
    arg0->hp                   = (u16)Actor02100_D03D78.hpMax;
    work->hitEffect.coord      = coord;
    work->hitEffect.spawnArgLo = 0x200;
    work->hitEffect.spawnArgHi = 1;
    Gp_IncStateF0Ref(0);
    scale = 0x19;
    if (work->patrolRange == 0) {
        work->mode = ACTOR_02100_MODE_WATCH;
        scale      = 0;
    } else {
        work->mode = ACTOR_02100_MODE_PATROL;
    }
    work->velocity.vx              = (coord->coord.m[0][0] * scale) >> 12;
    work->velocity.vy              = (coord->coord.m[1][0] * scale) >> 12;
    work->velocity.vz              = (coord->coord.m[2][0] * scale) >> 12;
    work->patrolVelocity.vx        = work->velocity.vx;
    work->patrolVelocity.vy        = work->velocity.vy;
    work->patrolVelocity.vz        = work->velocity.vz;
    table                          = work->hitContacts;
    work->hitBody.coord            = coord;
    work->hitBody.context.contacts = table;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vy           = 0;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = 0x30015;
    work->hitBody.radius           = 0x190;
    work->hitBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->hitBody);
    Gp_InitRec18Table(table, 1, 0);
    contacts                               = work->strikeContacts;
    work->playerStrikeShape.ends[0].vx     = 0;
    work->playerStrikeShape.ends[0].vy     = 0;
    work->playerStrikeShape.ends[0].vz     = ACTOR_02100_STRIKE_REACH;
    work->playerStrikeShape.ends[1].vx     = 0;
    work->playerStrikeShape.ends[1].vy     = 0;
    work->playerStrikeShape.ends[1].vz     = ACTOR_02100_MUZZLE_OFFSET;
    work->playerStrikeShape.end0Radius     = 0x14;
    work->playerStrikeShape.end1Radius     = 0x14;
    work->playerStrikeShape.contacts       = contacts;
    work->playerStrikeBody.context.capsule = &work->playerStrikeShape;
    work->playerStrikeBody.coord           = coord;
    work->playerStrikeBody.pos.vx          = 0;
    work->playerStrikeBody.pos.vy          = 0;
    work->playerStrikeBody.pos.vz          = 0;
    work->playerStrikeBody.key             = 0;
    work->playerStrikeBody.radius          = 0;
    work->playerStrikeBody.flags           = (u32)WORLD_COLLISION_BODY_CAPSULE;
    work->hitBody.flags                    = (u16)(work->hitBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    Gp_LinkObj(3, &work->playerStrikeBody);
    Gp_InitRec18Table(contacts, 1, 0);
    work->enemyStrikeShape.ends[0].vx     = 0;
    work->enemyStrikeShape.ends[0].vy     = 0;
    work->enemyStrikeShape.ends[0].vz     = ACTOR_02100_STRIKE_REACH;
    work->enemyStrikeShape.ends[1].vx     = 0;
    work->enemyStrikeShape.ends[1].vy     = 0;
    work->enemyStrikeShape.ends[1].vz     = ACTOR_02100_MUZZLE_OFFSET;
    work->enemyStrikeShape.end0Radius     = 0x14;
    work->enemyStrikeShape.end1Radius     = 0x14;
    work->enemyStrikeShape.contacts       = contacts;
    work->enemyStrikeBody.coord           = coord;
    work->enemyStrikeBody.context.capsule = &work->enemyStrikeShape;
    work->enemyStrikeBody.pos.vx          = 0;
    work->enemyStrikeBody.pos.vy          = 0;
    work->enemyStrikeBody.pos.vz          = 0;
    work->enemyStrikeBody.key             = 0;
    work->enemyStrikeBody.radius          = 0;
    work->enemyStrikeBody.flags           = (u32)WORLD_COLLISION_BODY_CAPSULE;
    work->playerStrikeBody.flags          = (u16)((work->playerStrikeBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED))) | (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT));
    Gp_LinkObj(1, &work->enemyStrikeBody);
    work->enemyStrikeBody.flags                    = (u16)((work->enemyStrikeBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED))) | WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT);
    arg1->state                                    = 1;
    SCRATCH_STACK_CURSOR(Actor02100Fn00048Scratch) = SCRATCH_STACK_CURSOR(Actor02100Fn00048Scratch) + 1;
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
                func_800DA6E8(&enemy->node, 0, 0);
            } else if ((((u32)work->hitContacts[0].key.value >> 8) & 0x3F) < 0x21U) {
                src             = gPlayerActorTasks[((u32)work->hitContacts[0].key.value >> 7) & 1]->extra.tmd->coords;
                scratch->vec.vx = src->coord.t[0] - coord->coord.t[0];
                scratch->vec.vy = src->coord.t[1] - coord->coord.t[1];
                scratch->vec.vz = src->coord.t[2] - coord->coord.t[2];
                damage          = Gp_ComputeDamage(work->hitContacts[0].key.value,
                                                   SquareRoot0(scratch->vec.vx * scratch->vec.vx +
                                                               scratch->vec.vy * scratch->vec.vy +
                                                               scratch->vec.vz * scratch->vec.vz),
                                                   0, 0);
                if (Gp_RollEnemyChance(arg0->spawnArg2.pointer,
                                       work->hitContacts[0].key.value, 0) != 0) {
                    damage *= 4;
                    Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 0, 0);
                }
                enemy->hp -= damage;
                func_800DA6E8(&enemy->node, damage, 0);
                work->hitThisTick = 1;
                if (enemy->hp <= 0) {
                    Gp_SpawnEff(EFFECT_EXPLOSION, coord, 0x10002400, 0);
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0x32FF1400, 0);
                    work->mode                    = ACTOR_02100_MODE_DESTROYED;
                    work->step                    = ACTOR_02100_DEATH_STEP_RELEASE;
                    work->playerStrikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->enemyStrikeBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->playerStrikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
                    work->enemyStrikeBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
                    arg0->state                   = 2;
                    sound                         = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4015000A;
                    SndEvt_EnqueueType6(sound, (s8)worldCoordGetOriginAudioPan(coord),
                                        (s8)worldCoordGetOriginAudioDepth(coord));
                } else if (damage > 0) {
                    if (work->hitEffectCooldown == 0) {
                        scratch->shortVec.vx = 0;
                        scratch->shortVec.vy = 0;
                        scratch->shortVec.vz = 0xC8;
                        if ((Gp_GetIdParam0(work->hitContacts[0].key.value) & 0xFFFF) == 7) {
                            Gp_SpawnEff(EFFECT_HIT_BLAST, coord,
                                        work->hitEffect.spawnArgLo | (work->hitEffect.spawnArgHi << 16),
                                        &scratch->shortVec);
                        }
                        func_800FDB18(7, coord, &scratch->shortVec, &work->hitEffect);
                        work->hitEffectCooldown = 10;
                    }
                    sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40150009;
                    pan   = (s8)worldCoordGetOriginAudioPan(coord);
                    depth = (s8)worldCoordGetOriginAudioDepth(coord);
                    SndEvt_EnqueueType6(sound, pan, depth);
                    stun = Gp_GetIdParam2(work->hitContacts[0].key.value);
                    if (stun > 0) {
                        work->hitCooldown = stun;
                    }
                }
            }
        }
    }

    Gp_ClearRec18Occupied(work->hitContacts);
    work->beamBlocked = 0;
    if (Gp_CountRec18Hi(work->strikeContacts, WORLD_COLLISION_CONTACT_GRID) != 0) {
        index   = func_800E1B24(work->strikeContacts[0].key.value);
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
            Gp_SpawnEff(EFFECT_IMPACT_SPARK, coord, 0, &scratch->shortVec);
        }
    }

    Gp_ClearRec18Occupied(work->strikeContacts);
    SCRATCH_STACK_RELEASE_BLOCK(_Actor02100VectorScratch);
}

/// Stops the looping effect, clears the offset vector and starts the tail
/// sound. Expanded at the end of both active steps, which the compiler emits
/// as one shared tail; `sound` is deliberately a caller-scope variable, since
/// both expansions must name the same object.
#define STOP_SOUND                                                                                                        \
    work->velocity.vx = 0;                                                                                                \
    work->velocity.vy = 0;                                                                                                \
    work->velocity.vz = 0;                                                                                                \
    SndEvt_EnqueueType7(work->loopSound, 1);                                                                              \
    work->loopSoundKind = ACTOR_02100_LOOP_SOUND_NONE;                                                                    \
    sound               = (((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40150008); \
    {                                                                                                                     \
        s32 pan;                                                                                                          \
        s32 depth;                                                                                                        \
                                                                                                                          \
        pan   = (s8)worldCoordGetOriginAudioPan(coord);                                                                   \
        depth = (s8)worldCoordGetOriginAudioDepth(coord);                                                                 \
        SndEvt_EnqueueType6(sound, pan, depth);                                                                           \
    }

/// Per-frame handler for the four-step patrol cycle at `patrolStep`: two active
/// steps that keep a looping effect playing while `velocity` moves the actor,
/// separated by 60-frame gaps. Steps 0 and 2 start the effect on their
/// first frame, retrigger it each frame, and run for `patrolRange * 40` frames;
/// each then stops the effect, clears `velocity` and starts the tail sound.
/// Step 1 installs the negated `patrolVelocity`, step 3 restores it and returns
/// to 0.
static void Actor02100_Fn00ADC(Task* arg0)
{
    _Actor02100Work* work;
    GfxCoord*        coord;
    s32              negX;
    s32              negY;
    s32              negZ;
    s16              state;
    s32              one;
    s32              sound;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    one   = 1;
    work->patrolFrames++;
    state = work->patrolStep;

    switch (state) {
        case ACTOR_02100_PATROL_STEP_OUT:
            if (work->patrolFrames == one) {
                work->loopSound = (((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40150007);
                {
                    s32 pan;
                    s32 depth;

                    pan   = (s8)worldCoordGetOriginAudioPan(coord);
                    depth = (s8)worldCoordGetOriginAudioDepth(coord);
                    SndEvt_EnqueueType6(work->loopSound, pan, depth);
                }
                work->loopSoundKind = one;
            }
            {
                s32 pan;
                s32 depth;

                pan   = (s8)worldCoordGetOriginAudioPan(coord);
                depth = (s8)worldCoordGetOriginAudioDepth(coord);
                SndEvt_EnqueueTypeA(work->loopSound, pan, depth);
            }
            if (work->patrolFrames < (work->patrolRange * 40)) {
                break;
            }
            work->patrolFrames = 0;
            work->patrolStep   = one;
            STOP_SOUND;
            break;

        case ACTOR_02100_PATROL_STEP_OUT_PAUSE:
            if (work->patrolFrames < 60) {
                break;
            }
            work->patrolStep   = (state = 2);
            work->patrolFrames = 0;
            work->velocity.vx  = (negX = -work->patrolVelocity.vx);
            work->velocity.vy  = (negY = -work->patrolVelocity.vy);
            work->velocity.vz  = (negZ = -work->patrolVelocity.vz);
            break;

        case ACTOR_02100_PATROL_STEP_BACK: {
            s32 timer;

            timer = work->patrolFrames;
            if (timer == one) {
                work->loopSound = (((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40150007);
                {
                    s32 pan;
                    s32 depth;

                    pan   = (s8)worldCoordGetOriginAudioPan(coord);
                    depth = (s8)worldCoordGetOriginAudioDepth(coord);
                    SndEvt_EnqueueType6(work->loopSound, pan, depth);
                }
                work->loopSoundKind = timer;
            }
        }
            {
                s32 pan;
                s32 depth;

                pan   = (s8)worldCoordGetOriginAudioPan(coord);
                depth = (s8)worldCoordGetOriginAudioDepth(coord);
                SndEvt_EnqueueTypeA(work->loopSound, pan, depth);
            }
            if (work->patrolFrames < (work->patrolRange * 40)) {
                break;
            }
            work->patrolFrames = 0;
            work->patrolStep   = ACTOR_02100_PATROL_STEP_BACK_PAUSE;
            STOP_SOUND;
            break;

        case ACTOR_02100_PATROL_STEP_BACK_PAUSE:
            if (work->patrolFrames < 60) {
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

/// Line-of-sight scan. Takes a 0x20-byte block from the scratch stack, builds
/// the world-space delta from this actor's coordinate to the player's (entry 0
/// of the player's coordinate array in mode 4, entry 3 otherwise) and, when the
/// player is in front of the actor, checks the distance against the sight range
/// in `Actor02100_D03E00` and asks `Actor02100_Fn0337C` whether the segment is
/// clear. A hit latches the player onto `target` and switches `mode` to the
/// beam attack (or the gun attack for weapon 4). `scanDelay` holds the scan
/// off while the session's view state is 1, and for 5 frames after.
static void Actor02100_Fn00DCC(Task* arg0)
{
    _Actor02100Work* work;
    Actor02100Sight* blk;
    GfxCoord*        self;
    GfxCoord*        target;
    u8*              head;
    u32              dist;
    s32              mode;

    self = arg0->extra.tmd->coords;
    work = arg0->work;

    if (gGameSession->viewReady == 1) {
        work->scanDelay = 5;
    }
    if (work->scanDelay != 0) {
        work->scanDelay--;
        return;
    }

    work->target         = NULL;
    work->targetDistance = 0;
    work->targetKind     = ACTOR_02100_TARGET_NONE;
    self->composeStamp   = GRAPHICS_COORD_DIRTY;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x20;
    blk                      = (Actor02100Sight*)(head - 0x20);

    if (work->weapon == ACTOR_02100_WEAPON_GUN) {
        target = &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[0];
    } else {
        target = &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[3];
    }
    target->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(target);

    if (work->hitThisTick == 0) {
        blk->delta.vx = target->workm.t[0] - self->workm.t[0];
        blk->delta.vy = target->workm.t[1] - self->workm.t[1];
        blk->delta.vz = target->workm.t[2] - self->workm.t[2];

        if (((blk->delta.vx * self->workm.m[0][2]) + (blk->delta.vy * self->workm.m[1][2]) +
             (blk->delta.vz * self->workm.m[2][2])) > 0) {
            dist = SquareRoot0((blk->delta.vx * blk->delta.vx) + (blk->delta.vy * blk->delta.vy) +
                               (blk->delta.vz * blk->delta.vz));
            if (dist < Actor02100_D03E00[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex & 7]) {
                blk->from.vx = target->workm.t[0];
                blk->from.vy = target->workm.t[1];
                blk->from.vz = target->workm.t[2];
                blk->to.vx   = self->workm.t[0];
                blk->to.vy   = self->workm.t[1];
                blk->to.vz   = self->workm.t[2];
                if (Actor02100_Fn0337C(&blk->from, &blk->to) == 0) {
                    work->target         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                    work->targetDistance = dist;
                    work->targetKind     = ACTOR_02100_TARGET_PLAYER;
                }
            }
        }
        Actor02100_Fn011C4(arg0);
    } else {
        blk->delta.vx = target->workm.t[0] - self->workm.t[0];
        blk->delta.vy = target->workm.t[1] - self->workm.t[1];
        blk->delta.vz = target->workm.t[2] - self->workm.t[2];

        if (((blk->delta.vx * self->workm.m[0][2]) + (blk->delta.vy * self->workm.m[1][2]) +
             (blk->delta.vz * self->workm.m[2][2])) > 0) {
            blk->from.vx = target->workm.t[0];
            blk->from.vy = target->workm.t[1];
            blk->from.vz = target->workm.t[2];
            blk->to.vx   = self->workm.t[0];
            blk->to.vy   = self->workm.t[1];
            blk->to.vz   = self->workm.t[2];
            if (Actor02100_Fn0337C(&blk->from, &blk->to) == 0) {
                work->target         = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                work->targetDistance = 1;
                work->targetKind     = ACTOR_02100_TARGET_PLAYER;
            }
        }
    }

    if (work->targetKind != ACTOR_02100_TARGET_NONE) {
        Gp_ArmStateF0(1);
        if (work->weapon == ACTOR_02100_WEAPON_GUN) {
            mode = ACTOR_02100_MODE_GUN;
        } else {
            mode = ACTOR_02100_MODE_BEAM;
        }
        work->resumeVelocity.vx = work->velocity.vx;
        work->resumeVelocity.vy = work->velocity.vy;
        work->resumeVelocity.vz = work->velocity.vz;
        // Either attack opens with its aim step; the two share the value.
        work->mode        = mode;
        work->step        = ACTOR_02100_BEAM_STEP_AIM;
        work->stepFrames  = 0;
        work->shotsFired  = 0;
        work->velocity.vx = 0;
        work->velocity.vy = 0;
        work->velocity.vz = 0;
        if (work->loopSoundKind == ACTOR_02100_LOOP_SOUND_PATROL) {
            SndEvt_EnqueueType7(work->loopSound, 1);
            work->loopSoundKind = ACTOR_02100_LOOP_SOUND_NONE;
        }
    }

    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

static void Actor02100_Fn011C4(Task* arg0)
{
    Actor02100Fn011C4Scratch* scratch;
    Task*                     list;
    Task*                     head;
    Task*                     current;
    Enemy*                    enemy;
    _Actor02100Work*          work;
    GfxCoord*                 coord;
    u32                       index;
    u32                       dist;

    list  = gameGetTaskSlot(GAME_TASK_SLOT_SCENE);
    coord = arg0->extra.tmd->coords;
    head  = list->firstChild;
    work  = arg0->work;
    if (head != NULL) {
        scratch = SCRATCH_STACK_RESERVE_BLOCK(Actor02100Fn011C4Scratch);
        current = head;
        SOFT_TOUCH_REG(head);
        do {
            enemy = current->spawnArg2.pointer;
            index = enemy->place->entryId;
            if (index >= 0x50) {
                index = 0;
            }
            if (Actor02100_D03E2C[index] == 0 && enemy->hp > 0) {
                Gp_GetLockPos(&enemy->node, &scratch->lock);
                scratch->delta.vx = scratch->lock.vx - coord->coord.t[0];
                scratch->delta.vy = scratch->lock.vy - coord->coord.t[1];
                scratch->delta.vz = scratch->lock.vz - coord->coord.t[2];
                if ((scratch->delta.vx * coord->coord.m[0][2]) +
                        (scratch->delta.vy * coord->coord.m[1][2]) +
                        (scratch->delta.vz * coord->coord.m[2][2]) >
                    0) {
                    dist = SquareRoot0((scratch->delta.vx * scratch->delta.vx) +
                                       (scratch->delta.vy * scratch->delta.vy) +
                                       (scratch->delta.vz * scratch->delta.vz));
                    if ((work->targetDistance == 0 || dist < work->targetDistance) &&
                        dist < Actor02100_D03E00[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex & 7]) {
                        scratch->from.vx = scratch->lock.vx;
                        scratch->from.vy = scratch->lock.vy;
                        scratch->from.vz = scratch->lock.vz;
                        gte_SetRotMatrix(&gGfxViewCoord.workm);
                        gte_ldv0(&scratch->from);
                        gte_rtv0();
                        gte_stlvnl(&scratch->transformed);
                        scratch->transformed.vx += gGfxViewCoord.workm.t[0];
                        scratch->transformed.vy += gGfxViewCoord.workm.t[1];
                        scratch->transformed.vz += gGfxViewCoord.workm.t[2];
                        scratch->from.vx         = scratch->transformed.vx;
                        scratch->from.vy         = scratch->transformed.vy;
                        scratch->from.vz         = scratch->transformed.vz;
                        scratch->to.vx           = coord->workm.t[0];
                        scratch->to.vy           = coord->workm.t[1];
                        scratch->to.vz           = coord->workm.t[2];
                        if (Actor02100_Fn0337C(&scratch->from, &scratch->to) == 0) {
                            work->target         = current;
                            work->targetDistance = dist;
                            work->targetKind     = ACTOR_02100_TARGET_ENEMY;
                        }
                    }
                }
            }
            current = current->nextSibling;
        } while (current != head);
        SCRATCH_STACK_RELEASE_BLOCK(Actor02100Fn011C4Scratch);
    }
}

static s32 Actor02100_Fn014E4(Task* arg0)
{
    _Actor02100VectorScratch* scratch;
    _Actor02100Work*          work;
    GfxCoord*                 coord;
    GfxCoord*                 targetCoord;
    VECTOR*                   vec;
    WorldTargetNode*          lock;
    s32                       result;
    s32                       state;

    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    result = 0;
    if (work->target == NULL) {
        return result;
    }

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_Actor02100VectorScratch);
    vec     = &scratch->vec;
    state   = work->targetKind;
    if (state == ACTOR_02100_TARGET_PLAYER) {
        goto case1;
    }
    if (state < ACTOR_02100_TARGET_ENEMY) {
        goto cleanup;
    }
    if (state == ACTOR_02100_TARGET_ENEMY) {
        goto case2;
    }
    goto cleanup;

case1:
    if (work->weapon == ACTOR_02100_WEAPON_GUN) {
        targetCoord = work->target->extra.tmd->coords;
    } else {
        targetCoord = &work->target->extra.tmd->coords[3];
    }
    vec->vx = targetCoord->workm.t[0];
    vec->vy = targetCoord->workm.t[1];
    vec->vz = targetCoord->workm.t[2];
    ApplyTransposeMatrixLV(&coord->workm, vec, &work->targetPos);
    result = 1;
    goto cleanup;

case2:
    if (((Enemy*)work->target->spawnArg2.pointer)->hp <= 0) {
        goto cleanup;
    }
    lock = &((Enemy*)work->target->spawnArg2.pointer)->node;
    // The lock position is written over the three components of `vec`, then
    // narrowed for the GTE and rotated back into `vec` in view space.
    Gp_GetLockPos(lock, (VECTOR3*)&scratch->vec);
    scratch->shortVec.vx = scratch->vec.vx;
    scratch->shortVec.vy = scratch->vec.vy;
    scratch->shortVec.vz = scratch->vec.vz;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&scratch->shortVec);
    gte_rtv0();
    gte_stlvnl(vec);
    scratch->vec.vx += gGfxViewCoord.workm.t[0];
    scratch->vec.vy += gGfxViewCoord.workm.t[1];
    scratch->vec.vz += gGfxViewCoord.workm.t[2];
    ApplyTransposeMatrixLV(&coord->workm, &scratch->vec, &work->targetPos);
    result = 1;

cleanup:
    SCRATCH_STACK_RELEASE_BLOCK(_Actor02100VectorScratch);
    return result;
}

/// Aims the actor at its stored target and rebuilds both direction vectors from
/// the new facing. A fixed forward offset is rotated by the coordinate's matrix,
/// translated into the coordinate's own frame and subtracted from the target
/// position; `Gp_OrientAlong` turns the vector that remains into the facing
/// matrix at `aim`. The near vector at `beamPoints[1]` and the far vector
/// at `playerStrikeShape.ends[0]`, mirrored into `enemyStrikeShape.ends[0]`,
/// are then rotated through that matrix. Each step borrows scratch from the scratch stack and releases it.
///
/// `Actor02100_OrientScratch`, `Actor02100_UpdateVectors` and
/// `Actor02100_SetVector` do the same three steps for their own callers. This
/// copy is not interchangeable with them: the statement order here is what
/// reproduces this function's schedule and register allocation.
static __inline__ void Actor02100_AimAndBuildVectors(Task* arg0)
{
    Actor02100Fn01FF0Scratch* scratch;
    _Actor02100VectorScratch* shortScratch;
    _Actor02100Work*          work;
    _Actor02100Work*          nextWork;
    _Actor02100Work*          nextWork2;
    GfxCoord*                 coord;
    SVECTOR*                  shortVec;
    u8*                       head0;
    u8*                       head1;
    u8*                       head2;

    coord                    = arg0->extra.tmd->coords;
    head0                    = SCRATCH_STACK_CURSOR(u8);
    scratch                  = (Actor02100Fn01FF0Scratch*)(head0 - 0x28);
    SCRATCH_STACK_CURSOR(u8) = (u8*)scratch;
    work                     = arg0->work;

    scratch->shortVec.vx = 0;
    scratch->shortVec.vy = 0;
    scratch->shortVec.vz = ACTOR_02100_MUZZLE_OFFSET;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&scratch->shortVec);
    gte_rtv0();
    gte_stlvnl(&scratch->transformed);
    scratch->transformed.vx += coord->workm.t[0];
    scratch->transformed.vy += coord->workm.t[1];
    scratch->transformed.vz += coord->workm.t[2];
    ApplyTransposeMatrixLV(&coord->workm, &scratch->transformed, &scratch->delta);
    scratch->transformed.vx = work->targetPos.vx - scratch->delta.vx;
    scratch->transformed.vy = work->targetPos.vy - scratch->delta.vy;
    scratch->transformed.vz = work->targetPos.vz - scratch->delta.vz;
    Gp_OrientAlong(&scratch->transformed, &work->aim, 0);

    // Release the aim block and reserve a vector block in its place for the
    // beam's end point.
    head1                      = SCRATCH_STACK_CURSOR(u8);
    nextWork                   = arg0->work;
    shortScratch               = (_Actor02100VectorScratch*)(head1 + sizeof(Actor02100Fn01FF0Scratch) - sizeof(_Actor02100VectorScratch));
    nextWork->beamPoints[0].vx = 0;
    nextWork->beamPoints[0].vy = 0;
    nextWork->beamPoints[0].vz = ACTOR_02100_MUZZLE_OFFSET;
    shortScratch->shortVec.vx  = 0;
    shortScratch->shortVec.vy  = 0;
    SCRATCH_STACK_CURSOR(u8)   = head1 + sizeof(Actor02100Fn01FF0Scratch);
    shortScratch->shortVec.vz  = nextWork->beamLength;
    SCRATCH_STACK_CURSOR(u8)   = (u8*)shortScratch;
    gte_SetRotMatrix(&nextWork->aim);
    gte_ldv0(&shortScratch->shortVec);
    gte_rtv0();
    gte_stsv(&nextWork->beamPoints[1]);
    nextWork->beamPoints[1].vz += ACTOR_02100_MUZZLE_OFFSET;

    // Release the vector block and reserve the far point's input in its place.
    head2                    = SCRATCH_STACK_CURSOR(u8);
    shortVec                 = (SVECTOR*)(head2 + sizeof(_Actor02100VectorScratch) - sizeof(SVECTOR));
    SCRATCH_STACK_CURSOR(u8) = head2 + sizeof(_Actor02100VectorScratch);
    nextWork2                = arg0->work;
    SCRATCH_STACK_CURSOR(u8) = (u8*)shortVec;
    shortVec->vx             = 0;
    shortVec->vy             = 0;
    shortVec->vz             = ACTOR_02100_STRIKE_REACH;
    gte_SetRotMatrix(&nextWork2->aim);
    gte_ldv0(shortVec);
    gte_rtv0();
    gte_stsv(&nextWork2->playerStrikeShape.ends[0]);
    nextWork2->enemyStrikeShape.ends[0] = nextWork2->playerStrikeShape.ends[0];
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// Rewrites the far vector: the `ACTOR_02100_STRIKE_REACH` offset rotated by
/// the facing matrix into `playerStrikeShape.ends[0]`, mirrored into
/// `enemyStrikeShape.ends[0]`. The offset is built in an `SVECTOR` borrowed
/// from the scratch stack for the rotation.
static __inline__ void Actor02100_SetVector(Task* arg0)
{
    _Actor02100Work* work;
    SVECTOR*         shortVec;

    work         = arg0->work;
    shortVec     = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    shortVec->vx = 0;
    shortVec->vy = 0;
    shortVec->vz = ACTOR_02100_STRIKE_REACH;
    gte_SetRotMatrix(&work->aim);
    gte_ldv0(shortVec);
    gte_rtv0();
    gte_stsv(&work->playerStrikeShape.ends[0]);
    work->enemyStrikeShape.ends[0] = work->playerStrikeShape.ends[0];
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Stores the near vector the GTE has just rotated into `beamPoints[1]`,
/// advances it by `ACTOR_02100_MUZZLE_OFFSET` and releases the input block the
/// rotation borrowed from the scratch stack.
static __inline__ void _actor02100StoreNearVector(_Actor02100Work* work)
{
    gte_stsv(&work->beamPoints[1]);
    work->beamPoints[1].vz += ACTOR_02100_MUZZLE_OFFSET;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor02100VectorScratch);
}

/// Rebuilds the same two direction vectors as `Actor02100_AimAndBuildVectors`
/// from the facing matrix the actor already holds, without re-aiming. The two
/// arms of the frame test hold the same code, each rotating the near vector's
/// input and finishing through `_actor02100StoreNearVector`; the branch has no
/// effect on what is written. The far vector follows through
/// `Actor02100_SetVector`.
static __inline__ void Actor02100_BuildVectors(Task* arg0, _Actor02100Work* currentWork)
{
    _Actor02100VectorScratch* scratch;
    _Actor02100VectorScratch* scratch2;
    _Actor02100Work*          work;
    u8*                       head;

    if (currentWork->stepFrames == 0xE) {
        work                     = arg0->work;
        work->beamPoints[0].vz   = ACTOR_02100_MUZZLE_OFFSET;
        head                     = SCRATCH_STACK_CURSOR(u8);
        scratch                  = (_Actor02100VectorScratch*)(head - sizeof(_Actor02100VectorScratch));
        work->beamPoints[0].vx   = 0;
        work->beamPoints[0].vy   = 0;
        scratch->shortVec.vx     = 0;
        scratch->shortVec.vy     = 0;
        SCRATCH_STACK_CURSOR(u8) = (u8*)scratch;
        scratch->shortVec.vz     = work->beamLength;
        gte_SetRotMatrix(&work->aim);
        // The input is addressed through `head`: reusing that variable is what
        // keeps the address in the cursor's register.
        head = (u8*)&scratch->shortVec;
        gte_ldv0((SVECTOR*)head);
        gte_rtv0();
        _actor02100StoreNearVector(work);
    } else {
        work                     = arg0->work;
        work->beamPoints[0].vz   = ACTOR_02100_MUZZLE_OFFSET;
        head                     = SCRATCH_STACK_CURSOR(u8);
        scratch2                 = (_Actor02100VectorScratch*)(head - sizeof(_Actor02100VectorScratch));
        work->beamPoints[0].vx   = 0;
        work->beamPoints[0].vy   = 0;
        scratch2->shortVec.vx    = 0;
        scratch2->shortVec.vy    = 0;
        SCRATCH_STACK_CURSOR(u8) = (u8*)scratch2;
        scratch2->shortVec.vz    = work->beamLength;
        gte_SetRotMatrix(&work->aim);
        head = (u8*)&scratch2->shortVec;
        gte_ldv0((SVECTOR*)head);
        gte_rtv0();
        _actor02100StoreNearVector(work);
    }
    Actor02100_SetVector(arg0);
}

/// Four-state sweep with a charge-up, a strike and a recovery wait. State 0 aims
/// at the target every frame until `Actor02100_Fn014E4` loses it - which drops
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
                if (Actor02100_Fn014E4(arg0) == 0) {
                    work->step       = ACTOR_02100_BEAM_STEP_RECOVER;
                    work->stepFrames = 0;
                    if (work->loopSoundKind == ACTOR_02100_LOOP_SOUND_CHARGE) {
                        SndEvt_EnqueueType7(work->loopSound, 1);
                        work->loopSoundKind = ACTOR_02100_LOOP_SOUND_NONE;
                    }
                    break;
                }

                Actor02100_AimAndBuildVectors(arg0);
            }

            if (work->stepFrames == 1) {
                work->loopSound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40150001;
                SndEvt_EnqueueType6(work->loopSound, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
                work->loopSoundKind = ACTOR_02100_LOOP_SOUND_CHARGE;
            }
            if (work->stepFrames >= 2) {
                Actor02100_Fn034E0(arg0);
                Actor02100_Fn02924(arg0, 0);
            }
            work->playerStrikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            frame                         = (u16)work->stepFrames + 1;
            work->stepFrames              = frame;
            work->enemyStrikeBody.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->playerStrikeBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work->enemyStrikeBody.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
            if (frame >= Actor02100_D03D88[work->weapon].bounds.field_0) {
                work->stepFrames = 0;
                work->step       = ACTOR_02100_BEAM_STEP_LOCK;
            }
            break;

        case ACTOR_02100_BEAM_STEP_LOCK:
            if (work->stepFrames == state) {
                SndEvt_EnqueueType7(work->loopSound, 1);
                work->loopSoundKind = ACTOR_02100_LOOP_SOUND_NONE;
            }
            Actor02100_BuildVectors(arg0, work);
            Actor02100_Fn034E0(arg0);
            Actor02100_Fn02924(arg0, 0);
            frame            = (u16)work->stepFrames + 1;
            work->stepFrames = frame;
            if (frame >= 0xF) {
                work->stepFrames = 0;
                work->step       = ACTOR_02100_BEAM_STEP_FIRE;
                soundId          = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40150002;
                SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord),
                                    (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;

        case ACTOR_02100_BEAM_STEP_FIRE:
            Actor02100_Fn034E0(arg0);
            Actor02100_Fn02924(arg0, 1);
            frame = work->stepFrames;
            if (frame == 1) {
                work->playerStrikeBody.key = Gp_PackPair(Actor02100_D03D64, work->weapon);
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
                SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord),
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
            if (frame >= Actor02100_D03D88[work->weapon].bounds.field_2) {
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

/// Points the actor at its stored target. Rotates a fixed forward offset by the
/// coordinate's matrix, maps it back into the coordinate's own frame, and hands
/// the vector from there to the target position to `Gp_OrientAlong`, which
/// writes the facing matrix at `aim`.
static __inline__ void Actor02100_OrientScratch(Task* arg0)
{
    Actor02100Fn01FF0Scratch* scratch;
    _Actor02100Work*          work;
    GfxCoord*                 coord;
    u8*                       head;

    coord                    = arg0->extra.tmd->coords;
    head                     = SCRATCH_STACK_CURSOR(u8);
    work                     = arg0->work;
    scratch                  = (Actor02100Fn01FF0Scratch*)(head - 0x28);
    SCRATCH_STACK_CURSOR(u8) = (u8*)scratch;

    scratch->shortVec.vx = 0;
    scratch->shortVec.vy = 0;
    scratch->shortVec.vz = ACTOR_02100_MUZZLE_OFFSET;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&scratch->shortVec);
    gte_rtv0();
    gte_stlvnl(&scratch->transformed);
    scratch->transformed.vx += coord->workm.t[0];
    scratch->transformed.vy += coord->workm.t[1];
    scratch->transformed.vz += coord->workm.t[2];
    ApplyTransposeMatrixLV(&coord->workm, &scratch->transformed, &scratch->delta);
    scratch->transformed.vx = work->targetPos.vx - scratch->delta.vx;
    scratch->transformed.vy = work->targetPos.vy - scratch->delta.vy;
    scratch->transformed.vz = work->targetPos.vz - scratch->delta.vz;
    Gp_OrientAlong(&scratch->transformed, &work->aim, 0);
}

/// Refreshes the two vectors the actor's facing matrix defines: the near one at
/// `beamPoints[1]`, rotated from the length in `beamLength` and advanced by
/// `ACTOR_02100_MUZZLE_OFFSET`, and then the far one through
/// `Actor02100_SetVector`. The near rotation's input is built in a block
/// borrowed from the scratch stack, and `beamPoints[0]` is reset to the muzzle
/// offset it starts from.
static __inline__ void Actor02100_UpdateVectors(Task* arg0)
{
    _Actor02100VectorScratch* scratch;
    _Actor02100Work*          work;

    work                   = arg0->work;
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
    Actor02100_SetVector(arg0);
}

/// Releases the block `Actor02100_OrientScratch` leaves on the scratch stack.
static __inline__ void Actor02100_ReleaseScratch28(void)
{
    SCRATCH_STACK_RELEASE_BLOCK(Actor02100Fn01FF0Scratch);
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
/// and returns to state 0. `Actor02100_Fn014E4` failing at any aim point drops
/// straight to state 6.
static void Actor02100_Fn01FF0(Task* arg0)
{
    Actor02100Fn01FF0Block* root;
    _Actor02100Work*        work;
    GfxCoord*               coord;
    s32                     pan0;
    s32                     pan2;
    s32                     sound2;
    u32                     random;
    s32                     packed2;
    s16                     state;

    root  = SCRATCH_STACK_RESERVE_BLOCK(Actor02100Fn01FF0Block);
    work  = arg0->work;
    state = work->step;
    coord = arg0->extra.tmd->coords;

    switch (state) {
        case ACTOR_02100_GUN_STEP_AIM:
            if (work->stepFrames != 0) {
                if (Actor02100_Fn014E4(arg0) == 0) {
                    work->step       = ACTOR_02100_GUN_STEP_RECOVER;
                    work->stepFrames = 0;
                    if (work->loopSoundKind == ACTOR_02100_LOOP_SOUND_CHARGE) {
                        SndEvt_EnqueueType7(work->loopSound, 1);
                        work->loopSoundKind = ACTOR_02100_LOOP_SOUND_NONE;
                    }
                    break;
                }

                Actor02100_OrientScratch(arg0);
                Actor02100_ReleaseScratch28();
                Actor02100_UpdateVectors(arg0);
            }

            if (work->stepFrames == 1) {
                work->loopSound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40150001;
                pan0            = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(work->loopSound, pan0, (s8)worldCoordGetOriginAudioDepth(coord));
                work->loopSoundKind = ACTOR_02100_LOOP_SOUND_CHARGE;
            }
            if (work->stepFrames >= 2) {
                Actor02100_Fn034E0(arg0);
                Actor02100_Fn02924(arg0, 0);
            }
            work->playerStrikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->enemyStrikeBody.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->playerStrikeBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work->enemyStrikeBody.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
            if (++work->stepFrames >= Actor02100_D03D88[work->weapon].bounds.field_0) {
                work->stepFrames = 0;
                work->step       = ACTOR_02100_GUN_STEP_LOCK;
            }
            break;

        case ACTOR_02100_GUN_STEP_LOCK:
            if (work->stepFrames == 1) {
                SndEvt_EnqueueType7(work->loopSound, 1);
                work->loopSoundKind = ACTOR_02100_LOOP_SOUND_NONE;
            }
            if (++work->stepFrames >= 4) {
                work->stepFrames = 0;
                Actor02100_SetVector(arg0);
                work->step = ACTOR_02100_GUN_STEP_SHOT;
            }
            break;

        case ACTOR_02100_GUN_STEP_SHOT:
            root->shortVec.vx = 0;
            root->shortVec.vy = 0;
            root->shortVec.vz = ACTOR_02100_MUZZLE_OFFSET;
            random            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            packed2           = ((random >> 16) & 0x1FF) | 0x200;
            gRandomLcgState   = random;
            Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, packed2, &root->shortVec);
            Gp_SpawnEff(EFFECT_MUZZLE_FLARE_ADDITIVE, coord, packed2, &root->shortVec);
            sound2 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4015000B;
            pan2   = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(sound2, pan2, (s8)worldCoordGetOriginAudioDepth(coord));
            work->step = ACTOR_02100_GUN_STEP_ARM;
            if (Actor02100_Fn014E4(arg0) == 0) {
                work->step       = ACTOR_02100_GUN_STEP_RECOVER;
                work->stepFrames = 0;
            } else {
                Actor02100_OrientScratch(arg0);
                Actor02100_ReleaseScratch28();
            }
            break;

        case ACTOR_02100_GUN_STEP_ARM:
            work->playerStrikeBody.key = Gp_PackPair(Actor02100_D03D64, work->weapon);
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
                Actor02100_SetVector(arg0);
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
            if (++work->stepFrames >= Actor02100_D03D88[work->weapon].bounds.field_2) {
                work->stepFrames  = 0;
                work->step        = ACTOR_02100_GUN_STEP_AIM;
                work->mode        = work->patrolRange != 0;
                work->velocity.vx = work->resumeVelocity.vx;
                work->velocity.vy = work->resumeVelocity.vy;
                work->velocity.vz = work->resumeVelocity.vz;
            }
            break;
    }

    SCRATCH_STACK_RELEASE_BLOCK(Actor02100Fn01FF0Block);
}

/// Draws one beam between the two screen points held in `_Actor02100Work`
/// (`beamScreenX`/`beamScreenY` and their depths in `beamScreenDepth`). The span is
/// normalised once, its y component negated, and the beam then emitted as eight
/// segments of rising depth; a segment nearer than 30 is dropped. Each segment
/// is a bright centre line plus two gouraud quads that fade from the beam
/// colour on that line to black at the edges, all linked into the ordering
/// table at the segment's own depth and followed by a draw-mode packet. `arg1`
/// selects the style: it picks the edge offsets out of `Actor02100_D03DD8` and
/// the colour triplet out of `Actor02100_D03D88`, and style 1 draws its centre
/// line in flat grey instead of the table colour.
static void Actor02100_Fn02924(Task* arg0, s32 arg1)
{
    Actor02100Fn02924Corners* corners;
    POLY_G4*                  quad;
    DR_TPAGE*                 mode;
    LINE_F2*                  line;
    Actor02100Fn02924Scratch* scratch;
    u8*                       head;
    u8*                       newHead;
    u_long*                   quadSlot;
    u_long*                   modeSlot;
    u_long*                   lineSlot;
    s32                       next;
    s32                       offsetX0;
    s32                       offsetX1;
    s32                       normalY;
    u8                        blue;
    s32                       offsetY0;
    s32                       offsetY1;
    s32                       depth;
    s32                       corner;
    s32                       segment;
    s32                       spanX;
    s32                       spanY;
    s32                       spanZ;
    _Actor02100Work*          work;

    head                     = SCRATCH_STACK_CURSOR(u8);
    work                     = arg0->work;
    newHead                  = head - 0x3C;
    SCRATCH_STACK_CURSOR(u8) = newHead;
    scratch                  = (Actor02100Fn02924Scratch*)newHead;

    ((Actor02100Fn02924Scratch*)(head - 0x3C))->delta.vx = work->beamScreenX[1] - work->beamScreenX[0];
    scratch->delta.vy                                    = work->beamScreenY[1] - work->beamScreenY[0];
    scratch->delta.vz                                    = 0;
    VectorNormalS((VECTOR*)newHead, &scratch->normal);
    normalY            = -scratch->normal.vy;
    scratch->normal.vy = normalY;

    spanX = work->beamScreenX[1] - work->beamScreenX[0];
    if (spanX < 0) {
        spanX += 7;
    }
    scratch->stepX = (s16)(spanX >> 3);
    spanY          = work->beamScreenY[1] - work->beamScreenY[0];
    if (spanY < 0) {
        spanY += 7;
    }
    scratch->stepY = (s16)(spanY >> 3);
    spanZ          = work->beamScreenDepth[1] - work->beamScreenDepth[0];
    if (spanZ < 0) {
        spanZ += 7;
    }
    scratch->depthStep = spanZ >> 3;
    segment            = 0;

    do {
        next           = segment + 1;
        depth          = (scratch->depthStep * next) + work->beamScreenDepth[0];
        scratch->depth = depth;
        if (depth >= 0x1E) {
            scratch->x[0] = (u16)((u16)work->beamScreenX[0] + (scratch->stepX * segment));
            scratch->x[1] = (u16)((u16)work->beamScreenX[0] + (scratch->stepX * next));
            corner        = 0;
            offsetX0 =
                (s32)((s32)(scratch->normal.vy * Actor02100_D03DD8[work->weapon].styles[arg1].first * 0x300) >>
                      0xC) /
                (s32)scratch->depth;
            scratch->x[2] = (s16)(scratch->x[0] + offsetX0);
            scratch->x[3] = (s16)(scratch->x[1] + offsetX0);
            offsetX1 =
                (s32)((s32)(scratch->normal.vy * Actor02100_D03DD8[work->weapon].styles[arg1].second * 0x300) >>
                      0xC) /
                (s32)scratch->depth;
            scratch->x[4] = (s16)(scratch->x[0] + offsetX1);
            scratch->x[5] = (s16)(scratch->x[1] + offsetX1);
            scratch->y[0] = (u16)((u16)work->beamScreenY[0] + (scratch->stepY * segment));
            scratch->y[1] = (u16)((u16)work->beamScreenY[0] + (scratch->stepY * next));
            offsetY0 =
                (s32)((s32)(scratch->normal.vx * Actor02100_D03DD8[work->weapon].styles[arg1].first * 0x300) >>
                      0xC) /
                (s32)scratch->depth;
            scratch->y[2] = (s16)(scratch->y[0] + offsetY0);
            scratch->y[3] = (s16)(scratch->y[1] + offsetY0);
            offsetY1 =
                (s32)((s32)(scratch->normal.vx * Actor02100_D03DD8[work->weapon].styles[arg1].second * 0x300) >>
                      0xC) /
                (s32)scratch->depth;
            scratch->y[4] = (s16)(scratch->y[0] + offsetY1);
            scratch->y[5] = (s16)(scratch->y[1] + offsetY1);

            do {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                setlen(quad, 8);
                setcode(quad, 0x3A);
                corners  = &Actor02100_D03E1C[corner];
                quad->x0 = (u16)scratch->x[corners->corners[0]];
                quad->y0 = (u16)scratch->y[corners->corners[0]];
                quad->x1 = (u16)scratch->x[corners->corners[1]];
                quad->y1 = (u16)scratch->y[corners->corners[1]];
                quad->x2 = (u16)scratch->x[corners->corners[2]];
                quad->y2 = (u16)scratch->y[corners->corners[2]];
                quad->x3 = (u16)scratch->x[corners->corners[3]];
                quad->y3 = (u16)scratch->y[corners->corners[3]];
                quad->r0 = (u8)Actor02100_D03D88[work->weapon].shorts[(arg1 * 3) + 2];
                quad->g0 = (u8)Actor02100_D03D88[work->weapon].shorts[(arg1 * 3) + 3];
                quad->b0 = (u8)Actor02100_D03D88[work->weapon].shorts[(arg1 * 3) + 4];
                quad->r1 = (u8)Actor02100_D03D88[work->weapon].shorts[(arg1 * 3) + 2];
                quad->g1 = (u8)Actor02100_D03D88[work->weapon].shorts[(arg1 * 3) + 3];
                blue     = (u8)Actor02100_D03D88[work->weapon].shorts[(arg1 * 3) + 4];
                quad->r2 = 0;
                quad->g2 = 0;
                quad->b2 = 0;
                quad->r3 = 0;
                quad->g3 = 0;
                quad->b3 = 0;
                quad->b1 = blue;
                corner  += 1;
                setaddr(quad,
                        getaddr((((u32)(scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK) +
                                (u32)gGpuCurrentOt));
                quadSlot = (u_long*)((((u32)(scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK) +
                                     (u32)gGpuCurrentOt);
                setaddr(quadSlot, quad);
            } while (corner < 2);

            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setlen(line, 3);
            setcode(line, 0x42);
            line->x0 = (u16)scratch->x[0];
            line->y0 = (u16)scratch->y[0];
            line->x1 = (u16)scratch->x[1];
            line->y1 = (u16)scratch->y[1];
            if (arg1 == 1) {
                line->r0 = 0x80U;
                line->g0 = 0x80U;
                line->b0 = 0x80U;
            } else {
                line->r0 = (u8)Actor02100_D03D88[work->weapon].shorts[(arg1 * 3) + 2];
                line->g0 = (u8)Actor02100_D03D88[work->weapon].shorts[(arg1 * 3) + 3];
                line->b0 = (u8)Actor02100_D03D88[work->weapon].shorts[(arg1 * 3) + 4];
            }
            setaddr(line,
                    getaddr((((u32)(scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK) + (u32)gGpuCurrentOt));
            mode           = gGpuPrimCursor;
            lineSlot       = (u_long*)((((u32)(scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK) + (u32)gGpuCurrentOt);
            gGpuPrimCursor = mode + 1;
            setaddr(lineSlot, line);
            setlen(mode, 1);
            mode->code[0] = 0xE1000620;
            setaddr(mode,
                    getaddr((((u32)(scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK) + (u32)gGpuCurrentOt));
            modeSlot = (u_long*)((((u32)(scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK) + (u32)gGpuCurrentOt);
            setaddr(modeSlot, mode);
        }
        segment += 1;
    } while (segment < 8);

    SCRATCH_STACK_RELEASE_BYTES(0x3C);
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
/// task over to `Actor02100_Fn035D4` - once `gSceneCombatState.generatorDeathStarted` reports the kill.
static void Actor02100_Fn031C4(Enemy* arg0, Task* arg1)
{
    TmdObject*       obj;
    _Actor02100Work* work;
    GfxCoord*        coord;
    s32              mode;
    s32              one;

    obj   = arg1->extra.tmd;
    mode  = gSceneCombatState.actorControl;
    work  = arg1->work;
    coord = obj->coords;
    one   = 1;
    if (mode == one) {
        goto case1;
    }
    if (mode >= 2) {
        goto ge2;
    }
    if (mode == 0) {
        goto case0;
    }
    goto body;
ge2:
    if (mode == 2) {
        goto case2;
    }
    goto body;
case0:
    obj->flags                   = 0;
    arg0->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
    goto body;
case1:
    Actor02100_Fn03488(arg1);
    return;
case2:
    obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags = one;
    return;
body:
    Actor02100_Fn004C4(arg1);
    coord->coord.t[0]  += work->velocity.vx;
    coord->coord.t[1]  += work->velocity.vy;
    coord->coord.t[2]  += work->velocity.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
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
            Actor02100_Fn00ADC(arg0);
        case ACTOR_02100_MODE_WATCH:
            if (GameFlag_GetNibble(GAME_FLAG_SHELTER_WATCHERS_DISABLED) == 0) {
                Actor02100_Fn00DCC(arg0);
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

static s32 Actor02100_Fn0337C(SVECTOR* arg0, SVECTOR* arg1)
{
    VECTOR*                 vec;
    WorldCollisionOccluder* node;
    s32                     ret;

    ret     = 0;
    node    = D_80115550;
    vec     = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    vec->vx = arg1->vx - arg0->vx;
    vec->vy = arg1->vy - arg0->vy;
    vec->vz = arg1->vz - arg0->vz;
    VectorNormal(vec, vec);
    for (; node != NULL; node = node->next) {
        if (node->flags & WORLD_COLLISION_OCCLUDER_ENABLED) {
            ret = func_800DFCCC(node, arg0, arg1, vec);
            if (ret == 1) {
                break;
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
    return ret;
}

static void Actor02100_Fn03488(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

/// Projects the two `beamPoints` through the actor's own coordinate, storing
/// screen x/y in `beamScreenX`/`beamScreenY` and depth in `beamScreenDepth`.
static void Actor02100_Fn034E0(Task* arg0)
{
    Actor02100Screen* scratch;
    GfxCoord*         coord;
    _Actor02100Work*  work;
    s32               i;
    u8*               head;
    s32               y;

    head                     = SCRATCH_STACK_CURSOR(u8);
    work                     = arg0->work;
    SCRATCH_STACK_CURSOR(u8) = head - 8;
    scratch                  = SCRATCH_STACK_CURSOR(Actor02100Screen);
    coord                    = arg0->extra.tmd->coords;
    for (i = 0; i < 2; i++) {
        gte_SetRotMatrix(&coord->workm);
        gte_SetTransMatrix(&coord->workm);
        gte_ldv0(&work->beamPoints[i]);
        gte_rtps();
        gte_stsxy(&scratch->sxy);
        gte_stszotz(&scratch->sz);
        work->beamScreenX[i]     = scratch->sxy.vx;
        y                        = scratch->sxy.vy;
        work->beamScreenY[i]     = y;
        work->beamScreenDepth[i] = scratch->sz;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}

static void Actor02100_Fn035D4(Enemy* arg0, Task* arg1)
{
    _Actor02100Work* work;
    s16              state;
    u16              timer;

    work  = arg1->work;
    state = work->step;
    if (state == ACTOR_02100_DEATH_STEP_RELEASE) {
        goto case0;
    }
    if (state == ACTOR_02100_DEATH_STEP_WAIT) {
        goto case1;
    }
    goto epilogue;
case0:
    arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    worldTargetUnlinkNode(&arg0->node);
    Gp_UnlinkObj(&work->hitBody);
    Gp_UnlinkObj(&work->playerStrikeBody);
    Gp_UnlinkObj(&work->enemyStrikeBody);
    arg0->recs = 0;
    Gp_ReleaseStateF0Add(arg1, 0x15);
    work->step       = ACTOR_02100_DEATH_STEP_WAIT;
    work->stepFrames = 0x3C;
    if (work->loopSoundKind != ACTOR_02100_LOOP_SOUND_NONE) {
        SndEvt_EnqueueType7(work->loopSound, 1);
    }
    goto epilogue;
case1:
    timer = work->stepFrames;
    timer--;
    work->stepFrames = timer;
    if ((s16)timer > 0) {
        goto epilogue;
    }
    enemyDestroy(arg0, arg1);
epilogue:
    return;
}
