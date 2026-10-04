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
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/object_fields.h"
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

/// States this package puts the charge effect's task in.
///
/// They are states of the glow disc the room stores in `gRoomEffectGlowDiscId`.
/// Spawned, the disc grows while sparks fly to it from the player.
enum {
    ACTOR_02400_CHARGE_EFFECT_FLICKER = 2, // Full size with a flickering second disc and no more sparks
    ACTOR_02400_CHARGE_EFFECT_RELEASE = 3, // Drifts away and fades inside a ring, then ends itself
    ACTOR_02400_CHARGE_EFFECT_CANCEL  = 4, // Ends at once
};

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

static void Actor02400_Fn0095C(Enemy* enemy, Task* task);
static void Actor02400_Fn024F8(Enemy* enemy, Task* task);
static void Actor02400_Fn02790(Enemy* enemy, Task* task);
static void Actor02400_Fn02AF0(Enemy* enemy, Task* task);
static void Actor02400_Fn02E0C(Enemy* enemy, Task* task);
static void Actor02400_Fn033B4(Enemy* enemy, Task* task);
static void Actor02400_Fn02EDC(Task* task);
static void Actor02400_Fn02F94(Task* task);
static void Actor02400_Fn03098(Task* task);
static void Actor02400_Fn03140(Task* task);
static void Actor02400_Fn031D0(Task* task);
static void Actor02400_Fn03228(Task* task);
static void Actor02400_Fn03278(Task* task);

static TmdSource _gActor02400AmoebaBody;
void             Actor02400_Fn02DB0(Task*);
void             Actor02400_Fn03358(Task*);

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
    { { { TASK_BODY_TMD, 96 } }, Actor02400_Fn02DB0, { .model = &_gActor02400AmoebaBody } },
    { { { TASK_BODY_COORD, 96 } }, Actor02400_Fn03358, { .value = 0 } },
};

static void Actor02400_Fn00C08(Task* task);
static void Actor02400_Fn01420(Task* task);
static void Actor02400_Fn01590(Task* task);
static void Actor02400_Fn01A10(Task* task);
static void Actor02400_Fn01B90(Task* task);
static void Actor02400_Fn01F74(Task* task);
static void Actor02400_Fn0208C(Task* task);
static void Actor02400_Fn02264(Task* task);
static void Actor02400_Fn023B4(Task* task);

#include "../../shared/fireball_glow.inc.c"

#include "../../shared/fireball_ground_glow.inc.c"

/// The main body's state handlers, run by `Actor02400_Fn02DB0` for the task's
/// state: spawn, per-frame tick and death.
static const EnemyTaskFuncTable3 Actor02400_D00004 = {
    { Actor02400_Fn0095C, Actor02400_Fn02E0C, Actor02400_Fn024F8 },
};

/// Spawn handler of the main body: allocates the work block, picks the model
/// and parameter variant from the placement, links the enemy node and both
/// collision bodies, starts dormant at `ACTOR_02400_SCALE_FLAT` with a random
/// countdown, and moves the task to state 1.
static void Actor02400_Fn0095C(Enemy* enemy, Task* task)
{
    TmdObject*       obj;
    GfxCoord*        coord;
    _Actor02400Work* work;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(_Actor02400Work), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->light;
    obj->colorMtx       = &work->color;
    work->variant       = enemy->place->mode & 1;
    if (work->variant != 0) {
        obj->clutRowOffset += 1;
        tmdProcessStream(obj);
        tmdProcessStream(obj);
    }
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    Gp_LinkNode(&enemy->node);
    enemy->bodyPos.vy             = -0x96;
    enemy->coord                  = coord;
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
    Gp_IncStateF0Ref(0);
    work->scale.vx              = ACTOR_02400_SCALE_FLAT;
    work->scale.vy              = ACTOR_02400_SCALE_FLAT;
    work->scale.vz              = ACTOR_02400_SCALE_FLAT;
    work->baseMatrix            = coord->coord;
    gRandomLcgState             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->counter               = (gRandomLcgState >> 16) & 0xF;
    work->body.key              = 0x30018;
    work->body.coord            = coord;
    work->body.context.contacts = work->bodyContacts;
    work->body.pos.vy           = -0xC8;
    work->body.pos.vx           = 0;
    work->body.pos.vz           = 0;
    work->body.radius           = 0xC8;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->body);
    Gp_InitRec18Table(work->bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);
    work->body.flags                 |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->attackBody.coord            = &task->extra.tmd->coords[3];
    work->attackBody.context.contacts = work->attackContacts;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 0x1F4;
    work->attackBody.key              = Gp_PackPair(Actor02400_BodyPairs, work->variant * 2);
    work->attackBody.radius           = 0x64;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->attackBody);
    Gp_InitRec18Table(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    task->state             = 1;
}

