#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/random.h"
#include "main/gfx.h"
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

#include "rooms/acropolis_fire_escape.h"
#include "../../shared/actor_contacts.h"

/// Task phases of this package's stationary enemy.
enum {
    ACTOR_311500_PHASE_INIT  = 0,
    ACTOR_311500_PHASE_IDLE  = 1,
    ACTOR_311500_PHASE_HIT   = 2,
    ACTOR_311500_PHASE_DEATH = 3,
    ACTOR_311500_PHASE_DONE  = 4
};

/// Results of one hit-reaction update, consumed through a signed low halfword.
enum {
    ACTOR_311500_HIT_REACTION_LETHAL   = -1,
    ACTOR_311500_HIT_REACTION_PLAYING  = 0,
    ACTOR_311500_HIT_REACTION_FINISHED = 1
};

/// Animation-set indices, the hit-body part and the area-placement identity.
enum {
    ACTOR_311500_ANIMATION_IDLE = 0,
    ACTOR_311500_ANIMATION_HIT  = 1,
    ACTOR_311500_BODY_PART      = 2,
    ACTOR_311500_PLACEMENT_ID   = 10
};

/// Values of `_Actor311500Work::step`, the stage of the phase `Task::state`
/// selects.
///
/// Every phase starts at step 0 and gives the steps its own meaning.
enum {
    ACTOR_311500_IDLE_STEP_CHOOSE = 0, // draws between a pause and a replay of animation 0; two replays in a row force the pause
    ACTOR_311500_IDLE_STEP_PAUSE  = 1, // holds the pose for 32 ticks
    ACTOR_311500_IDLE_STEP_REPLAY = 2, // plays animation 0 through from its start
    ACTOR_311500_HIT_STEP_START   = 0, // blends into animation 1 and spawns the hit effect; ends the phase at once when the hit points are gone
    ACTOR_311500_HIT_STEP_PLAY    = 1, // plays animation 1 through
    ACTOR_311500_DEATH_STEP_CRY   = 0, // queues the death sound
    ACTOR_311500_DEATH_STEP_BURN  = 1  // runs the burn-away sequence that `stepFrame` times
};

/// Work block of the package's enemy task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It
/// holds the animation rig, the body that takes the hits with its one contact
/// record, storage for the model's matrices and the state of the idle, hit and
/// death phases.
///
/// Animation numbers are indices into the package's animation-set table, and
/// only slots 1 to 18 of the rig are driven. A contact key is a
/// `WorldCollisionContact` key: its category in the high half and the
/// identity of what made the contact in the low half.
typedef struct {
    ActorAnimRig19        rig;              // playback of the model's parts; slot 1 reaching a boundary ends an idle replay and the hit reaction
    WorldCollisionBody    hitBody;          // sphere of radius 400 at model part 2 that takes the hits; unlinked when the death sequence begins
    WorldCollisionContact hitContacts[1];   // the one contact of `hitBody`; also the enemy's hit records until the death sequence begins
    MATRIX                lightMtx;         // storage for the model's `TmdObject::lightMtx`
    MATRIX                colorMtx;         // storage for the model's `TmdObject::colorMtx`
    Task*                 playerTask;       // the player's task as the spawn found it; never read
    MATRIX*               playerCoordMtx;   // the player's root coordinate matrix as the spawn found it; never read
    u32                   savedModelFlags;  // the model's `TmdObject::flags` as actor control 2 found them before hiding it; written back when the control returns to 0 from any other value
    s16                   step;             // `ACTOR_311500_*_STEP_*` of the running phase; every phase change restarts it at 0
    byte                  pad_4C2[2];       // never accessed
    s16                   stepFrame;        // ticks counted by the idle pause and by the burn-away sequence, each of which restarts it at 0
    byte                  pad_4C6[2];       // never accessed
    s16                   idlePlayCount;    // idle replays finished since the last pause
    byte                  pad_4CA[2];       // never accessed
    s32                   hitKey;           // contact key of the category-2 contact found in `hitContacts` this tick; 0 when there is none
    s32                   lastHitKey;       // `hitKey` of the last hit taken; its id parameter 1 selects the hit effect
    u16                   present;          // answer to `ACTOR_MESSAGE_IS_PRESENT` (1 from spawn, 0 once the hit points are gone)
    u16                   prevActorControl; // `gSceneCombatState.actorControl` as the tick last recorded it (0 update, 1 pause, 2 hide); not recorded once the death phase runs
} _Actor311500Work;
STATIC_ASSERT_SIZEOF(_Actor311500Work, 0x4D8);

