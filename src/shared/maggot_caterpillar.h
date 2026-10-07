/* The code the Maggot (actor_102600) and the Caterpillar (actor_105500) share;
 * the code sees the work block as MaggotCaterpillarWork. It waits until the player
 * comes near, then either drops from above on a line, drawn fading grey, or
 * makes a scripted entrance. After that it turns toward the player and
 * chooses an attack. Close up it pounces forward, stepping along a per-frame
 * stride table; if the pounce hits something it bounces back. If it is not
 * burning and the player is not blinded, it instead sprays a burst of short-
 * lived puff projectiles, each a growing textured sprite. A type-7 hit sets it
 * burning: it gives off an effect from two body nodes, plays a periodic sound
 * and its pounce does elemental damage. Timed status hits damage it, a type-2
 * hit stuns it, and when it dies it collapses, squashes flat and fades,
 * leaving a husk model lit with the room's texture page.
 *
 * Each package states which enemy it builds before including this header:
 * MAGGOT_CATERPILLAR_KIND is MAGGOT (actor_02600) or CATERPILLAR
 * (actor_05500); the parameters below follow from it.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_MAGGOT_CATERPILLAR_H
#define SRC_SHARED_MAGGOT_CATERPILLAR_H

#define MAGGOT      1
#define CATERPILLAR 2
#ifndef MAGGOT_CATERPILLAR_KIND
#error "define MAGGOT_CATERPILLAR_KIND (MAGGOT or CATERPILLAR) before including maggot_caterpillar.h"
#endif

/* Per kind: the type id (in its collision keys, 0x30000 | id); the flag the
 * spawn stores in `isCaterpillar`, which the dying sequence reads back for the
 * id and which keeps the Caterpillar from spraying; and the player distances
 * at which it wakes, drops from its ambush, and pounces. */
#if MAGGOT_CATERPILLAR_KIND == MAGGOT
#define MAGGOT_CATERPILLAR_ID             0x1A
#define MAGGOT_CATERPILLAR_IS_CATERPILLAR 0
#define MAGGOT_CATERPILLAR_WAKE_RANGE     0x9C4
#define MAGGOT_CATERPILLAR_AMBUSH_RANGE   0x7D0
#define MAGGOT_CATERPILLAR_POUNCE_RANGE   0x9C4
#else
#define MAGGOT_CATERPILLAR_ID             0x37
#define MAGGOT_CATERPILLAR_IS_CATERPILLAR 1
#define MAGGOT_CATERPILLAR_WAKE_RANGE     0x7D0
#define MAGGOT_CATERPILLAR_AMBUSH_RANGE   0x5DC
#define MAGGOT_CATERPILLAR_POUNCE_RANGE   0x8FC
#endif

#include "types.h"

#include "actors/actor.h"

#include "main/task_types.h"

/// Behaviour the per-frame tick runs, held in `MaggotCaterpillarWork::behaviour`.
enum {
    MAGGOT_CATERPILLAR_BEHAVIOUR_WAIT     = 0, // lies still until the player comes near, then wakes and crawls off
    MAGGOT_CATERPILLAR_BEHAVIOUR_AIM      = 1, // holds its facing and pounces once the player is in range in front of it
    MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH   = 2, // hangs on its thread, drops, lands and gets up
    MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM     = 3, // idles, crawls round to face the player and chooses an attack
    MAGGOT_CATERPILLAR_BEHAVIOUR_SPRAY    = 4, // sprays puff projectiles
    MAGGOT_CATERPILLAR_BEHAVIOUR_POUNCE   = 5, // leaps forward to bite, rebounding off what blocks it
    MAGGOT_CATERPILLAR_BEHAVIOUR_HURT     = 6, // flinches from a hit
    MAGGOT_CATERPILLAR_BEHAVIOUR_STUN     = 7, // held until the enemy's stun runs out
    MAGGOT_CATERPILLAR_BEHAVIOUR_ENTRANCE = 8, // hidden until the room releases it, then leaps or drops in
    MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD     = 9  // runs nothing: the task's dying state has taken over
};

