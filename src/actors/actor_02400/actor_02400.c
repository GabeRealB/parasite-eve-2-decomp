#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/player_state.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
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

#include "rooms/room_visual_effects.h"

#include "overlay.h"
#include "../../shared/fireball.h"

/// Behaviours of the main body, in `_Actor02400Work::mode`.
enum {
    ACTOR_02400_MODE_DORMANT = 0, // Small and still until the player comes near or the group is woken
    ACTOR_02400_MODE_CRAWL   = 1, // Awake: alternately turns to the player and surges forward
    ACTOR_02400_MODE_STRIKE  = 2, // The arm lashes out and back after the attack body touched the player
    ACTOR_02400_MODE_CAST    = 3, // Swells, aims, spawns the projectile and shrinks back
    ACTOR_02400_MODE_HURT    = 4, // Shakes for 16 frames after a hit
    ACTOR_02400_MODE_STUNNED = 5, // Shrinks and stays put for 360 frames once the stagger damage adds up
};

/// Phases of `ACTOR_02400_MODE_CRAWL`, in `_Actor02400Work::phase`.
enum {
    ACTOR_02400_CRAWL_PHASE_TURN  = 0, // Standing tall, turning towards the player at the slow speed
    ACTOR_02400_CRAWL_PHASE_SURGE = 1, // Flattened, moving straight ahead at the fast speed
    ACTOR_02400_CRAWL_PHASE_WAKE  = 2, // Swelling in pulses on leaving `ACTOR_02400_MODE_DORMANT`
};

/// Phases of `ACTOR_02400_MODE_STRIKE`, in `_Actor02400Work::phase`.
enum {
    ACTOR_02400_STRIKE_PHASE_EXTEND  = 0, // The arm slides out
    ACTOR_02400_STRIKE_PHASE_RETRACT = 1, // The arm slides back
};

/// Phases of `ACTOR_02400_MODE_CAST`, in `_Actor02400Work::phase`.
enum {
    ACTOR_02400_CAST_PHASE_SWELL   = 0, // Swelling in pulses to `ACTOR_02400_SCALE_SWOLLEN`
    ACTOR_02400_CAST_PHASE_AIM     = 1, // Swollen, turning towards the player for the variant's wait
    ACTOR_02400_CAST_PHASE_RELEASE = 2, // Spawns the projectile and lets the charge effect go
    ACTOR_02400_CAST_PHASE_SHRINK  = 3, // Shrinking back to the crawling shape
};

/// Phases of the main body's death handler, in `_Actor02400Work::phase`.
///
/// Every hit that can kill leaves `phase` at 0, so the handler starts at
/// `BEGIN` whatever the body was doing.
enum {
    ACTOR_02400_DEATH_PHASE_BEGIN   = 0, // Unlinks the body and cancels the charge effect
    ACTOR_02400_DEATH_PHASE_SQUASH  = 1, // Flattens the model for 60 frames
    ACTOR_02400_DEATH_PHASE_DESTROY = 2, // Destroys the enemy
};

/// Landmarks of `_Actor02400Work::scale`, where `ONE` is the model's own size.
enum {
    ACTOR_02400_SCALE_FLAT    = 0x600,  // Every axis while dormant; afterwards the height of the flattened body
    ACTOR_02400_SCALE_SWOLLEN = 0x1C00, // Height that ends a swell
};

/// Fixed radius and compensated height of the hit sphere, in coordinate units.
enum { ACTOR_02400_BODY_RADIUS = 200 };

/// Work block of the main body, the enemy drawn with `_gActor02400AmoebaBody`.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work` for the
/// task's life; the model object borrows `color` and `light` for as long.
///
/// The model has four parts in one chain: the root, the trunk above it, and a
/// two-part arm pointing forward along Z. Each frame the root coordinate's
/// rotation is rebuilt from `yaw`, stepped forward by `speed` and then scaled
/// per axis by `scale`, so the body changes shape without its parts being
/// animated.
///
/// The body crawls at the player. When `attackBody`, on the tip of the arm,
/// touches the player it takes MP, and the body then swells and spawns a
/// projectile. `variant` picks one of two parameter sets from the placement:
/// hit points, MP taken, attack keys, the wait before the projectile and the
/// model's palette row.
///
/// No code reads or writes `prevPos.pad` or `scale.pad`.
typedef struct {
    MATRIX                color;             // Colour matrix the model object is lit with
    MATRIX                light;             // Light matrix the model object is lit with
    WorldCollisionBody    body;              // Sphere of radius 200 held 200 above the root; takes the hits and is pushed out of the room grid and other bodies
    WorldCollisionContact bodyContacts[4];   // Contacts of `body`; also the enemy's hit records
    WorldCollisionBody    attackBody;        // Sphere of radius 100 ahead of the arm's tip carrying the variant's attack key; disabled from a touch until the body crawls again
    WorldCollisionContact attackContacts[1]; // The one contact of `attackBody`; any record there is a touch
    EffectSpawnArg        effectArg;         // Argument record of the effects a hit spawns, hung off the trunk
    MATRIX                baseMatrix;        // Root coordinate's local matrix that `scale` is applied to: this frame's unscaled one, or the last scaled one while dying
    SVECTOR               prevPos;           // Root coordinate's position before this frame's step; restored when contacts push in opposing directions
    SVECTOR               scale;             // Per-axis scale of the root coordinate, 4096 = 1.0
    EffectWork*           chargeEffect;      // Glow disc held from a touch until the cast releases it or a hit cancels it, adopted as a child task; NULL otherwise
    s16                   armOut;            // Nonzero while the arm's two parts slide out along Z; they slide back while it is 0
    s16                   hitCooldown;       // Frames left during which hits are ignored, set by a hit whose attack carries one
    s16                   speed;             // Distance moved along the root's Z axis each frame; eased towards `targetSpeed` while crawling
    s16                   turnRate;          // Most `yaw` may change in a frame (0 holds the heading)
    s16                   mode;              // Current behaviour, an `ACTOR_02400_MODE_` value
    s16                   phase;             // Step within `mode`, or within the death handler; see the phase enumerations
    s16                   counter;           // Counter of the current mode or phase: frames left of a wait, frames elapsed, or a swell's pulse in -0x100..0x100
    s16                   swingDown;         // Direction of the running oscillation, the height's bob or a swell's pulse (0 rising, 1 falling)
    s16                   yaw;               // Root coordinate's heading about Y, 4096 per turn, read back from its matrix each frame
    u16                   targetYaw;         // Heading `yaw` turns towards, in 0..0xFFF
    s16                   targetSpeed;       // Speed `speed` eases towards while crawling
    s16                   idleSoundTimer;    // Frames since the idle sound last played; it plays every 25
    s16                   attackLanded;      // Set when `attackBody` touched the player; cleared when the strike it starts hands over to the cast
    s16                   variant;           // Parameter set, the low bit of the placement's mode (0 or 1)
    s16                   staggerDamage;     // Damage rolled by hits that take no HP; reaching 20 stuns the body
    s16                   staggerWindow;     // Frames left before `staggerDamage` is forgotten, 60 from each such hit (0 not counting)
} _Actor02400Work;
STATIC_ASSERT_SIZEOF(_Actor02400Work, 0x154);

/// Steps of the fireball's teardown state, in `_Actor02400FireballWork::teardownStep`.
enum {
    ACTOR_02400_FIREBALL_TEARDOWN_UNLINK = 0, // Unlinks the three bodies and arms the wait
    ACTOR_02400_FIREBALL_TEARDOWN_WAIT   = 1, // Counts `timer` down, then destroys the enemy
};

/// Work block of the fireball the main body spawns at the end of a cast.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. The
/// fireball has no model: its task carries one coordinate, which starts above
/// the parent with the parent's rotation, is moved each frame along
/// `direction` and is where the glow is drawn and all three bodies sit.
///
/// Two spheres at the coordinate's origin share one contact: one carries the
/// variant's fireball attack to the player, the other strikes the room's
/// enemies. A thin capsule trailing the fireball finds the room surface it
/// has flown into. The flight ends, and the burst effect is spawned, on a
/// contact of either sphere, on a surface that blocks probes, or when `timer`
/// runs out.
///
/// No code reads or writes `direction.pad`; that the four halfwords are one
/// `SVECTOR` rather than three components and an unused one is unproven.
typedef struct {
    WorldCollisionBody    playerStrikeBody;  // Sphere of radius 200 whose key carries the variant's fireball entry of the package's `DamageAttack` table to the player
    WorldCollisionBody    enemyStrikeBody;   // Sphere of the same size on the list enemy bodies are tested against; its key, 0x22D2D or 0x22E2E by variant, is of the category enemies take damage from
    WorldCollisionContact strikeContacts[1]; // Contact table both strike spheres share; an occupied entry ends the flight
    WorldCollisionBody    wallBody;          // Keyless capsule body tested against the room grid, clipped at its first contact
    WorldCollisionCapsule wallCapsule;       // Its shape: radius 1, from the coordinate's origin to 210 down its -Z
    WorldCollisionContact wallContacts[1];   // The surface the capsule met; cleared each frame once read, since one that lets probes through does not stop the fireball
    SVECTOR               direction;         // The parent's Z axis at spawn, 4096 per unit: the heading flown along on X and Z at 200 units a frame; `vy` is stored and never read
    s16                   timer;             // Frames left: of the flight, from 90, and then of the teardown's wait, from 60
    s16                   teardownStep;      // Step of the teardown state, an `ACTOR_02400_FIREBALL_TEARDOWN_` value
} _Actor02400FireballWork;
STATIC_ASSERT_SIZEOF(_Actor02400FireballWork, 0xB4);