extern EnemyParams   D_actor_311500_801692C0;
extern AnimationSet* D_actor_311500_801692F4[2];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_311500_80169330[1];

extern SVECTOR                D_actor_311500_801692FC[1];
extern SVECTOR                D_actor_311500_80169304[4];
extern WorldCollisionGridFace D_actor_311500_80169324[1];

static AnimationSet _gActor311500Animation07188;
static AnimationSet _gActor311500Animation07470;
static TmdSource    _gActor311500StrangerBody;
static void         _actor311500Task(Task* actorTask);
static s32          _actor311500IsPresentMessage(Task* actorTask, s32 messageId, s32 unusedArg, u32* presenceResult);

static TmdBone _gActor311500StrangerBodySkeleton[19] = {
#include "assets/stranger_body_skeleton.inc"
};

static u32 _gActor311500StrangerBodyPartVerts[19] = {
#include "assets/stranger_body_partVerts.inc"
};

static SVECTOR _gActor311500StrangerBodyVerts[311] = {
#include "assets/stranger_body_verts.inc"
};

static SVECTOR _gActor311500StrangerBodyNormals[309] = {
#include "assets/stranger_body_normals.inc"
};

static u32 _gActor311500StrangerBodyStream[4027] = {
#include "assets/stranger_body_stream.inc"
};

static TmdSource _gActor311500StrangerBody = {
    0,
    20180,
    7860,
    19,
    _gActor311500StrangerBodyPartVerts,
    _gActor311500StrangerBodyVerts,
    _gActor311500StrangerBodyNormals,
    _gActor311500StrangerBodySkeleton,
    _gActor311500StrangerBodyStream,
};

static AnimationPackedPose _gActor311500Animation07188Bank1[6] = {
#include "assets/actor_311500_animation_07188_bank1.inc"
};

static AnimationPackedRotation _gActor311500Animation07188Bank4[81] = {
#include "assets/actor_311500_animation_07188_bank4.inc"
};

static AnimationRecord _gActor311500Animation07188Records[118] = {
#include "assets/actor_311500_animation_07188_records.inc"
};

static u16 _gActor311500Animation07188Indices[20] = {
#include "assets/actor_311500_animation_07188_indices.inc"
};

