#include "actor_205200_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/damage.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/random.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/shelter_b6_corridor.h"

#include "rooms/shelter_b6_training_room.h"

/// Values of `_Actor205200Work::anim`: indices into the package's
/// animation-set table.
enum {
    ACTOR_205200_ANIM_IDLE         = 1, // between the others; the slots are reset on it at spawn
    ACTOR_205200_ANIM_HEAL         = 2, // answer to the paired enemy's heal request: 84 ticks, the heal is signalled on tick 60
    ACTOR_205200_ANIM_HIT_REACTION = 3, // `ACTION_HIT_REACTION`: 35 ticks
    ACTOR_205200_ANIM_FIDGET       = 5  // drawn at random while idle: 64 ticks
};

/// Values of `_Actor205200Work::action`.
enum {
    ACTOR_205200_ACTION_IDLE         = 0, // waits, fidgets and answers the paired enemy's heal requests
    ACTOR_205200_ACTION_HIT_REACTION = 1  // reacts to a hit and has the paired enemy reset
};

/// Values of `_Actor205200Work::actionStep` under `ACTOR_205200_ACTION_IDLE`.
enum {
    ACTOR_205200_IDLE_READY      = 0, // a pending heal request is taken; the fidget draw runs
    ACTOR_205200_IDLE_HEAL_BEGIN = 1, // starts the heal animation
    ACTOR_205200_IDLE_HEAL       = 2, // the heal animation plays
    ACTOR_205200_IDLE_COOLDOWN   = 3, // `healCooldown` runs down and heal requests stay pending; the fidget draw runs
    ACTOR_205200_IDLE_FIDGET     = 4  // the fidget animation plays, then `COOLDOWN` or `READY` follows
};

/// Values of `_Actor205200Work::actionStep` under
/// `ACTOR_205200_ACTION_HIT_REACTION`. The action ends by returning to
/// `ACTOR_205200_ACTION_IDLE` at `ACTOR_205200_IDLE_COOLDOWN`.
enum {
    ACTOR_205200_HIT_REACTION_BEGIN = 0, // starts the animation and raises the paired enemy's reset request
    ACTOR_205200_HIT_REACTION_WAIT  = 1  // the animation plays
};

/// Values of `_Actor205200Work::knockbackStep`.
enum {
    ACTOR_205200_KNOCKBACK_BEGIN = 0, // starts the player's first animation with its rumble, sound and effect
    ACTOR_205200_KNOCKBACK_PUSH  = 1, // moves the player away for 16 ticks, then starts the second animation
    ACTOR_205200_KNOCKBACK_END   = 2  // waits for the second animation to end and releases the player
};

/// Values of `_Actor205200Work::room`: the room the actor was placed in.
enum {
    ACTOR_205200_ROOM_CORRIDOR      = 0, // Shelter B6 corridor
    ACTOR_205200_ROOM_TRAINING_ROOM = 1  // Shelter B6 training room
};

/// Ticks `_Actor205200Work::healCooldown` is loaded with when a heal or a hit
/// reaction ends.
enum { ACTOR_205200_HEAL_COOLDOWN = 600 };

/// States accepted by the stationary body's task dispatcher.
enum {
    ACTOR_205200_BODY_TASK_SPAWN   = 0,
    ACTOR_205200_BODY_TASK_LIVE    = 1,
    ACTOR_205200_BODY_TASK_DESTROY = 2
};

/// Work block of the package's enemy task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It
/// holds the animation rig, storage for the model's matrices, two collision
/// spheres with their contact tables, and the state the per-frame tick runs
/// the actor with. Timers count ticks.
///
/// The actor takes no damage and does not move. It is the partner of actor
/// 105100 in `gSceneCombatState.pairedEnemySignals`: it answers that enemy's
/// heal requests, and a hit it reacts to makes that enemy reset. A player
/// character touching it is knocked back.
typedef struct {
    ActorAnimRig19        rig;                 // playback of the model's parts; slots 1 to 18 are driven
    MATRIX                colorMtx;            // storage for the model's `TmdObject::colorMtx`
    MATRIX                lightMtx;            // storage for the model's `TmdObject::lightMtx`
    WorldCollisionBody    hitBody;             // sphere of radius 300 on the model's root, 300 up, that takes the hits
    WorldCollisionContact hitContacts[3];      // contacts of `hitBody`; also the enemy's hit records
    WorldCollisionBody    touchBody;           // sphere on the model's root that senses what touches the actor; `room` picks its centre and radius
    WorldCollisionContact touchContacts[1];    // contacts of `touchBody`; a player character's body in it starts the knockback
    byte                  unknown_51C[0x38];   // No access found; role unproven
    EffectSpawnArg        hitEffectArg;        // argument record of the effect a landed hit spawns, hung off the model's part 3
    byte                  unknown_55C[0x20];   // No access found; role unproven
    s16                   hitCooldown;         // ticks further hits are ignored for, set by a hit whose id asks for it
    s16                   anim;                // `ACTOR_205200_ANIM_*` the actions ask for; 0 from spawn until the first request
    s16                   playingAnim;         // `anim` the slots were last started on
    s16                   animFrame;           // ticks since `playingAnim` was started
    s16                   action;              // `ACTOR_205200_ACTION_*`
    s16                   actionStep;          // stage of the running action: `ACTOR_205200_IDLE_*` or `ACTOR_205200_HIT_REACTION_*`
    s16                   knockbackActive;     // 1 from the player touching `touchBody` until the knockback has ended (0 otherwise)
    s16                   knockbackStep;       // `ACTOR_205200_KNOCKBACK_*`
    s16                   knockbackFrame;      // ticks in the knockback's current stage
    s16                   knockbackFromBehind; // 1 when the player touched the actor while facing away from it, which picks the knockback's animations and facing (0 otherwise)
    s16                   healCooldown;        // ticks before a heal request is answered again: 600 from the end of a heal or of a hit reaction
    s16                   fidgetTimer;         // ticks until the next draw for the fidget animation, 15 to 46 between draws
    s16                   destroyRequested;    // 1 once an actor command has asked for the actor's teardown (0 otherwise)
    s16                   room;                // `ACTOR_205200_ROOM_*`, copied from the placement's `mode`; picks the touch sphere, the room's own tick and the knockback's effect and sound
} _Actor205200Work;
STATIC_ASSERT_SIZEOF(_Actor205200Work, 0x598);