/// Resolves this frame's contacts. The push-back from `func_800E0C10` moves
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
    s32                             z;
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
    res     = func_800E0C10(work->bodyContacts, &scratch->delta, ARRAY_SIZE(work->bodyContacts), NULL);
    if (res == 1)
        goto move_delta;
    if (res < 2)
        goto move_done;
    if (res == 2)
        goto move_absolute;
    goto move_done;
move_delta:
    coord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
    coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
    z                  = coord->coord.t[2] + scratch->delta.fixed.vz.halves.integer;
    goto move_z;
move_absolute:
    coord->coord.t[0] = work->prevPos.vx;
    coord->coord.t[1] = work->prevPos.vy;
    z                 = work->prevPos.vz;
move_z:
    coord->coord.t[2] = z;
move_done:
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
                switch (Gp_GetIdParam0(work->bodyContacts[i].key.value) & 0xFFFF) {
                    case 3:
                    case 4:
                        kind = 3;
                        break;
                    case 1:
                    case 7:
                        kind = 1;
                        break;
                    case 0:
                    case 2:
                    case 5:
                    case 6:
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
                        work->staggerDamage     += Gp_ComputeDamage(work->bodyContacts[i].key.value, SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy + scratch->delta.vector.vz * scratch->delta.vector.vz), 0, 0);
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
                        damage                   = Gp_ComputeDamage(work->bodyContacts[i].key.value, SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy + scratch->delta.vector.vz * scratch->delta.vector.vz), 0, 0);
                        work->mode               = ACTOR_02400_MODE_HURT;
                        work->phase              = 0;
                        work->counter            = 0;
                        work->speed              = 0;
                        work->turnRate           = 0;
                        work->armOut             = 0;
                        param                    = Gp_GetIdParam1(work->bodyContacts[i].key.value) & 0xFFFF;
                        if (Actor02400_D0463C[param] == 0 && lastId != work->bodyContacts[i].key.value) {
                            lastId = work->bodyContacts[i].key.value;
                            func_800FDB18(param, coord, NULL, &work->effectArg);
                        }
                        break;
                    case 2:
                        src                      = gPlayerActorTasks[(work->bodyContacts[i].key.value >> 7) & 1]->extra.tmd->coords;
                        scratch->delta.vector.vx = src->coord.t[0] - coord->coord.t[0];
                        scratch->delta.vector.vy = src->coord.t[1] - coord->coord.t[1];
                        scratch->delta.vector.vz = src->coord.t[2] - coord->coord.t[2];
                        damage                   = (s16)Gp_ComputeDamage(work->bodyContacts[i].key.value, SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx + scratch->delta.vector.vy * scratch->delta.vector.vy + scratch->delta.vector.vz * scratch->delta.vector.vz), 0, 0) * 5;
                        work->mode               = ACTOR_02400_MODE_HURT;
                        work->phase              = 0;
                        work->counter            = 0;
                        work->speed              = 0;
                        work->turnRate           = 0;
                        work->armOut             = 0;
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 2, NULL);
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
                        Gp_SpawnEff(EFFECT_CRITICAL_HIT, coord, 2, NULL);
                        break;
                }
                dmg = damage;
                func_800E2C78(task->spawnArg2.pointer, work->bodyContacts[i].key.value, dmg, 0);
                func_800DA6E8(&((Enemy*)task->spawnArg2.pointer)->node, dmg, 0);
                if ((enemy->hp -= damage) <= 0) {
                    task->state = 2;
                }
                stun = Gp_GetIdParam2(work->bodyContacts[i].key.value);
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
    Gp_ClearRec18Occupied(work->bodyContacts);
    // The attack body touched the player: disable it until the body crawls again and take the MP.
    if (Gp_FindRec18(work->attackContacts, 0) != 0) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ClearRec18Occupied(work->attackContacts);
        work->attackLanded = 1;
        Gp_SpendMp(Actor02400_D045D8[work->variant]);
    }
    if (work->staggerWindow != 0) {
        work->staggerWindow--;
        if (work->staggerWindow <= 0) {
            work->staggerDamage = 0;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactOverlapPushScratch);
}