static AnimationSet _gActor311500Animation07188 = {
    _gActor311500Animation07188Records,
    _gActor311500Animation07188Indices,
    { NULL, _gActor311500Animation07188Bank1, NULL, NULL, _gActor311500Animation07188Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor311500Animation07470Bank1[5] = {
#include "assets/actor_311500_animation_07470_bank1.inc"
};

static AnimationPackedRotation _gActor311500Animation07470Bank4[52] = {
#include "assets/actor_311500_animation_07470_bank4.inc"
};

static AnimationRecord _gActor311500Animation07470Records[99] = {
#include "assets/actor_311500_animation_07470_records.inc"
};

static u16 _gActor311500Animation07470Indices[20] = {
#include "assets/actor_311500_animation_07470_indices.inc"
};

static AnimationSet _gActor311500Animation07470 = {
    _gActor311500Animation07470Records,
    _gActor311500Animation07470Indices,
    { NULL, _gActor311500Animation07470Bank1, NULL, NULL, _gActor311500Animation07470Bank4, NULL, NULL, NULL },
};

DamageAttack D_actor_311500_801692B8[2] = {
    { 18, 7 },
    { 18, 0 },
};

EnemyParams D_actor_311500_801692C0 = { D_actor_311500_801692B8, 30, 42, 82, 4, 250, 0, 100, 0 };

u16 D_actor_311500_801692D0[18] = {
    20,
    900,
    12,
    2000,
    0,
    0,
    10,
    800,
    12,
    2500,
    0,
    0,
    0,
    500,
    12,
    3000,
    0,
    0,
};

AnimationSet* D_actor_311500_801692F4[2] = {
    &_gActor311500Animation07470,
    &_gActor311500Animation07188,
};

SVECTOR D_actor_311500_801692FC[1] = {
#include "assets/actor_311500_collision_074DC.inc"
};

SVECTOR D_actor_311500_80169304[4] = {
#include "assets/actor_311500_collision_074E4.inc"
};

WorldCollisionGridFace D_actor_311500_80169324[1] = {
#include "assets/actor_311500_collision_07504.inc"
};

TaskMessageEntry D_actor_311500_80169330[1] = {
    { ACTOR_MESSAGE_IS_PRESENT, _actor311500IsPresentMessage },
};

TaskDesc D_actor_311500_80169338 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor311500Task, { .model = &_gActor311500StrangerBody } };

static void        _actor311500Init(Task* actorTask);
static inline void _actor311500ResetAnim(ActorAnimRig19* rig, u8 requestedRate, s32 slotIndex);
static inline u16  _actor311500TickAnim(Task* task);
static void        _actor311500TickIdle(Task* actorTask);
static s16         _actor311500ApplyHit(Task* actorTask);
static inline void _actor311500BlendHitAnimation(Task* task);
static inline void _actor311500SpawnHitEffect(Task* task);
static s32         _actor311500TickHitReaction(Task* actorTask);
static s32         _actor311500TickDeath(Task* actorTask);

/// Installs all nine Q12 root coefficients and invalidates composition.
///
/// `rootCoord` and the MATRIX value `rotation` must be side-effect-free locals;
/// `matrixElement` is a caller-owned u16 temporary. Arguments are repeated.
/// Translation is retained; the dirty stamp precedes the final halfword store.
/// Use as a standalone block, not as an unbraced if/else arm.
#define ACTOR_311500_INSTALL_ROOT_ROTATION(rootCoord, rotation, matrixElement) \
    {                                                                          \
        (matrixElement)            = (u16)(rotation).m[0][0];                  \
        (rootCoord)->coord.m[0][0] = (matrixElement);                          \
        (matrixElement)            = (u16)(rotation).m[0][1];                  \
        (rootCoord)->coord.m[0][1] = (matrixElement);                          \
        (matrixElement)            = (u16)(rotation).m[0][2];                  \
        (rootCoord)->coord.m[0][2] = (matrixElement);                          \
        (matrixElement)            = (u16)(rotation).m[1][0];                  \
        (rootCoord)->coord.m[1][0] = (matrixElement);                          \
        (matrixElement)            = (u16)(rotation).m[1][1];                  \
        (rootCoord)->coord.m[1][1] = (matrixElement);                          \
        (matrixElement)            = (u16)(rotation).m[1][2];                  \
        (rootCoord)->coord.m[1][2] = (matrixElement);                          \
        (matrixElement)            = (u16)(rotation).m[2][0];                  \
        (rootCoord)->coord.m[2][0] = (matrixElement);                          \
        (matrixElement)            = (u16)(rotation).m[2][1];                  \
        (rootCoord)->coord.m[2][1] = (matrixElement);                          \
        (matrixElement)            = (u16)(rotation).m[2][2];                  \
        (rootCoord)->composeStamp  = GRAPHICS_COORD_DIRTY;                     \
        (rootCoord)->coord.m[2][2] = (matrixElement);                          \
    }

#include "../../shared/actor_contacts_find_push.inc.c"

#include "../../shared/actor_contacts_steer.inc.c"

#include "../../shared/actor_contacts_turn_joint.inc.c"

/// Restarts the remaining idle tracks of the package rig at normal playback rate.
///
/// `slotIndex` is in 1..18; its low 16 bits select that slot through slot 18.
/// The initialized rig, model and clip data must remain live. `requestedRate`
/// is in sixteenths of a frame and is written before each reset, which
/// overwrites it with `ANIMATION_RATE_ONE`.
static inline void _actor311500ResetAnim(ActorAnimRig19* rig, u8 requestedRate, s32 slotIndex)
{
    do {
        rig->slots[slotIndex & 0xFFFF].rate = requestedRate;
        animationResetSlot(&rig->anim, slotIndex & 0xFFFF, ACTOR_311500_ANIMATION_IDLE);
        slotIndex += 1;
    } while ((u32)(slotIndex & 0xFFFF) < ARRAY_SIZE(rig->slots));
}

/// Initializes the stationary enemy's playback, targeting and single-contact hit body.
///
/// Requires the package model, enemy spawn argument and fire-escape placement data.
/// The task owns the allocated work and primitive buffer; model and clip data must
/// stay loaded while it is live. Allocation failure kills the task.
static void _actor311500Init(Task* actorTask)
{
    enum { ACTOR_311500_INITIAL_HP = 50,
           ACTOR_311500_HIT_RADIUS = 400 };
    _Actor311500Work* allocatedWork;
    _Actor311500Work* work;
    _Actor311500Work* animationWork;
    Enemy*            enemy;
    GfxCoord*         rootCoord;
    TmdObject*        model;
    AreaPlacement*    placement;
    s32               slotIndex;
    u8                rate;

    rootCoord       = actorTask->extra.tmd->coords;
    enemy           = actorTask->spawnArg2.pointer;
    model           = actorTask->extra.tmd;
    allocatedWork   = memCalloc(sizeof(_Actor311500Work), 0);
    actorTask->work = allocatedWork;
    if (allocatedWork == NULL) {
        taskKill(actorTask);
        return;
    }
    // Bind task-owned playback and matrix storage before linking the live enemy.
    sceneAcquireBattleRef(0);
    work = actorTask->work;
    memFillBytes(work, 0, sizeof(*work));
    rootCoord->parent = &gGfxViewCoord;
    tmdAllocPrimitiveBuffer(model);
    model->lightMtx = &work->lightMtx;
    model->colorMtx = &work->colorMtx;
    model->flags    = 0;
    animationInitContext(&work->rig.anim, D_actor_311500_801692F4, model, work->rig.poses,
                         &work->rig.slots[0]);
    work->playerTask     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    work->playerCoordMtx = gPlayerStatus.coordMtx;
    rate                 = ANIMATION_RATE_ONE;
    slotIndex            = 1;
    animationWork        = actorTask->work;
    _actor311500ResetAnim(&animationWork->rig, rate, slotIndex);
    // The target and collision body share model part 2; this body retains one hit.
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actorTask->extra.tmd->coords[ACTOR_311500_BODY_PART];
    worldTargetLinkNode(&enemy->node);
    enemy->hp                      = ACTOR_311500_INITIAL_HP;
    enemy->node.state.parts.flags  = 0;
    enemy->reactionFlags           = 0;
    enemy->param                   = &D_actor_311500_801692C0;
    work->hitBody.coord            = &actorTask->extra.tmd->coords[ACTOR_311500_BODY_PART];
    work->hitBody.context.contacts = &work->hitContacts[0];
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vy           = 0;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_311500_PLACEMENT_ID;
    work->hitBody.radius           = ACTOR_311500_HIT_RADIUS;
    work->hitBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hitBody);
    work->hitBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(&work->hitContacts[0], ARRAY_SIZE(work->hitContacts), 0);
    enemy->recs         = &work->hitContacts[0];
    actorTask->msgTable = D_actor_311500_80169330;
    work->present       = 1;
    placement           = areaGetVariant(&gGameSession->location.loc)->placements;
    while (placement->entryId != AREA_PLACEMENT_END && placement->entryId != ACTOR_311500_PLACEMENT_ID) {
        placement++;
    }
    tmdSetTextureOffsets(model, placement->texturePageOffset, placement->clutRowOffset);
}