/// Animation block the attack body hands the player with message 0x3F4.
extern AnimationSet* D_actor_205200_80156800[5];
extern AnimationSet* D_actor_205200_801567E8[6];
extern s16           D_actor_205200_801567B0[];
// One collision centre for each of the two placement modes.
extern SVECTOR D_actor_205200_801567B4[2];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_205200_801567D0[3];

static void _actor205200SpawnBody(Enemy* enemy, Task* task);
static void _actor205200ScanContacts(Task* task);
static void _actor205200TickIdle(Task* task);
static void _actor205200TickPlayerKnockback(Task* task);
static void func_actor_205200_8014C59C(Enemy* arg0, Task* arg1);
static void func_actor_205200_8014C67C(Task* arg0);
static void _actor205200TickHitReaction(Task* task);
static void _actor205200UpdateAnimation(Task* task);
static void _actor205200UpdateLighting(Task* task);
static void _actor205200DrawShadow(Task* task);
static void _actor205200DestroyBody(Enemy* enemy, Task* task);

/// The actor's own state handlers - spawn, per-frame tick and teardown - that
/// `_actor205200BodyTask` dispatches through by state.
static const EnemyTaskFuncTable3 D_actor_205200_80149E30 = {
    _actor205200SpawnBody,
    func_actor_205200_8014C59C,
    _actor205200DestroyBody,
};

static AnimationSet _gActor205200Animation08B50;
static AnimationSet _gActor205200Animation0943C;
static AnimationSet _gActor205200Animation09C1C;
static AnimationSet _gActor205200Animation0A964;
static AnimationSet _gActor205200Animation0B1AC;
static AnimationSet _gActor205200Animation0B948;
static AnimationSet _gActor205200Animation0C154;
static AnimationSet _gActor205200Animation0C968;

static s32  _actor205200SetModelDrawMsg(Task* task, s32 messageId, s32 drawEnabled, s32 unusedSecondArg);
static s32  _actor205200ApplyCommandMsg(Task* task, s32 messageId, const ActorCommand* request, s32 unusedSecondArg);
static void _actor205200BodyTask(Task* task);

static AnimationPackedPose _gActor205200Animation08B50Bank1[29] = {
#include "assets/actor_205200_animation_08B50_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation08B50Bank4[472] = {
#include "assets/actor_205200_animation_08B50_bank4.inc"
};

static AnimationRecord _gActor205200Animation08B50Records[543] = {
#include "assets/actor_205200_animation_08B50_records.inc"
};

static u16 _gActor205200Animation08B50Indices[20] = {
#include "assets/actor_205200_animation_08B50_indices.inc"
};