/// Scratch-stack block of the per-frame rescale of the main body's root.
///
/// One block serves one rescale and is released before the call returns. It
/// is the family's rescale block followed by the root's translation, which is
/// saved before the scale is multiplied into the coordinate and written back
/// afterwards. `translation.pad` is never written.
typedef struct {
    ActorScaleScratch rescale;     // Identity rotation scaled per axis by the body's `scale`, then multiplied into the root
    VECTOR            translation; // The root coordinate's translation on entry, in its parent's units
} _Actor02400ScaleScratch;
STATIC_ASSERT_SIZEOF(_Actor02400ScaleScratch, 0x40);

extern DamageAttack Actor02400_BodyPairs[4];
extern EnemyParams  Actor02400_Params0;
extern EnemyParams  Actor02400_Params1;
/// Frames the grown body waits before it spawns, indexed by `variant`.
extern s16      Actor02400_D045D4[];
extern s16      Actor02400_D045D8[];
extern s16      Actor02400_D045DC[];
extern s16      Actor02400_D0463C[];
extern TaskDesc Actor02400_D0465C[];

static void _actor02400InitBody(Enemy* enemy, Task* task);
static void Actor02400_Fn024F8(Enemy* enemy, Task* task);
static void _actor02400InitFireball(Enemy* enemy, Task* task);
static void Actor02400_Fn02AF0(Enemy* enemy, Task* task);
static void Actor02400_Fn02E0C(Enemy* enemy, Task* task);
static void _actor02400TeardownFireball(Enemy* enemy, Task* task);
static void Actor02400_Fn02EDC(Task* task);
static void _actor02400UpdateHurt(Task* task);
static void _actor02400UpdateArm(Task* task);
static void _actor02400StepForward(Task* task);
static void Actor02400_Fn031D0(Task* task);
static void _actor02400DrawShadow(Task* task);
static void _actor02400ApplyDeathScale(Task* task);

static TmdSource _gActor02400AmoebaBody;
static void      _actor02400BodyTask(Task* task);
static void      _actor02400FireballTask(Task* task);

static TmdBone _gActor02400AmoebaBodySkeleton[4] = {
#include "assets/amoeba_body_skeleton.inc"
};

static u32 _gActor02400AmoebaBodyPartVerts[4] = {
#include "assets/amoeba_body_partVerts.inc"
};

static SVECTOR _gActor02400AmoebaBodyVerts[87] = {
#include "assets/amoeba_body_verts.inc"
};

static SVECTOR _gActor02400AmoebaBodyNormals[58] = {
#include "assets/amoeba_body_normals.inc"
};

static u32 _gActor02400AmoebaBodyStream[772] = {
#include "assets/amoeba_body_stream.inc"
};

static TmdSource _gActor02400AmoebaBody = {
    0,
    4168,
    1040,
    4,
    _gActor02400AmoebaBodyPartVerts,
    _gActor02400AmoebaBodyVerts,
    _gActor02400AmoebaBodyNormals,
    _gActor02400AmoebaBodySkeleton,
    _gActor02400AmoebaBodyStream,
};

DamageAttack Actor02400_BodyPairs[4] = {
    { 0, 8 },
    { 28, 6 },
    { 0, 11 },
    { 38, 6 },
};

EnemyParams Actor02400_Params0 = { Actor02400_BodyPairs, 80, 12, 86, 8, 0, 0, 100, 99 };

EnemyParams Actor02400_Params1 = { Actor02400_BodyPairs, 280, 16, 420, 30, 0, 0, 100, 99 };

s16 Actor02400_D045D4[2] = {
    90,
    30,
};

s16 Actor02400_D045D8[2] = {
    2,
    5,
};

s16 Actor02400_D045DC[48] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    0,
    3,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    0,
    2,
    1,
    1,
    1,
    1,
    0,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    1,
    0,
    1,
    1,
    1,
    1,
    0,
    1,
    1,
    0,
    0,
    0,
};

s16 Actor02400_D0463C[16] = {
    0,
    1,
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
};

TaskDesc Actor02400_D0465C[2] = {
    { { { TASK_BODY_TMD, 96 } }, _actor02400BodyTask, { .model = &_gActor02400AmoebaBody } },
    { { { TASK_BODY_COORD, 96 } }, _actor02400FireballTask, { .value = 0 } },
};

static void Actor02400_Fn00C08(Task* task);
static void Actor02400_Fn01590(Task* task);
static void Actor02400_Fn01B90(Task* task);

#include "../../shared/fireball_glow.inc.c"

#include "../../shared/fireball_ground_glow.inc.c"

/// The main body's state handlers, run by `_actor02400BodyTask` for the task's
/// state: spawn, per-frame tick and death.
static const EnemyTaskFuncTable3 Actor02400_D00004 = {
    { _actor02400InitBody, Actor02400_Fn02E0C, Actor02400_Fn024F8 },
};

/// Links the amoeba's radius-200 hit sphere and initializes its four contacts.
///
/// Requires zeroed body work and a live root. Its centre is 200 local units
/// above the root; per-frame scaling compensates that height afterwards.
static __inline__ void _actor02400InitHitSphere(_Actor02400Work* work, GfxCoord* rootCoord)
{
    enum {
        ACTOR_02400_BODY_CONTACT_ID = 0x18,
    };

    work->body.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_02400_BODY_CONTACT_ID;
    work->body.coord            = rootCoord;
    work->body.context.contacts = work->bodyContacts;
    work->body.pos.vy           = -ACTOR_02400_BODY_RADIUS;
    work->body.pos.vx           = 0;
    work->body.pos.vz           = 0;
    work->body.radius           = ACTOR_02400_BODY_RADIUS;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);
    work->body.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
}

/// Creates the amoeba body's collision, lighting and behaviour work.
///
/// Requires a four-part TMD task and its live enemy record. Placement bit 0
/// selects parameters, attack entries and the palette row. The task owns the
/// zeroed work; the model borrows its lighting matrices until teardown.
/// Starts dormant at 1536/4096 scale and selects the active task handler.
/// Allocation failure destroys the enemy and task before any links are added.
static void _actor02400InitBody(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_02400_BODY_CONTACT_ID   = 0x18,
        ACTOR_02400_BODY_RADIUS       = 200,
        ACTOR_02400_TARGET_HEIGHT     = 150,
        ACTOR_02400_ARM_STRIKE_RADIUS = 100,
        ACTOR_02400_ARM_INITIAL_REACH = 500,
        ACTOR_02400_BODY_TASK_ACTIVE  = 1,
    };

    TmdObject*       model;
    GfxCoord*        rootCoord;
    _Actor02400Work* work;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    work      = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work              = work;
    model->flags            = 0;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx         = &work->light;
    model->colorMtx         = &work->color;
    work->variant           = enemy->place->mode & 1;
    if (work->variant != 0) {
        model->clutRowOffset += 1;
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
    enemy->field_4  = &rootCoord->coord;
    enemy->field_48 = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->bodyPos.vy             = -ACTOR_02400_TARGET_HEIGHT;
    enemy->coord                  = rootCoord;
    enemy->node.state.parts.flags = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->recs                   = work->bodyContacts;
    if (work->variant == 0) {
        enemy->param = &Actor02400_Params0;
        enemy->hp    = Actor02400_Params0.hpMax;
    } else {
        enemy->param = &Actor02400_Params1;
        enemy->hp    = Actor02400_Params1.hpMax;
    }
    work->effectArg.coord      = &task->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = 0x200;
    work->effectArg.spawnArgHi = 1;
    sceneAcquireBattleRef(0);
    work->scale.vx   = ACTOR_02400_SCALE_FLAT;
    work->scale.vy   = ACTOR_02400_SCALE_FLAT;
    work->scale.vz   = ACTOR_02400_SCALE_FLAT;
    work->baseMatrix = rootCoord->coord;
    gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->counter    = (gRandomLcgState >> 16) & 0xF;
    _actor02400InitHitSphere(work, rootCoord);
    work->attackBody.coord            = &task->extra.tmd->coords[3];
    work->attackBody.context.contacts = work->attackContacts;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = ACTOR_02400_ARM_INITIAL_REACH;
    work->attackBody.key              = damagePackAttackKey(Actor02400_BodyPairs, work->variant * 2);
    work->attackBody.radius           = ACTOR_02400_ARM_STRIKE_RADIUS;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    task->state             = ACTOR_02400_BODY_TASK_ACTIVE;
}