/// Clips of `MaggotCaterpillarWork::animId`, indexing `gMaggotCaterpillarAnimSets`.
///
/// Each is named for the behaviour that plays it. Clip 8 has no set.
enum {
    MAGGOT_CATERPILLAR_ANIM_IDLE    = 1,
    MAGGOT_CATERPILLAR_ANIM_CRAWL   = 2, // the roam's turn toward the player; it moves from frame 0xB and loops back there at 0x29
    MAGGOT_CATERPILLAR_ANIM_SPRAY   = 3,
    MAGGOT_CATERPILLAR_ANIM_POUNCE  = 4, // also the entrance's leap
    MAGGOT_CATERPILLAR_ANIM_REBOUND = 5,
    MAGGOT_CATERPILLAR_ANIM_HANG    = 6,
    MAGGOT_CATERPILLAR_ANIM_DROP    = 7,
    MAGGOT_CATERPILLAR_ANIM_LAND    = 9,
    MAGGOT_CATERPILLAR_ANIM_GET_UP  = 10,
    MAGGOT_CATERPILLAR_ANIM_HURT    = 11, // also the collapse of the dying sequence
    MAGGOT_CATERPILLAR_ANIM_DORMANT = 12,
    MAGGOT_CATERPILLAR_ANIM_WAKE    = 13,
    MAGGOT_CATERPILLAR_ANIM_STUN    = 14
};

/// How a hit is taken, held in `MaggotCaterpillarWork::reactionMode`.
enum {
    MAGGOT_CATERPILLAR_REACTION_NORMAL    = 0, // a hit interrupts the behaviour
    MAGGOT_CATERPILLAR_REACTION_COMMITTED = 1, // hanging, dropping or in the air of a leap: a hit does its damage and interrupts nothing, and death waits for the landing
    MAGGOT_CATERPILLAR_REACTION_REBOUND   = 2  // the rebound clip has turned the model round: a hit that interrupts it, or the clip's end, turns the root half a turn to match
};

/// Work block of the Maggot and the Caterpillar, allocated by the spawn and
/// kept at `Task::work`.
///
/// It holds the model's animation playback, the four collision spheres with
/// their contact tables, and the state the per-frame tick steps. The puff
/// projectile has a work block of its own, `MaggotCaterpillarPuffWork`.
typedef struct {
    ActorAnimRig8         rig;               // playback of the model's parts; slots 1 to 7 are driven, all on `animId`
    MATRIX                colorMtx;          // storage for the model's `TmdObject::colorMtx`
    MATRIX                lightMtx;          // storage for the model's `TmdObject::lightMtx`
    WorldCollisionBody    gridBody;          // sphere 0x12C above the root that only the room grid tests; its contacts push the root
    WorldCollisionContact gridContacts[4];   // contact table of `gridBody`
    WorldCollisionBody    body;              // sphere on part 1 that takes the hits and is pushed out of other bodies
    WorldCollisionContact bodyContacts[2];   // contact table of `body`, also the enemy's hit records
    WorldCollisionBody    attackBody;        // sphere carrying the attack's key: on part 4 for the bite, enabled over the leap; on the root, larger, through an ambush drop
    WorldCollisionContact attackContacts[1]; // contact table of `attackBody`; a body or a wall in it sets `blocked`
    WorldCollisionBody    flameBody;         // sphere on part 4 carrying the flame's key, enabled once it burns
    WorldCollisionContact flameContacts[1];  // contact table of `flameBody`; never read
    EffectSpawnArg        effectArg;         // argument record of the hit and burn effects, naming the root
    VECTOR3               prevPos;           // root translation before the last move step; restored when the room grid pushes two ways, and what a drop measures its fall from
    byte                  field_368[4];      // never accessed
    TaskDesc*             taskTable;         // the package's task descriptors: the body's, then the puff's that the spray spawns
    MATRIX                baseMatrix;        // copy of the root's matrix: its world matrix through the ambush, the frame the thread is drawn in; its local one from death, which the squash scales
    s16                   hitCooldown;       // ticks before another hit is taken; set from the hit's id parameter 2
    s16                   animId;            // `MAGGOT_CATERPILLAR_ANIM_*` requested of `rig`
    s16                   appliedAnim;       // clip `rig` was last started on; a different `animId` restarts the slots
    s16                   animFrame;         // ticks since the clip was started, which time every behaviour
    s16                   forwardSpeed;      // world units the root moves along its facing each tick
    s16                   behaviour;         // `MAGGOT_CATERPILLAR_BEHAVIOUR_*` the tick runs
    s16                   step;              // stage within the behaviour or within the dying sequence, from 0
    s16                   stateCounter;      // ticks left of the idle and of the entrance's delay; distance left of the crawl; ticks elapsed of each dying stage
    union {
        s16 threadRise;                      // ambush: Y offset from the thread's lower end to its upper end: 0 hanging, less by each tick's fall in the drop
        s16 squashScale;                     // dying: vertical scale of the root, 0x1000 shrinking to 0x200
    } vertical;                              // one word the ambush and the dying sequence each use their own way
    s16  yaw;                                // heading of the root as last read back from its matrix, 4096 a turn
    s16  targetYaw;                          // heading the turn step turns toward
    s16  turnRate;                           // most the turn step turns in a tick; 0 leaves the turn step out
    s16  fallSpeed;                          // added to the root's height each tick: 0x80 on the ground, 0 hanging, the drop's speed in a drop
    s16  field_3AA;                          // set to 1 as a rebound ends; never read
    u16  puffCount;                          // puffs the current spray has spawned; each puff copies it at setup
    byte field_3AE[2];                       // never accessed
    s16  burning;                            // 1 from the first type-7 hit: runs the burn step, strengthens the bite and rules out the spray
    s16  burnFrame;                          // tick of the burn's 0x50-tick cycle; a contact carrying the flame's key is taken only at 0
    s16  burnEffectTimer;                    // ticks since the last burn effect, one every 0xC
    s16  burnEffectSide;                     // part the next burn effect hangs off (0 part 3, 1 part 5)
    byte field_3B8[2];                       // never accessed
    s16  burst;                              // 1 on a tick a type-4 or type-6 hit landed outside `MAGGOT_CATERPILLAR_REACTION_COMMITTED`; when that hit kills, the dying sequence counts it to 2 and swaps the model for the husk
    s16  threadFade;                         // brightness of the thread out of 45, counted down from the landing and held at 1; 0 draws it at full brightness
    s16  burnSoundTimer;                     // ticks before the burn sound plays again, every 0x24
    s16  isCaterpillar;                      // `MAGGOT_CATERPILLAR_IS_CATERPILLAR` of the package that built it; a Caterpillar never sprays
    s16  entranceKind;                       // scripted entrance (0 leaps in, 1 drops in)
    s16  entranceSlot;                       // the placement's `variant`: row of the entrance's delay, speed, spot and yaw tables
    s16  ambushFollower;                     // 1 when the ambush leaves the trigger to the others: it drops only once one of them has sprung
    s16  reactionMode;                       // `MAGGOT_CATERPILLAR_REACTION_*`
    s16  midLeap;                            // 1 over the leap frames of a pounce, when a hit does double damage
    s16  landed;                             // 1 on a tick the room grid touched it during the ambush
    s16  blocked;                            // 1 on a tick `attackBody` met a body or a wall
    s16  struck;                             // 1 on a tick a hit other than the flame's landed: it wakes the wait, springs the ambush and turns a leap into a rebound
    s16  stunned;                            // 1 from the buildup that stuns it until the stun runs out; a flinch in between returns to the stun
} MaggotCaterpillarWork;
STATIC_ASSERT_SIZEOF(MaggotCaterpillarWork, 0x3D4);