/// Advances model slots 1..18 and reports whether slot 1 reached a playback boundary.
///
/// Requires initialized package playback with live model and clip data. Returns
/// 1 for `ANIMATION_SLOT_REACHED_BOUNDARY`, otherwise 0; this need not mean a stop.
static inline u16 _actor311500TickAnim(Task* task)
{
    _Actor311500Work* work = task->work;
    s32               slotIndex;

    slotIndex = 1;
    do {
        animationTickSlot(&work->rig.anim, slotIndex & 0xFFFF);
        slotIndex += 1;
    } while ((u32)(slotIndex & 0xFFFF) < ARRAY_SIZE(work->rig.slots));
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        return 1;
    }
    return 0;
}

/// Chooses a 32-tick pause or an idle replay, forcing a pause after two replays.
///
/// The replay's requested double rate is overwritten by the slot reset.
static void _actor311500TickIdle(Task* actorTask)
{
    enum { ACTOR_311500_IDLE_PAUSE_FRAMES = 32,
           ACTOR_311500_IDLE_MAX_REPLAYS  = 2 };
    _Actor311500Work* work;
    SVECTOR           unusedProbe; // The target reserves this unused stack slot.
    u32               randomChoice;

    work = actorTask->work;

    switch (work->step) {
        case ACTOR_311500_IDLE_STEP_CHOOSE:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomChoice    = gRandomLcgState >> 16;
            if (work->idlePlayCount >= ACTOR_311500_IDLE_MAX_REPLAYS) {
                work->step++;
            } else if (randomChoice & 1) {
                work->step++;
            } else {
                _Actor311500Work* animationWork = actorTask->work;

                _actor311500ResetAnim(&animationWork->rig, 2 * ANIMATION_RATE_ONE, 1);
                work->step += 2;
            }
            work->stepFrame = 0;
            break;

        case ACTOR_311500_IDLE_STEP_PAUSE:
            if (work->stepFrame++ >= ACTOR_311500_IDLE_PAUSE_FRAMES - 1) {
                work->idlePlayCount = 0;
                work->step          = ACTOR_311500_IDLE_STEP_CHOOSE;
            }
            break;

        case ACTOR_311500_IDLE_STEP_REPLAY:
            if (_actor311500TickAnim(actorTask)) {
                work->step = ACTOR_311500_IDLE_STEP_CHOOSE;
                work->idlePlayCount++;
            }
            break;

        default:
            break;
    }
}