/// Resolves this frame's contacts. The push-back from `worldCollisionResolvePushback` moves
/// the body, or puts it back at `prevPos`; then each of `bodyContacts` is
/// handled by kind. A hit (kind 2) outside `hitCooldown` is classified by the
/// attacker's parameters: it adds to `staggerDamage` without taking HP, deals
/// its damage, deals five times that, or kills outright. It puts the body in
/// `ACTOR_02400_MODE_HURT` or `ACTOR_02400_MODE_STUNNED`, may spawn sparks,
/// plays the hurt sound, and a kill sets the task to state 2. A solid contact
/// (kind 3) pushes the body out along the deepest overlap. A record in
/// `attackContacts` is a touch: it sets `attackLanded` and costs the player the
/// variant's MP.
static void Actor02400_Fn00C08(Task* task)
{
    ActorContactOverlapPushScratch* scratch;
    GfxCoord*                       coord;
    GfxCoord*                       src;
    _Actor02400Work*                work;
    Enemy*                          enemy;
    s32                             push;
    s32                             reach;
    s32                             val;
    s32                             res;
    s32                             i;
    s32                             kind;
    s32                             param;
    s32                             damage;
    s32                             lastId;
    s16                             dmg;
    s32                             sndId;
    s32                             pan;
    s32                             stun;

    push    = 0;
    lastId  = 0;
    work    = task->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(ActorContactOverlapPushScratch);
    coord   = task->extra.tmd->coords;
    enemy   = task->spawnArg2.pointer;
    res     = worldCollisionResolvePushback(work->bodyContacts, &scratch->delta, ARRAY_SIZE(work->bodyContacts), NULL);
    switch (res) {
        case 1:
            coord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
            coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            coord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case 2:
            coord->coord.t[0] = work->prevPos.vx;
            coord->coord.t[1] = work->prevPos.vy;
            coord->coord.t[2] = work->prevPos.vz;
            break;
        case 0:
        default:
            break;
    }
    if (work->hitCooldown != 0) {
        work->hitCooldown--;
        if (work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    i = 0;
    do {
        switch ((u32)work->bodyContacts[i].key.value >> 16) {
            case 2:
                if (work->hitCooldown != 0) {
                    break;
                }
                kind   = 0;
                damage = 0;
                if (!(work->bodyContacts[i].key.value & 0x8000)) {
                    kind = Actor02400_D045DC[work->bodyContacts[i].key.value & 0x7F];
                }
                switch (damageGetPlayerAttackReaction(work->bodyContacts[i].key.value) & 0xFFFF) {
                    case DAMAGE_PLAYER_REACTION_POISON:
                    case 4:
                        kind = 3;
                        break;
                    case DAMAGE_PLAYER_REACTION_STAGGER:
                    case DAMAGE_PLAYER_REACTION_INCENDIARY:
                        kind = 1;
                        break;
                    case DAMAGE_PLAYER_REACTION_NONE:
                    case DAMAGE_PLAYER_REACTION_BUILDUP:
                    case 5:
                    case DAMAGE_PLAYER_REACTION_EXPLOSION:
                    case 8:
                    case 9:
                        break;
                }
                switch (kind) {
                    case 0:
                        // Takes no HP: the roll adds to the stagger total, which stuns once it reaches 20.
                        src                      = gPlayerActorTasks[(work->bodyContacts[i].key.value >> 7) & 1]->extra.tmd->coords;
                        scratch->delta.vector.vx = src->coord.t[0] - coord->coord.t[0];
                        scratch->delta.vector.vy = src->coord.t[1] - coord->coord.t[1];
                        scratch->delta.vector.vz = src->coord.t[2] - coord->coord.t[2];
                        work->staggerDamage     += damageComputePlayerAttack(work->bodyContacts[i].key.value, SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy + scratch->delta.vector.vz * scratch->delta.vector.vz), 0, 0);
                        if (work->staggerDamage >= 20 || work->mode == ACTOR_02400_MODE_STUNNED) {
                            work->mode          = ACTOR_02400_MODE_STUNNED;
                            work->phase         = 0;
                            work->staggerWindow = 0;
                            work->staggerDamage = 0;
                        } else {
                            work->mode          = ACTOR_02400_MODE_HURT;
                            work->phase         = 0;
                            work->staggerWindow = 60;
                        }
                        work->counter  = 0;
                        work->speed    = 0;
                        work->turnRate = 0;
                        work->armOut   = 0;
                        break;
                    case 1:
                        src                      = gPlayerActorTasks[(work->bodyContacts[i].key.value >> 7) & 1]->extra.tmd->coords;
                        scratch->delta.vector.vx = src->coord.t[0] - coord->coord.t[0];
                        scratch->delta.vector.vy = src->coord.t[1] - coord->coord.t[1];
                        scratch->delta.vector.vz = src->coord.t[2] - coord->coord.t[2];
                        damage                   = damageComputePlayerAttack(work->bodyContacts[i].key.value, SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy + scratch->delta.vector.vz * scratch->delta.vector.vz), 0, 0);
                        work->mode               = ACTOR_02400_MODE_HURT;
                        work->phase              = 0;
                        work->counter            = 0;
                        work->speed              = 0;
                        work->turnRate           = 0;
                        work->armOut             = 0;
                        param                    = damageGetPlayerAttackEffectId(work->bodyContacts[i].key.value);
                        if (Actor02400_D0463C[param] == 0 && lastId != work->bodyContacts[i].key.value) {
                            lastId = work->bodyContacts[i].key.value;
                            effectSpawnHit(param, coord, NULL, &work->effectArg);
                        }
                        break;
                    case 2:
                        src                      = gPlayerActorTasks[(work->bodyContacts[i].key.value >> 7) & 1]->extra.tmd->coords;
                        scratch->delta.vector.vx = src->coord.t[0] - coord->coord.t[0];
                        scratch->delta.vector.vy = src->coord.t[1] - coord->coord.t[1];
                        scratch->delta.vector.vz = src->coord.t[2] - coord->coord.t[2];
                        damage                   = (s16)damageComputePlayerAttack(work->bodyContacts[i].key.value, SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy + scratch->delta.vector.vz * scratch->delta.vector.vz), 0, 0) * 5;
                        work->mode               = ACTOR_02400_MODE_HURT;
                        work->phase              = 0;
                        work->counter            = 0;
                        work->speed              = 0;
                        work->turnRate           = 0;
                        work->armOut             = 0;
                        effectSpawn(EFFECT_CRITICAL_HIT, coord, 2, NULL);
                        break;
                    case 3:
                        work->mode              = ACTOR_02400_MODE_HURT;
                        work->phase             = 0;
                        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        if (work->variant == 0) {
                            damage = Actor02400_Params0.hpMax;
                        } else {
                            damage = Actor02400_Params1.hpMax;
                        }
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        damage         += (s16)(((gRandomLcgState >> 16) & 0x7F) + 200);
                        effectSpawn(EFFECT_CRITICAL_HIT, coord, 2, NULL);
                        break;
                }
                dmg = damage;
                damageAccumulateLifeDrainHp(task->spawnArg2.pointer, work->bodyContacts[i].key.value, dmg, 0);
                worldTargetAddReadoutAmount(&((Enemy*)task->spawnArg2.pointer)->node, dmg, 0);
                if ((enemy->hp -= damage) <= 0) {
                    task->state = 2;
                }
                stun = damageGetPlayerAttackHitCooldown(work->bodyContacts[i].key.value);
                if (stun > 0) {
                    work->hitCooldown = stun;
                }
                sndId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40180003;
                pan   = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(sndId, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                break;
            case 0:
            case 1:
                break;
            case 3:
                scratch->delta.vector.vx = coord->workm.t[0] - work->bodyContacts[i].point.vx;
                scratch->delta.vector.vy = coord->workm.t[1] - work->bodyContacts[i].point.vy;
                scratch->delta.vector.vz = coord->workm.t[2] - work->bodyContacts[i].point.vz;
                reach                    = work->bodyContacts[i].distance - SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy + scratch->delta.vector.vz * scratch->delta.vector.vz);
                val                      = reach;
                if (reach <= 0) {
                    val = 0;
                }
                reach = val;
                if (push < reach) {
                    push = reach;
                    VectorNormal(&scratch->delta.vector, &scratch->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->normal, &scratch->pushDirection);
                }
                break;
        }
    } while (++i < (s32)ARRAY_SIZE(work->bodyContacts));
    if (push > 0) {
        coord->coord.t[0] += (push * scratch->pushDirection.vx) >> 12;
        coord->coord.t[2] += (push * scratch->pushDirection.vz) >> 12;
    }
    worldCollisionClearContacts(work->bodyContacts);
    // The attack body touched the player: disable it until the body crawls again and take the MP.
    if (worldCollisionFindContactIndex(work->attackContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(work->attackContacts);
        work->attackLanded = 1;
        playerStateSpendMp(Actor02400_D045D8[work->variant]);
    }
    if (work->staggerWindow != 0) {
        work->staggerWindow--;
        if (work->staggerWindow <= 0) {
            work->staggerDamage = 0;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactOverlapPushScratch);
}

/// The projectile's state handlers, run by `_actor02400FireballTask` for the task's
/// state: spawn, flight and teardown.
static const EnemyTaskFuncTable3 Actor02400_D0003C = {
    { _actor02400InitFireball, Actor02400_Fn02AF0, _actor02400TeardownFireball },
};

/// Wakes a dormant amoeba when the player approaches or combat alerts it.
///
/// Requires live body work and a TMD root. Ground-plane distance below 1500
/// units or the group's latched fireball alert wakes it immediately; the
/// PE-active signal is sampled when the randomized countdown expires.
/// Waking begins the crawl's swelling phase at unit scale and engages combat.
/// Reserves and releases one `VECTOR` on the initialized scratch stack.
static void _actor02400UpdateDormant(Task* task)
{
    enum {
        ACTOR_02400_WAKE_DISTANCE = 1500,
    };

    _Actor02400Work* work;
    GfxCoord*        rootCoord;
    s32              shouldWake;
    s32              dx;
    s32              dz;
    u32              randomState;
    VECTOR*          delta;

    delta      = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    work       = task->work;
    rootCoord  = task->extra.tmd->coords;
    shouldWake = 0;
    if (--work->counter < 0) {
        randomState     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        work->counter   = (randomState >> 16) & 0xF;
        gRandomLcgState = randomState;
        if (gSceneCombatState.signals.bytes.actionFlags & SCENE_COMBAT_ACTION_PE_ACTIVE) {
            shouldWake = 1;
        }
    }
    delta->vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
    delta->vy = 0;
    dz        = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
    delta->vz = dz;
    dx        = delta->vx;
    if (SquareRoot0((dx * dx) + (dz * dz)) < ACTOR_02400_WAKE_DISTANCE) {
        shouldWake = 1;
    }
    if (gSceneCombatState.actor02400Alert != 0) {
        shouldWake = 1;
    }
    if (shouldWake != 0) {
        work->mode     = ACTOR_02400_MODE_CRAWL;
        work->phase    = ACTOR_02400_CRAWL_PHASE_WAKE;
        work->scale.vx = ONE;
        work->scale.vy = ONE;
        work->scale.vz = ONE;
        work->counter  = 0;
        sceneEngageBattle(1);
    }
    SCRATCH_STACK_CURSOR(VECTOR) += 1;
}

/// `ACTOR_02400_MODE_CRAWL`: `TURN` faces the player and `SURGE` moves straight
/// ahead, each for a random time before handing to the other; `WAKE` pulses the
/// scale up until the height passes `ACTOR_02400_SCALE_SWOLLEN`. Outside `WAKE`
/// X and Z relax to unit scale while the height bobs around unit scale in `TURN`
/// and `ACTOR_02400_SCALE_FLAT` in `SURGE`, and `speed` eases toward
/// `targetSpeed`. Once `attackLanded` is set the body stops, enters
/// `ACTOR_02400_MODE_STRIKE`, pitches the arm at random, plays the sound and
/// adopts the glow disc it spawns as `chargeEffect`.
static void Actor02400_Fn01590(Task* task)
{
    _Actor02400Work*  work;
    GfxCoord*         coord;
    s16               limit;
    s32               diff;
    s32               step;
    s32               sound;
    s32               pan;
    EffectWork*       effect;
    ActorFaceScratch* scratch;
    ActorFaceScratch* scratchEnd;

    scratchEnd                             = SCRATCH_STACK_CURSOR(ActorFaceScratch);
    SCRATCH_STACK_CURSOR(ActorFaceScratch) = scratchEnd - 1;
    scratch                                = scratchEnd - 1;
    work                                   = task->work;
    coord                                  = task->extra.tmd->coords;
    limit                                  = ONE;
    switch (work->phase) {
        case ACTOR_02400_CRAWL_PHASE_TURN:
            scratchEnd[-1].delta.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            scratch->delta.vy       = 0;
            scratch->delta.vz       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->targetYaw         = ratan2((s16)scratchEnd[-1].delta.vx, (s16)scratch->delta.vz) & 0xFFF;
            work->turnRate          = 0x19;
            work->targetSpeed       = 0xA;
            work->counter--;
            if (work->counter < 0) {
                work->phase     = ACTOR_02400_CRAWL_PHASE_SURGE;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->counter   = ((gRandomLcgState >> 16) & 0x3F) + 0x1E;
            }
            break;
        case ACTOR_02400_CRAWL_PHASE_SURGE:
            work->turnRate    = 0;
            work->targetSpeed = 0x14;
            work->counter--;
            limit = ACTOR_02400_SCALE_FLAT;
            if (work->counter < 0) {
                work->phase     = ACTOR_02400_CRAWL_PHASE_TURN;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->counter   = ((gRandomLcgState >> 16) & 0x1F) + 0x1E;
            }
            break;
        case ACTOR_02400_CRAWL_PHASE_WAKE:
            work->turnRate = 0;
            if (work->swingDown == 0) {
                work->counter += 0x40;
                if (work->counter >= 0x100) {
                    work->swingDown = 1;
                }
            } else {
                work->counter -= 0x40;
                if (work->counter < -0xFF) {
                    work->swingDown = 0;
                }
            }
            work->scale.vx += (u16)work->counter + 0x80;
            work->scale.vy += (u16)work->counter + 0x80;
            work->scale.vz += (u16)work->counter + 0x80;
            if (work->scale.vy > ACTOR_02400_SCALE_SWOLLEN) {
                work->phase     = ACTOR_02400_CRAWL_PHASE_TURN;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->counter   = ((gRandomLcgState >> 16) & 0x1F) + 0x1E;
            }
            break;
    }
    if (work->phase < ACTOR_02400_CRAWL_PHASE_WAKE) {
        work->scale.vx -= 0x80;
        if (work->scale.vx <= ONE) {
            work->scale.vx = ONE;
        }
        if (work->swingDown == 0) {
            work->scale.vy += 0x80;
            if (work->scale.vy > limit + 0x100) {
                work->swingDown = 1;
            }
        } else {
            work->scale.vy -= 0x80;
            if (work->scale.vy < limit - 0x100) {
                work->swingDown = 0;
            }
        }
        work->scale.vz -= 0x80;
        if (work->scale.vz <= ONE) {
            work->scale.vz = ONE;
        }
    }
    diff = work->targetSpeed - work->speed;
    step = -3;
    if (diff > 0) {
        step = 2;
    }
    if ((diff >= 0 ? diff : -diff) < (step >= 0 ? step : -step)) {
        work->speed = work->targetSpeed;
    } else {
        work->speed += step;
    }
    if (work->attackLanded != 0) {
        // The touch took the player's MP: stop, strike, and start gathering the charge.
        work->mode      = ACTOR_02400_MODE_STRIKE;
        work->phase     = ACTOR_02400_STRIKE_PHASE_EXTEND;
        work->counter   = 0;
        work->turnRate  = 0;
        work->speed     = 0;
        work->scale.vx  = ONE;
        work->scale.vy  = ONE;
        work->scale.vz  = ONE;
        scratch->rot.vy = 0;
        scratch->rot.vz = 0;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        scratch->rot.vx = ((gRandomLcgState >> 16) & 0xFF) + 0x100;
        RotMatrix(&scratch->rot, &task->extra.tmd->coords[2].coord);
        sound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40180002;
        pan   = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        effect             = effectSpawn(gRoomEffectGlowDiscId, coord, (s32)(work->variant), NULL);
        work->chargeEffect = effect;
        if (effect != NULL) {
            taskReparent(task, effect->task);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

/// Extends and retracts the amoeba's arm before returning to crawl or casting.
///
/// Each phase lasts seven ticks; a recorded player touch ends extension early.
/// After retraction that touch is consumed and starts the cast's swelling
/// phase. Without a touch, crawling and arm collision resume. The body height
/// continues its 4096-centred oscillation while striking.
static void _actor02400UpdateStrike(Task* task)
{
    enum {
        ACTOR_02400_STRIKE_PHASE_FRAMES = 7,
        ACTOR_02400_STRIKE_SCALE_SWING  = ONE / 16,
    };

    _Actor02400Work* work;
    s32              phase;

    work  = task->work;
    phase = work->phase;
    switch (phase) {
        case ACTOR_02400_STRIKE_PHASE_EXTEND:
            work->armOut = 1;
            work->counter++;
            if (work->counter >= ACTOR_02400_STRIKE_PHASE_FRAMES) {
                work->phase   = ACTOR_02400_STRIKE_PHASE_RETRACT;
                work->counter = 0;
            }
            if (work->attackLanded != 0) {
                work->phase   = ACTOR_02400_STRIKE_PHASE_RETRACT;
                work->counter = 0;
            }
            break;
        case ACTOR_02400_STRIKE_PHASE_RETRACT:
            work->armOut = 0;
            work->counter++;
            if (work->counter >= ACTOR_02400_STRIKE_PHASE_FRAMES) {
                if (work->attackLanded != 0) {
                    work->attackLanded = 0;
                    work->mode         = ACTOR_02400_MODE_CAST;
                    work->phase        = ACTOR_02400_CAST_PHASE_SWELL;
                    work->counter      = 0;
                    work->swingDown    = 0;
                    work->scale.vx     = work->scale.vy;
                    work->scale.vz     = work->scale.vy;
                } else {
                    work->mode              = ACTOR_02400_MODE_CRAWL;
                    work->phase             = ACTOR_02400_CRAWL_PHASE_TURN;
                    gRandomLcgState         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->counter           = ((gRandomLcgState >> 16) & 0x1F) + 0x1E;
                    work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
            }
            break;
    }
    if (work->swingDown == 0) {
        work->scale.vy += 0x80;
        if (work->scale.vy > ONE + ACTOR_02400_STRIKE_SCALE_SWING) {
            work->swingDown = 1;
        }
    } else {
        work->scale.vy -= 0x80;
        if (work->scale.vy < ONE - ACTOR_02400_STRIKE_SCALE_SWING) {
            work->swingDown = 0;
        }
    }
}

/// `ACTOR_02400_MODE_CAST`: `SWELL` pulses the scale up until the height passes
/// `ACTOR_02400_SCALE_SWOLLEN` and sets the charge effect flickering, `AIM`
/// faces the player while the height wobbles around that mark for the variant's
/// wait, `RELEASE` spawns the projectile from `Actor02400_D0465C`, raises the
/// wake flag, lets the charge effect go and plays the sound, and `SHRINK`
/// brings the scale back to the crawling shape before returning to crawling
/// with a random wait.
static void Actor02400_Fn01B90(Task* task)
{
    _Actor02400Work* work;
    GfxCoord*        coord;
    s32              flags;
    s32              sound;
    s32              pan;
    VECTOR*          delta;
    VECTOR*          scratchEnd;

    scratchEnd                                                                = *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET);
    delta                                                                     = scratchEnd - 1;
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = delta;
    work                                                                      = task->work;
    coord                                                                     = task->extra.tmd->coords;
    flags                                                                     = 0;
    switch (work->phase) {
        case ACTOR_02400_CAST_PHASE_SWELL:
            work->turnRate = 0;
            work->speed    = 0;
            if (work->swingDown == 0) {
                work->counter += 0x40;
                if (work->counter >= 0x100) {
                    work->swingDown = 1;
                }
            } else {
                work->counter -= 0x40;
                if (work->counter < -0xFF) {
                    work->swingDown = 0;
                }
            }
            work->scale.vx += (u16)work->counter + 0x40;
            work->scale.vy += (u16)work->counter + 0x40;
            work->scale.vz += (u16)work->counter + 0x40;
            if (work->scale.vy > ACTOR_02400_SCALE_SWOLLEN) {
                work->phase   = ACTOR_02400_CAST_PHASE_AIM;
                work->counter = 0;
                if (work->chargeEffect != NULL) {
                    work->chargeEffect->task->state = ROOM_VISUAL_EFFECTS_GLOW_DISC_FLICKER;
                }
            }
            break;
        case ACTOR_02400_CAST_PHASE_AIM:
            work->turnRate    = 0x19;
            scratchEnd[-1].vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            delta->vy         = 0;
            delta->vz         = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->targetYaw   = ratan2((s16)scratchEnd[-1].vx, (s16)delta->vz) & 0xFFF;
            if (work->swingDown == 0) {
                work->scale.vy += 0x80;
                if (work->scale.vy > ACTOR_02400_SCALE_SWOLLEN + 0x100) {
                    work->swingDown = 1;
                }
            } else {
                work->scale.vy -= 0x80;
                if (work->scale.vy < ACTOR_02400_SCALE_SWOLLEN - 0x100) {
                    work->swingDown = 0;
                }
            }
            work->counter++;
            if (work->counter > Actor02400_D045D4[work->variant]) {
                work->phase    = ACTOR_02400_CAST_PHASE_RELEASE;
                work->counter  = 0;
                work->turnRate = 0;
            }
            break;
        case ACTOR_02400_CAST_PHASE_RELEASE:
            enemySpawnFromTable(Actor02400_D0465C, 1, 0, task->spawnArg2.pointer);
            gSceneCombatState.actor02400Alert = 1;
            work->phase                       = ACTOR_02400_CAST_PHASE_SHRINK;
            if (work->chargeEffect != NULL) {
                work->chargeEffect->task->state = ROOM_VISUAL_EFFECTS_GLOW_DISC_RELEASE;
            }
            work->chargeEffect = NULL;
            sound              = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40180004;
            pan                = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            break;
        case ACTOR_02400_CAST_PHASE_SHRINK:
            work->scale.vx -= 0x80;
            if (work->scale.vx <= ONE) {
                work->scale.vx = ONE;
                flags          = 1;
            }
            work->scale.vy -= 0x80;
            if (work->scale.vy <= ACTOR_02400_SCALE_FLAT) {
                work->scale.vy = ACTOR_02400_SCALE_FLAT;
                flags         |= 2;
            }
            work->scale.vz -= 0x80;
            if (work->scale.vz <= ONE) {
                work->scale.vz = ONE;
                flags         |= 4;
            }
            if (flags == 7) {
                work->mode              = ACTOR_02400_MODE_CRAWL;
                work->phase             = ACTOR_02400_CRAWL_PHASE_TURN;
                gRandomLcgState         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->counter           = ((gRandomLcgState >> 16) & 0x1F) + 0x1E;
                work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            break;
    }
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) += 1;
}

/// Flattens the stunned amoeba, cancels its charge and disables arm collision.
///
/// Requires live body work with the counter reset on entry to the mode.
/// Resumes crawling after the counter exceeds 360 ticks and re-enables the
/// arm's pair tests. X/Z shrink toward unit scale and Y toward 1536/4096.
static void _actor02400UpdateStunned(Task* task)
{
    enum {
        ACTOR_02400_STUN_FRAMES = 360,
    };

    _Actor02400Work* work = task->work;

    work->scale.vx -= 0x40;
    if (work->scale.vx < ONE) {
        work->scale.vx = ONE;
    }
    work->scale.vy -= 0x80;
    if (work->scale.vy < ACTOR_02400_SCALE_FLAT) {
        work->scale.vy = ACTOR_02400_SCALE_FLAT;
    }
    work->scale.vz -= 0x40;
    if (work->scale.vz < ONE) {
        work->scale.vz = ONE;
    }
    if (work->chargeEffect != NULL) {
        work->chargeEffect->task->state = ROOM_VISUAL_EFFECTS_GLOW_DISC_CANCEL;
        work->chargeEffect              = NULL;
    }
    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->counter++;
    if (work->counter > ACTOR_02400_STUN_FRAMES) {
        work->mode              = ACTOR_02400_MODE_CRAWL;
        work->phase             = ACTOR_02400_CRAWL_PHASE_TURN;
        gRandomLcgState         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->counter           = ((gRandomLcgState >> 16) & 0x1F) + 0x1E;
        work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
}

/// Applies the amoeba's per-axis Q12 scale while preserving its local origin.
///
/// Requires a live TMD body, nonzero Y scale and 0x40 free scratch-stack bytes.
/// Saves the current unscaled root matrix, keeps the body sphere 200 units
/// above the scaled root and adjusts arm reach to Z scale. The root rotation
/// is multiplied by the scale and marked dirty; translation is restored.
static void _actor02400ApplyBodyScale(Task* task)
{
    GfxCoord*                rootCoord;
    _Actor02400Work*         work;
    _Actor02400ScaleScratch* scratch;

    rootCoord        = task->extra.tmd->coords;
    work             = task->work;
    work->baseMatrix = rootCoord->coord;
    scratch          = SCRATCH_STACK_RESERVE_BLOCK(_Actor02400ScaleScratch);
    // Keep the hit sphere at a fixed height as the root changes shape.
    work->body.pos.vy         = -(ACTOR_02400_BODY_RADIUS * ONE) / work->scale.vy;
    work->attackBody.pos.vz   = (work->scale.vz * 250) / ONE;
    scratch->rescale.scale.vx = work->scale.vx;
    scratch->rescale.scale.vy = work->scale.vy;
    scratch->rescale.scale.vz = work->scale.vz;
    scratch->translation.vx   = rootCoord->coord.t[0];
    scratch->translation.vy   = rootCoord->coord.t[1];
    scratch->translation.vz   = rootCoord->coord.t[2];
    rootCoord->coord          = work->baseMatrix;
    gfxSetRotIdentity(&scratch->rescale.matrix);
    ScaleMatrix(&scratch->rescale.matrix, &scratch->rescale.scale);
    MulMatrix(&rootCoord->coord, &scratch->rescale.matrix);
    rootCoord->coord.t[0] = scratch->translation.vx;
    rootCoord->coord.t[1] = scratch->translation.vy;
    rootCoord->coord.t[2] = scratch->translation.vz;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor02400ScaleScratch);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Turns the amoeba toward its target heading and rebuilds its root rotation.
///
/// Current yaw comes from the root's Z axis. Target headings are in 0..4095
/// and `turnRate` is a nonnegative limit in 4096 units per turn per tick.
/// Chooses the shorter arc, snapping when the remaining turn fits the limit;
/// the half-turn tie uses the wrapped arc. Replaces pitch, roll and scale.
/// Reserves and releases one `ActorFaceScratch` on the initialized scratch stack.
static void _actor02400TurnTowardTarget(Task* task)
{
    _Actor02400Work*  work;
    GfxCoord*         rootCoord;
    ActorFaceScratch* scratch;
    s32               currentYaw;
    u16               targetYaw;
    s16               yawDelta;
    s32               absoluteDelta;
    s32               turnStep;
    s32               previousYaw;
    s32               nextYaw;
    s32               wrappedTurnStep;

    scratch       = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    rootCoord     = task->extra.tmd->coords;
    work          = task->work;
    currentYaw    = ratan2(rootCoord->coord.m[0][2], rootCoord->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
    targetYaw     = work->targetYaw;
    yawDelta      = targetYaw - currentYaw;
    absoluteDelta = yawDelta >= 0 ? yawDelta : -yawDelta;

    work->yaw = currentYaw;
    if (absoluteDelta < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        turnStep = work->turnRate;
        if (turnStep >= absoluteDelta) {
            work->yaw = targetYaw;
        } else {
            nextYaw = work->yaw;
            if (yawDelta <= 0) {
                nextYaw -= turnStep;
            } else {
                nextYaw += turnStep;
            }
            work->yaw = nextYaw;
        }
    } else {
        turnStep = work->turnRate;
        if (yawDelta > 0 ? turnStep >= ACTOR_TRANSFORM_ANGLE_TURN - yawDelta : turnStep >= ACTOR_TRANSFORM_ANGLE_TURN + yawDelta) {
            work->yaw = work->targetYaw;
        } else {
            wrappedTurnStep = work->turnRate;
            previousYaw     = work->yaw;
            if (yawDelta > 0) {
                work->yaw = previousYaw - wrappedTurnStep;
            } else {
                work->yaw = previousYaw + wrappedTurnStep;
            }
        }
    }
    scratch->rot.vx = 0;
    scratch->rot.vy = work->yaw;
    scratch->rot.vz = 0;
    RotMatrix(&scratch->rot, &rootCoord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

/// Plays the amoeba's positional idle sound every 25 active behaviour ticks.
///
/// Requires live body work and an enemy in `spawnArg2`. Height maps the sound
/// depth from 50 to 100 percent over the flat-to-swollen wobble range, clamped
/// at both ends. Placement selects the script instance; the root supplies
/// pan and distance depth. Dormant, hurt and stunned modes do not call it.
static void _actor02400UpdateIdleSound(Task* task)
{
    enum {
        ACTOR_02400_IDLE_SOUND_FRAMES = 25,
        ACTOR_02400_IDLE_SOUND_SCRIPT = 0x40180001,
    };

    GfxCoord*        rootCoord;
    s16              heightScale;
    s16              heightAboveFlat;
    s32              soundId;
    s32              volumePercent;
    s8               depth;
    _Actor02400Work* work;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    work->idleSoundTimer++;
    if (work->idleSoundTimer >= ACTOR_02400_IDLE_SOUND_FRAMES) {
        work->idleSoundTimer = 0;
        heightScale          = work->scale.vy;
        soundId              = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_02400_IDLE_SOUND_SCRIPT;
        // Clamp height before converting it to a percentage of the positional sound depth.
        if (heightScale > ACTOR_02400_SCALE_SWOLLEN + 0x100) {
            heightAboveFlat = ACTOR_02400_SCALE_SWOLLEN + 0x100 - ACTOR_02400_SCALE_FLAT;
        } else {
            heightAboveFlat = work->scale.vy - ACTOR_02400_SCALE_FLAT;
            if (heightScale < ACTOR_02400_SCALE_FLAT) {
                heightAboveFlat = 0;
            }
        }
        volumePercent = (heightAboveFlat * 0x32) / (ACTOR_02400_SCALE_SWOLLEN + 0x100 - ACTOR_02400_SCALE_FLAT) + 0x32;
        depth         = 0x7F - (((0x7F - worldCoordGetOriginAudioDepth(rootCoord)) * (s16)volumePercent) / 100);
        sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(rootCoord), depth);
    }
}

/// Death handler of the main body. While the room's actors are paused it only
/// refreshes the colour, and while they are hidden it hides the model.
/// Otherwise `ACTOR_02400_DEATH_PHASE_BEGIN` saves the model matrix, unlinks
/// the enemy node and both bodies, starts the light fade and cancels the charge
/// effect; `SQUASH` flattens the body, turning it semi-transparent at frame 10,
/// spawning the death effect at frame 15 and hiding it at frame 60; `DESTROY`
/// destroys the enemy.
static void Actor02400_Fn024F8(Enemy* arg0, Task* arg1)
{
    VECTOR           pos;
    _Actor02400Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    GfxCoord*        cur;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    coord = obj->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            pos.vx = coord->workm.t[0];
            pos.vy = coord->workm.t[1];
            pos.vz = coord->workm.t[2];
            worldCoordUpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            switch (work->phase) {
                case ACTOR_02400_DEATH_PHASE_BEGIN:
                    work->counter    = 0;
                    work->scale.vy   = ONE;
                    work->baseMatrix = coord->coord;
                    arg0->recs       = 0;
                    worldTargetUnlinkNode(&arg0->node);
                    worldCollisionUnlinkBody(&work->body);
                    worldCollisionUnlinkBody(&work->attackBody);
                    worldCoordSetActorColorMode(arg0, ENEMY_COLOR_WEIGHTED);
                    sceneReleaseBattleRefWithRewards(arg1, 0x18);
                    work->phase = ACTOR_02400_DEATH_PHASE_SQUASH;
                    cur         = arg1->extra.tmd->coords;
                    pos.vx      = cur->workm.t[0];
                    pos.vy      = cur->workm.t[1];
                    pos.vz      = cur->workm.t[2];
                    worldCoordUpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
                    if (work->chargeEffect != NULL) {
                        work->chargeEffect->task->state = ROOM_VISUAL_EFFECTS_GLOW_DISC_CANCEL;
                    }
                    break;
                case ACTOR_02400_DEATH_PHASE_SQUASH:
                    work->counter++;
                    if (work->counter == 10) {
                        obj->flags = TMD_OBJECT_SEMI_TRANS;
                    }
                    if (work->counter == 15) {
                        effectSpawn(EFFECT_CORPSE_BURN, coord, 3, NULL);
                    }
                    if (work->counter >= 60) {
                        work->phase = ACTOR_02400_DEATH_PHASE_DESTROY;
                        obj->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    if (work->scale.vy > 0x200) {
                        work->scale.vy -= 0x50;
                    }
                    _actor02400ApplyDeathScale(arg1);
                    cur    = arg1->extra.tmd->coords;
                    pos.vx = cur->workm.t[0];
                    pos.vy = cur->workm.t[1];
                    pos.vz = cur->workm.t[2];
                    worldCoordUpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
                    break;
                case ACTOR_02400_DEATH_PHASE_DESTROY:
                    enemyDestroy(arg0, arg1);
                    break;
            }
            break;
    }
}

/// Links the fireball's two strike spheres and its trailing wall capsule.
///
/// Requires zeroed fireball work, a coordinate-body task and live parent work
/// whose variant is 0 or 1. The spheres share one contact; the wall probe has
/// its own. Coordinates are reloaded after collision calls. No scratch is used.
static __inline__ void _actor02400InitFireballCollision(Task* task, _Actor02400FireballWork* work, _Actor02400Work* parentWork)
{
    enum {
        ACTOR_02400_FIREBALL_RADIUS                 = 200,
        ACTOR_02400_FIREBALL_WALL_TRAIL_LENGTH      = 210,
        ACTOR_02400_FIREBALL_PROBE_RADIUS           = 1,
        ACTOR_02400_FIREBALL_ENEMY_ATTACK_VARIANT_0 = 0x22D2D, // Weapon and distance-scale row 45, category 2.
        ACTOR_02400_FIREBALL_ENEMY_ATTACK_VARIANT_1 = 0x22E2E, // Weapon and distance-scale row 46, category 2.
    };

    GfxCoord* strikeCoord;
    GfxCoord* wallProbeCoord;

    strikeCoord                             = task->extra.coordBody->coord;
    work->playerStrikeBody.context.contacts = work->strikeContacts;
    work->playerStrikeBody.pos.vx           = 0;
    work->playerStrikeBody.pos.vy           = 0;
    work->playerStrikeBody.pos.vz           = 0;
    work->playerStrikeBody.coord            = strikeCoord;
    work->playerStrikeBody.key              = damagePackAttackKey(Actor02400_BodyPairs, (parentWork->variant * 2) | 1);
    work->playerStrikeBody.radius           = ACTOR_02400_FIREBALL_RADIUS;
    work->playerStrikeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->playerStrikeBody);
    worldCollisionInitContacts(work->strikeContacts, ARRAY_SIZE(work->strikeContacts), 0);
    work->playerStrikeBody.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    strikeCoord                            = task->extra.coordBody->coord;
    work->enemyStrikeBody.context.contacts = work->strikeContacts;
    work->enemyStrikeBody.pos.vx           = 0;
    work->enemyStrikeBody.pos.vy           = 0;
    work->enemyStrikeBody.pos.vz           = 0;
    work->enemyStrikeBody.coord            = strikeCoord;
    if (parentWork->variant == 0) {
        work->enemyStrikeBody.key = ACTOR_02400_FIREBALL_ENEMY_ATTACK_VARIANT_0;
    } else {
        work->enemyStrikeBody.key = ACTOR_02400_FIREBALL_ENEMY_ATTACK_VARIANT_1;
    }
    work->enemyStrikeBody.radius = ACTOR_02400_FIREBALL_RADIUS;
    work->enemyStrikeBody.flags  = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &work->enemyStrikeBody);

    work->wallCapsule.ends[1].vz   = -ACTOR_02400_FIREBALL_WALL_TRAIL_LENGTH;
    work->wallCapsule.end0Radius   = ACTOR_02400_FIREBALL_PROBE_RADIUS;
    work->wallCapsule.end1Radius   = ACTOR_02400_FIREBALL_PROBE_RADIUS;
    work->wallCapsule.ends[0].vx   = 0;
    work->wallCapsule.ends[0].vy   = 0;
    work->wallCapsule.ends[0].vz   = 0;
    work->wallCapsule.ends[1].vx   = 0;
    work->wallCapsule.ends[1].vy   = 0;
    work->wallCapsule.contacts     = work->wallContacts;
    work->enemyStrikeBody.flags   |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    wallProbeCoord                 = task->extra.coordBody->coord;
    work->wallBody.context.capsule = &work->wallCapsule;
    work->wallBody.pos.vx          = 0;
    work->wallBody.pos.vy          = 0;
    work->wallBody.pos.vz          = 0;
    work->wallBody.key             = 0;
    work->wallBody.radius          = 0;
    work->wallBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->wallBody.coord           = wallProbeCoord;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->wallBody);
    worldCollisionInitContacts(work->wallContacts, ARRAY_SIZE(work->wallContacts), 0);
}

/// Launches a coordinate-only fireball from its parent amoeba.
///
/// Requires a live parent with body work and a TMD root, a coordinate-body
/// child task and 0x18 free initialized scratch-stack bytes. Copies the
/// parent's local matrix, offsets the origin 350 units along negative Y,
/// and saves its Q12 Z axis for flight. Two radius-200 strike spheres share
/// one contact, and a trailing capsule probes the room. The child owns the
/// zeroed work, detaches from its parent and enters its 90-tick flight.
/// Allocation failure destroys it without releasing the scratch reservation.
static void _actor02400InitFireball(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_02400_FIREBALL_LAUNCH_HEIGHT   = 350,
        ACTOR_02400_FIREBALL_LIFETIME_FRAMES = 90,
        ACTOR_02400_FIREBALL_TASK_FLIGHT     = 1,
    };

    _Actor02400Work*         parentWork;
    Task*                    parent;
    _Actor02400FireballWork* work;
    ActorOffsetScratch*      scratch;
    SVECTOR*                 offset;
    GfxCoord*                fireballCoord;
    GfxCoord*                parentCoord;

    scratch       = SCRATCH_STACK_RESERVE_BLOCK(ActorOffsetScratch);
    offset        = &scratch->offset;
    parent        = task->parent;
    fireballCoord = task->extra.coordBody->coord;
    parentCoord   = parent->extra.tmd->coords;
    parentWork    = parent->work;
    work          = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work = work;
    // Rotate the launch offset through the parent basis before detaching the task.
    scratch->offset.vx = 0;
    scratch->offset.vy = -ACTOR_02400_FIREBALL_LAUNCH_HEIGHT;
    scratch->offset.vz = 0;
    gte_SetRotMatrix(&parentCoord->coord);
    gte_ldv0(offset);
    gte_rtv0();
    gte_stlvnl(&scratch->rotated);
    fireballCoord->parent       = &gGfxViewCoord;
    fireballCoord->coord        = parentCoord->coord;
    fireballCoord->coord.t[0]   = parentCoord->coord.t[0] + scratch->rotated.vx;
    fireballCoord->coord.t[1]   = parentCoord->coord.t[1] + scratch->rotated.vy;
    fireballCoord->coord.t[2]   = parentCoord->coord.t[2] + scratch->rotated.vz;
    fireballCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->direction.vx          = parentCoord->coord.m[0][2];
    work->direction.vy          = parentCoord->coord.m[1][2];
    work->direction.vz          = parentCoord->coord.m[2][2];

    _actor02400InitFireballCollision(task, work, parentWork);
    work->timer           = ACTOR_02400_FIREBALL_LIFETIME_FRAMES;
    work->wallBody.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
    // Flight and delayed teardown no longer follow the parent's lifetime.
    taskDetachFromParent(task);
    task->state = ACTOR_02400_FIREBALL_TASK_FLIGHT;
    SCRATCH_STACK_RELEASE_BLOCK(ActorOffsetScratch);
}

/// Flight handler of the projectile: moves it along `direction` on X and Z
/// and draws its glow. Once the lifetime `timer` runs out, its record is
/// hit, or its swept shape touches a surface whose room parameter blocks it,
/// it spawns the burst effect and moves the task to state 2.
static void Actor02400_Fn02AF0(Enemy* arg0, Task* arg1)
{
    _Actor02400FireballWork* work;
    GfxCoord*                coord;
    s32                      rec;
    s32                      spawn;

    coord = arg1->extra.tmd->coords;
    work  = arg1->work;
    spawn = 0;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            fireballDrawGlow(coord, 0x100);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            coord->coord.t[0]  += (work->direction.vx * 25) >> 9;
            coord->coord.t[2]  += (work->direction.vz * 25) >> 9;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            fireballDrawGlow(coord, 0x100);
            rec = work->wallContacts[0].key.value;
            if ((rec != 0) &&
                (Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1]
                                   [worldCollisionSurfaceClassFromKey(rec)]
                                       ->probePassThrough == WORLD_COLLISION_SURFACE_BLOCK_PROBES)) {
                spawn = 1;
            }
            worldCollisionClearContacts(work->wallContacts);
            work->timer--;
            if ((work->timer <= 0) || (work->strikeContacts[0].flags & WORLD_COLLISION_CONTACT_OCCUPIED) || (spawn != 0)) {
                effectSpawn(gRoomEffectOrangeBurst2Id, coord, 0, NULL);
                arg1->state        = 2;
                work->teardownStep = ACTOR_02400_FIREBALL_TEARDOWN_UNLINK;
            }
            break;
    }
}

#include "../../shared/fireball_ember.inc.c"

/// Dispatches the amoeba body's spawn, active or death handler.
///
/// Requires a live enemy in `spawnArg2` and `Task::state` in 0..2; there is no bounds
/// check. Copies the three callback pointers before dispatch. The selected
/// handler may destroy the enemy and task, so neither is used afterwards.
static void _actor02400BodyTask(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = Actor02400_D00004;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Per-frame handler of the main body. In global mode 2 it only hides the
/// model; in mode 1 it only refreshes the colour and the ground mark. Otherwise
/// it resolves contacts, runs the current mode, turns, steps forward, moves
/// the two front parts, rescales the model and updates its coordinate first.
static void Actor02400_Fn02E0C(Enemy* enemy, Task* task)
{
    GfxCoord*  coord;
    TmdObject* obj;

    obj   = task->extra.tmd;
    coord = obj->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            Actor02400_Fn031D0(task);
            _actor02400DrawShadow(task);
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            obj->flags                    = 0;
            enemy->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags                    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    Actor02400_Fn00C08(task);
    Actor02400_Fn02EDC(task);
    _actor02400TurnTowardTarget(task);
    _actor02400StepForward(task);
    _actor02400UpdateArm(task);
    _actor02400ApplyBodyScale(task);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    Actor02400_Fn031D0(task);
    _actor02400DrawShadow(task);
}

/// Runs the handler of the body's current `mode`. The crawling, striking and
/// casting modes also play the idle sound.
static void Actor02400_Fn02EDC(Task* task)
{
    _Actor02400Work* work = task->work;

    switch (work->mode) {
        case ACTOR_02400_MODE_DORMANT:
            _actor02400UpdateDormant(task);
            break;
        case ACTOR_02400_MODE_CRAWL:
            Actor02400_Fn01590(task);
            _actor02400UpdateIdleSound(task);
            break;
        case ACTOR_02400_MODE_STRIKE:
            _actor02400UpdateStrike(task);
            _actor02400UpdateIdleSound(task);
            break;
        case ACTOR_02400_MODE_CAST:
            Actor02400_Fn01B90(task);
            _actor02400UpdateIdleSound(task);
            break;
        case ACTOR_02400_MODE_HURT:
            _actor02400UpdateHurt(task);
            break;
        case ACTOR_02400_MODE_STUNNED:
            _actor02400UpdateStunned(task);
            break;
    }
}

/// Shakes the hit amoeba for 16 ticks before returning to crawl.
///
/// Requires live body work with counter reset on entry. Height oscillates
/// through the 5120/4096 and 6144/4096 thresholds, charge is cancelled and
/// arm pair tests stay disabled. Recovery resumes turning with a random wait
/// and re-enables those tests.
static void _actor02400UpdateHurt(Task* task)
{
    enum {
        ACTOR_02400_HURT_FRAMES     = 16,
        ACTOR_02400_HURT_SCALE_LOW  = 5 * ONE / 4,
        ACTOR_02400_HURT_SCALE_HIGH = 3 * ONE / 2,
    };

    _Actor02400Work* work;
    u32              randomState;

    work = task->work;
    if (work->swingDown == 0) {
        work->scale.vy += 0x200;
        if (work->scale.vy > ACTOR_02400_HURT_SCALE_HIGH) {
            work->swingDown = 1;
        }
    } else {
        work->scale.vy -= 0x200;
        if (work->scale.vy < ACTOR_02400_HURT_SCALE_LOW) {
            work->swingDown = 0;
        }
    }
    if (work->chargeEffect != NULL) {
        work->chargeEffect->task->state = ROOM_VISUAL_EFFECTS_GLOW_DISC_CANCEL;
        work->chargeEffect              = NULL;
    }
    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->counter++;
    if (work->counter >= ACTOR_02400_HURT_FRAMES) {
        randomState             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        work->mode              = ACTOR_02400_MODE_CRAWL;
        work->phase             = ACTOR_02400_CRAWL_PHASE_TURN;
        gRandomLcgState         = randomState;
        work->counter           = ((randomState >> 16) & 0x1F) + 0x1E;
        work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
}

/// Slides the amoeba's two arm joints along their local Z axes.
///
/// Requires at least four TMD coordinates. `armOut` extends the base by 20
/// and the tip by 80 coordinate units per tick; otherwise they retract by
/// 40 and 160, clamping Z at 0 and 30 respectively. Fixes the base at Y=-95
/// and the tip at Y=0, clears their X offsets and marks both matrices dirty.
static void _actor02400UpdateArm(Task* task)
{
    enum {
        ACTOR_02400_ARM_BASE_Y     = -95,
        ACTOR_02400_ARM_TIP_REST_Z = 30,
    };

    _Actor02400Work* work;
    GfxCoord*        coords;
    GfxCoord*        armBase;
    GfxCoord*        armTip;

    work    = task->work;
    coords  = task->extra.tmd->coords;
    armBase = coords + 2;
    armTip  = coords + 3;

    armBase->coord.t[0] = 0;
    armBase->coord.t[1] = ACTOR_02400_ARM_BASE_Y;
    if (work->armOut != 0) {
        armBase->coord.t[2] += 0x14;
    } else {
        armBase->coord.t[2] -= 0x28;
        if (armBase->coord.t[2] < 0) {
            armBase->coord.t[2] = 0;
        }
    }
    armBase->composeStamp = GRAPHICS_COORD_DIRTY;
    armTip->coord.t[0]    = 0;
    armTip->coord.t[1]    = 0;
    if (work->armOut != 0) {
        armTip->coord.t[2] += 0x50;
    } else {
        armTip->coord.t[2] -= 0xA0;
        if (armTip->coord.t[2] < ACTOR_02400_ARM_TIP_REST_Z) {
            armTip->coord.t[2] = ACTOR_02400_ARM_TIP_REST_Z;
        }
    }
    armTip->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Saves the amoeba's local position and steps it along its root's Z axis.
///
/// `speed` is coordinate units per tick, multiplied by the Q12 X/Z basis;
/// Y increases by 128 each tick before contact resolution on the next tick.
/// `prevPos` narrows all three coordinates to signed halfwords for rollback.
/// The caller scales the root and invalidates its composed matrix afterwards.
static void _actor02400StepForward(Task* task)
{
    _Actor02400Work* work;
    GfxCoord*        rootCoord;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    work->prevPos.vx       = rootCoord->coord.t[0];
    work->prevPos.vy       = rootCoord->coord.t[1];
    work->prevPos.vz       = rootCoord->coord.t[2];
    rootCoord->coord.t[0] += (rootCoord->coord.m[0][2] * work->speed) >> 12;
    rootCoord->coord.t[1] += 0x80;
    rootCoord->coord.t[2] += (rootCoord->coord.m[2][2] * work->speed) >> 12;
}

/// Refreshes the body's colour from where its root coordinate stands.
static void Actor02400_Fn031D0(Task* task)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = task->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &vec, 0, 0);
}

/// Draws a ground shadow at the amoeba root's composed world position.
///
/// Requires a live TMD task with an up-to-date root work matrix. The shadow
/// has half-side 512 coordinate units and GPU modulation shade 48.
static void _actor02400DrawShadow(Task* task)
{
    enum {
        ACTOR_02400_SHADOW_HALF_SIDE = 512,
        ACTOR_02400_SHADOW_SHADE     = 48,
    };

    GfxCoord* rootCoord;
    VECTOR3   worldPosition;

    rootCoord        = task->extra.tmd->coords;
    worldPosition.vx = rootCoord->workm.t[0];
    worldPosition.vy = rootCoord->workm.t[1];
    worldPosition.vz = rootCoord->workm.t[2];
    effectDrawGroundShadow(&worldPosition, ACTOR_02400_SHADOW_HALF_SIDE, ACTOR_02400_SHADOW_SHADE);
}

/// Multiplies a prepared local-axis scale into a saved root matrix.
///
/// Borrows a live root, a readable full saved matrix and a reserved
/// `ActorScaleScratch` whose three Q12 scale factors are initialized.
/// Scratch must be disjoint from both matrices. Its translation and vector
/// pad are unused. Restores the saved translation and marks composition dirty;
/// the caller owns scratch reservation and release.
static __inline__ void _actor02400ApplySavedRootScale(GfxCoord* rootCoord, const MATRIX* savedMatrix,
                                                      ActorScaleScratch* scaleScratch)
{
    rootCoord->coord = *savedMatrix;
    gfxSetRotIdentity(&scaleScratch->matrix);
    ScaleMatrix(&scaleScratch->matrix, &scaleScratch->scale);
    MulMatrix(&rootCoord->coord, &scaleScratch->matrix);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Applies the dying amoeba's Y scale to its saved root matrix.
///
/// Requires live TMD body work and 0x30 free initialized scratch-stack bytes.
/// `baseMatrix` is captured on death entry; `scale.vy` is Q12. Restoring that
/// matrix before scaling prevents successive death ticks compounding the
/// scale. X/Z factors are unity; marks composition dirty and releases scratch.
static void _actor02400ApplyDeathScale(Task* task)
{
    ActorScaleScratch* scaleScratch;
    _Actor02400Work*   work;
    GfxCoord*          rootCoord;

    scaleScratch = SCRATCH_STACK_RESERVE_BLOCK(ActorScaleScratch);
    rootCoord    = task->extra.tmd->coords;
    work         = task->work;

    scaleScratch->scale.vx = ONE;
    scaleScratch->scale.vy = work->scale.vy;
    scaleScratch->scale.vz = ONE;
    _actor02400ApplySavedRootScale(rootCoord, &work->baseMatrix, scaleScratch);
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}

/// Dispatches the fireball's spawn, flight or teardown handler.
///
/// Requires a live enemy in `spawnArg2` and `Task::state` in 0..2; there is no bounds
/// check. Copies the three callback pointers before dispatch. The selected
/// handler may destroy the enemy and task, so neither is used afterwards.
static void _actor02400FireballTask(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = Actor02400_D0003C;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Unlinks the fireball's collision bodies, then releases it after 60 ticks.
///
/// Requires live fireball work with `teardownStep` initially `UNLINK`. The first
/// call removes both strike spheres and the wall probe and arms the wait;
/// subsequent calls decrement it. Destruction releases the task-owned work
/// and coordinate body. Teardown continues while combat actors are paused.
static void _actor02400TeardownFireball(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_02400_FIREBALL_TEARDOWN_FRAMES = 60,
    };

    _Actor02400FireballWork* work;

    work = task->work;
    switch (work->teardownStep) {
        case ACTOR_02400_FIREBALL_TEARDOWN_UNLINK:
            worldCollisionUnlinkBody(&work->playerStrikeBody);
            worldCollisionUnlinkBody(&work->enemyStrikeBody);
            worldCollisionUnlinkBody(&work->wallBody);
            work->teardownStep = ACTOR_02400_FIREBALL_TEARDOWN_WAIT;
            work->timer        = ACTOR_02400_FIREBALL_TEARDOWN_FRAMES;
            return;
        case ACTOR_02400_FIREBALL_TEARDOWN_WAIT:
            work->timer--;
            if (work->timer <= 0) {
                enemyDestroy(enemy, task);
            }
            return;
    }
}