/// Work block of a puff projectile, allocated by the puff's setup and kept at
/// its `Task::work`.
///
/// A puff is a child task of the enemy that sprayed it. It flies along its
/// own facing, slowing as it goes, and ends after 0xF ticks or when it meets
/// the room's collision grid.
typedef struct {
    WorldCollisionBody    body;         // sphere at the puff's origin carrying the spray attack's key; its grid and pair tests are enabled on every fourth tick only
    WorldCollisionContact contacts[1];  // contact table of `body`: a room-grid contact ends the puff, any other is discarded
    s16                   age;          // ticks flown, from 0: picks the sprite's radius each tick and its cell every other tick, and ends the puff at 0xF
    s16                   forwardSpeed; // units it moves along its facing each tick: 0xC0 at setup, less a random 0 to 0x1F each tick, held at 0
    u16                   sprayOrdinal; // the spraying enemy's `MaggotCaterpillarWork::puffCount` as the puff was set up; never read
} MaggotCaterpillarPuffWork;
STATIC_ASSERT_SIZEOF(MaggotCaterpillarPuffWork, 0x40);

/// Scratch-stack block of the thread drawer, in which each end of the thread
/// is projected in turn.
///
/// The drawer reserves one block, stages an end in `position`, projects it
/// with one perspective transform through the work block's `baseMatrix` and
/// gives up when the end lies nearer than depth 30. The upper end goes first
/// and its screen position is copied out before the lower end takes its
/// place, so the block is left holding the lower end, whose depth sorts the
/// line. The block is released before the drawer returns, on each of its
/// paths.
///
/// The drawer never touches the leading bytes, so the size is that of the
/// reservation alone and what they were laid out to hold is unproven.
typedef struct {
    byte    field_0[0x10]; // reserved with the block and never accessed; role unproven
    SVECTOR position;      // end being projected, in the frame of `baseMatrix` and on its Y axis: the upper end, `vertical.threadRise` from the lower, then the lower end at -0x352; `pad` is never written
    s32     screenPos;     // projected screen position of that end, X in bits 0..15 and Y in bits 16..31
    s32     depth;         // screen Z / 4 of that end: under 30 drops the thread; the lower end's is the ordering-table depth of the line
} MaggotCaterpillarLineScratch;
STATIC_ASSERT_SIZEOF(MaggotCaterpillarLineScratch, 0x20);