/// Finds the first attack contact before an empty record or the contact limit.
///
/// `contactCount` is a nonnegative element count within the readable `contacts`
/// array. A hit writes only signed game-coordinate XYZ of `contactPoint` in the
/// contact's frame
/// and returns its packed key; no hit returns 0 and leaves the point untouched.
static inline s32 _actor311500FindHit(SVECTOR* contactPoint, const WorldCollisionContact* contacts, s16 contactCount)
{
    s16 contactIndex;

    for (contactIndex = 0; contactIndex < contactCount; contactIndex++) {
        if (contacts[contactIndex].key.value == 0) {
            break;
        }
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_ATTACK) {
            contactPoint->vx = contacts[contactIndex].point.vx;
            contactPoint->vy = contacts[contactIndex].point.vy;
            contactPoint->vz = contacts[contactIndex].point.vz;
            return contacts[contactIndex].key.value;
        }
    }
    return 0;
}

/// Applies the queued attack's HP damage, critical effect and target readout.
///
/// Consumes the single hit contact and remembers its full key for the reaction
/// effect. Returns the key's signed low halfword, or 0 when there is no attack.
static s16 _actor311500ApplyHit(Task* actorTask)
{
    enum { ACTOR_311500_CRITICAL_DAMAGE_MULTIPLIER = 5 };
    _Actor311500Work* work = actorTask->work;
    Enemy*            enemy;
    SVECTOR           contactPoint;
    s32               damage;

    enemy        = actorTask->spawnArg2.pointer;
    work->hitKey = _actor311500FindHit(&contactPoint, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    if (work->hitKey != 0) {
        work->lastHitKey = work->hitKey;
        // Use zero distance and disable reaction scaling before applying the critical multiplier.
        damage = damageComputePlayerAttack(work->hitKey, 0, 0, 0x1000);
        if (damageRollCriticalHit(enemy, work->hitKey, 0) != 0) {
            damage *= ACTOR_311500_CRITICAL_DAMAGE_MULTIPLIER;
            effectSpawn(EFFECT_CRITICAL_HIT, actorTask->extra.tmd->coords, 0, 0);
        }
        enemy->hp -= damage;
        worldCollisionClearContacts(work->hitContacts);
        worldTargetAddReadoutAmount(&enemy->node, damage, 0);
    }
    return work->hitKey;
}

/// Blends model slots 1..18 into the hit clip over ten normal-rate frames.
///
/// Captures their advanced poses first. Requires initialized package playback;
/// its transition buffers, clip data and model must remain live through the blend.
static inline void _actor311500BlendHitAnimation(Task* task)
{
    enum { ACTOR_311500_HIT_BLEND_FRAMES = 10 };
    _Actor311500Work* work = task->work;
    s32               slotIndex;

    slotIndex = 1;
    do {
        animationSeekSlotWithBlend(&work->rig.anim, slotIndex & 0xFFFF, ACTOR_311500_ANIMATION_HIT, 0, ACTOR_311500_HIT_BLEND_FRAMES);
        slotIndex += 1;
    } while ((u32)(slotIndex & 0xFFFF) < ARRAY_SIZE(work->rig.slots));
}

/// Spawns the last attack's hit effect at the body's fixed local offset.
///
/// The offset (60, -12, 30) is in model part 2's coordinate frame. Effect selection
/// comes from the full saved attack key; the borrowed spawn record is synchronous.
static inline void _actor311500SpawnHitEffect(Task* task)
{
    enum { ACTOR_311500_HIT_EFFECT_MAGNITUDE = 256,
           ACTOR_311500_HIT_EFFECT_COUNT     = 2 };
    _Actor311500Work* work = task->work;
    SVECTOR           localOffset;
    EffectSpawnArg    spawnArgs;

    spawnArgs.coord      = &task->extra.tmd->coords[ACTOR_311500_BODY_PART];
    spawnArgs.spawnArgLo = ACTOR_311500_HIT_EFFECT_MAGNITUDE;
    spawnArgs.spawnArgHi = ACTOR_311500_HIT_EFFECT_COUNT;
    localOffset.vx       = 0x3C;
    localOffset.vy       = -0xC;
    localOffset.vz       = 0x1E;
    effectSpawnHit(damageGetPlayerAttackEffectId(work->lastHitKey), &task->extra.tmd->coords[ACTOR_311500_BODY_PART], &localOffset, &spawnArgs);
}

/// Starts or advances the hit reaction, returning -1 for death, 1 for a boundary, or 0.
///
/// A lethal start blends and spawns the hit effect without advancing the step;
/// another call repeats those effects. A boundary returns the actor to idle.
static s32 _actor311500TickHitReaction(Task* actorTask)
{
    _Actor311500Work* work;
    Enemy*            enemy;

    work  = actorTask->work;
    enemy = actorTask->spawnArg2.pointer;

    switch (work->step) {
        case ACTOR_311500_HIT_STEP_START:
            _actor311500BlendHitAnimation(actorTask);
            _actor311500SpawnHitEffect(actorTask);
            if (enemy->hp <= 0) {
                return ACTOR_311500_HIT_REACTION_LETHAL;
            }
            work->step++;
            break;

        case ACTOR_311500_HIT_STEP_PLAY:
            if (_actor311500TickAnim(actorTask)) {
                return ACTOR_311500_HIT_REACTION_FINISHED;
            }
            break;
    }
    return ACTOR_311500_HIT_REACTION_PLAYING;
}

/// Runs the death sound, reward release and timed burn-away of the model.
///
/// Stops targeting and hit collection at the burn step's first tick. The root
/// retains its translation while its yaw basis is rebuilt with vertical Q12
/// scale. Returns 1 at burn tick 260, otherwise 0; the task stays allocated.
static s32 _actor311500TickDeath(Task* actorTask)
{
    enum {
        ACTOR_311500_DEATH_BURN_FRAME        = 10,
        ACTOR_311500_DEATH_BLACK_FRAME       = 22,
        ACTOR_311500_DEATH_TRANSLUCENT_FRAME = 28,
        ACTOR_311500_DEATH_HIDE_FRAME        = 80,
        ACTOR_311500_DEATH_END_FRAME         = 260,
        ACTOR_311500_DEATH_SCALE_START_FRAME = 6,
        ACTOR_311500_DEATH_SCALE_UNITY_FRAME = 20,
        ACTOR_311500_DEATH_FLAME_BURSTS      = 3
    };
    _Actor311500Work* work;
    Enemy*            enemy;
    GfxCoord*         rootCoord;
    MATRIX            scaledYaw;
    VECTOR            scale;
    u16               matrixElement;
    s32               deathStep;
    s16               frame;
    s32               verticalScale;
    s16               yaw;
    s32               pan;

    work      = actorTask->work;
    enemy     = actorTask->spawnArg2.pointer;
    deathStep = work->step;

    switch (deathStep) {
        case ACTOR_311500_DEATH_STEP_CRY:
            pan = (s8)worldCoordGetOriginAudioPan(actorTask->extra.tmd->coords);
            sndEvtRequestScriptStart(SOUND_ACTOR_311500_DEATH, pan, (s8)worldCoordGetOriginAudioDepth(actorTask->extra.tmd->coords));
            work->stepFrame = 0;
            work->step++;
            break;

        case ACTOR_311500_DEATH_STEP_BURN:
            switch (work->stepFrame) {
                case 0:
                    sceneReleaseBattleRefWithRewards(actorTask, 0xA);
                    enemy->recs = NULL;
                    worldCollisionUnlinkBody(&work->hitBody);
                    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                    break;

                case ACTOR_311500_DEATH_BURN_FRAME:
                    effectSpawn(EFFECT_CORPSE_BURN, &actorTask->extra.tmd->coords[ACTOR_311500_BODY_PART], ACTOR_311500_DEATH_FLAME_BURSTS, NULL);
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                    break;

                case ACTOR_311500_DEATH_BLACK_FRAME:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                    break;

                case ACTOR_311500_DEATH_TRANSLUCENT_FRAME:
                    actorTask->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                    break;

                case ACTOR_311500_DEATH_HIDE_FRAME:
                    actorTask->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    break;

                case ACTOR_311500_DEATH_END_FRAME:
                    return 1;
            }

            // Rebuild root yaw with a Q12 vertical scale, retaining its translation.
            frame = work->stepFrame;
            if (frame >= ACTOR_311500_DEATH_SCALE_START_FRAME) {
                rootCoord     = actorTask->extra.tmd->coords;
                verticalScale = ONE - (frame - ACTOR_311500_DEATH_SCALE_UNITY_FRAME) * 10;
                yaw           = ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]);
                gfxRotMatrixY(&scaledYaw, yaw, 1);
                scale.vx = ONE;
                scale.vy = (s16)verticalScale;
                scale.vz = ONE;
                ScaleMatrix(&scaledYaw, &scale);

                ACTOR_311500_INSTALL_ROOT_ROTATION(rootCoord, scaledYaw, matrixElement);
            }

            work->stepFrame++;
            break;

        default:
            return 0;
    }
    return 0;
}