static AnimationSet _gActor205200Animation08B50 = {
    _gActor205200Animation08B50Records,
    _gActor205200Animation08B50Indices,
    { NULL, _gActor205200Animation08B50Bank1, NULL, NULL, _gActor205200Animation08B50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation0943CBank1[16] = {
#include "assets/actor_205200_animation_0943C_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation0943CBank4[223] = {
#include "assets/actor_205200_animation_0943C_bank4.inc"
};

static AnimationRecord _gActor205200Animation0943CRecords[280] = {
#include "assets/actor_205200_animation_0943C_records.inc"
};

static u16 _gActor205200Animation0943CIndices[20] = {
#include "assets/actor_205200_animation_0943C_indices.inc"
};

static AnimationSet _gActor205200Animation0943C = {
    _gActor205200Animation0943CRecords,
    _gActor205200Animation0943CIndices,
    { NULL, _gActor205200Animation0943CBank1, NULL, NULL, _gActor205200Animation0943CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation09C1CBank1[11] = {
#include "assets/actor_205200_animation_09C1C_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation09C1CBank4[167] = {
#include "assets/actor_205200_animation_09C1C_bank4.inc"
};

static AnimationRecord _gActor205200Animation09C1CRecords[284] = {
#include "assets/actor_205200_animation_09C1C_records.inc"
};

static u16 _gActor205200Animation09C1CIndices[20] = {
#include "assets/actor_205200_animation_09C1C_indices.inc"
};

static AnimationSet _gActor205200Animation09C1C = {
    _gActor205200Animation09C1CRecords,
    _gActor205200Animation09C1CIndices,
    { NULL, _gActor205200Animation09C1CBank1, NULL, NULL, _gActor205200Animation09C1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation0A964Bank1[23] = {
#include "assets/actor_205200_animation_0A964_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation0A964Bank4[347] = {
#include "assets/actor_205200_animation_0A964_bank4.inc"
};

static AnimationRecord _gActor205200Animation0A964Records[414] = {
#include "assets/actor_205200_animation_0A964_records.inc"
};

static u16 _gActor205200Animation0A964Indices[20] = {
#include "assets/actor_205200_animation_0A964_indices.inc"
};

static AnimationSet _gActor205200Animation0A964 = {
    _gActor205200Animation0A964Records,
    _gActor205200Animation0A964Indices,
    { NULL, _gActor205200Animation0A964Bank1, NULL, NULL, _gActor205200Animation0A964Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation0B1ACBank1[21] = {
#include "assets/actor_205200_animation_0B1AC_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation0B1ACBank4[193] = {
#include "assets/actor_205200_animation_0B1AC_bank4.inc"
};

static AnimationRecord _gActor205200Animation0B1ACRecords[254] = {
#include "assets/actor_205200_animation_0B1AC_records.inc"
};

static u16 _gActor205200Animation0B1ACIndices[20] = {
#include "assets/actor_205200_animation_0B1AC_indices.inc"
};

static AnimationSet _gActor205200Animation0B1AC = {
    _gActor205200Animation0B1ACRecords,
    _gActor205200Animation0B1ACIndices,
    { NULL, _gActor205200Animation0B1ACBank1, NULL, NULL, _gActor205200Animation0B1ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation0B948Bank1[20] = {
#include "assets/actor_205200_animation_0B948_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation0B948Bank4[172] = {
#include "assets/actor_205200_animation_0B948_bank4.inc"
};

static AnimationRecord _gActor205200Animation0B948Records[235] = {
#include "assets/actor_205200_animation_0B948_records.inc"
};

static u16 _gActor205200Animation0B948Indices[20] = {
#include "assets/actor_205200_animation_0B948_indices.inc"
};

static AnimationSet _gActor205200Animation0B948 = {
    _gActor205200Animation0B948Records,
    _gActor205200Animation0B948Indices,
    { NULL, _gActor205200Animation0B948Bank1, NULL, NULL, _gActor205200Animation0B948Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation0C154Bank1[15] = {
#include "assets/actor_205200_animation_0C154_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation0C154Bank4[206] = {
#include "assets/actor_205200_animation_0C154_bank4.inc"
};

static AnimationRecord _gActor205200Animation0C154Records[244] = {
#include "assets/actor_205200_animation_0C154_records.inc"
};

static u16 _gActor205200Animation0C154Indices[20] = {
#include "assets/actor_205200_animation_0C154_indices.inc"
};

static AnimationSet _gActor205200Animation0C154 = {
    _gActor205200Animation0C154Records,
    _gActor205200Animation0C154Indices,
    { NULL, _gActor205200Animation0C154Bank1, NULL, NULL, _gActor205200Animation0C154Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor205200Animation0C968Bank1[14] = {
#include "assets/actor_205200_animation_0C968_bank1.inc"
};

static AnimationPackedRotation _gActor205200Animation0C968Bank4[207] = {
#include "assets/actor_205200_animation_0C968_bank4.inc"
};

static AnimationRecord _gActor205200Animation0C968Records[248] = {
#include "assets/actor_205200_animation_0C968_records.inc"
};

static u16 _gActor205200Animation0C968Indices[20] = {
#include "assets/actor_205200_animation_0C968_indices.inc"
};

static AnimationSet _gActor205200Animation0C968 = {
    _gActor205200Animation0C968Records,
    _gActor205200Animation0C968Indices,
    { NULL, _gActor205200Animation0C968Bank1, NULL, NULL, _gActor205200Animation0C968Bank4, NULL, NULL, NULL },
};

s16 D_actor_205200_801567B0[2] = {
    4000,
    700,
};

SVECTOR D_actor_205200_801567B4[2] = {
    { 0, -700, -3500, 0 },
    { 0, -700, 0, 0 },
};

TaskDesc D_actor_205200_801567C4 = { { { TASK_BODY_TMD, 96 } }, _actor205200BodyTask, { .model = &gActor205200EveBreaMaskedBody } };

TaskMessageEntry D_actor_205200_801567D0[3] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor205200SetModelDrawMsg },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor205200ApplyCommandMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationSet* D_actor_205200_801567E8[6] = {
    NULL,
    &_gActor205200Animation09C1C,
    &_gActor205200Animation08B50,
    &_gActor205200Animation0943C,
    NULL,
    &_gActor205200Animation0A964,
};

AnimationSet* D_actor_205200_80156800[5] = {
    NULL,
    &_gActor205200Animation0B1AC,
    &_gActor205200Animation0B948,
    &_gActor205200Animation0C154,
    &_gActor205200Animation0C968,
};

ScreenWaveCtx* gScreenWaveCtx = NULL;

ScreenWaveGridOscillator gScreenWaveColumns[10];

ScreenWaveGridOscillator gScreenWaveRows[30];

POLY_FT4 gScreenWaveGrid[2][30][8];

ScreenWaveCtx D_actor_205200_8015B458;

/// Links the stationary body's hit sphere and room-specific touch sphere.
///
/// Requires zeroed work, a live model root and a room index in 0..1. Both
/// contact tables are initialized before pair testing is enabled; the caller
/// keeps their storage and coordinate live until the spheres are unlinked.
static inline void _actor205200LinkBodySpheres(_Actor205200Work* work, GfxCoord* rootCoord)
{
    enum {
        ACTOR_205200_HIT_BODY_KEY      = WORLD_COLLISION_CONTACT_ENEMY_BODY | 60,
        ACTOR_205200_HIT_SPHERE_RADIUS = 300
    };
    work->hitBody.pos.vy           = -ACTOR_205200_HIT_SPHERE_RADIUS;
    work->hitBody.coord            = rootCoord;
    work->hitBody.context.contacts = work->hitContacts;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = ACTOR_205200_HIT_BODY_KEY;
    work->hitBody.radius           = ACTOR_205200_HIT_SPHERE_RADIUS;
    work->hitBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hitBody);
    worldCollisionInitContacts(work->hitContacts, ARRAY_SIZE(work->hitContacts), 0);
    work->hitBody.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->touchBody.coord            = rootCoord;
    work->touchBody.context.contacts = work->touchContacts;
    work->touchBody.pos.vx           = D_actor_205200_801567B4[work->room].vx;
    work->touchBody.pos.vy           = D_actor_205200_801567B4[work->room].vy;
    work->touchBody.pos.vz           = D_actor_205200_801567B4[work->room].vz;
    work->touchBody.key              = 0;
    work->touchBody.radius           = D_actor_205200_801567B0[work->room];
    work->touchBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->touchBody);
    worldCollisionInitContacts(work->touchContacts, ARRAY_SIZE(work->touchContacts), 0);
    work->touchBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
}

/// Initializes the stationary body, its animation rig and its two contact spheres.
///
/// Requires a live enemy task with the nineteen-part model and a placement mode
/// of `ACTOR_205200_ROOM_CORRIDOR` or `ACTOR_205200_ROOM_TRAINING_ROOM`.
/// The task owns the zeroed work and the model borrows its lighting matrices;
/// allocation failure destroys the enemy and task. Successful setup enters
/// `ACTOR_205200_BODY_TASK_LIVE`. Hits cause reactions without reducing HP.
static void _actor205200SpawnBody(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_205200_HIT_EFFECT_ARGUMENT = 0x200,
        ACTOR_205200_HIT_EFFECT_COUNT    = 1
    };
    TmdObject*        model;
    GfxCoord*         rootCoord;
    _Actor205200Work* work;
    s32               slotIndex;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    work      = memCalloc(sizeof(_Actor205200Work), 0);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work              = work;
    model->flags            = 0;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx         = &work->lightMtx;
    model->colorMtx         = &work->colorMtx;
    enemy->field_4          = &rootCoord->coord;
    enemy->field_48         = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = &task->extra.tmd->coords[3];
    enemy->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->recs                   = work->hitContacts;
    enemy->param                  = NULL;
    enemy->hp                     = 0;
    work->hitEffectArg.coord      = &task->extra.tmd->coords[3];
    work->hitEffectArg.spawnArgLo = ACTOR_205200_HIT_EFFECT_ARGUMENT;
    work->hitEffectArg.spawnArgHi = ACTOR_205200_HIT_EFFECT_COUNT;
    // The root is not an animation slot; initialize the eighteen child parts.
    animationInitContext(&work->rig.anim, D_actor_205200_801567E8, model, work->rig.poses, work->rig.slots);
    slotIndex = 1;
    do {
        animationResetSlot(&work->rig.anim, slotIndex, ACTOR_205200_ANIM_IDLE);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->rig.slots));
    work->room = enemy->place->mode;
    _actor205200LinkBodySpheres(work, rootCoord);
    task->msgTable = D_actor_205200_801567D0;
    task->state    = ACTOR_205200_BODY_TASK_LIVE;
}

/// Consumes attack and touch contacts, requesting reactions and player knockback.
///
/// Requires initialized body work and contact tables. A stagger attack starts
/// the reaction without HP loss; a positive attack cooldown suppresses later
/// scans until it expires. A living player or companion body contact locks
/// attachments and requests the player knockback. Both tables are cleared.
/// Reserves sixteen scratch bytes across nested calls without accessing them
/// directly; the purpose of that reservation is unproven.
static void _actor205200ScanContacts(Task* task)
{
    enum { ACTOR_205200_CONTACT_SCRATCH_BYTES = 16 };
    _Actor205200Work* work;
    s32               contactIndex;
    s32               staggerSeen;
    s32               lastAttackKey;
    s32               attackCooldown;

    staggerSeen   = 0;
    work          = task->work;
    lastAttackKey = 0;
    SCRATCH_STACK_RESERVE_BYTES(ACTOR_205200_CONTACT_SCRATCH_BYTES);
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    if (work->hitCooldown == 0) {
        for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->hitContacts); contactIndex++) {
            if ((work->hitContacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_ATTACK) {
                worldTargetAddReadoutAmount(&((Enemy*)task->spawnArg2.pointer)->node, 0, 0);
                switch ((u16)damageGetPlayerAttackReaction(work->hitContacts[contactIndex].key.value)) {
                    case DAMAGE_PLAYER_REACTION_STAGGER:
                        staggerSeen = 1;
                        break;
                    case DAMAGE_PLAYER_REACTION_BUILDUP:
                        break;
                }
                // Once a stagger has been seen, later attack records also react.
                if (staggerSeen == 0) {
                    break;
                }
                work->action     = ACTOR_205200_ACTION_HIT_REACTION;
                work->actionStep = ACTOR_205200_HIT_REACTION_BEGIN;
                if (lastAttackKey != work->hitContacts[contactIndex].key.value) {
                    lastAttackKey = work->hitContacts[contactIndex].key.value;
                    effectSpawnHit(damageGetPlayerAttackEffectId(lastAttackKey), &task->extra.tmd->coords[3], NULL,
                                   &work->hitEffectArg);
                }
                if ((attackCooldown = damageGetPlayerAttackHitCooldown(work->hitContacts[contactIndex].key.value)) > 0) {
                    work->hitCooldown = attackCooldown;
                }
            }
        }
    }
    worldCollisionClearContacts(work->hitContacts);
    if (work->touchContacts[0].flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
        if ((work->touchContacts[0].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY && gPlayerStatus.hp > 0) {
            work->knockbackActive = 1;
            Gp_StateC08.flags    |= ATTACHMENT_FLAG_EVENT_LOCK;
        }
        worldCollisionClearContacts(work->touchContacts);
    }
    SCRATCH_STACK_RELEASE_BYTES(ACTOR_205200_CONTACT_SCRATCH_BYTES);
}

/// Counts down to a one-in-eight fidget draw, reseeding the delay to 15..46 ticks.
///
/// Requires live idle-action work. A due draw consumes two successive shared
/// LCG values and may replace a heal-start step selected earlier in the tick.
static inline void _actor205200TickFidget(_Actor205200Work* work)
{
    enum {
        ACTOR_205200_FIDGET_MIN_DELAY  = 15,
        ACTOR_205200_FIDGET_DELAY_MASK = 31,
        ACTOR_205200_FIDGET_DRAW_MASK  = 7
    };
    if (--work->fidgetTimer <= 0) {
        work->fidgetTimer = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & ACTOR_205200_FIDGET_DELAY_MASK) + ACTOR_205200_FIDGET_MIN_DELAY;
        if (!(((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & ACTOR_205200_FIDGET_DRAW_MASK)) {
            work->actionStep = ACTOR_205200_IDLE_FIDGET;
            work->anim       = ACTOR_205200_ANIM_FIDGET;
        }
    }
}

/// Steps idle fidgets and the paired enemy's healing-request animation.
///
/// Requires live body work in `ACTOR_205200_ACTION_IDLE`. A ready request is
/// consumed before the fidget draw, which can replace it on the same tick.
/// The healing animation publishes `SCENE_COMBAT_PAIRED_HEAL_READY` at frame
/// 60 and ends at frame 84; the partner applies the HP change. Completion
/// starts the 600-tick cooldown. Fidgets return to readiness or the remaining
/// cooldown after frame 64; the cooldown does not count down during a fidget.
static void _actor205200TickIdle(Task* task)
{
    enum {
        ACTOR_205200_HEAL_READY_FRAME = 60,
        ACTOR_205200_HEAL_END_FRAME   = 84,
        ACTOR_205200_FIDGET_END_FRAME = 64
    };
    _Actor205200Work* work;
    s16               nextStep;

    work = task->work;
    switch (work->actionStep) {
        case ACTOR_205200_IDLE_READY:
            if (gSceneCombatState.pairedEnemySignals & SCENE_COMBAT_PAIRED_HEAL_REQUEST) {
                gSceneCombatState.pairedEnemySignals &= (0xFF ^ SCENE_COMBAT_PAIRED_HEAL_REQUEST);
                work->actionStep                      = ACTOR_205200_IDLE_HEAL_BEGIN;
            }
            _actor205200TickFidget(work);
            break;
        case ACTOR_205200_IDLE_HEAL_BEGIN:
            work->anim       = ACTOR_205200_ANIM_HEAL;
            work->actionStep = ACTOR_205200_IDLE_HEAL;
            break;
        case ACTOR_205200_IDLE_HEAL:
            if (work->animFrame == ACTOR_205200_HEAL_READY_FRAME) {
                gSceneCombatState.pairedEnemySignals |= SCENE_COMBAT_PAIRED_HEAL_READY;
            }
            if (work->animFrame >= ACTOR_205200_HEAL_END_FRAME) {
                work->anim         = ACTOR_205200_ANIM_IDLE;
                work->actionStep   = ACTOR_205200_IDLE_COOLDOWN;
                work->healCooldown = ACTOR_205200_HEAL_COOLDOWN;
            }
            break;
        case ACTOR_205200_IDLE_COOLDOWN:
            if (--work->healCooldown <= 0) {
                work->actionStep = ACTOR_205200_IDLE_READY;
            }
            _actor205200TickFidget(work);
            break;
        case ACTOR_205200_IDLE_FIDGET:
            if (work->animFrame >= ACTOR_205200_FIDGET_END_FRAME) {
                nextStep = ACTOR_205200_IDLE_READY;
                if (work->healCooldown > 0) {
                    nextStep = ACTOR_205200_IDLE_COOLDOWN;
                }
                work->actionStep = nextStep;
                work->anim       = ACTOR_205200_ANIM_IDLE;
            }
            break;
    }
}

/// Steps the player's touch-triggered knockback, including animations and room effects.
///
/// Requires live body work and the player task, with both roots in the same
/// parent frame. A player already under scripted control cancels the request.
/// The first stage selects front/behind clips; the next pushes away for sixteen
/// ticks, placing Y at zero. Its Q12-normalized XYZ direction is scaled to
/// 100 units, then only X/Z are applied. Recovery begins after 30 ticks from
/// behind or 32 from the front. After at least 37 recovery ticks, animation
/// completion releases scripted control and clears the request. Each call
/// reserves and releases one scratch block; message receivers keep no pointer
/// to it. The package's animation data must remain loaded through recovery.
static void _actor205200TickPlayerKnockback(Task* task)
{
    /// Installs a clip in 1..4 and plays it with world collision enabled.
    ///
    /// Pointer arguments must have no side effects: scratchBlock is evaluated
    /// six times, playerTask and clipId once. The player borrows the request for
    /// synchronous dispatch and retains the animation table; the package data
    /// must outlive playback. The macro captures the package's player-set table.
#define ACTOR_205200_PLAY_PLAYER_KNOCKBACK_CLIP(playerTask, scratchBlock, clipId)                                        \
    {                                                                                                                    \
        (scratchBlock)->playerAnim.source.sets          = D_actor_205200_80156800;                                       \
        (scratchBlock)->playerAnim.animationId          = (clipId);                                                      \
        (scratchBlock)->playerAnim.blend                = ANIMATION_BLEND_RESET;                                         \
        (scratchBlock)->playerAnim.blendFrames          = 0;                                                             \
        (scratchBlock)->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;                              \
        TASK_MESSAGE_DISPATCH_POINTER((playerTask), ANIMATION_MESSAGE_INSTALL_AND_PLAY, &(scratchBlock)->playerAnim, 0); \
    }
    enum {
        ACTOR_205200_KNOCKBACK_PUSH_TICKS       = 16,
        ACTOR_205200_KNOCKBACK_BEHIND_TICKS     = 30,
        ACTOR_205200_KNOCKBACK_FRONT_TICKS      = 32,
        ACTOR_205200_KNOCKBACK_RECOVERY_TICKS   = 37,
        ACTOR_205200_KNOCKBACK_FIRST_CLIP       = 1,
        ACTOR_205200_KNOCKBACK_RECOVERY_CLIP    = 3,
        ACTOR_205200_KNOCKBACK_PUSH_NUMERATOR   = 25,
        ACTOR_205200_KNOCKBACK_PUSH_SHIFT       = 10,
        ACTOR_205200_KNOCKBACK_SOUND            = 7,
        ACTOR_205200_KNOCKBACK_CORRIDOR_SOUND   = 0x55180002,
        ACTOR_205200_KNOCKBACK_TRAINING_SOUND   = 0x55190003,
        ACTOR_205200_KNOCKBACK_SOUND_SLOT_SHIFT = 8,
        ACTOR_205200_KNOCKBACK_RUMBLE_TICKS     = 15,
        ACTOR_205200_KNOCKBACK_RUMBLE_START     = 255,
        ACTOR_205200_KNOCKBACK_RUMBLE_END       = 128
    };
    _Actor205200Work*            work;
    GfxCoord*                    actorCoord;
    Task*                        player;
    GfxCoord*                    playerCoord;
    ActorPlayerKnockbackScratch* scratch;
    s32                          soundId;
    s32                          knockbackFrame;

    work        = task->work;
    player      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch     = SCRATCH_STACK_RESERVE_BLOCK(ActorPlayerKnockbackScratch);
    actorCoord  = task->extra.tmd->coords;
    playerCoord = player->extra.tmd->coords;

    switch (work->knockbackStep) {
        case ACTOR_205200_KNOCKBACK_BEGIN:
            if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED) {
                scratch->toPlayer.vx      = playerCoord->coord.t[0] - actorCoord->coord.t[0];
                scratch->toPlayer.vy      = 0;
                scratch->toPlayer.vz      = playerCoord->coord.t[2] - actorCoord->coord.t[2];
                work->knockbackFromBehind = (scratch->toPlayer.vx * playerCoord->coord.m[0][2] + scratch->toPlayer.vz * playerCoord->coord.m[2][2]) > 0;
                ACTOR_205200_PLAY_PLAYER_KNOCKBACK_CLIP(player, scratch, work->knockbackFromBehind + ACTOR_205200_KNOCKBACK_FIRST_CLIP);
                work->knockbackStep  = ACTOR_205200_KNOCKBACK_PUSH;
                work->knockbackFrame = 0;
                padScriptSpawnVariableMotorRamp(ACTOR_205200_KNOCKBACK_RUMBLE_TICKS, ACTOR_205200_KNOCKBACK_RUMBLE_START, ACTOR_205200_KNOCKBACK_RUMBLE_END);
                soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_205200_KNOCKBACK_SOUND_SLOT_SHIFT) | ACTOR_205200_KNOCKBACK_SOUND;
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(actorCoord), (s8)worldCoordGetOriginAudioDepth(actorCoord));
                scratch->pushDirection.vx = 0;
                scratch->pushDirection.vy = -1000;
                scratch->pushDirection.vz = 0;
                if (work->room == ACTOR_205200_ROOM_CORRIDOR) {
                    effectSpawn(EFFECT_SHELTER_B6_CORRIDOR_PLAYER_HIT_RING, player->extra.tmd->coords, 0, &scratch->pushDirection);
                } else {
                    effectSpawn(EFFECT_SHELTER_B6_TRAINING_ROOM_HIT_FLASH, player->extra.tmd->coords, 0, &scratch->pushDirection);
                }
            } else {
                work->knockbackActive = 0;
            }
            break;
        case ACTOR_205200_KNOCKBACK_PUSH:
            // Recompute separation each tick; the normalized Y component is not applied.
            if (work->knockbackFrame < ACTOR_205200_KNOCKBACK_PUSH_TICKS) {
                scratch->toPlayer.vx = playerCoord->coord.t[0] - actorCoord->coord.t[0];
                scratch->toPlayer.vy = playerCoord->coord.t[1] - actorCoord->coord.t[1];
                scratch->toPlayer.vz = playerCoord->coord.t[2] - actorCoord->coord.t[2];
                VectorNormalS(&scratch->toPlayer, &scratch->pushDirection);
                scratch->playerPlacement.pos.vx = playerCoord->coord.t[0] + ((scratch->pushDirection.vx * ACTOR_205200_KNOCKBACK_PUSH_NUMERATOR) >> ACTOR_205200_KNOCKBACK_PUSH_SHIFT);
                scratch->playerPlacement.pos.vy = 0;
                scratch->playerPlacement.pos.vz = playerCoord->coord.t[2] + ((scratch->pushDirection.vz * ACTOR_205200_KNOCKBACK_PUSH_NUMERATOR) >> ACTOR_205200_KNOCKBACK_PUSH_SHIFT);
                scratch->playerPlacement.rot.vx = 0;
                if (work->knockbackFromBehind == 0) {
                    scratch->playerPlacement.rot.vy = (ratan2((s16)scratch->toPlayer.vx, (s16)scratch->toPlayer.vz) + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
                } else {
                    scratch->playerPlacement.rot.vy = ratan2((s16)scratch->toPlayer.vx, (s16)scratch->toPlayer.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
                }
                scratch->playerPlacement.rot.vz = 0;
                TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &scratch->playerPlacement, 0);
            }
            if (work->knockbackFrame == ACTOR_205200_KNOCKBACK_PUSH_TICKS) {
                if (work->room == ACTOR_205200_ROOM_CORRIDOR) {
                    soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_205200_KNOCKBACK_SOUND_SLOT_SHIFT) | ACTOR_205200_KNOCKBACK_CORRIDOR_SOUND;
                    sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(actorCoord), (s8)worldCoordGetOriginAudioDepth(actorCoord));
                } else {
                    soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_205200_KNOCKBACK_SOUND_SLOT_SHIFT) | ACTOR_205200_KNOCKBACK_TRAINING_SOUND;
                    sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(actorCoord), (s8)worldCoordGetOriginAudioDepth(actorCoord));
                }
            }
            knockbackFrame = ++work->knockbackFrame;
            if ((work->knockbackFromBehind != 0 && knockbackFrame >= ACTOR_205200_KNOCKBACK_BEHIND_TICKS) || (work->knockbackFromBehind == 0 && knockbackFrame >= ACTOR_205200_KNOCKBACK_FRONT_TICKS)) {
                ACTOR_205200_PLAY_PLAYER_KNOCKBACK_CLIP(player, scratch, work->knockbackFromBehind + ACTOR_205200_KNOCKBACK_RECOVERY_CLIP);
                work->knockbackStep  = ACTOR_205200_KNOCKBACK_END;
                work->knockbackFrame = 0;
            }
            break;
        case ACTOR_205200_KNOCKBACK_END:
            if (++work->knockbackFrame >= ACTOR_205200_KNOCKBACK_RECOVERY_TICKS) {
                if (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                    taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                    work->knockbackStep   = ACTOR_205200_KNOCKBACK_BEGIN;
                    work->knockbackFrame  = 0;
                    work->knockbackActive = 0;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorPlayerKnockbackScratch);
#undef ACTOR_205200_PLAY_PLAYER_KNOCKBACK_CLIP
}

/// Dispatches the stationary body's spawn, live update or teardown state.
///
/// `Task::state` must be an `ACTOR_205200_BODY_TASK_*` value in 0..2, and
/// `spawnArg2.pointer` must be its live enemy. Dispatch copies the three handlers
/// by value and performs no bounds check; teardown invalidates both arguments.
static void _actor205200BodyTask(Task* task)
{
    EnemyTaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_205200_80149E30;
    stateHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}

static void func_actor_205200_8014C59C(Enemy* arg0, Task* arg1)
{
    GfxCoord*         coord;
    TmdObject*        obj;
    _Actor205200Work* work;

    work  = arg1->work;
    obj   = arg1->extra.tmd;
    coord = obj->coords;
    if (gGameSession->eventState != 0) {
        return;
    }
    if (work->destroyRequested != 0) {
        arg1->state = 2;
        return;
    }
    switch (gSceneCombatState.actorControl) {
        case 0:
            obj->flags = 0;
            break;
        case 1:
            _actor205200UpdateLighting(arg1);
            _actor205200DrawShadow(arg1);
            return;
        case 2:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
    _actor205200ScanContacts(arg1);
    func_actor_205200_8014C67C(arg1);
    _actor205200UpdateAnimation(arg1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    _actor205200UpdateLighting(arg1);
    _actor205200DrawShadow(arg1);
}
/// Per-frame tick of the live state, run from `func_actor_205200_8014C59C`'s
/// shared body. Bit 0 of `gSceneCombatState.pairedEnemySignals` is a one-shot
/// request: it is cleared here and starts `ACTOR_205200_ACTION_HIT_REACTION`
/// from its first step, as a hit does. `action` then picks the idle or the
/// hit-reaction handler, `room` which of the two rooms' own ticks follows, and
/// `knockbackActive` keeps the knockback running until that body clears it
/// itself.
static void func_actor_205200_8014C67C(Task* arg0)
{
    _Actor205200Work* work;

    work = arg0->work;
    if (gSceneCombatState.pairedEnemySignals & SCENE_COMBAT_PAIRED_CHARGE_REQUEST) {
        gSceneCombatState.pairedEnemySignals &= (0xFF ^ SCENE_COMBAT_PAIRED_CHARGE_REQUEST);
        work->action                          = ACTOR_205200_ACTION_HIT_REACTION;
        work->actionStep                      = ACTOR_205200_HIT_REACTION_BEGIN;
    }
    switch (work->action) {
        case ACTOR_205200_ACTION_IDLE:
            _actor205200TickIdle(arg0);
            break;
        case ACTOR_205200_ACTION_HIT_REACTION:
            _actor205200TickHitReaction(arg0);
            break;
    }
    if (work->room == ACTOR_205200_ROOM_CORRIDOR) {
        func_shelter_b6_corridor_8017EBA4(arg0);
    } else {
        shelterB6TrainingRoomDrawBodyGlow(arg0);
    }
    if (work->knockbackActive != 0) {
        _actor205200TickPlayerKnockback(arg0);
    }
}

/// Plays the hit reaction and asks the paired enemy to reset its attack.
///
/// Requires live body work in `ACTOR_205200_ACTION_HIT_REACTION`. Entry
/// publishes `SCENE_COMBAT_PAIRED_RESET_REQUEST`. After 35 animation frames,
/// returns to idle with the 600-tick healing cooldown restarted.
static void _actor205200TickHitReaction(Task* task)
{
    enum { ACTOR_205200_HIT_REACTION_END_FRAME = 35 };
    _Actor205200Work* work;
    s16               actionStep;

    work       = task->work;
    actionStep = work->actionStep;
    switch (actionStep) {
        case ACTOR_205200_HIT_REACTION_BEGIN:
            work->anim                            = ACTOR_205200_ANIM_HIT_REACTION;
            work->actionStep                      = ACTOR_205200_HIT_REACTION_WAIT;
            gSceneCombatState.pairedEnemySignals |= SCENE_COMBAT_PAIRED_RESET_REQUEST;
            return;
        case ACTOR_205200_HIT_REACTION_WAIT:
            if (work->animFrame >= ACTOR_205200_HIT_REACTION_END_FRAME) {
                work->actionStep   = ACTOR_205200_IDLE_COOLDOWN;
                work->anim         = ACTOR_205200_ANIM_IDLE;
                work->action       = ACTOR_205200_ACTION_IDLE;
                work->healCooldown = ACTOR_205200_HEAL_COOLDOWN;
            }
            return;
    }
}

/// Synchronizes the eighteen child-part slots with the requested body animation.
///
/// Requires the initialized rig and a valid `ACTOR_205200_ANIM_*` request;
/// the initial zero request keeps the slots reset on idle at spawn.
/// A changed request resets the signed-halfword frame count and seeks every
/// child slot with an eight-frame blend; an unchanged request increments that
/// count and ticks every child slot. The root slot is excluded in both cases.
static void _actor205200UpdateAnimation(Task* task)
{
    enum { ACTOR_205200_ANIMATION_BLEND_FRAMES = 8 };
    _Actor205200Work* work;
    s32               slotIndex;

    work = task->work;
    if (work->anim != work->playingAnim) {
        work->playingAnim = work->anim;
        work->animFrame   = 0;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->anim, 0, ACTOR_205200_ANIMATION_BLEND_FRAMES);
        }
    } else {
        work->animFrame++;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
}

/// Refreshes the body model's lighting and colour from its cached root translation.
///
/// Requires a live enemy/model with writable lighting matrices and an already
/// composed root. Passes its three signed coordinate components unchanged to
/// `worldCoordUpdateActorColor`; that query borrows the sample for the call.
static void _actor205200UpdateLighting(Task* task)
{
    GfxCoord* rootCoord;
    VECTOR3   lightingPosition;

    rootCoord           = task->extra.tmd->coords;
    lightingPosition.vx = rootCoord->workm.t[0];
    lightingPosition.vy = rootCoord->workm.t[1];
    lightingPosition.vz = rootCoord->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &lightingPosition, 0, 0);
}

/// Draws the body's subtractive ground shadow at its cached root translation.
///
/// Requires an already composed model root. The shadow has a 384-unit half-side
/// and shade 128; `effectDrawGroundShadow` borrows the three-component centre
/// for this call and applies the scene's effect visibility gate.
static void _actor205200DrawShadow(Task* task)
{
    enum {
        ACTOR_205200_SHADOW_HALF_SIZE = 384,
        ACTOR_205200_SHADOW_SHADE     = 128
    };
    GfxCoord* rootCoord;
    VECTOR3   shadowCentre;

    rootCoord       = task->extra.tmd->coords;
    shadowCentre.vx = rootCoord->workm.t[0];
    shadowCentre.vy = rootCoord->workm.t[1];
    shadowCentre.vz = rootCoord->workm.t[2];
    effectDrawGroundShadow(&shadowCentre, ACTOR_205200_SHADOW_HALF_SIZE, ACTOR_205200_SHADOW_SHADE);
}

/// Unlinks the body's target and contact spheres before destroying its enemy task.
///
/// Requires a live initialized enemy and body work. Destruction releases the
/// enemy, task, model and work; neither argument nor their storage may be used
/// after this call.
static void _actor205200DestroyBody(Enemy* enemy, Task* task)
{
    _Actor205200Work* work;

    work = task->work;
    worldTargetUnlinkNode(&enemy->node);
    worldCollisionUnlinkBody(&work->hitBody);
    worldCollisionUnlinkBody(&work->touchBody);
    enemyDestroy(enemy, task);
}

/// Sets the body's active model drawing for `ACTOR_MESSAGE_SET_MODEL_DRAW`.
///
/// Requires a live model. Zero `drawEnabled` replaces its flags with
/// `TMD_OBJECT_SKIP_ACTIVE_DRAW`; any nonzero value clears all model flags.
/// The message ID and second payload are ignored. Returns zero.
static s32 _actor205200SetModelDrawMsg(Task* task, s32 messageId, s32 drawEnabled, s32 unusedSecondArg)
{
    TmdObject* model;

    model = task->extra.tmd;
    if (drawEnabled == 0) {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        model->flags = 0;
    }
    return 0;
}

/// Latches body teardown for a nonzero `ACTOR_COMMAND_MESSAGE_APPLY` command.
///
/// Requires live body work and a non-NULL borrowed command record. Only the
/// unsigned command halfword is read; context, message ID and second payload
/// are ignored. Zero is a no-op; other values request teardown during a later
/// live update. The command is neither modified nor retained. Returns zero.
static s32 _actor205200ApplyCommandMsg(Task* task, s32 messageId, const ActorCommand* request, s32 unusedSecondArg)
{
    enum { ACTOR_205200_COMMAND_NONE = 0 };
    _Actor205200Work* work;

    work = task->work;
    if (request->command != ACTOR_205200_COMMAND_NONE) {
        work->destroyRequested = 1;
    }
    return 0;
}