/// Scratch-stack block of the per-frame contact pass.
///
/// The pass reserves one block and has the push-back of the room's collision
/// grid resolved from the grid sphere's contact records into `delta`, adding
/// the whole units of that correction to the root. It then walks the hit
/// sphere's records, reusing `delta` for each. A damaging contact, kind
/// 0x20000, takes the offset to the attacking player, whose length is the
/// range the damage is worked out for. A contact with a player's body, kind
/// 0x10000, or an enemy's, kind 0x30000, takes the offset from that body's
/// centre; the overlap is the record's summed radii less that offset's
/// length. The deepest overlap leaves its direction in `normal` and
/// `pushDirection`, and once the walk is over the root is moved that deep
/// along `pushDirection` on X and Z. The block is released before the pass
/// returns.
///
/// It is `ActorOverlapPushScratch` of `include/actors/actor.h` with one
/// short vector after it, which the pass fills in for two calls that take
/// one.
typedef struct {
    WorldCollisionDelta delta;         // correction resolved from the grid contacts, in signed 16.16 units; then, in whole world units, the offset to the attacker or from the centre of the body being tested
    VECTOR              normal;        // `delta` of the deepest overlap met so far, normalised: away from that body, 4096 = 1.0
    VECTOR              pushDirection; // `normal` turned into the frame of the collision grid's coordinate; the root is pushed along its X and Z
    SVECTOR             shortVector;   // filled in for the call that takes it: the angles of the half turn that ends a rebound, 4096 a turn, then the hit effect's offset from part 1, -0xC8 on Y; `pad` is never written
} MaggotCaterpillarContactsScratch;
STATIC_ASSERT_SIZEOF(MaggotCaterpillarContactsScratch, 0x38);

/// Body task phase entered when an interruptible reaction exhausts HP.
enum { MAGGOT_CATERPILLAR_TASK_DYING = 2 };

/// Puff task dispatch indices; the destroy handler releases its work and body.
enum {
    MAGGOT_CATERPILLAR_PUFF_TASK_SETUP   = 0,
    MAGGOT_CATERPILLAR_PUFF_TASK_FLY     = 1,
    MAGGOT_CATERPILLAR_PUFF_TASK_DESTROY = 2
};

/// Lifetime and initial speed of one puff, in ticks and coordinate units per tick.
enum {
    MAGGOT_CATERPILLAR_PUFF_LIFETIME      = 15,
    MAGGOT_CATERPILLAR_PUFF_INITIAL_SPEED = 0xC0
};

/// Thread brightness denominator and countdown started at the landing clip.
enum { MAGGOT_CATERPILLAR_THREAD_FADE_FRAMES = 45 };

/// Attack-table entries used by the bite, the puff and the ambush drop.
enum {
    MAGGOT_CATERPILLAR_ATTACK_BITE_LATE          = 0,
    MAGGOT_CATERPILLAR_ATTACK_BITE_EARLY         = 1,
    MAGGOT_CATERPILLAR_ATTACK_PUFF               = 2,
    MAGGOT_CATERPILLAR_ATTACK_BURNING_BITE_LATE  = 3,
    MAGGOT_CATERPILLAR_ATTACK_BURNING_BITE_EARLY = 4,
    MAGGOT_CATERPILLAR_ATTACK_DROP               = 5
};