/// The projectile's state handlers, run by `Actor02400_Fn03358` for the task's
/// state: spawn, flight and teardown.
static const EnemyTaskFuncTable3 Actor02400_D0003C = {
    { Actor02400_Fn02790, Actor02400_Fn02AF0, Actor02400_Fn033B4 },
};

/// `ACTOR_02400_MODE_DORMANT`: counts `counter` down, re-arming it at random and
/// checking the global wake flag each time it runs out, and wakes the body
/// (`ACTOR_02400_CRAWL_PHASE_WAKE` at unit scale) when the player comes within
/// 1500 units on the ground plane or once a projectile has been spawned.
static void Actor02400_Fn01420(Task* task)
{
    _Actor02400Work* work;
    GfxCoord*        coord;
    s32              flag;
    s32              dx;
    s32              dz;
    u32              random;
    VECTOR*          delta;
    VECTOR*          scratchEnd;

    scratchEnd                                                                = *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET);
    delta                                                                     = scratchEnd - 1;
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = delta;
    work                                                                      = task->work;
    coord                                                                     = task->extra.tmd->coords;
    flag                                                                      = 0;
    if (--work->counter < 0) {
        random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        work->counter   = (random >> 0x10) & 0xF;
        gRandomLcgState = random;
        if (gSceneCombatState.signals.bytes.actionFlags & SCENE_COMBAT_ACTION_PE_ACTIVE) {
            flag = 1;
        }
    }
    scratchEnd[-1].vx = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
    delta->vy         = 0;
    dz                = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    delta->vz         = dz;
    dx                = scratchEnd[-1].vx;
    if (SquareRoot0((dx * dx) + (dz * dz)) < 0x5DC) {
        flag = 1;
    }
    if (gSceneCombatState.actor02400Alert != 0) {
        flag = 1;
    }
    if (flag != 0) {
        work->mode     = ACTOR_02400_MODE_CRAWL;
        work->phase    = ACTOR_02400_CRAWL_PHASE_WAKE;
        work->scale.vx = ONE;
        work->scale.vy = ONE;
        work->scale.vz = ONE;
        work->counter  = 0;
        Gp_ArmStateF0(1);
    }
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) += 1;
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
        effect             = Gp_SpawnEff(gRoomEffectGlowDiscId, coord, (s32)(work->variant), NULL);
        work->chargeEffect = effect;
        if (effect != NULL) {
            taskReparent(task, effect->task);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

/// `ACTOR_02400_MODE_STRIKE`: `EXTEND` holds `armOut` for up to 7 frames, cut
/// short while `attackLanded` is set; `RETRACT` clears it and after 7 frames
/// either moves on to `ACTOR_02400_MODE_CAST`, when `attackLanded` is set, or
/// returns to crawling with a random wait. Every frame the height swings
/// between 0xF00 and 0x1100.
static void Actor02400_Fn01A10(Task* task)
{
    _Actor02400Work* work;
    s32              phase;

    work  = task->work;
    phase = work->phase;
    switch (phase) {
        case ACTOR_02400_STRIKE_PHASE_EXTEND:
            work->armOut = 1;
            work->counter++;
            if (work->counter >= 7) {
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
            if (work->counter >= 7) {
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
        if (work->scale.vy > ONE + 0x100) {
            work->swingDown = 1;
        }
    } else {
        work->scale.vy -= 0x80;
        if (work->scale.vy < ONE - 0x100) {
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
                    work->chargeEffect->task->state = ACTOR_02400_CHARGE_EFFECT_FLICKER;
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
            Gp_SpawnEnemyFromTable(Actor02400_D0465C, 1, 0, task->spawnArg2.pointer);
            gSceneCombatState.actor02400Alert = 1;
            work->phase                       = ACTOR_02400_CAST_PHASE_SHRINK;
            if (work->chargeEffect != NULL) {
                work->chargeEffect->task->state = ACTOR_02400_CHARGE_EFFECT_RELEASE;
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

/// `ACTOR_02400_MODE_STUNNED`: shrinks the scale toward the flat crawling
/// shape, cancels the charge effect and keeps `attackBody` disabled; after 360
/// frames returns to crawling with a random wait.
static void Actor02400_Fn01F74(Task* task)
{
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
        work->chargeEffect->task->state = ACTOR_02400_CHARGE_EFFECT_CANCEL;
        work->chargeEffect              = NULL;
    }
    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->counter++;
    if (work->counter > 0x168) {
        work->mode              = ACTOR_02400_MODE_CRAWL;
        work->phase             = ACTOR_02400_CRAWL_PHASE_TURN;
        gRandomLcgState         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->counter           = ((gRandomLcgState >> 16) & 0x1F) + 0x1E;
        work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
}

/// Saves the root coordinate's matrix in `baseMatrix` and scales it per axis by
/// `scale`, keeping the coordinate's translation. The height of `body` and the
/// reach of `attackBody` follow the Y and Z scale.
static void Actor02400_Fn0208C(Task* task)
{
    GfxCoord*                coord;
    _Actor02400Work*         work;
    _Actor02400ScaleScratch* scratch;
    _Actor02400ScaleScratch* head;

    coord                                         = task->extra.tmd->coords;
    work                                          = task->work;
    work->baseMatrix                              = coord->coord;
    head                                          = SCRATCH_STACK_CURSOR(_Actor02400ScaleScratch);
    scratch                                       = head - 1;
    SCRATCH_STACK_CURSOR(_Actor02400ScaleScratch) = scratch;
    work->body.pos.vy                             = -0xC8000 / work->scale.vy;
    work->attackBody.pos.vz                       = (work->scale.vz * 250) / 4096;
    scratch->rescale.scale.vx                     = work->scale.vx;
    scratch->rescale.scale.vy                     = work->scale.vy;
    scratch->rescale.scale.vz                     = work->scale.vz;
    scratch->translation.vx                       = coord->coord.t[0];
    scratch->translation.vy                       = coord->coord.t[1];
    scratch->translation.vz                       = coord->coord.t[2];
    coord->coord                                  = work->baseMatrix;
    scratch->rescale.matrix.rotationWords.m00M01  = ONE;
    scratch->rescale.matrix.rotationWords.m02M10  = 0;
    scratch->rescale.matrix.rotationWords.m11M12  = ONE;
    scratch->rescale.matrix.rotationWords.m20M21  = 0;
    scratch->rescale.matrix.rotationWords.m22     = ONE;
    ScaleMatrix(&scratch->rescale.matrix.mat, &scratch->rescale.scale);
    MulMatrix(&coord->coord, &scratch->rescale.matrix.mat);
    coord->coord.t[0] = scratch->translation.vx;
    coord->coord.t[1] = scratch->translation.vy;
    SCRATCH_STACK_RELEASE_BLOCK(_Actor02400ScaleScratch);
    coord->coord.t[2]   = scratch->translation.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Turns the body towards `targetYaw` by at most `turnRate` per frame and
/// rebuilds the root coordinate's rotation from the resulting `yaw`. The yaw
/// wraps at 0x1000: when the remaining turn would overshoot through the wrap the
/// body snaps to the target instead.
static void Actor02400_Fn02264(Task* task)
{
    _Actor02400Work*  work;
    GfxCoord*         coord;
    ActorFaceScratch* sc;
    s32               ang;
    u16               want;
    s16               diff;
    s32               adiff;
    s32               step;
    s32               cur;
    s32               next;
    s32               wrapStep;

    sc    = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    coord = task->extra.tmd->coords;
    work  = task->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->targetYaw;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->yaw = ang;
    if (adiff < 0x800) {
        step = work->turnRate;
        if (step >= adiff) {
            work->yaw = want;
        } else {
            next = work->yaw;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            work->yaw = next;
        }
    } else {
        step = work->turnRate;
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->yaw = work->targetYaw;
        goto done;
    turn:
        wrapStep = work->turnRate;
        cur      = work->yaw;
        if (diff > 0) {
            work->yaw = cur - wrapStep;
        } else {
            work->yaw = cur + wrapStep;
        }
    }
done:
    sc->rot.vx = 0;
    sc->rot.vy = work->yaw;
    sc->rot.vz = 0;
    RotMatrix(&sc->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

/// Counts `idleSoundTimer` and every 25 frames plays the body's idle sound,
/// panned and placed from the root coordinate and made louder the taller the
/// body has grown.
static void Actor02400_Fn023B4(Task* task)
{
    GfxCoord*        object;
    s16              scale;
    s16              ramp;
    s32              soundId;
    s32              volume;
    s8               depth;
    _Actor02400Work* work;

    work   = task->work;
    object = task->extra.tmd->coords;
    work->idleSoundTimer++;
    if (work->idleSoundTimer >= 0x19) {
        work->idleSoundTimer = 0;
        scale                = work->scale.vy;
        soundId              = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40180001;
        // The volume ramps over the height's whole range, flat to the top of the cast's wobble.
        if (scale > ACTOR_02400_SCALE_SWOLLEN + 0x100) {
            ramp = ACTOR_02400_SCALE_SWOLLEN + 0x100 - ACTOR_02400_SCALE_FLAT;
        } else {
            ramp = work->scale.vy - ACTOR_02400_SCALE_FLAT;
            if (scale < ACTOR_02400_SCALE_FLAT) {
                ramp = 0;
            }
        }
        volume = (ramp * 0x32) / (ACTOR_02400_SCALE_SWOLLEN + 0x100 - ACTOR_02400_SCALE_FLAT) + 0x32;
        depth  = 0x7F - (((0x7F - worldCoordGetOriginAudioDepth(object)) * (s16)volume) / 100);
        sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(object), depth);
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
            Gp_UpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
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
                    Gp_UnlinkObj(&work->body);
                    Gp_UnlinkObj(&work->attackBody);
                    Gp_SetLightMode(arg0, ENEMY_COLOR_WEIGHTED);
                    Gp_ReleaseStateF0Add(arg1, 0x18);
                    work->phase = ACTOR_02400_DEATH_PHASE_SQUASH;
                    cur         = arg1->extra.tmd->coords;
                    pos.vx      = cur->workm.t[0];
                    pos.vy      = cur->workm.t[1];
                    pos.vz      = cur->workm.t[2];
                    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
                    if (work->chargeEffect != NULL) {
                        work->chargeEffect->task->state = ACTOR_02400_CHARGE_EFFECT_CANCEL;
                    }
                    break;
                case ACTOR_02400_DEATH_PHASE_SQUASH:
                    work->counter++;
                    if (work->counter == 10) {
                        obj->flags = TMD_OBJECT_SEMI_TRANS;
                    }
                    if (work->counter == 15) {
                        Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 3, NULL);
                    }
                    if (work->counter >= 60) {
                        work->phase = ACTOR_02400_DEATH_PHASE_DESTROY;
                        obj->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    if (work->scale.vy > 0x200) {
                        work->scale.vy -= 0x50;
                    }
                    Actor02400_Fn03278(arg1);
                    cur    = arg1->extra.tmd->coords;
                    pos.vx = cur->workm.t[0];
                    pos.vy = cur->workm.t[1];
                    pos.vz = cur->workm.t[2];
                    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
                    break;
                case ACTOR_02400_DEATH_PHASE_DESTROY:
                    enemyDestroy(arg0, arg1);
                    break;
            }
            break;
    }
}

/// Spawn handler of the projectile: allocates its work block, places its model
/// 0x15E units along the parent's Y axis with the parent's rotation, takes the
/// parent's Z axis as its direction, links its three collision bodies (keys
/// picked by the parent's variant), arms the 90-frame lifetime, detaches from
/// the parent and moves the task to state 1.
static void Actor02400_Fn02790(Enemy* arg0, Task* arg1)
{
    _Actor02400Work*         parentWork;
    Task*                    parent;
    _Actor02400FireballWork* work;
    ActorOffsetScratch*      scratch;
    ActorOffsetScratch*      head;
    GfxCoord*                objCoord;
    GfxCoord*                objCoord2;
    GfxCoord*                objCoord3;
    SVECTOR*                 offset;
    GfxCoord*                coord;
    GfxCoord*                parentCoord;

    head                                     = SCRATCH_STACK_CURSOR(ActorOffsetScratch);
    scratch                                  = head - 1;
    SCRATCH_STACK_CURSOR(ActorOffsetScratch) = scratch;
    offset                                   = &scratch->offset;
    parent                                   = arg1->parent;
    coord                                    = arg1->extra.tmd->coords;
    parentCoord                              = parent->extra.tmd->coords;
    parentWork                               = parent->work;
    work                                     = memCalloc(sizeof(_Actor02400FireballWork), 0);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work         = work;
    scratch->offset.vx = 0;
    scratch->offset.vy = -0x15E;
    scratch->offset.vz = 0;
    gte_SetRotMatrix(&parentCoord->coord);
    gte_ldv0(offset);
    gte_rtv0();
    gte_stlvnl(&scratch->rotated);
    coord->parent       = &gGfxViewCoord;
    coord->coord        = parentCoord->coord;
    coord->coord.t[0]   = parentCoord->coord.t[0] + scratch->rotated.vx;
    coord->coord.t[1]   = parentCoord->coord.t[1] + scratch->rotated.vy;
    coord->coord.t[2]   = parentCoord->coord.t[2] + scratch->rotated.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->direction.vx  = parentCoord->coord.m[0][2];
    work->direction.vy  = parentCoord->coord.m[1][2];
    work->direction.vz  = parentCoord->coord.m[2][2];

    objCoord                                = arg1->extra.tmd->coords;
    work->playerStrikeBody.context.contacts = work->strikeContacts;
    work->playerStrikeBody.pos.vx           = 0;
    work->playerStrikeBody.pos.vy           = 0;
    work->playerStrikeBody.pos.vz           = 0;
    work->playerStrikeBody.coord            = objCoord;
    work->playerStrikeBody.key              = Gp_PackPair(Actor02400_BodyPairs, (parentWork->variant * 2) | 1);
    work->playerStrikeBody.radius           = 0xC8;
    work->playerStrikeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->playerStrikeBody);
    Gp_InitRec18Table(work->strikeContacts, ARRAY_SIZE(work->strikeContacts), 0);
    work->playerStrikeBody.flags          |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    objCoord2                              = arg1->extra.tmd->coords;
    work->enemyStrikeBody.context.contacts = work->strikeContacts;
    work->enemyStrikeBody.pos.vx           = 0;
    work->enemyStrikeBody.pos.vy           = 0;
    work->enemyStrikeBody.pos.vz           = 0;
    work->enemyStrikeBody.coord            = objCoord2;
    if (parentWork->variant == 0) {
        work->enemyStrikeBody.key = 0x22D2D;
    } else {
        work->enemyStrikeBody.key = 0x22E2E;
    }
    work->enemyStrikeBody.radius = 0xC8;
    work->enemyStrikeBody.flags  = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(1, &work->enemyStrikeBody);

    work->wallCapsule.ends[1].vz   = -0xD2;
    work->wallCapsule.end0Radius   = 1;
    work->wallCapsule.end1Radius   = 1;
    work->wallCapsule.ends[0].vx   = 0;
    work->wallCapsule.ends[0].vy   = 0;
    work->wallCapsule.ends[0].vz   = 0;
    work->wallCapsule.ends[1].vx   = 0;
    work->wallCapsule.ends[1].vy   = 0;
    work->wallCapsule.contacts     = work->wallContacts;
    work->enemyStrikeBody.flags   |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    objCoord3                      = arg1->extra.tmd->coords;
    work->wallBody.context.capsule = &work->wallCapsule;
    work->wallBody.pos.vx          = 0;
    work->wallBody.pos.vy          = 0;
    work->wallBody.pos.vz          = 0;
    work->wallBody.key             = 0;
    work->wallBody.radius          = 0;
    work->wallBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->wallBody.coord           = objCoord3;
    Gp_LinkObj(3, &work->wallBody);
    Gp_InitRec18Table(work->wallContacts, ARRAY_SIZE(work->wallContacts), 0);
    work->timer           = 90;
    work->wallBody.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
    taskDetachFromParent(arg1);
    arg1->state = 1;
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
                                   [func_800E1B24(rec)]
                                       ->probePassThrough == WORLD_COLLISION_SURFACE_BLOCK_PROBES)) {
                spawn = 1;
            }
            Gp_ClearRec18Occupied(work->wallContacts);
            work->timer--;
            if ((work->timer <= 0) || (work->strikeContacts[0].flags & WORLD_COLLISION_CONTACT_OCCUPIED) || (spawn != 0)) {
                Gp_SpawnEff(gRoomEffectOrangeBurst2Id, coord, 0, NULL);
                arg1->state        = 2;
                work->teardownStep = ACTOR_02400_FIREBALL_TEARDOWN_UNLINK;
            }
            break;
    }
}

#include "../../shared/fireball_ember.inc.c"

/// Task callback of the main body: runs the `Actor02400_D00004` handler for
/// the task's state.
void Actor02400_Fn02DB0(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor02400_D00004;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
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
            goto case1;
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
    Actor02400_Fn02264(task);
    Actor02400_Fn03140(task);
    Actor02400_Fn03098(task);
    Actor02400_Fn0208C(task);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
case1:
    Actor02400_Fn031D0(task);
    Actor02400_Fn03228(task);
}

/// Runs the handler of the body's current `mode`. The crawling, striking and
/// casting modes also play the idle sound.
static void Actor02400_Fn02EDC(Task* task)
{
    _Actor02400Work* work = task->work;

    switch (work->mode) {
        case ACTOR_02400_MODE_DORMANT:
            Actor02400_Fn01420(task);
            break;
        case ACTOR_02400_MODE_CRAWL:
            Actor02400_Fn01590(task);
            Actor02400_Fn023B4(task);
            break;
        case ACTOR_02400_MODE_STRIKE:
            Actor02400_Fn01A10(task);
            Actor02400_Fn023B4(task);
            break;
        case ACTOR_02400_MODE_CAST:
            Actor02400_Fn01B90(task);
            Actor02400_Fn023B4(task);
            break;
        case ACTOR_02400_MODE_HURT:
            Actor02400_Fn02F94(task);
            break;
        case ACTOR_02400_MODE_STUNNED:
            Actor02400_Fn01F74(task);
            break;
    }
}

/// `ACTOR_02400_MODE_HURT`: swings the height between 0x1400 and 0x1800 in
/// steps of 0x200, cancels the charge effect and keeps `attackBody` disabled;
/// after 16 frames returns to crawling with a random wait.
static void Actor02400_Fn02F94(Task* task)
{
    _Actor02400Work* work;
    u32              state;

    work = task->work;
    if (work->swingDown == 0) {
        work->scale.vy += 0x200;
        if (work->scale.vy > 0x1800) {
            work->swingDown = 1;
        }
    } else {
        work->scale.vy -= 0x200;
        if (work->scale.vy < 0x1400) {
            work->swingDown = 0;
        }
    }
    if (work->chargeEffect != NULL) {
        work->chargeEffect->task->state = ACTOR_02400_CHARGE_EFFECT_CANCEL;
        work->chargeEffect              = NULL;
    }
    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->counter++;
    if (work->counter >= 0x10) {
        state                   = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
        work->mode              = ACTOR_02400_MODE_CRAWL;
        work->phase             = ACTOR_02400_CRAWL_PHASE_TURN;
        gRandomLcgState         = state;
        work->counter           = ((state >> 0x10) & 0x1F) + 0x1E;
        work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
}

/// Moves the model's arm: coordinates 2 and 3 slide out along their Z axis
/// while `armOut` is set and back while it is clear, part 2 down to 0 and part 3
/// down to 0x1E.
static void Actor02400_Fn03098(Task* task)
{
    _Actor02400Work* work;
    GfxCoord*        coord;
    GfxCoord*        c2;
    GfxCoord*        c3;

    work  = task->work;
    coord = task->extra.tmd->coords;
    c2    = coord + 2;
    c3    = coord + 3;

    c2->coord.t[0] = 0;
    c2->coord.t[1] = -0x5F;
    if (work->armOut != 0) {
        c2->coord.t[2] += 0x14;
    } else {
        c2->coord.t[2] -= 0x28;
        if (c2->coord.t[2] < 0) {
            c2->coord.t[2] = 0;
        }
    }
    c2->composeStamp = GRAPHICS_COORD_DIRTY;
    c3->coord.t[0]   = 0;
    c3->coord.t[1]   = 0;
    if (work->armOut != 0) {
        c3->coord.t[2] += 0x50;
    } else {
        c3->coord.t[2] -= 0xA0;
        if (c3->coord.t[2] < 0x1E) {
            c3->coord.t[2] = 0x1E;
        }
    }
    c3->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Saves the root coordinate's position into `prevPos`, then steps it forward
/// along its own Z axis by `speed` and drops it 0x80.
static void Actor02400_Fn03140(Task* task)
{
    _Actor02400Work* work;
    GfxCoord*        coord;

    coord = task->extra.tmd->coords;
    work  = task->work;

    work->prevPos.vx   = coord->coord.t[0];
    work->prevPos.vy   = coord->coord.t[1];
    work->prevPos.vz   = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->speed) >> 12;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->speed) >> 12;
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
    Gp_UpdateActorColor(task->spawnArg2.pointer, &vec, 0, 0);
}

/// Draws the body's ground mark at its root coordinate's world position.
static void Actor02400_Fn03228(Task* task)
{
    GfxCoord* coord;
    VECTOR3   vec;

    coord  = task->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x200, 0x30);
}

/// Squashes the dying body: restores `baseMatrix` into the root coordinate and
/// scales it on Y by `scale.vy`.
static void Actor02400_Fn03278(Task* task)
{
    void**             scratch;
    ActorScaleScratch* head;
    ActorScaleScratch* blk;
    _Actor02400Work*   work;
    GfxCoord*          coord;

    scratch                                     = SCRATCH_HEAD_ADDR;
    head                                        = SCRATCH_HEAD_AT(scratch, ActorScaleScratch);
    blk                                         = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorScaleScratch) = blk;
    coord                                       = task->extra.tmd->coords;
    work                                        = task->work;

    blk->scale.vx                    = ONE;
    blk->scale.vy                    = work->scale.vy;
    blk->scale.vz                    = ONE;
    coord->coord                     = work->baseMatrix;
    blk->matrix.rotationWords.m00M01 = ONE;
    blk->matrix.rotationWords.m02M10 = 0;
    blk->matrix.rotationWords.m11M12 = ONE;
    blk->matrix.rotationWords.m20M21 = 0;
    blk->matrix.rotationWords.m22    = ONE;
    ScaleMatrix(&blk->matrix.mat, &blk->scale);
    MulMatrix(&coord->coord, &blk->matrix.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_POP_AT(scratch, ActorScaleScratch);
}

/// Task callback of the projectile: runs the `Actor02400_D0003C` handler for
/// the task's state.
void Actor02400_Fn03358(Task* arg0)
{
    EnemyTaskFuncTable3 sp;

    sp = Actor02400_D0003C;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

/// Teardown handler of the projectile: `UNLINK` unlinks its three collision
/// bodies and arms a 60-frame wait, then `WAIT` counts it down and destroys
/// the enemy.
static void Actor02400_Fn033B4(Enemy* arg0, Task* arg1)
{
    _Actor02400FireballWork* work;

    work = arg1->work;
    switch (work->teardownStep) {
        case ACTOR_02400_FIREBALL_TEARDOWN_UNLINK:
            Gp_UnlinkObj(&work->playerStrikeBody);
            Gp_UnlinkObj(&work->enemyStrikeBody);
            Gp_UnlinkObj(&work->wallBody);
            work->teardownStep = ACTOR_02400_FIREBALL_TEARDOWN_WAIT;
            work->timer        = 60;
            return;
        case ACTOR_02400_FIREBALL_TEARDOWN_WAIT:
            work->timer--;
            if (work->timer <= 0) {
                enemyDestroy(arg0, arg1);
            }
            return;
    }
}