/// Composes the body coordinate and refreshes lighting from the root's cached view position.
///
/// Requires the live model and enemy spawn argument. Marks the root dirty for
/// the next composition; the task's model renderer emits the primitives.
static inline void _actor311500UpdatePresentation(Task* actor)
{
    Enemy* enemy;
    VECTOR viewPosition;

    enemy = actor->spawnArg2.pointer;
    actorRenderComposeCoord(&actor->extra.tmd->coords[1]);
    viewPosition.vx = actor->extra.tmd->coords->workm.t[0];
    viewPosition.vy = actor->extra.tmd->coords->workm.t[1];
    viewPosition.vz = actor->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(enemy, &viewPosition, 0, 0);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Updates the fire-escape enemy's idle, hit and death phases and presentation.
///
/// Scene actor controls pause updates or hide the model. The fire-escape grid
/// must be loaded: initialization installs its barrier patch and death clears
/// that patch before marking the enemy absent. The done phase keeps the task live.
static void _actor311500Task(Task* actorTask)
{
    _Actor311500Work* work;
    TmdObject*        model;
    s32               pan;

    work  = actorTask->work;
    model = actorTask->extra.tmd;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->prevActorControl != SCENE_COMBAT_ACTORS_RUNNING) {
                model->flags = work->savedModelFlags;
            }
            switch (actorTask->state) {
                case ACTOR_311500_PHASE_INIT:
                    // Install the opening barrier patch into the loaded fire-escape grid.
                    memCopyBytes(&D_actor_311500_80169304, gAcropolisFireEscapeCollision04CE8Verts, sizeof(D_actor_311500_80169304));
                    memCopyBytes(&D_actor_311500_801692FC, gAcropolisFireEscapeCollision04CE8Normals, sizeof(D_actor_311500_801692FC));
                    memCopyBytes(&D_actor_311500_80169324, gAcropolisFireEscapeCollision04CE8Faces, sizeof(D_actor_311500_80169324));
                    _actor311500Init(actorTask);
                    work = actorTask->work;
                    _actor311500TickAnim(actorTask);
                    actorTask->state += 1;
                    break;

                case ACTOR_311500_PHASE_IDLE:
                    _actor311500TickIdle(actorTask);
                    if (_actor311500ApplyHit(actorTask) != 0) {
                        pan = (s8)worldCoordGetOriginAudioPan(actorTask->extra.tmd->coords);
                        sndEvtRequestScriptStart(SOUND_ACTOR_311500_HURT, pan,
                                                 (s8)worldCoordGetOriginAudioDepth(actorTask->extra.tmd->coords));
                        work->step        = ACTOR_311500_HIT_STEP_START;
                        actorTask->state += 1;
                    }
                    worldCollisionClearContacts(work->hitContacts);
                    break;

                case ACTOR_311500_PHASE_HIT:
                    if (_actor311500ApplyHit(actorTask) != 0) {
                        work->step = ACTOR_311500_HIT_STEP_START;
                    }
                    // Both calls advance the reaction; a killing hit repeats its start effects.
                    if ((s16)_actor311500TickHitReaction(actorTask) > ACTOR_311500_HIT_REACTION_PLAYING) {
                        work->step        = ACTOR_311500_IDLE_STEP_CHOOSE;
                        actorTask->state -= 1;
                        break;
                    }
                    if ((s16)_actor311500TickHitReaction(actorTask) < ACTOR_311500_HIT_REACTION_PLAYING) {
                        memFillBytes(gAcropolisFireEscapeCollision04CE8Verts, 0, sizeof(D_actor_311500_80169304));
                        memFillBytes(gAcropolisFireEscapeCollision04CE8Normals, 0, sizeof(D_actor_311500_801692FC));
                        memFillBytes(gAcropolisFireEscapeCollision04CE8Faces, 0, sizeof(D_actor_311500_80169324));
                        work->present     = 0;
                        work->step        = ACTOR_311500_DEATH_STEP_CRY;
                        actorTask->state += 1;
                    }
                    break;

                case ACTOR_311500_PHASE_DEATH:
                    if ((s16)_actor311500TickDeath(actorTask) != 0) {
                        actorTask->state += 1;
                        return;
                    }
                    _actor311500UpdatePresentation(actorTask);
                    return;

                case ACTOR_311500_PHASE_DONE:
                    return;
            }
            break;

        case SCENE_COMBAT_ACTORS_HIDDEN:
            if (work->prevActorControl != SCENE_COMBAT_ACTORS_HIDDEN) {
                work->savedModelFlags = model->flags;
            }
            actorTask->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;

        case SCENE_COMBAT_ACTORS_PAUSED:
            break;
    }
    work->prevActorControl = gSceneCombatState.actorControl;
    _actor311500UpdatePresentation(actorTask);
}

/// Reports whether the enemy is present, both as the result and through the output word.
///
/// Handles `ACTOR_MESSAGE_IS_PRESENT`; the first payload is unused and the second
/// must point to one writable `u32`. Requires initialized package work. Presence
/// is 1 until the killing hit starts the death phase, then 0.
static s32 _actor311500IsPresentMessage(Task* actorTask, s32 messageId, s32 unusedArg, u32* presenceResult)
{
    _Actor311500Work* work    = actorTask->work;
    u32               present = work->present;

    *presenceResult = present;
    return present;
}