void        maggotCaterpillarSprayState(Task* arg0);
static void _maggotCaterpillarPounceState(Task* actor);
static void _maggotCaterpillarHurtState(Task* actor);
void        maggotCaterpillarEntranceState(Task* arg0);
void        maggotCaterpillarBurnStep(Task* arg0);
void        maggotCaterpillarTurnStep(Task* arg0);
void        maggotCaterpillarDyingState(Enemy* arg0, Task* arg1);
static void _maggotCaterpillarPuffTick(Enemy* enemy, Task* task);
static void _maggotCaterpillarDrawPuff(Task* actor, s32 frame);
static void _maggotCaterpillarDrawThread(Task* actor);
void        maggotCaterpillarTick(Enemy* arg0, Task* arg1);
static void _maggotCaterpillarApplyStatus(Task* actor);
static void _maggotCaterpillarStunState(Task* actor);
static void _maggotCaterpillarMoveStep(Task* actor);
void        maggotCaterpillarDrawShadow(Task* arg0);
static void _maggotCaterpillarSquash(Task* actor);
void        maggotCaterpillarSpawnHusk(Task* actor);
static void _maggotCaterpillarShrinkNode2(Task* actor);
static void _maggotCaterpillarPuffSetup(Enemy* enemy, Task* task);

void        maggotCaterpillarWaitState(Task* arg0);
static void _maggotCaterpillarAimState(Task* actor);
static void _maggotCaterpillarAmbushState(Task* actor);
static void _maggotCaterpillarRoamState(Task* actor);
void        maggotCaterpillarSpawn(Enemy* ctx, Task* actor);
void        maggotCaterpillarResolveContacts(Task* arg0);
static void _maggotCaterpillarPuffTask(Task* task);
void        maggotCaterpillarTask(Task* arg0);
void        maggotCaterpillarRunBehaviour(Task* arg0);
void        maggotCaterpillarTickAnim(Task* arg0);
void        maggotCaterpillarUpdateColor(Task* arg0);

static inline void _maggotCaterpillarTickAnimInline(Task* task);

/// Stores the yaw that `coord`'s frame faces in `work->yaw`, then
/// rebuilds the frame's rotation as a level turn half a revolution away from
/// it, with `rot` holding the angles.
#define MAGGOT_CATERPILLAR_TURN_AROUND(work, coord, rot)                              \
    do {                                                                              \
        (work)->yaw = ratan2((coord)->coord.m[0][2], (coord)->coord.m[2][2]) & 0xFFF; \
        (rot)->vx   = 0;                                                              \
        (rot)->vy   = (u16)(work)->yaw + 0x800;                                       \
        (rot)->vz   = 0;                                                              \
        RotMatrix((rot), &(coord)->coord);                                            \
    } while (0)

/// How far the origin of `coord`'s frame lies inside contact `rec`, clamped at
/// zero, into `out`. `delta` receives the offset from the contact point to
/// the origin.
#define MAGGOT_CATERPILLAR_CONTACT_OVERLAP(out, coord, rec, delta)                                 \
    do {                                                                                           \
        s32 offX;                                                                                  \
        s32 offY;                                                                                  \
        s32 offZ;                                                                                  \
        s32 clamped;                                                                               \
        offX              = (coord)->workm.t[0] - (rec).point.vx;                                  \
        (delta).vector.vx = offX;                                                                  \
        offY              = (coord)->workm.t[1] - (rec).point.vy;                                  \
        (delta).vector.vy = offY;                                                                  \
        offZ              = (coord)->workm.t[2] - (rec).point.vz;                                  \
        (delta).vector.vz = offZ;                                                                  \
        (out)             = (rec).distance - SquareRoot0(offX * offX + offY * offY + offZ * offZ); \
        clamped           = (out);                                                                 \
        if ((out) <= 0) {                                                                          \
            clamped = 0;                                                                           \
        }                                                                                          \
        (out) = clamped;                                                                           \
    } while (0)

/// Normalises `delta` into `unit` and expresses the direction in the frame of
/// the collision grid, into `out`.
#define MAGGOT_CATERPILLAR_GRID_DIRECTION(delta, unit, out)                      \
    do {                                                                         \
        VectorNormal(&(delta)->vector, (unit));                                  \
        ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, (unit), (out)); \
    } while (0)

/// Sets `work->blocked` when contact `rec` is a body, or a face of the
/// collision grid whose normal has no vertical component.
#define MAGGOT_CATERPILLAR_NOTE_BLOCKING_CONTACT(work, rec)                                         \
    do {                                                                                            \
        if ((((rec).key.value & 0xFFFF0000) == 0x10000) ||                                          \
            ((((rec).key.value & 0xFFFF0000) == 0x100000) && ((rec).response.direction.vy == 0))) { \
            (work)->blocked = 1;                                                                    \
        }                                                                                           \
    } while (0)

#endif /* SRC_SHARED_MAGGOT_CATERPILLAR_H */
