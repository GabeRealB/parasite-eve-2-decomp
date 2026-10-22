#include "actors/actor_521100.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actor_521100_private.h"

#include "actors/actor.h"
#include "actors/task_tables.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/fs.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"
#include "../../shared/actor/actor_messages.h"

static void _no9GolemAimHead(const Task* actor);
static void _no9GolemDrawShadow(const Task* task);

/// Native clips used by the fight's movement and hit reactions.
enum {
    ACTOR_521100_ANIM_IDLE        = 1,
    ACTOR_521100_ANIM_GUARD_START = 14,
    ACTOR_521100_ANIM_GUARD_END   = 15,
    ACTOR_521100_ANIM_STAGGER     = 16,
    ACTOR_521100_ANIM_FLINCH      = 17,
    ACTOR_521100_ANIM_WALK        = 18,
};

/// Initial and resolved responses used by the body's hit handler.
enum {
    ACTOR_521100_HIT_RESPONSE_NORMAL  = 0,
    ACTOR_521100_HIT_RESPONSE_GUARD   = 1,
    ACTOR_521100_HIT_RESPONSE_STAGGER = 2,
};

/// Phases and random-table index shared by the hit reactions.
enum {
    ACTOR_521100_REACTION_BEGIN           = 0,
    ACTOR_521100_REACTION_WAIT            = 1,
    ACTOR_521100_RECOVERY_WAIT_INDEX_MASK = 0xF,
};

/// Resumes approach or the saved route after a hit reaction, then draws its wait.
///
/// `bodyWork` must be a side-effect-free pointer expression to the live work
/// block; it is evaluated repeatedly. `waitFrameTable` is evaluated once and
/// supplies sixteen read-only frame counts. Consumes one LCG draw. Expands
/// to a braced block; use only as a statement inside an existing block. A zero
/// resumeRoute selects APPROACH step zero; any nonzero value restores WALK_ROUTE
/// at resumeRouteLeg (a valid route leg). Selects idle animation but leaves speed
/// and attack flags to the reaction's entry. The upper random halfword selects
/// index 0..15, and the u16 wait narrows to s16 stateCounter; normal waits must
/// fit 0..32767 ticks. Captures shared LCG state, but no caller locals or pointers.
#define ACTOR_521100_RESUME_AFTER_HIT(bodyWork, waitFrameTable)                                             \
    {                                                                                                       \
        const u16* waitFrames;                                                                              \
        u32        randomState;                                                                             \
        if ((bodyWork)->resumeRoute == 0) {                                                                 \
            (bodyWork)->state    = ACTOR_521100_STATE_APPROACH;                                             \
            (bodyWork)->subState = 0;                                                                       \
        } else {                                                                                            \
            (bodyWork)->state    = ACTOR_521100_STATE_WALK_ROUTE;                                           \
            (bodyWork)->subState = (bodyWork)->resumeRouteLeg;                                              \
        }                                                                                                   \
        (bodyWork)->animationId  = ACTOR_521100_ANIM_IDLE;                                                  \
        waitFrames               = (waitFrameTable);                                                        \
        randomState              = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;          \
        gRandomLcgState          = randomState;                                                             \
        (bodyWork)->stateCounter = waitFrames[(randomState >> 16) & ACTOR_521100_RECOVERY_WAIT_INDEX_MASK]; \
    }

/// Task callbacks and combat phases used by the body handlers.
enum {
    ACTOR_521100_BODY_SPAWN            = 0,
    ACTOR_521100_BODY_UPDATE           = 1,
    ACTOR_521100_APPROACH_WAIT         = 0,
    ACTOR_521100_APPROACH_WALK         = 1,
    ACTOR_521100_ATTACK_SELECT         = 0,
    ACTOR_521100_ATTACK_RUN_SLASH      = 1,
    ACTOR_521100_ATTACK_RUN_LONG_SLASH = 2,
    ACTOR_521100_ATTACK_RUN_STANCE     = 3,
    ACTOR_521100_STANCE_ENTER          = 0,
    ACTOR_521100_STANCE_HOLD           = 1,
    ACTOR_521100_STANCE_SWING          = 2,
    ACTOR_521100_STANCE_RECOVER        = 3,
    ACTOR_521100_GRAB_SEIZE            = 0,
    ACTOR_521100_GRAB_HOLD             = 1,
    ACTOR_521100_GRAB_THROW            = 2,
    ACTOR_521100_GRAB_COUNTER          = 3,
    ACTOR_521100_GRAB_COUNTER_DEATH    = 4,
    ACTOR_521100_GRAB_KILL             = 5,
    ACTOR_521100_GRAB_KILL_SOUND       = 6,
};

/// Native body clips selected by spawn, attacks and the grab.
enum {
    ACTOR_521100_ANIM_STANCE_ENTER   = 3,
    ACTOR_521100_ANIM_STANCE_HOLD    = 4,
    ACTOR_521100_ANIM_SLASH          = 5,
    ACTOR_521100_ANIM_LONG_SLASH     = 6,
    ACTOR_521100_ANIM_STANCE_RECOVER = 7,
    ACTOR_521100_ANIM_STANCE_SWING   = 8,
    ACTOR_521100_ANIM_GRAB_SEIZE     = 10,
    ACTOR_521100_ANIM_GRAB_HOLD      = 11,
    ACTOR_521100_ANIM_COUNTER_DEATH  = 13,
    ACTOR_521100_ANIM_THROW          = 19,
    ACTOR_521100_ANIM_KILL_PLAYER    = 20,
    ACTOR_521100_ANIM_SPAWN          = 21,
};

static s32 _actor521100ReleasePlayerHold(Task* task, s32 unusedMessageId, s32 unusedArg, s32 unusedSecondArg);

static s32 _actor521100PlayEventAnimation(Task* task, s32 unusedMessageId, const AnimationPlayRequest* request, s32 unusedSecondArg);

/// Scratch-stack block of the grab, reserved for one tick of that state.
///
/// It holds the payloads of the two messages the grab sends the player - the
/// animation of the package's own player set, and the placement that walks the
/// player to the hold and carries them through the throw - and the vectors the
/// placement is worked out in. The player's task reads each payload while its
/// message is dispatched and keeps neither address. No vector's fourth
/// component is written.
typedef struct {
    VECTOR               worldVector;     // Long vector along the world axes: `localOffset` rotated through the golem's root. The seizure adds the root's position to make it the spot the player is held at, then overwrites it with the unit direction of `toHoldSpot` (4096 = 1.0) while the player is still short of the spot
    VECTOR               toHoldSpot;      // Hold spot minus the player's position on X and Z, `vy` zero; a player further than 0x32 from the spot is placed 0x32 nearer to it, any other on it
    SVECTOR              localOffset;     // Short vector in a model's own frame: the hold spot 0x4E2 ahead of the golem's root, the throw's sideways travel for the frame, or the offset the kill passes with the player's coordinate 4 when it spawns its effect
    AnimationPlayRequest playerAnim;      // Animation of the package's player set the player is told to install and play at each stage of the grab
    ActorTransform       playerPlacement; // Where `GAME_ACTOR_MESSAGE_PLACE` puts the player; of its angles only the yaw is ever nonzero
} _Actor521100GrabScratch;
STATIC_ASSERT_SIZEOF(_Actor521100GrabScratch, 0x54);

static void _actor521100ResolveContacts(Task* task);
static void _actor521100ApproachState(Task* task);
static s32  _actor521100TryStartGrab(Task* task);
static void _actor521100AttackState(Task* task);
static void _actor521100SlashAttack(Task* task);
static void _actor521100LongSlashAttack(Task* task);
static void _actor521100StanceAttack(Task* task);
static void _actor521100GrabState(Task* task);
static void _actor521100GuardState(Task* task);
static void _actor521100WalkRouteState(Task* task);
static void _actor521100TurnTowardTargetYaw(Task* task);
static void _actor521100PlayFootsteps(Task* task);
static void _actor521100ApplyHitTwist(Task* task);
static void _actor521100TickEventFire(Task* task);
static void _actor521100Update(Enemy* enemy, Task* task);
static void _actor521100EventTick(Enemy* enemy, Task* task);
static void _actor521100FightTick(Enemy* enemy, Task* task);
static void _actor521100TickState(Task* task);
static void _actor521100TryStartAttack(Task* task);
static void _actor521100StaggerState(Task* task);
static void _actor521100FlinchState(Task* task);
static void _actor521100StepRootPosition(Task* task);
static void _actor521100TickAnimation(Task* task);
static void _actor521100UpdateLighting(Task* task);
static void _actor521100AttachGunblade(Enemy* unusedEnemy, Task* task);
static void _actor521100UpdateGunbladeDrawMode(Enemy* unusedEnemy, Task* task);

static TmdSource _gGolemNo9BodyDryfield;
static TmdSource _gGolemNo9Gunblade;
static void      _actor521100BodyTask(Task* task);
static void      _actor521100GunbladeTask(Task* task);

#include "assets/enemies/golem/no_9/package_dryfield_clips_1.inc"

#include "assets/enemies/golem/no_9/body_dryfield.inc"

#include "assets/enemies/golem/no_9/gunblade.inc"

#include "assets/enemies/golem/no_9/package_dryfield_clips_2.inc"

/// Distinct, status-free player hit presentation of the golem's weapon attacks.
enum { ACTOR_521100_WEAPON_HIT_REACTION = 7 };

/// No. 9 GOLEM damage rows for slash, long slash, stance swing and held-player strikes.
///
/// Rows 0..2 have powers 15, 25 and 40 with hit presentation 7; row 3 has
/// power 10 and the ordinary reaction. The enemy parameters and both attack
/// spheres borrow these entries; keep them loaded through the fight.
static DamageAttack _gActor521100Attacks[4] = {
    { 15, ACTOR_521100_WEAPON_HIT_REACTION },
    { 25, ACTOR_521100_WEAPON_HIT_REACTION },
    { 40, ACTOR_521100_WEAPON_HIT_REACTION },
    { 10, GAME_ACTOR_REACTION_ORDINARY },
};

/// No. 9 GOLEM combat tuning and rewards for the Dryfield fight.
///
/// Body spawn borrows this record and starts at 1100 HP. Rewards are 800 EXP,
/// 300 BP and 50 MP; critical and status-reaction tunings are zero. The grab
/// counterattack also compares remaining enemy HP with a third of this maximum.
static EnemyParams _gActor521100EnemyParams = { _gActor521100Attacks, 1100, 800, 300, 50, 0, 0, 0, 0 };

/// Player HP thresholds for a fatal held-player strike, indexed by combat difficulty.
///
/// At each damage interval, HP at or below the selected limit starts the kill
/// clip instead of another strike. Rows 0..4 correspond to saved modes and
/// normal replay; retained row 5 is zero and its gameplay use is unproven.
static s16 _gActor521100GrabKillHpByDifficulty[6] = {
    6,
    11,
    16,
    16,
    4,
    0,
};

/// Weighted attack choices within 2300 parent-coordinate units of the player.
///
/// A four-bit LCG draw selects ten slash entries or six stance entries. The
/// last two stance entries also precede the repeat-choice rows in storage.
static s16 _gActor521100NearAttackChoices[16] = {
    ACTOR_521100_ATTACK_SLASH,
    ACTOR_521100_ATTACK_SLASH,
    ACTOR_521100_ATTACK_SLASH,
    ACTOR_521100_ATTACK_SLASH,
    ACTOR_521100_ATTACK_SLASH,
    ACTOR_521100_ATTACK_SLASH,
    ACTOR_521100_ATTACK_SLASH,
    ACTOR_521100_ATTACK_SLASH,
    ACTOR_521100_ATTACK_SLASH,
    ACTOR_521100_ATTACK_SLASH,
    ACTOR_521100_ATTACK_STANCE,
    ACTOR_521100_ATTACK_STANCE,
    ACTOR_521100_ATTACK_STANCE,
    ACTOR_521100_ATTACK_STANCE,
    ACTOR_521100_ATTACK_STANCE,
    ACTOR_521100_ATTACK_STANCE,
};

/// Near-range choices that prevent a third identical attack.
///
/// Rows 0..2 select alternatives to the previous slash, long slash or stance;
/// a random bit selects the column. The initial NONE choice (-1) currently
/// reads the preceding weighted table's final two stance entries.
static s16 _gActor521100NearAttackChoicesAfterRepeat[3][2] = {
    { ACTOR_521100_ATTACK_STANCE, ACTOR_521100_ATTACK_STANCE },
    { ACTOR_521100_ATTACK_SLASH, ACTOR_521100_ATTACK_STANCE },
    { ACTOR_521100_ATTACK_SLASH, ACTOR_521100_ATTACK_SLASH },
};

/// Weighted attack choices at least 2300 parent-coordinate units from the player.
///
/// A four-bit LCG draw selects four slash, eight long-slash or four stance
/// entries. The final two stance entries precede the repeat-choice rows.
static s16 _gActor521100FarAttackChoices[16] = {
    ACTOR_521100_ATTACK_SLASH,
    ACTOR_521100_ATTACK_SLASH,
    ACTOR_521100_ATTACK_SLASH,
    ACTOR_521100_ATTACK_SLASH,
    ACTOR_521100_ATTACK_LONG_SLASH,
    ACTOR_521100_ATTACK_LONG_SLASH,
    ACTOR_521100_ATTACK_LONG_SLASH,
    ACTOR_521100_ATTACK_LONG_SLASH,
    ACTOR_521100_ATTACK_LONG_SLASH,
    ACTOR_521100_ATTACK_LONG_SLASH,
    ACTOR_521100_ATTACK_LONG_SLASH,
    ACTOR_521100_ATTACK_LONG_SLASH,
    ACTOR_521100_ATTACK_STANCE,
    ACTOR_521100_ATTACK_STANCE,
    ACTOR_521100_ATTACK_STANCE,
    ACTOR_521100_ATTACK_STANCE,
};

/// Far-range choices that prevent a third identical attack.
///
/// Rows 0..2 select alternatives to the previous slash, long slash or stance;
/// a random bit selects the column. The initial NONE choice (-1) currently
/// reads the preceding weighted table's final two stance entries.
static s16 _gActor521100FarAttackChoicesAfterRepeat[3][2] = {
    { ACTOR_521100_ATTACK_LONG_SLASH, ACTOR_521100_ATTACK_STANCE },
    { ACTOR_521100_ATTACK_SLASH, ACTOR_521100_ATTACK_STANCE },
    { ACTOR_521100_ATTACK_SLASH, ACTOR_521100_ATTACK_LONG_SLASH },
};

/// Sixteen stance-hold durations in state ticks, selected by a four-bit LCG draw.
///
/// All entries are zero. The stance still consumes the draw and decrements
/// before testing the timer, so a zero wait does not remove its hold tick.
static u16 _gActor521100StanceHoldFrames[16] = { 0 };

/// No. 9 GOLEM's sixteen-entry distribution of approach and attack-recovery waits.
///
/// A four-bit LCG draw selects 0 (eight entries), 15 (four) or 30 (four) frames
/// for stateCounter, including after flinch, grabs and ordinary attacks.
/// The approach state decrements before testing; zero may still occupy a tick.
static u16 _gActor521100ApproachWaitFrames[16] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    15,
    15,
    15,
    15,
    30,
    30,
    30,
    30,
};

/// Sixteen approach-walk durations in state ticks, selected by a four-bit LCG draw.
///
/// Every entry is fifteen. Initial and retried approach walks copy a sample
/// into stateCounter; the walk consumes it by decrementing once per update.
static u16 _gActor521100WalkDurationFrames[16] = {
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
    15,
};

/// Sixteen post-hit wait durations in state ticks, selected by a four-bit LCG draw.
///
/// Guard and stagger completion resume approach or the saved route and load
/// stateCounter from this table. All entries are zero; the draw and subsequent
/// state timer behavior are retained.
static u16 _gActor521100HitRecoveryWaitFrames[16] = { 0 };

/// Three horizontal route targets indexed by ACTOR_521100_ROUTE_* (0..2).
///
/// XYZ are integer game units in the model root's parent frame; consumers use
/// only X/Z. The first two legs may target the player instead. The return leg
/// always aims at its waypoint; entries 0 and 2 are the same location.
static VECTOR _gActor521100RouteWaypoints[3] = {
    { -4000, 0, -2000, 0 },
    { -5250, 0, -1200, 0 },
    { -4000, 0, -2000, 0 },
};

/// No. 9 GOLEM base hit responses indexed by the weapon key's low six bits.
///
/// The caller must supply a row in 0..47. Rows 10..12 request guard and all
/// others normal damage. Attachment keys bypass the table and request guard;
/// the contact handler may refine either response for the current fight state.
static s16 _gActor521100WeaponHitResponses[48] = {
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_GUARD,
    ACTOR_521100_HIT_RESPONSE_GUARD,
    ACTOR_521100_HIT_RESPONSE_GUARD,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
    ACTOR_521100_HIT_RESPONSE_NORMAL,
};

TaskDesc gActor521100No9GolemTaskDescs[2] = {
    { { { TASK_BODY_TMD, 96 } }, _actor521100BodyTask, { .model = &_gGolemNo9BodyDryfield } },
    { { { TASK_BODY_TMD, 96 } }, _actor521100GunbladeTask, { .model = &_gGolemNo9Gunblade } },
};

/// No. 9 GOLEM body message bindings, terminated by TASK_MESSAGE_TABLE_END.
///
/// Handles player-hold release, scripted animation, placement, model visibility,
/// room commands, activation and presence queries. Installed after body work
/// initialization and borrowed until the body task is destroyed.
static TaskMessageEntry _gActor521100BodyMessageTable[8] = {
    { ACTOR_MESSAGE_RELEASE_HOLD, _actor521100ReleasePlayerHold },
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor521100PlayEventAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceRotMatrix },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actor521100SetModelDrawFlags },
    { ACTOR_COMMAND_MESSAGE_APPLY, actor521100ApplyCommand },
    { ACTOR_521100_MESSAGE_ACTIVATE, actor521100Activate },
    { ACTOR_MESSAGE_IS_PRESENT, actor521100IsPresent },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void       _actor521100SpawnBody(Enemy* enemy, Task* task);
static inline s32 _actor521100GetBaseHitResponse(s32 attackKey);

/// Allocates the Dryfield GOLEM body work, gunblade child and collision bodies.
///
/// Requires the loaded nineteen-part body model and its live Enemy record. The
/// zeroed work block owns animation, lighting and six linked collision bodies;
/// the child gunblade borrows its lighting and shares its attack contacts.
/// Allocation failure destroys the enemy. Successful setup installs messages
/// and advances the task from spawn to update. The child spawn must succeed.
static void _actor521100SpawnBody(Enemy* enemy, Task* task)
{
    GameLocationKey  location;
    TmdObject*       bodyModel;
    GfxCoord*        rootCoord;
    Actor521100Work* work;
    Enemy*           weaponEnemy;
    TmdObject*       weaponModel;
    GameLocationKey* sessionLocation;
    AreaPlacement*   placement;
    s32              placementIndex;
    u32              placementKey;
    s32              slotIndex;

    bodyModel = task->extra.tmd;
    rootCoord = bodyModel->coords;
    work      = memCalloc(sizeof(Actor521100Work), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work              = work;
    bodyModel->flags        = 0;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    bodyModel->lightMtx     = &work->light;
    bodyModel->colorMtx     = &work->color;
    enemy->field_4          = &rootCoord->coord;
    enemy->field_48         = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = &task->extra.tmd->coords[3];
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &_gActor521100EnemyParams;
    enemy->recs                   = work->bodyContacts;
    enemy->hp                     = _gActor521100EnemyParams.hpMax;
    work->hitEffectArg.coord      = &task->extra.tmd->coords[3];
    work->hitEffectArg.spawnArgLo = 0x400;
    work->hitEffectArg.spawnArgHi = 3;
    animationInitContext(&work->rig.anim, gActor521100BodyAnimationSets, bodyModel, work->rig.poses, work->rig.slots);
    work->animationId       = ACTOR_521100_ANIM_SPAWN;
    work->seededAnimationId = ACTOR_521100_ANIM_SPAWN;
    slotIndex               = 1;
    do {
        animationResetSlot(&work->rig.anim, slotIndex, work->animationId);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->rig.slots));
    work->present          = 1;
    work->attackChoice     = ACTOR_521100_ATTACK_NONE;
    work->prevAttackChoice = ACTOR_521100_ATTACK_NONE;

    work->groundBody.coord            = task->extra.tmd->coords;
    work->groundBody.context.contacts = work->groundContacts;
    work->groundBody.pos.vx           = 0;
    work->groundBody.pos.vy           = -0x190;
    work->groundBody.pos.vz           = 0;
    work->groundBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | 0x22;
    work->groundBody.radius           = 0x190;
    work->groundBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->groundBody);
    worldCollisionInitContacts(work->groundContacts, ARRAY_SIZE(work->groundContacts), 0);
    work->groundBody.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);

    work->body.coord            = &task->extra.tmd->coords[3];
    work->body.context.contacts = work->bodyContacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | 0x22;
    work->body.radius           = 0x190;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);
    work->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    weaponEnemy        = enemySpawnFromTable(gActor521100No9GolemTaskDescs, 1, 0, enemy);
    weaponModel        = weaponEnemy->task->extra.tmd;
    placementKey       = enemy->placeKey;
    sessionLocation    = &gGameSession->location;
    location.stage     = sessionLocation->stage;
    location.area      = sessionLocation->area;
    location.roomState = sessionLocation->roomState;
    placementIndex     = placementKey >> ENEMY_PLACE_INDEX_SHIFT;
    location.view      = sessionLocation->view;
    areaSyncLocationVariant(&location);
    // The child weapon uses the body placement's texture page and CLUT.
    placement                      = AREA_PLACEMENT_AT(areaGetVariant(&location)->placements, placementIndex);
    weaponModel->texturePageOffset = placement->texturePageOffset;
    weaponModel->clutRowOffset     = placement->clutRowOffset;
    if (weaponModel->buffer != NULL) {
        tmdBuildBufferHalf(weaponModel);
        tmdBuildBufferHalf(weaponModel);
    }
    work->weaponTask = weaponEnemy->task;

    work->weaponAttack.coord            = weaponEnemy->task->extra.tmd->coords;
    work->weaponAttack.pos.vx           = -0x226;
    work->weaponAttack.context.contacts = work->attackContacts;
    work->weaponAttack.pos.vy           = 0x64;
    work->weaponAttack.pos.vz           = 0;
    work->weaponAttack.key              = 0;
    work->weaponAttack.radius           = 0x1C2;
    work->weaponAttack.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->weaponAttack);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->weaponAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->forearmAttack.coord            = &task->extra.tmd->coords[7];
    work->forearmAttack.context.contacts = work->attackContacts;
    work->forearmAttack.pos.vx           = 0;
    work->forearmAttack.pos.vy           = 0;
    work->forearmAttack.pos.vz           = 0;
    work->forearmAttack.key              = 0;
    work->forearmAttack.radius           = 0x1C2;
    work->forearmAttack.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->forearmAttack);

    work->grabPathCapsule.ends[0].vz = 0x5DC;
    work->grabPathCapsule.ends[0].vx = 0;
    work->grabPathCapsule.ends[0].vy = 0;
    work->grabPathCapsule.ends[1].vx = 0;
    work->grabPathCapsule.ends[1].vy = 0;
    work->grabPathCapsule.ends[1].vz = 0;
    work->grabPathCapsule.end0Radius = 1;
    work->grabPathCapsule.end1Radius = 1;
    work->grabPathCapsule.contacts   = work->grabProbeContacts;
    work->forearmAttack.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->grabPathProbe.coord           = task->extra.tmd->coords;
    work->grabPathProbe.context.capsule = &work->grabPathCapsule;
    work->grabPathProbe.pos.vy          = -0x1F4;
    work->grabPathProbe.pos.vx          = 0;
    work->grabPathProbe.pos.vz          = 0;
    work->grabPathProbe.key             = 0;
    work->grabPathProbe.radius          = 0;
    work->grabPathProbe.flags           = WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->grabPathProbe);
    worldCollisionInitContacts(work->grabProbeContacts, ARRAY_SIZE(work->grabProbeContacts), 0);
    work->grabPathProbe.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;

    work->grabSpotProbe.coord            = task->extra.tmd->coords;
    work->grabSpotProbe.pos.vy           = -0x320;
    work->grabSpotProbe.context.contacts = work->grabProbeContacts;
    work->grabSpotProbe.pos.vx           = 0;
    work->grabSpotProbe.pos.vz           = 0x4E2;
    work->grabSpotProbe.key              = 0;
    work->grabSpotProbe.radius           = 0x1C2;
    work->grabSpotProbe.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->grabSpotProbe);
    work->grabSpotProbe.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;

    task->msgTable = _gActor521100BodyMessageTable;
    task->state    = ACTOR_521100_BODY_UPDATE;
}

/// Returns the golem's initial response to a player attack: normal or guard.
///
/// Attachment keys (bit 15) always request a guard; weapon keys select the
/// 48-entry response table with their low six bits, which must be in 0..47.
/// Category and other bits are ignored. The hit handler may turn a guard into
/// a normal hit during a grab or a stagger when struck off-axis or attacking.
static inline s32 _actor521100GetBaseHitResponse(s32 attackKey)
{
    enum {
        ACTOR_521100_ATTACK_KEY_ATTACHMENT = 0x8000,
        ACTOR_521100_ATTACK_KEY_ROW_MASK   = 0x3F,
    };
    if (attackKey & ACTOR_521100_ATTACK_KEY_ATTACHMENT) {
        return ACTOR_521100_HIT_RESPONSE_GUARD;
    }
    return _gActor521100WeaponHitResponses[attackKey & ACTOR_521100_ATTACK_KEY_ROW_MASK];
}

/// Resolves ground contacts, player hits, attack contacts and grab obstructions.
///
/// Requires live body work and its Enemy in spawnArg2.pointer. Opposed ground
/// contacts restore the previous root position. Hits apply the golem's guard,
/// stagger or normal response, unsigned damage and per-hit cooldowns; consumed
/// contact tables are cleared. Lethal damage clears presence only while the
/// player lives.
static void _actor521100ResolveContacts(Task* task)
{
    enum {
        ACTOR_521100_CONTACT_ATTACHMENT_BIT = 0x8000,
        ACTOR_521100_CRITICAL_ATTRIBUTE     = 5,
        ACTOR_521100_SOUND_GUARD            = 0x401C0003,
        ACTOR_521100_SOUND_STAGGER_FIRST    = 0x401C0004,
        ACTOR_521100_SOUND_STAGGER_SECOND   = 0x401C0005,
        ACTOR_521100_SOUND_FLINCH           = 0x401C0006,
    };
    ActorContactDeltaWideScratch* scratch;
    Actor521100Work*              work;
    Enemy*                        enemy;
    GfxCoord*                     rootCoord;
    u32                           lastEffectAttackKey;
    u32                           soundScript;
    u32                           contactKind;
    u32                           damage;
    u32                           twistRandomState;
    u32                           flinchRandomState;
    u32                           twistRandomBits;
    s32                           pushbackResult;
    s32                           dx;
    s32                           rootX;
    s32                           dz;
    s32                           differenceMagnitude;
    s32                           yawRandomBits;
    s32                           pitchTwist;
    s32                           yawTwist;
    s32                           hitResponse;
    s32                           contactIndex;
    s16                           signedDifference;
    s16                           facingError;
    s16                           remainingCooldown;
    s32                           flinchPan;
    s32                           guardPan;
    s32                           staggerPan;
    s32                           audioDepth;
    s16                           hitWaitFrames;

    lastEffectAttackKey = 0;
    work                = task->work;
    scratch             = SCRATCH_STACK_RESERVE_BLOCK(ActorContactDeltaWideScratch);
    rootCoord           = task->extra.tmd->coords;
    enemy               = task->spawnArg2.pointer;
    pushbackResult      = worldCollisionResolvePushback(work->groundContacts, &scratch->delta, ARRAY_SIZE(work->groundContacts), NULL);
    switch (pushbackResult) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            rootCoord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
            rootCoord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            rootCoord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            rootCoord->coord.t[0] = work->prevRootPos.vx;
            rootCoord->coord.t[1] = work->prevRootPos.vy;
            rootCoord->coord.t[2] = work->prevRootPos.vz;
            break;
    }
    worldCollisionClearContacts(work->groundContacts);
    if (work->hitCooldown != 0) {
        remainingCooldown = (u16)work->hitCooldown - 1;
        work->hitCooldown = remainingCooldown;
        if ((remainingCooldown << 0x10) <= 0) {
            work->hitCooldown = 0;
        }
    }
    if (work->flinchCooldown != 0) {
        work->flinchCooldown = (u16)work->flinchCooldown - 1;
    }
    // Apply attack contacts before latching this tick's outgoing hits and probes.
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->bodyContacts); contactIndex++) {
        contactKind = work->bodyContacts[contactIndex].key.parts.kind;
        if (contactKind < (WORLD_COLLISION_CONTACT_ATTACK >> 16)) {
            continue;
        }
        if (contactKind != (WORLD_COLLISION_CONTACT_ATTACK >> 16)) {
            continue;
        }
        if (work->hitCooldown != 0) {
            continue;
        }
        rootX                    = rootCoord->coord.t[0];
        dx                       = gPlayerStatus.coordMtx->t[0] - rootX;
        scratch->delta.vector.vx = dx;
        scratch->delta.vector.vy = 0;
        dz                       = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
        scratch->delta.vector.vz = dz;
        damage                   = damageComputePlayerAttack(work->bodyContacts[contactIndex].key.value, SquareRoot0(dx * dx + dz * dz), 0, 0);
        hitResponse              = _actor521100GetBaseHitResponse(work->bodyContacts[contactIndex].key.value);
        if (hitResponse == ACTOR_521100_HIT_RESPONSE_GUARD) {
            if (work->state == ACTOR_521100_STATE_GRAB) {
                hitResponse = ACTOR_521100_HIT_RESPONSE_NORMAL;
            } else {
                signedDifference    = work->yaw - (ratan2((s16)scratch->delta.vector.vx, (s16)scratch->delta.vector.vz) & ACTOR_TRANSFORM_ANGLE_MASK);
                differenceMagnitude = abs(signedDifference);
                if (differenceMagnitude < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                    facingError = differenceMagnitude;
                } else if (signedDifference > 0) {
                    facingError = ACTOR_TRANSFORM_ANGLE_TURN - signedDifference;
                } else {
                    facingError = signedDifference + ACTOR_TRANSFORM_ANGLE_TURN;
                }
                if ((facingError >= 0x301) || (work->attackLive == 1)) {
                    hitResponse = ACTOR_521100_HIT_RESPONSE_STAGGER;
                }
            }
        }
        switch (hitResponse) {
            case ACTOR_521100_HIT_RESPONSE_NORMAL:
                twistRandomState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                twistRandomBits  = twistRandomState >> 0x10;
                pitchTwist       = (twistRandomBits & 0x7F) + 0x40;
                gRandomLcgState  = twistRandomState;
                if (!(twistRandomBits & 1)) {
                    pitchTwist = -pitchTwist;
                }
                work->hitTwist.vx = pitchTwist;
                yawRandomBits     = (s16)twistRandomBits >> 8;
                yawTwist          = (yawRandomBits & 0x7F) + 0x40;
                if (!(yawRandomBits & 1)) {
                    yawTwist = -yawTwist;
                }
                work->hitTwist.vy    = yawTwist;
                work->hitTwistActive = 1;
                damage             >>= 1;
                if ((damageGetPlayerAttackReaction(work->bodyContacts[contactIndex].key.value) & 0xFFFF) == ACTOR_521100_CRITICAL_ATTRIBUTE) {
                    damage *= 2;
                    effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[3], 2, NULL);
                }
                if ((work->state == ACTOR_521100_STATE_APPROACH) && (work->flinchCooldown <= 0)) {
                    work->state          = ACTOR_521100_STATE_FLINCH;
                    work->subState       = ACTOR_521100_REACTION_BEGIN;
                    flinchRandomState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->flinchCooldown = ((flinchRandomState >> 0x10) & 0xFF) + 0x96;
                    gRandomLcgState      = flinchRandomState;
                    soundScript          = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_521100_SOUND_FLINCH;
                    flinchPan            = (s8)worldCoordGetOriginAudioPan(rootCoord);
                    audioDepth           = (s8)worldCoordGetOriginAudioDepth(rootCoord);
                    sndEvtRequestScriptStart((s32)soundScript, flinchPan, audioDepth);
                }
                break;
            case ACTOR_521100_HIT_RESPONSE_GUARD:
                work->state    = ACTOR_521100_STATE_GUARD;
                work->subState = ACTOR_521100_REACTION_BEGIN;
                if (work->bodyContacts[contactIndex].key.value & ACTOR_521100_CONTACT_ATTACHMENT_BIT) {
                    damage >>= 2;
                } else {
                    damage >>= 3;
                }
                soundScript = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_521100_SOUND_GUARD;
                guardPan    = (s8)worldCoordGetOriginAudioPan(rootCoord);
                audioDepth  = (s8)worldCoordGetOriginAudioDepth(rootCoord);
                sndEvtRequestScriptStart((s32)soundScript, guardPan, audioDepth);
                break;
            case ACTOR_521100_HIT_RESPONSE_STAGGER:
                work->state      = ACTOR_521100_STATE_STAGGER;
                work->subState   = ACTOR_521100_REACTION_BEGIN;
                work->attackLive = 0;
                if (work->bodyContacts[contactIndex].key.value & ACTOR_521100_CONTACT_ATTACHMENT_BIT) {
                    damage *= 2;
                } else {
                    damage >>= 1;
                }
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((gRandomLcgState >> 16) & 1) {
                    soundScript = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_521100_SOUND_STAGGER_FIRST;
                } else {
                    soundScript = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_521100_SOUND_STAGGER_SECOND;
                }
                staggerPan = (s8)worldCoordGetOriginAudioPan(rootCoord);
                audioDepth = (s8)worldCoordGetOriginAudioDepth(rootCoord);
                sndEvtRequestScriptStart((s32)soundScript, staggerPan, audioDepth);
                break;
        }
        damageAccumulateLifeDrainHp(enemy, work->bodyContacts[contactIndex].key.value, (s32)damage, 0);
        worldTargetAddReadoutAmount(&enemy->node, (s32)damage, 0);
        enemy->hp = (u16)enemy->hp - damage;
        if (lastEffectAttackKey != work->bodyContacts[contactIndex].key.value) {
            lastEffectAttackKey = work->bodyContacts[contactIndex].key.value;
            effectSpawnHit(damageGetPlayerAttackEffectId(work->bodyContacts[contactIndex].key.value), &task->extra.tmd->coords[3], NULL, &work->hitEffectArg);
        }
        hitWaitFrames = damageGetPlayerAttackHitCooldown(work->bodyContacts[contactIndex].key.value);
        if (hitWaitFrames > 0) {
            work->hitCooldown = hitWaitFrames;
        }
    }
    worldCollisionClearContacts(work->bodyContacts);
    if (work->attackContacts[0].flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
        work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(work->attackContacts);
        work->attackLanded = 1;
    }
    work->grabBlocked = 0;
    if (work->grabProbeContacts[0].flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
        work->grabBlocked = 1;
        worldCollisionClearContacts(work->grabProbeContacts);
    }
    if (enemy->hp <= 0) {
        if (gPlayerStatus.hp > 0) {
            work->present = 0;
        } else {
            enemy->hp = 1;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactDeltaWideScratch);
}

/// Alternates idle waits and walking towards the player, trying grabs and attacks.
///
/// Requires live body work. Measures the horizontal player delta in root-parent
/// units, narrows distance to s16, and stores a bearing in 4096 units per turn.
/// Each random wait selects one of sixteen frame counts. View 2 switches to the
/// fixed route. One scratch VECTOR lives only until this call returns.
static void _actor521100ApproachState(Task* task)
{
    Actor521100Work* work;
    GfxCoord*        rootCoord;
    VECTOR*          previousScratchTop;
    VECTOR*          playerDelta;
    u16*             walkDurationTable;
    u16*             idleWaitTable;
    u16*             retryWalkDurationTable;
    u32              walkRandomState;
    u32              idleRandomState;
    u32              retryRandomState;
    s16              approachPhase;
    s16              signedDifference;
    s16              playerBearing;
    s16              remainingFrames;
    s16              facingError;
    s32              differenceMagnitude;

    rootCoord                    = task->extra.tmd->coords;
    work                         = task->work;
    previousScratchTop           = SCRATCH_STACK_CURSOR(VECTOR);
    playerDelta                  = previousScratchTop - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = playerDelta;
    previousScratchTop[-1].vx    = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
    playerDelta->vy              = 0;
    playerDelta->vz              = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
    work->playerDistance         = SquareRoot0(previousScratchTop[-1].vx * previousScratchTop[-1].vx + playerDelta->vz * playerDelta->vz);
    playerBearing                = ratan2((s16)previousScratchTop[-1].vx, (s16)playerDelta->vz) & ACTOR_TRANSFORM_ANGLE_MASK;
    work->targetYaw              = playerBearing;
    if (gGameSession->location.view == 2) {
        work->state    = ACTOR_521100_STATE_WALK_ROUTE;
        work->subState = 0;
    } else {
        approachPhase = work->subState;
        switch (approachPhase) {
            case ACTOR_521100_APPROACH_WAIT:
                work->forwardSpeed = 0;
                work->turnSpeed    = 0;
                remainingFrames    = (u16)work->stateCounter - 1;
                work->stateCounter = remainingFrames;
                if (remainingFrames < 0) {
                    work->subState     = ACTOR_521100_APPROACH_WALK;
                    work->animationId  = ACTOR_521100_ANIM_WALK;
                    walkDurationTable  = _gActor521100WalkDurationFrames;
                    walkRandomState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState    = walkRandomState;
                    work->stateCounter = walkDurationTable[(walkRandomState >> 16) & ACTOR_521100_RECOVERY_WAIT_INDEX_MASK];
                } else if (_actor521100TryStartGrab(task) == 0) {
                    _actor521100TryStartAttack(task);
                }
                break;
            case ACTOR_521100_APPROACH_WALK:
                if ((work->grabBlocked == approachPhase) && (work->playerDistance < 0x7D0)) {
                    signedDifference    = playerBearing - work->yaw;
                    differenceMagnitude = abs(signedDifference);
                    if (differenceMagnitude < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                        facingError = differenceMagnitude;
                    } else {
                        if (signedDifference > 0) {
                            facingError = ACTOR_TRANSFORM_ANGLE_TURN - signedDifference;
                        } else {
                            facingError = signedDifference + ACTOR_TRANSFORM_ANGLE_TURN;
                        }
                    }
                    if (facingError < 0x100) {
                        work->stateCounter = 0;
                    }
                }
                remainingFrames    = (u16)work->stateCounter - 1;
                work->stateCounter = remainingFrames;
                if (remainingFrames <= 0) {
                    work->turnSpeed    = 0x78;
                    work->forwardSpeed = 0;
                    idleWaitTable      = _gActor521100ApproachWaitFrames;
                    idleRandomState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState    = idleRandomState;
                    remainingFrames    = idleWaitTable[(idleRandomState >> 16) & ACTOR_521100_RECOVERY_WAIT_INDEX_MASK];
                    work->stateCounter = remainingFrames;
                    if (remainingFrames == 0) {
                        _actor521100TryStartAttack(task);
                        if (work->state == ACTOR_521100_STATE_APPROACH) {
                            retryWalkDurationTable = _gActor521100WalkDurationFrames;
                            retryRandomState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            gRandomLcgState        = retryRandomState;
                            work->stateCounter     = retryWalkDurationTable[(retryRandomState >> 16) & ACTOR_521100_RECOVERY_WAIT_INDEX_MASK];
                        }
                    } else {
                        work->subState    = ACTOR_521100_APPROACH_WAIT;
                        work->animationId = ACTOR_521100_ANIM_IDLE;
                    }
                } else {
                    work->forwardSpeed = 0x14;
                    work->turnSpeed    = 0x78;
                    _actor521100TryStartGrab(task);
                }
                break;
        }
    }
    work->attackLive = 0;
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Starts a grab when the nearby living player accepts a button-press hold.
///
/// Uses the approach state's distance and bearing. Within 1250 units and a
/// quarter turn, an unobstructed attempt sets turnSpeed to 80 even if no hold
/// starts. Within 32 angle units, a nonscripted player is asked for 25 presses.
/// Accepted dispatch enters seizure, stops movement and returns 1; otherwise
/// returns 0. The scratch request is borrowed only during synchronous dispatch.
static s32 _actor521100TryStartGrab(Task* task)
{
    enum {
        ACTOR_521100_GRAB_REACH        = 1250,
        ACTOR_521100_GRAB_PRESS_COUNT  = 25,
        ACTOR_521100_GRAB_FACING_LIMIT = 32,
        ACTOR_521100_GRAB_TURN_CONE    = ACTOR_TRANSFORM_ANGLE_TURN / 4,
    };
    Actor521100Work*          work;
    Task*                     playerTask;
    GameActorButtonPressHold* holdRequest;
    s16                       signedDifference;
    s32                       differenceMagnitude;
    s16                       facingError;
    s32                       started;

    work        = task->work;
    playerTask  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    holdRequest = SCRATCH_STACK_RESERVE_BLOCK(GameActorButtonPressHold);

    signedDifference    = work->targetYaw - work->yaw;
    differenceMagnitude = signedDifference >= 0 ? signedDifference : -signedDifference;
    started             = 0;
    if (differenceMagnitude < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        facingError = differenceMagnitude;
    } else if (signedDifference > 0) {
        facingError = ACTOR_TRANSFORM_ANGLE_TURN - signedDifference;
    } else {
        facingError = signedDifference + ACTOR_TRANSFORM_ANGLE_TURN;
    }
    if ((facingError < ACTOR_521100_GRAB_TURN_CONE) && (work->playerDistance < ACTOR_521100_GRAB_REACH) && (work->grabBlocked == 0) && (gPlayerStatus.hp > 0) && (work->turnSpeed = 0x50, (facingError < ACTOR_521100_GRAB_FACING_LIMIT)) && (((GameActor*)playerTask->work)->mode != GAME_ACTOR_MODE_SCRIPTED)) {
        holdRequest->pressCount = ACTOR_521100_GRAB_PRESS_COUNT;
        if (TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, holdRequest, 0) == 0) {
            started             = 1;
            work->playerEscaped = 0;
            work->state         = ACTOR_521100_STATE_GRAB;
            work->subState      = ACTOR_521100_GRAB_SEIZE;
            work->attackStep    = 0;
            work->animationId   = ACTOR_521100_ANIM_GRAB_SEIZE;
            work->forwardSpeed  = 0;
            work->turnSpeed     = 0;
            padScriptSpawnVariableMotorRamp(0xA, 0xFF, 0x80);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(GameActorButtonPressHold);
    return started;
}
/// Chooses and advances one of the GOLEM's three gunblade attacks.
///
/// Refreshes horizontal distance and bearing, then selects slash, long slash or
/// stance from the distance/repeat tables. Armed/casting attachments and view 2
/// force stance. The selected damage key is installed on both attack spheres.
/// The initial NONE/NONE pair indexes row -1 of the repeat table, retaining the
/// original read of the preceding table's last two halfwords.
static void _actor521100AttackState(Task* task)
{
    Actor521100Work* work;
    GfxCoord*        rootCoord;
    VECTOR*          previousScratchTop;
    VECTOR*          playerDelta;
    s16*             nearChoiceTable;
    s16*             farChoiceTable;
    u16              previousChoice;
    u32              nearRandomState;
    u32              farRandomState;
    s32              attackKey;
    s32              nextAttack;

    rootCoord          = task->extra.tmd->coords;
    work               = task->work;
    previousScratchTop = SCRATCH_STACK_CURSOR(VECTOR);
    playerDelta        = previousScratchTop - 1;

    SCRATCH_STACK_CURSOR(VECTOR) = playerDelta;
    previousScratchTop[-1].vx    = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
    playerDelta->vy              = 0;
    playerDelta->vz              = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];

    work->playerDistance = SquareRoot0(previousScratchTop[-1].vx * previousScratchTop[-1].vx + playerDelta->vz * playerDelta->vz);
    work->targetYaw      = ratan2((s16)previousScratchTop[-1].vx, (s16)playerDelta->vz) & ACTOR_TRANSFORM_ANGLE_MASK;

    switch (work->subState) {
        case ACTOR_521100_ATTACK_SELECT:
            if (((u32)((u8)gAttachmentState.mode - ATTACHMENT_MODE_ARMED) >= 2U) && (gGameSession->location.view != 2)) {
                if (work->playerDistance < 0x8FC) {
                    if (work->prevAttackChoice == work->attackChoice) {
                        nextAttack = _gActor521100NearAttackChoicesAfterRepeat[work->attackChoice][((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 1];
                    } else {
                        nearChoiceTable = _gActor521100NearAttackChoices;
                        nearRandomState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        nextAttack      = nearChoiceTable[(nearRandomState >> 16) & 0xF];
                        gRandomLcgState = nearRandomState;
                    }
                } else {
                    if (work->prevAttackChoice == work->attackChoice) {
                        nextAttack = _gActor521100FarAttackChoicesAfterRepeat[work->attackChoice][((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 1];
                    } else {
                        farChoiceTable  = _gActor521100FarAttackChoices;
                        farRandomState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        nextAttack      = farChoiceTable[(farRandomState >> 16) & 0xF];
                        gRandomLcgState = farRandomState;
                    }
                }
            } else {
                nextAttack = ACTOR_521100_ATTACK_STANCE;
            }

            previousChoice         = (u16)work->attackChoice;
            work->attackChoice     = nextAttack;
            work->prevAttackChoice = previousChoice;

            switch (nextAttack) {
                case ACTOR_521100_ATTACK_SLASH:
                    work->subState          = ACTOR_521100_ATTACK_RUN_SLASH;
                    work->animationId       = ACTOR_521100_ANIM_SLASH;
                    attackKey               = damagePackAttackKey(_gActor521100Attacks, ACTOR_521100_ATTACK_SLASH);
                    work->weaponAttack.key  = attackKey;
                    work->forearmAttack.key = attackKey;
                    break;
                case ACTOR_521100_ATTACK_LONG_SLASH:
                    work->subState          = ACTOR_521100_ATTACK_RUN_LONG_SLASH;
                    work->animationId       = ACTOR_521100_ANIM_LONG_SLASH;
                    attackKey               = damagePackAttackKey(_gActor521100Attacks, ACTOR_521100_ATTACK_LONG_SLASH);
                    work->weaponAttack.key  = attackKey;
                    work->forearmAttack.key = attackKey;
                    break;
                case ACTOR_521100_ATTACK_STANCE:
                    work->subState          = ACTOR_521100_ATTACK_RUN_STANCE;
                    work->attackStep        = 0;
                    work->animationId       = ACTOR_521100_ANIM_STANCE_ENTER;
                    attackKey               = damagePackAttackKey(_gActor521100Attacks, ACTOR_521100_ATTACK_STANCE);
                    work->weaponAttack.key  = attackKey;
                    work->forearmAttack.key = attackKey;
                    break;
            }
            break;
        case ACTOR_521100_ATTACK_RUN_SLASH:
            _actor521100SlashAttack(task);
            break;
        case ACTOR_521100_ATTACK_RUN_LONG_SLASH:
            _actor521100LongSlashAttack(task);
            break;
        case ACTOR_521100_ATTACK_RUN_STANCE:
            _actor521100StanceAttack(task);
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Runs the short slash's effects, contact window, forward step and recovery.
///
/// Requires the selected slash animation. Cue frames include its blend duration;
/// both attack spheres open at offset 28 and close at 40 or on contact. Moves
/// 100 units per tick at offsets 28..30; offset 122 resumes approach with a wait.
/// The scratch impact offset is borrowed only during effect spawning.
static void _actor521100SlashAttack(Task* task)
{
    enum {
        ACTOR_521100_SLASH_TRAIL_FRAME         = 0x1A,
        ACTOR_521100_SLASH_IMPACT_FRAME        = 0x1E,
        ACTOR_521100_SLASH_CONTACT_START_FRAME = 0x1C,
        ACTOR_521100_SLASH_CONTACT_END_FRAME   = 0x28,
        ACTOR_521100_SLASH_RECOVERY_END_FRAME  = 0x7A,
        ACTOR_521100_SOUND_SLASH               = 0x401C0008,
    };
    Actor521100Work* work;
    GfxCoord*        rootCoord;
    SVECTOR*         previousScratchTop;
    SVECTOR*         impactOffset;
    u16*             recoveryWaitTable;
    s16              blendFrames;
    s16              forwardSpeed;
    s16              cueFrame;
    s32              soundScript;

    work                          = task->work;
    rootCoord                     = task->extra.tmd->coords;
    previousScratchTop            = SCRATCH_STACK_CURSOR(SVECTOR);
    impactOffset                  = previousScratchTop - 1;
    SCRATCH_STACK_CURSOR(SVECTOR) = impactOffset;
    blendFrames                   = gActor521100AnimationFireTables.blendFrames[work->animationId];

    cueFrame = work->animationFrame;
    if (cueFrame == blendFrames + ACTOR_521100_SLASH_TRAIL_FRAME) {
        effectSpawn(EFFECT_NO9_GOLEM_SWING_TRAIL, task->extra.tmd->coords + 8, 0xC, NULL);
        padScriptSpawnVariableMotorRamp(0xA, 0x40, 0xFF);
    } else if (cueFrame == blendFrames + ACTOR_521100_SLASH_IMPACT_FRAME) {
        impactOffset->vx = -0x320;
        impactOffset->vy = 0x64;
        impactOffset->vz = 0;
        effectSpawn(EFFECT_CRITICAL_HIT, work->weaponTask->extra.tmd->coords, 0, impactOffset);
    }

    cueFrame = work->animationFrame;
    if (cueFrame == blendFrames + ACTOR_521100_SLASH_CONTACT_START_FRAME) {
        work->attackLive           = 1;
        work->weaponAttack.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->forearmAttack.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        soundScript                = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_521100_SOUND_SLASH;
        sndEvtRequestScriptStart(soundScript, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord));
    } else if (cueFrame == blendFrames + ACTOR_521100_SLASH_CONTACT_END_FRAME) {
        work->attackLanded         = 0;
        work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    forwardSpeed = 0;
    cueFrame     = work->animationFrame;
    if (cueFrame >= blendFrames + ACTOR_521100_SLASH_CONTACT_START_FRAME && cueFrame <= blendFrames + ACTOR_521100_SLASH_IMPACT_FRAME) {
        forwardSpeed = 0x64;
    }
    work->forwardSpeed = forwardSpeed;
    if (work->animationFrame >= blendFrames + ACTOR_521100_SLASH_RECOVERY_END_FRAME) {
        work->animationId  = ACTOR_521100_ANIM_IDLE;
        work->state        = ACTOR_521100_STATE_APPROACH;
        work->subState     = ACTOR_521100_APPROACH_WAIT;
        recoveryWaitTable  = _gActor521100ApproachWaitFrames;
        gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->stateCounter = recoveryWaitTable[(gRandomLcgState >> 16) & ACTOR_521100_RECOVERY_WAIT_INDEX_MASK];
        work->attackLive   = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Runs the turning long slash, its contact window and longer forward step.
///
/// Requires the selected long-slash animation. Cue frames include its blend
/// duration. Turning stops at offset 40, both attack spheres close at offset 45,
/// and movement is 136 units per tick at offsets 33..42. Offset 144 resumes
/// approach with a random wait. The scratch impact offset is not retained.
static void _actor521100LongSlashAttack(Task* task)
{
    enum {
        ACTOR_521100_LONG_SLASH_TURN_END_FRAME      = 0x28,
        ACTOR_521100_LONG_SLASH_CONTACT_START_FRAME = 0x23,
        ACTOR_521100_LONG_SLASH_IMPACT_FRAME        = 0x27,
        ACTOR_521100_LONG_SLASH_CONTACT_END_FRAME   = 0x2D,
        ACTOR_521100_LONG_SLASH_STEP_START_FRAME    = 0x20,
        ACTOR_521100_LONG_SLASH_STEP_END_FRAME      = 0x2A,
        ACTOR_521100_LONG_SLASH_RECOVERY_END_FRAME  = 0x90,
        ACTOR_521100_SOUND_SLASH                    = 0x401C0008,
    };
    Actor521100Work* work;
    GfxCoord*        rootCoord;
    SVECTOR*         previousScratchTop;
    SVECTOR*         impactOffset;
    u16*             recoveryWaitTable;
    u32              recoveryRandomState;
    s16              blendFrames;
    s16              turnLimit;
    s16              forwardSpeed;
    s16              cueFrame;
    s32              motionFrame;
    s32              soundScript;
    s32              audioPan;

    work                          = task->work;
    previousScratchTop            = SCRATCH_STACK_CURSOR(SVECTOR);
    impactOffset                  = previousScratchTop - 1;
    SCRATCH_STACK_CURSOR(SVECTOR) = impactOffset;
    blendFrames                   = gActor521100AnimationFireTables.blendFrames[work->animationId];
    rootCoord                     = task->extra.tmd->coords;

    turnLimit = 0;
    if (work->animationFrame < blendFrames + ACTOR_521100_LONG_SLASH_TURN_END_FRAME) {
        turnLimit = 0x50;
    }
    work->turnSpeed = turnLimit;

    cueFrame = work->animationFrame;
    if (cueFrame == blendFrames + ACTOR_521100_LONG_SLASH_CONTACT_START_FRAME) {
        effectSpawn(EFFECT_NO9_GOLEM_SWING_TRAIL, task->extra.tmd->coords + 8, 0xC, NULL);
        padScriptSpawnVariableMotorRamp(0xA, 0x40, 0xFF);
    } else if (cueFrame == blendFrames + ACTOR_521100_LONG_SLASH_IMPACT_FRAME) {
        impactOffset->vx = -0x320;
        impactOffset->vy = 0x64;
        impactOffset->vz = 0;
        effectSpawn(EFFECT_CRITICAL_HIT, work->weaponTask->extra.tmd->coords, 0, impactOffset);
    }

    // Effects and sound precede later snapshots of the live animation frame.
    cueFrame = work->animationFrame;
    if (cueFrame == blendFrames + ACTOR_521100_LONG_SLASH_CONTACT_START_FRAME) {
        work->attackLive           = 1;
        work->weaponAttack.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->forearmAttack.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        soundScript                = (((u32)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_521100_SOUND_SLASH;
        audioPan                   = (s8)worldCoordGetOriginAudioPan(rootCoord);
        sndEvtRequestScriptStart(soundScript, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
    } else if (cueFrame == blendFrames + ACTOR_521100_LONG_SLASH_CONTACT_END_FRAME) {
        work->attackLanded         = 0;
        work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    forwardSpeed = 0;
    motionFrame  = work->animationFrame;
    if (blendFrames + ACTOR_521100_LONG_SLASH_STEP_START_FRAME < motionFrame) {
        if (blendFrames + ACTOR_521100_LONG_SLASH_STEP_END_FRAME >= motionFrame) {
            forwardSpeed = 0x88;
        }
    }
    work->forwardSpeed = forwardSpeed;
    if (work->animationFrame >= blendFrames + ACTOR_521100_LONG_SLASH_RECOVERY_END_FRAME) {
        work->animationId   = ACTOR_521100_ANIM_IDLE;
        work->state         = ACTOR_521100_STATE_APPROACH;
        work->subState      = ACTOR_521100_APPROACH_WAIT;
        recoveryWaitTable   = _gActor521100ApproachWaitFrames;
        recoveryRandomState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState     = recoveryRandomState;
        work->stateCounter  = recoveryWaitTable[(recoveryRandomState >> 16) & ACTOR_521100_RECOVERY_WAIT_INDEX_MASK];
        work->attackLive    = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}
/// Takes and holds a stance, then swings or abandons it if the player is distant.
///
/// Requires attackStep in 0..3. The hold draws a sixteen-entry frame wait; the
/// swing enables only the gunblade sphere and steps forward during frames 34..38.
/// Recovery chooses approach or a route leg from the root's X and room view.
/// Reserves 24 unused scratch bytes for the duration of this call.
static void _actor521100StanceAttack(Task* task)
{
    enum {
        ACTOR_521100_STANCE_ROUTE_FIRST        = 0,
        ACTOR_521100_STANCE_ROUTE_MIDDLE       = 1,
        ACTOR_521100_STANCE_ROUTE_RETURN       = 2,
        ACTOR_521100_STANCE_SCRATCH_BYTES      = 24,
        ACTOR_521100_STANCE_COMMIT_FRAME       = 56,
        ACTOR_521100_STANCE_TURN_END_FRAME     = 32,
        ACTOR_521100_STANCE_CONTACT_END_FRAME  = 39,
        ACTOR_521100_STANCE_RECOVERY_END_FRAME = 94,
        ACTOR_521100_STANCE_MAX_DISTANCE       = 3500,
        ACTOR_521100_SOUND_STANCE_HOLD         = 0x401C0007,
        ACTOR_521100_SOUND_STANCE_SWING        = 0x401C0009,
    };
    Actor521100Work* work;
    GfxCoord*        rootCoord;
    u32              randomState;
    u16              remainingFrames;
    s16              turnLimit;
    s32              soundScript;
    s32              attackKey;

    SCRATCH_STACK_RESERVE_BYTES(ACTOR_521100_STANCE_SCRATCH_BYTES);
    work      = task->work;
    rootCoord = task->extra.tmd->coords;

    switch (work->attackStep) {
        case ACTOR_521100_STANCE_ENTER:
            if (work->animationFrame >= gActor521100AnimationFireTables.blendFrames[work->animationId] + ACTOR_521100_STANCE_COMMIT_FRAME) {
                u16* waitFrameTable = _gActor521100StanceHoldFrames;
                work->animationId   = ACTOR_521100_ANIM_STANCE_HOLD;
                work->attackStep    = ACTOR_521100_STANCE_HOLD;
                gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->stateCounter  = waitFrameTable[(gRandomLcgState >> 16) & ACTOR_521100_RECOVERY_WAIT_INDEX_MASK];
                soundScript         = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_521100_SOUND_STANCE_HOLD;
                sndEvtRequestScriptStart(soundScript, (s8)worldCoordGetOriginAudioPan(rootCoord),
                                         (s8)worldCoordGetOriginAudioDepth(rootCoord));
            }
            break;
        case ACTOR_521100_STANCE_HOLD:
            remainingFrames    = work->stateCounter - 1;
            work->stateCounter = remainingFrames;
            if ((s16)remainingFrames > 0) {
                break;
            }
            if (work->playerDistance >= ACTOR_521100_STANCE_MAX_DISTANCE) {
                u16* waitFrameTable = _gActor521100ApproachWaitFrames;
                work->state         = ACTOR_521100_STATE_APPROACH;
                work->subState      = ACTOR_521100_APPROACH_WAIT;
                work->attackStep    = ACTOR_521100_STANCE_ENTER;
                work->animationId   = ACTOR_521100_ANIM_IDLE;
                gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->stateCounter  = waitFrameTable[(gRandomLcgState >> 16) & ACTOR_521100_RECOVERY_WAIT_INDEX_MASK];
            } else {
                work->attackStep        = ACTOR_521100_STANCE_SWING;
                work->animationId       = ACTOR_521100_ANIM_STANCE_SWING;
                attackKey               = damagePackAttackKey(_gActor521100Attacks, ACTOR_521100_ATTACK_STANCE);
                work->weaponAttack.key  = attackKey;
                work->forearmAttack.key = attackKey;
            }
            break;
        case ACTOR_521100_STANCE_SWING:
            turnLimit = 0;
            if (work->animationFrame < ACTOR_521100_STANCE_TURN_END_FRAME) {
                turnLimit = 0x3C;
            }
            work->turnSpeed = turnLimit;
            if (work->animationFrame == ACTOR_521100_STANCE_TURN_END_FRAME) {
                work->attackLive          = 1;
                work->weaponAttack.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                soundScript               = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_521100_SOUND_STANCE_SWING;
                sndEvtRequestScriptStart(soundScript, (s8)worldCoordGetOriginAudioPan(rootCoord),
                                         (s8)worldCoordGetOriginAudioDepth(rootCoord));
                effectSpawn(EFFECT_NO9_GOLEM_SWING_TRAIL, task->extra.tmd->coords + 8, 8, NULL);
                padScriptSpawnVariableMotorRamp(0xA, 0x40, 0xFF);
            }
            if ((u32)((u16)work->animationFrame - 0x22) < 5) {
                work->forwardSpeed = 0x64;
            } else {
                work->forwardSpeed = 0;
            }
            if (work->animationFrame >= ACTOR_521100_STANCE_CONTACT_END_FRAME) {
                work->attackStep           = ACTOR_521100_STANCE_RECOVER;
                work->animationId          = ACTOR_521100_ANIM_STANCE_RECOVER;
                work->attackLanded         = 0;
                work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
        case ACTOR_521100_STANCE_RECOVER:
            if (work->animationFrame < ACTOR_521100_STANCE_RECOVERY_END_FRAME) {
                break;
            }
            if (gGameSession->location.view == 2) {
                work->state = ACTOR_521100_STATE_WALK_ROUTE;
                if (rootCoord->coord.t[0] < -0xFA0) {
                    work->subState = ACTOR_521100_STANCE_ROUTE_MIDDLE;
                } else {
                    work->subState = ACTOR_521100_STANCE_ROUTE_FIRST;
                }
            } else if (rootCoord->coord.t[0] < -0xFA0) {
                work->state    = ACTOR_521100_STATE_WALK_ROUTE;
                work->subState = ACTOR_521100_STANCE_ROUTE_RETURN;
            } else {
                u16* waitFrameTable = _gActor521100ApproachWaitFrames;
                work->state         = ACTOR_521100_STATE_APPROACH;
                work->subState      = ACTOR_521100_APPROACH_WAIT;
                work->animationId   = ACTOR_521100_ANIM_IDLE;
                gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->stateCounter  = waitFrameTable[(gRandomLcgState >> 16) & ACTOR_521100_RECOVERY_WAIT_INDEX_MASK];
            }
            work->attackLive = 0;
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(ACTOR_521100_STANCE_SCRATCH_BYTES);
}

/// Installs and plays one package player clip during the grab's synchronous dispatch.
///
/// Captures the live `scratch` and `playerTask` locals of the grab handler.
/// `playerAnimationId` is evaluated once and must select a loaded entry in
/// the fourteen-entry package player bank. The request remains live only for
/// dispatch; the set table must remain loaded throughout playback. Resets
/// blending with zero duration and enables world collision. Expands to a
/// braced statement block; existing calls deliberately omit a semicolon.
#define ACTOR_521100_PLAY_GRAB_PLAYER_ANIMATION(playerAnimationId)                                              \
    {                                                                                                           \
        scratch->playerAnim.source.sets          = gActor521100GrabPlayerAnimationSets;                         \
        scratch->playerAnim.animationId          = (playerAnimationId);                                         \
        scratch->playerAnim.blend                = ANIMATION_BLEND_RESET;                                       \
        scratch->playerAnim.blendFrames          = 0;                                                           \
        scratch->playerAnim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;                            \
        TASK_MESSAGE_DISPATCH_POINTER(playerTask, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &scratch->playerAnim, 0); \
    }

/// Seizes, holds and throws the player, or resolves either participant's death.
///
/// Requires a live player model task and a successfully accepted button hold.
/// The hold damages every 32 ticks and throws after 151 ticks or escape; a
/// front escape with low player and golem HP instead starts the counterattack.
/// Player animations and placements are synchronous borrowed scratch payloads.
/// Throw travel uses root-local X steps until frame 69; yaw is 4096 per turn.
static void _actor521100GrabState(Task* task)
{
    enum {
        ACTOR_521100_SEIZE_SOUND_FRAME            = 10,
        ACTOR_521100_SEIZE_SNAP_FRAME             = 12,
        ACTOR_521100_SEIZE_HOLD_FRAME             = 43,
        ACTOR_521100_THROW_END_FRAME              = 164,
        ACTOR_521100_COUNTER_FIRE_FRAME           = 32,
        ACTOR_521100_COUNTER_DEATH_FRAME          = 100,
        ACTOR_521100_KILL_FRAME                   = 26,
        ACTOR_521100_DEATH_SOUND_LOAD_BEGIN       = 0,
        ACTOR_521100_DEATH_SOUND_LOAD_WAIT        = 1,
        ACTOR_521100_DEATH_SOUND_LOAD_DONE        = 2,
        ACTOR_521100_PLAYER_ANIM_SEIZE_FRONT      = 2,
        ACTOR_521100_PLAYER_ANIM_SEIZE_BACK       = 6,
        ACTOR_521100_PLAYER_ANIM_HOLD_FRONT       = 3,
        ACTOR_521100_PLAYER_ANIM_HOLD_BACK        = 7,
        ACTOR_521100_PLAYER_ANIM_KILL_FRONT       = 0xC,
        ACTOR_521100_PLAYER_ANIM_KILL_BACK        = 0xD,
        ACTOR_521100_PLAYER_ANIM_COUNTER          = 5,
        ACTOR_521100_PLAYER_ANIM_THROW_FRONT      = 9,
        ACTOR_521100_PLAYER_ANIM_THROW_BACK       = 0xA,
        ACTOR_521100_PLAYER_ANIM_RELEASED         = 0xB,
        ACTOR_521100_PLAYER_ANIM_COUNTER_FINISH   = 4,
        ACTOR_521100_HOLD_MAX_FRAMES              = 151,
        ACTOR_521100_HOLD_DAMAGE_PERIOD           = 32,
        ACTOR_521100_GRAB_DAMAGE_ROW              = 3,
        ACTOR_521100_PLAYER_ENTER_SCRIPTED_ATTACK = 1024,
        ACTOR_521100_SOUND_THROW                  = 0x401C000F,
        ACTOR_521100_SOUND_COUNTER                = 0x401C000E,
        ACTOR_521100_SOUND_COUNTER_DEATH          = 0x401C000C,
    };
    Actor521100Work*         work;
    GfxCoord*                rootCoord;
    GfxCoord*                playerCoord;
    Task*                    playerTask;
    _Actor521100GrabScratch* scratch;
    s32                      phaseValue; // Facing dot or yaw error during seizure; decision flags during hold
    s32                      spanIndex;
    s32                      soundScript;
    s32                      differenceMagnitude;
    s32                      playerYaw;
    s16                      soundLoadStage;
    s16                      retreatSpeed;
    u16                      counterBits;
    u32                      recoveryRandomState;
    u16*                     recoveryWaitTable;

    work       = task->work;
    rootCoord  = task->extra.tmd->coords;
    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    SCRATCH_STACK_RESERVE_BLOCK(_Actor521100GrabScratch);
    scratch = SCRATCH_STACK_CURSOR(_Actor521100GrabScratch);

    switch (work->subState) {
        // Pull the player onto the hold spot and align their facing.
        case ACTOR_521100_GRAB_SEIZE:
            if (work->animationFrame == ACTOR_521100_SEIZE_SOUND_FRAME) {
                soundScript = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
                sndEvtRequestScriptStart(soundScript, (s8)worldCoordGetOriginAudioPan(rootCoord),
                                         (s8)worldCoordGetOriginAudioDepth(rootCoord));
            } else if (work->animationFrame == ACTOR_521100_SEIZE_SNAP_FRAME) {
                phaseValue          = (gPlayerStatus.coordMtx->m[0][2] * rootCoord->coord.m[0][2] + gPlayerStatus.coordMtx->m[1][2] * rootCoord->coord.m[1][2] + gPlayerStatus.coordMtx->m[2][2] * rootCoord->coord.m[2][2]);
                work->grabFromFront = (u32)phaseValue >> 31;
                ACTOR_521100_PLAY_GRAB_PLAYER_ANIMATION(work->grabFromFront ? ACTOR_521100_PLAYER_ANIM_SEIZE_FRONT : ACTOR_521100_PLAYER_ANIM_SEIZE_BACK)
            } else if (work->animationFrame >= ACTOR_521100_SEIZE_HOLD_FRAME) {
                work->subState     = ACTOR_521100_GRAB_HOLD;
                work->animationId  = ACTOR_521100_ANIM_GRAB_HOLD;
                work->stateCounter = 0;
                work->stateElapsed = 0;
                ACTOR_521100_PLAY_GRAB_PLAYER_ANIMATION(work->grabFromFront ? ACTOR_521100_PLAYER_ANIM_HOLD_FRONT : ACTOR_521100_PLAYER_ANIM_HOLD_BACK)
            }
            if ((work->animationFrame >= 4) && (work->animationFrame <= ACTOR_521100_SEIZE_SNAP_FRAME)) {
                scratch->localOffset.vz = 0x4E2;
                scratch->localOffset.vx = 0;
                scratch->localOffset.vy = 0;
                gte_SetRotMatrix(&rootCoord->coord);
                gte_ldv0(&scratch->localOffset);
                gte_rtv0();
                gte_stlvnl(&scratch->worldVector);
                scratch->worldVector.vx = rootCoord->coord.t[0] + scratch->worldVector.vx;
                scratch->worldVector.vy = rootCoord->coord.t[1] + scratch->worldVector.vy;
                scratch->worldVector.vz = rootCoord->coord.t[2] + scratch->worldVector.vz;
                playerCoord             = playerTask->extra.tmd->coords;
                scratch->toHoldSpot.vx  = scratch->worldVector.vx - playerCoord->coord.t[0];
                scratch->toHoldSpot.vy  = 0;
                scratch->toHoldSpot.vz  = scratch->worldVector.vz - playerCoord->coord.t[2];
                if ((SquareRoot0((scratch->toHoldSpot.vx * scratch->toHoldSpot.vx) + (scratch->toHoldSpot.vz * scratch->toHoldSpot.vz)) < 0x32) || (work->animationFrame == ACTOR_521100_SEIZE_SNAP_FRAME)) {
                    scratch->playerPlacement.pos.vx = scratch->worldVector.vx;
                    scratch->playerPlacement.pos.vy = scratch->worldVector.vy;
                    scratch->playerPlacement.pos.vz = scratch->worldVector.vz;
                } else {
                    VectorNormal(&scratch->toHoldSpot, &scratch->worldVector);
                    scratch->playerPlacement.pos.vx = playerCoord->coord.t[0] + ((scratch->worldVector.vx * 0x32) >> 12);
                    scratch->playerPlacement.pos.vy = playerCoord->coord.t[1] + ((scratch->worldVector.vy * 0x32) >> 12);
                    scratch->playerPlacement.pos.vz = playerCoord->coord.t[2] + ((scratch->worldVector.vz * 0x32) >> 12);
                }
                playerYaw                       = ratan2(playerCoord->coord.m[0][2], playerCoord->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
                phaseValue                      = work->yaw - playerYaw;
                scratch->playerPlacement.rot.vx = 0;
                scratch->playerPlacement.rot.vz = 0;
                if (work->animationFrame == ACTOR_521100_SEIZE_SNAP_FRAME) {
                    scratch->playerPlacement.rot.vy = work->yaw;
                } else {
                    differenceMagnitude = phaseValue >= 0 ? phaseValue : -phaseValue;
                    if ((u32)(differenceMagnitude - 0x400) >= 0x801U) {
                        if (differenceMagnitude < 0x65) {
                            scratch->playerPlacement.rot.vy = work->yaw;
                        } else if (phaseValue > 0) {
                            scratch->playerPlacement.rot.vy = playerYaw + 0x64;
                        } else {
                            scratch->playerPlacement.rot.vy = playerYaw - 0x64;
                        }
                    } else {
                        if (differenceMagnitude < 0x65) {
                            scratch->playerPlacement.rot.vy = (work->yaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
                        } else if (phaseValue > 0) {
                            scratch->playerPlacement.rot.vy = playerYaw - 0x64;
                        } else {
                            scratch->playerPlacement.rot.vy = playerYaw + 0x64;
                        }
                    }
                }
                TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_PLACE, &scratch->playerPlacement, 0);
            }
            break;
        // A hold can end in a throw, a player kill, or the player's counterattack.
        case ACTOR_521100_GRAB_HOLD:
            phaseValue = 0;
            if (work->stateCounter == 2) {
                padScriptSpawnVariableMotorRamp(5, 0xC0, 0x80);
            }
            counterBits        = work->stateCounter - 1;
            work->stateCounter = counterBits;
            if ((s16)counterBits <= 0) {
                if (gPlayerStatus.hp <= _gActor521100GrabKillHpByDifficulty[gSceneCombatState.difficulty]) {
                    work->animationId = ACTOR_521100_ANIM_KILL_PLAYER;
                    work->subState    = ACTOR_521100_GRAB_KILL;
                    ACTOR_521100_PLAY_GRAB_PLAYER_ANIMATION(work->grabFromFront ? ACTOR_521100_PLAYER_ANIM_KILL_FRONT : ACTOR_521100_PLAYER_ANIM_KILL_BACK)
                    phaseValue = 1;
                } else {
                    work->stateCounter = ACTOR_521100_HOLD_DAMAGE_PERIOD;
                    taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(_gActor521100Attacks, ACTOR_521100_GRAB_DAMAGE_ROW), 0);
                }
            }
            if (phaseValue != 1) {
                phaseValue = 0;
                if ((work->playerEscaped == 1) && (gPlayerStatus.hp < 0x3D) && (((_gActor521100EnemyParams.hpMax / 3) & 0xFFFF) >= ((Enemy*)task->spawnArg2.pointer)->hp)) {
                    phaseValue = work->grabFromFront == 1;
                }
                if (phaseValue != 0) {
                    work->subState       = ACTOR_521100_GRAB_COUNTER;
                    work->playerEscaped  = 0;
                    work->animationFrame = 0;
                    ACTOR_521100_PLAY_GRAB_PLAYER_ANIMATION(ACTOR_521100_PLAYER_ANIM_COUNTER)
                } else {
                    if (work->playerEscaped == 0) {
                        counterBits        = work->stateElapsed + 1;
                        work->stateElapsed = counterBits;
                        if ((s16)counterBits < ACTOR_521100_HOLD_MAX_FRAMES) {
                            break;
                        }
                    }
                    work->subState      = ACTOR_521100_GRAB_THROW;
                    work->animationId   = ACTOR_521100_ANIM_THROW;
                    work->playerEscaped = 0;
                    ACTOR_521100_PLAY_GRAB_PLAYER_ANIMATION(work->grabFromFront ? ACTOR_521100_PLAYER_ANIM_THROW_FRONT : ACTOR_521100_PLAYER_ANIM_THROW_BACK)
                }
            }
            break;
        case ACTOR_521100_GRAB_THROW:
            if (work->animationFrame == 0x22) {
                padScriptSpawnVariableMotorRamp(0xF, 0xFF, 0x80);
            }
            if (work->animationFrame == 0x25) {
                soundScript = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_521100_SOUND_THROW;
                sndEvtRequestScriptStart(soundScript, (s8)worldCoordGetOriginAudioPan(rootCoord),
                                         (s8)worldCoordGetOriginAudioDepth(rootCoord));
            }
            if (work->animationFrame < ACTOR_521100_THROW_RELEASE_FRAME) {
                // Carry the playerTask by the step of the span this frame of the throw falls in.
                for (spanIndex = 0; spanIndex < ARRAY_SIZE(gActor521100ThrowSpans[0]); spanIndex++) {
                    if (work->animationFrame < gActor521100ThrowSpans[work->grabFromFront][spanIndex].endFrame) {
                        scratch->localOffset.vx = gActor521100ThrowSpans[work->grabFromFront][spanIndex].sidewaysStep;
                        break;
                    }
                }
                scratch->localOffset.vy = 0;
                scratch->localOffset.vz = 0;
                gte_SetRotMatrix(&rootCoord->coord);
                gte_ldv0(&scratch->localOffset);
                gte_rtv0();
                gte_stlvnl(&scratch->worldVector);
                playerCoord                     = playerTask->extra.tmd->coords;
                scratch->playerPlacement.pos.vx = playerCoord->coord.t[0] + scratch->worldVector.vx;
                scratch->playerPlacement.pos.vy = playerCoord->coord.t[1] + scratch->worldVector.vy;
                scratch->playerPlacement.pos.vz = playerCoord->coord.t[2] + scratch->worldVector.vz;
                scratch->playerPlacement.rot.vx = 0;
                scratch->playerPlacement.rot.vy = work->yaw;
                scratch->playerPlacement.rot.vz = 0;
                TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_PLACE, &scratch->playerPlacement, 0);
            }
            if (work->animationFrame == 0x23) {
                effectSpawn(EFFECT_DUST_PUFF, playerTask->extra.tmd->coords + 3, 0x80003400, NULL);
                effectSpawn(EFFECT_DUST_PUFF, playerTask->extra.tmd->coords + 3, 0x80003400, NULL);
                effectSpawn(EFFECT_DUST_PUFF, playerTask->extra.tmd->coords + 3, 0x80003400, NULL);
            }
            if (work->animationFrame == ACTOR_521100_THROW_RELEASE_FRAME) {
                ACTOR_521100_PLAY_GRAB_PLAYER_ANIMATION(ACTOR_521100_PLAYER_ANIM_RELEASED)
            }
            retreatSpeed = 0;
            if (work->animationFrame < 0x5F) {
                retreatSpeed = -0x10;
            }
            work->forwardSpeed = retreatSpeed;
            if (work->animationFrame == 0x6F) {
                rootCoord                       = playerTask->extra.tmd->coords;
                scratch->playerPlacement.pos.vx = rootCoord->coord.t[0];
                scratch->playerPlacement.pos.vy = rootCoord->coord.t[1];
                scratch->playerPlacement.pos.vz = rootCoord->coord.t[2];
                scratch->playerPlacement.rot.vx = 0;
                scratch->playerPlacement.rot.vy = (work->yaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
                scratch->playerPlacement.rot.vz = 0;
                TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_PLACE, &scratch->playerPlacement, 0);
            }
            if ((work->animationFrame >= 0x6F) && (taskMessageDispatch(playerTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0)) {
                taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            }
            if (work->animationFrame >= ACTOR_521100_THROW_END_FRAME) {
                recoveryWaitTable   = _gActor521100ApproachWaitFrames;
                work->animationId   = ACTOR_521100_ANIM_IDLE;
                work->state         = ACTOR_521100_STATE_APPROACH;
                work->subState      = ACTOR_521100_APPROACH_WAIT;
                recoveryRandomState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState     = recoveryRandomState;
                work->stateCounter  = recoveryWaitTable[(recoveryRandomState >> 16) & ACTOR_521100_RECOVERY_WAIT_INDEX_MASK];
            }
            break;
        case ACTOR_521100_GRAB_COUNTER:
            if (work->animationFrame == ACTOR_521100_COUNTER_FIRE_FRAME) {
                effectSpawn(EFFECT_DILAPIDATED_HOUSE_FIRE_BLAST, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords + 0xC, 0, NULL);
                soundScript = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_521100_SOUND_COUNTER;
                sndEvtRequestScriptStart(soundScript, (s8)worldCoordGetOriginAudioPan(rootCoord),
                                         (s8)worldCoordGetOriginAudioDepth(rootCoord));
                work->subState    = ACTOR_521100_GRAB_COUNTER_DEATH;
                work->animationId = ACTOR_521100_ANIM_COUNTER_DEATH;
                ACTOR_521100_PLAY_GRAB_PLAYER_ANIMATION(ACTOR_521100_PLAYER_ANIM_COUNTER_FINISH)
            }
            break;
        case ACTOR_521100_GRAB_COUNTER_DEATH:
            if (work->animationFrame == 0xF) {
                soundScript = (((u16)((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_521100_SOUND_COUNTER_DEATH;
                sndEvtRequestScriptStart(soundScript, (s8)worldCoordGetOriginAudioPan(rootCoord),
                                         (s8)worldCoordGetOriginAudioDepth(rootCoord));
            }
            if (work->animationFrame == ACTOR_521100_COUNTER_DEATH_FRAME) {
                ((Enemy*)task->spawnArg2.pointer)->hp = 0;
            }
            break;
        case ACTOR_521100_GRAB_KILL:
            if (work->animationFrame == ACTOR_521100_KILL_FRAME) {
                ((GameActor*)playerTask->work)->state = 0xA;
                work->subState                        = ACTOR_521100_GRAB_KILL_SOUND;
                work->stateCounter                    = 0;
                gGameSession->deathRestartDelay       = 0x5A;
                gGameSession->deathSoundCountdown     = GAME_SESSION_DEATH_SOUND_HOLD;
                scratch->localOffset.vx               = 0;
                scratch->localOffset.vy               = -0x96;
                scratch->localOffset.vz               = 0xC8;
                effectSpawnHit(EFFECT_HIT_KIND_WEAPON_PUFF, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords + 4, &scratch->localOffset,
                               &gActor521100GrabKillEffectArg);
                padScriptSpawnVariableMotorRamp(0xA, 0xFF, 8);
                taskMessageDispatch(playerTask, ACTOR_521100_PLAYER_ENTER_SCRIPTED_ATTACK, 0, 0);
                gPlayerStatus.hp = 0;
            }
            break;
        case ACTOR_521100_GRAB_KILL_SOUND:
            soundLoadStage = work->stateCounter;
            if (soundLoadStage != ACTOR_521100_DEATH_SOUND_LOAD_WAIT) {
                if (soundLoadStage < ACTOR_521100_DEATH_SOUND_LOAD_DONE) {
                    if (soundLoadStage == ACTOR_521100_DEATH_SOUND_LOAD_BEGIN) {
                        cdCmdEnqueueDisplayResource(9, 0x1E, CD_COMMAND_DISPLAY_LOAD_DEFAULT);
                        work->stateCounter = ACTOR_521100_DEATH_SOUND_LOAD_WAIT;
                    }
                }
            } else if ((cdCmdIsIdle() & 0xFFFF) == soundLoadStage) {
                rootCoord = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
                sndEvtRequestScriptStart(SOUND_PLAYER_DEATH, (s8)worldCoordGetOriginAudioPan(rootCoord),
                                         (s8)worldCoordGetOriginAudioDepth(rootCoord));
                work->stateCounter = ACTOR_521100_DEATH_SOUND_LOAD_DONE;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor521100GrabScratch);
}

#undef ACTOR_521100_PLAY_GRAB_PLAYER_ANIMATION

/// Runs the guard reaction, then resumes approach or the interrupted route.
///
/// Requires the body's live work block. Entry stops movement and disables both
/// attack spheres; the two guard clips run for 5 and 38 animation ticks.
/// Completion restores idle and draws the next approach wait in frames.
static void _actor521100GuardState(Task* task)
{
    enum {
        ACTOR_521100_GUARD_WAIT_END     = 2,
        ACTOR_521100_GUARD_START_FRAMES = 5,
        ACTOR_521100_GUARD_END_FRAMES   = 38,
    };
    Actor521100Work* work;

    work = task->work;
    switch (work->subState) {
        case ACTOR_521100_REACTION_BEGIN:
            work->animationId          = ACTOR_521100_ANIM_GUARD_START;
            work->subState             = ACTOR_521100_REACTION_WAIT;
            work->forwardSpeed         = 0;
            work->turnSpeed            = 0;
            work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            return;
        case ACTOR_521100_REACTION_WAIT:
            if (work->animationFrame >= ACTOR_521100_GUARD_START_FRAMES) {
                work->animationId = ACTOR_521100_ANIM_GUARD_END;
                work->subState    = ACTOR_521100_GUARD_WAIT_END;
            }
            return;
        case ACTOR_521100_GUARD_WAIT_END:
            if (work->animationFrame >= ACTOR_521100_GUARD_END_FRAMES) {
                ACTOR_521100_RESUME_AFTER_HIT(work, _gActor521100HitRecoveryWaitFrames);
            }
            return;
    }
}
/// Walks the Dryfield fight's three route legs and turns to attack a nearby player.
///
/// Requires the body's live root coordinate and work block. Targets use the
/// root's parent frame and integer game units; yaw uses 4096 units per turn.
/// View 2 retains a route leg for resumption after an attack or hit reaction.
/// Reserves and releases one ActorFaceScratch block for the target delta.
static void _actor521100WalkRouteState(Task* task)
{
    enum {
        ACTOR_521100_ROUTE_FIRST           = 0,
        ACTOR_521100_ROUTE_SECOND          = 1,
        ACTOR_521100_ROUTE_RETURN          = 2,
        ACTOR_521100_ROUTE_VIEW            = 2,
        ACTOR_521100_ROUTE_WALK_SPEED      = 20,
        ACTOR_521100_ROUTE_TURN_SPEED      = 120,
        ACTOR_521100_ROUTE_ATTACK_DISTANCE = 2000,
        ACTOR_521100_ROUTE_WAYPOINT_RADIUS = 60,
        ACTOR_521100_ROUTE_PLAYER_Z_LIMIT  = -1500,
        ACTOR_521100_ROUTE_RETURN_X        = -4000,
    };
    Actor521100Work*  work;
    GfxCoord*         rootCoord;
    ActorFaceScratch* scratch;
    s16               routeLeg;

    scratch   = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    routeLeg  = work->subState;
    // Attack near the player; otherwise follow the current room waypoint.
    switch (routeLeg) {
        case ACTOR_521100_ROUTE_FIRST:
            work->animationId  = ACTOR_521100_ANIM_WALK;
            work->forwardSpeed = ACTOR_521100_ROUTE_WALK_SPEED;
            work->turnSpeed    = ACTOR_521100_ROUTE_TURN_SPEED;
            scratch->delta.vx  = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
            scratch->delta.vy  = 0;
            scratch->delta.vz  = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
            if ((SquareRoot0((scratch->delta.vx * scratch->delta.vx) + (scratch->delta.vz * scratch->delta.vz)) < ACTOR_521100_ROUTE_ATTACK_DISTANCE) && (gPlayerStatus.coordMtx->t[2] < ACTOR_521100_ROUTE_PLAYER_Z_LIMIT)) {
                work->targetYaw    = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
                work->forwardSpeed = 0;
                work->turnSpeed    = ACTOR_521100_ROUTE_TURN_SPEED;
                work->state        = ACTOR_521100_STATE_ATTACK;
                work->subState     = 0;
            } else {
                scratch->delta.vx = _gActor521100RouteWaypoints[ACTOR_521100_ROUTE_FIRST].vx - rootCoord->coord.t[0];
                scratch->delta.vy = 0;
                scratch->delta.vz = _gActor521100RouteWaypoints[ACTOR_521100_ROUTE_FIRST].vz - rootCoord->coord.t[2];
                work->targetYaw   = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
                if (SquareRoot0((scratch->delta.vx * scratch->delta.vx) + (scratch->delta.vz * scratch->delta.vz)) < ACTOR_521100_ROUTE_WAYPOINT_RADIUS) {
                    work->subState = ACTOR_521100_ROUTE_SECOND;
                } else {
                    if (gGameSession->location.view != ACTOR_521100_ROUTE_VIEW) {
                        work->state       = ACTOR_521100_STATE_APPROACH;
                        work->subState    = 0;
                        work->resumeRoute = 0;
                    } else {
                        work->resumeRoute = 1;
                    }
                    work->resumeRouteLeg = 0;
                }
            }
            break;
        case ACTOR_521100_ROUTE_SECOND:
            work->animationId  = ACTOR_521100_ANIM_WALK;
            work->forwardSpeed = ACTOR_521100_ROUTE_WALK_SPEED;
            work->turnSpeed    = ACTOR_521100_ROUTE_TURN_SPEED;
            scratch->delta.vx  = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
            scratch->delta.vy  = 0;
            scratch->delta.vz  = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
            if (SquareRoot0((scratch->delta.vx * scratch->delta.vx) + (scratch->delta.vz * scratch->delta.vz)) < ACTOR_521100_ROUTE_ATTACK_DISTANCE) {
                work->targetYaw    = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
                work->forwardSpeed = 0;
                work->turnSpeed    = ACTOR_521100_ROUTE_TURN_SPEED;
                work->state        = ACTOR_521100_STATE_ATTACK;
                work->subState     = 0;
            } else {
                scratch->delta.vx = _gActor521100RouteWaypoints[ACTOR_521100_ROUTE_SECOND].vx - rootCoord->coord.t[0];
                scratch->delta.vy = 0;
                scratch->delta.vz = _gActor521100RouteWaypoints[ACTOR_521100_ROUTE_SECOND].vz - rootCoord->coord.t[2];
                work->targetYaw   = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
                if (SquareRoot0((scratch->delta.vx * scratch->delta.vx) + (scratch->delta.vz * scratch->delta.vz)) < ACTOR_521100_ROUTE_WAYPOINT_RADIUS) {
                    scratch->delta.vx  = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
                    scratch->delta.vy  = 0;
                    scratch->delta.vz  = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
                    work->targetYaw    = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
                    work->forwardSpeed = 0;
                    work->turnSpeed    = ACTOR_521100_ROUTE_TURN_SPEED;
                    work->state        = ACTOR_521100_STATE_ATTACK;
                    work->subState     = 0;
                } else if (gGameSession->location.view != ACTOR_521100_ROUTE_VIEW) {
                    work->subState       = ACTOR_521100_ROUTE_RETURN;
                    work->resumeRoute    = 0;
                    work->resumeRouteLeg = 0;
                } else {
                    work->resumeRoute    = 1;
                    work->resumeRouteLeg = ACTOR_521100_ROUTE_SECOND;
                }
            }
            break;
        case ACTOR_521100_ROUTE_RETURN:
            work->animationId  = ACTOR_521100_ANIM_WALK;
            work->forwardSpeed = ACTOR_521100_ROUTE_WALK_SPEED;
            work->turnSpeed    = ACTOR_521100_ROUTE_TURN_SPEED;
            scratch->delta.vx  = _gActor521100RouteWaypoints[ACTOR_521100_ROUTE_RETURN].vx - rootCoord->coord.t[0];
            scratch->delta.vy  = 0;
            scratch->delta.vz  = _gActor521100RouteWaypoints[ACTOR_521100_ROUTE_RETURN].vz - rootCoord->coord.t[2];
            work->targetYaw    = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            if (rootCoord->coord.t[0] < ACTOR_521100_ROUTE_RETURN_X) {
                if (SquareRoot0((scratch->delta.vx * scratch->delta.vx) + (scratch->delta.vz * scratch->delta.vz)) < ACTOR_521100_ROUTE_WAYPOINT_RADIUS) {
                    work->state    = ACTOR_521100_STATE_APPROACH;
                    work->subState = 0;
                } else if (gGameSession->location.view == routeLeg) {
                    work->subState = 0;
                }
            } else {
                if (gGameSession->location.view != routeLeg) {
                    work->state = ACTOR_521100_STATE_APPROACH;
                }
                work->subState = 0;
            }
            work->resumeRoute    = 0;
            work->resumeRouteLeg = 0;
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}
/// Turns the body toward targetYaw by at most turnSpeed angle units this tick.
///
/// Requires a live root and work block with targetYaw in 0..4095 and a
/// nonnegative turnSpeed. Chooses the shorter arc and replaces the root's
/// rotation with pure yaw at unit scale, retaining its translation. Reserves
/// and releases one ActorFaceScratch block for the rotation vector.
static void _actor521100TurnTowardTargetYaw(Task* task)
{
    Actor521100Work*  work;
    GfxCoord*         rootCoord;
    ActorFaceScratch* scratch;
    s32               currentYaw;
    u16               targetYaw;
    s16               signedDifference;
    s32               differenceMagnitude;
    s32               turnStep;
    s32               currentStepYaw;
    s32               nextYaw;
    s32               wrappedTurnStep;

    scratch             = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    rootCoord           = task->extra.tmd->coords;
    work                = task->work;
    currentYaw          = ratan2(rootCoord->coord.m[0][2], rootCoord->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
    targetYaw           = work->targetYaw;
    signedDifference    = targetYaw - currentYaw;
    differenceMagnitude = signedDifference >= 0 ? signedDifference : -signedDifference;

    // Choose the shorter arc, including when the headings straddle zero.
    work->yaw = currentYaw;
    if (differenceMagnitude < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        turnStep = work->turnSpeed;
        if (turnStep >= differenceMagnitude) {
            work->yaw = targetYaw;
        } else {
            nextYaw = work->yaw;
            if (signedDifference <= 0) {
                nextYaw -= turnStep;
            } else {
                nextYaw += turnStep;
            }
            work->yaw = nextYaw;
        }
    } else {
        turnStep = work->turnSpeed;
        if (signedDifference > 0 ? turnStep >= ACTOR_TRANSFORM_ANGLE_TURN - signedDifference : turnStep >= ACTOR_TRANSFORM_ANGLE_TURN + signedDifference) {
            work->yaw = work->targetYaw;
        } else {
            wrappedTurnStep = work->turnSpeed;
            currentStepYaw  = work->yaw;
            if (signedDifference > 0) {
                work->yaw = currentStepYaw - wrappedTurnStep;
            } else {
                work->yaw = currentStepYaw + wrappedTurnStep;
            }
        }
    }
    scratch->rot.vx = 0;
    scratch->rot.vy = work->yaw;
    scratch->rot.vz = 0;
    RotMatrix(&scratch->rot, &rootCoord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}
/// Plays the body's two footstep cues when their slot-1 record bits fall.
///
/// Requires a live Enemy in spawnArg2 and an initialized body animation rig.
/// Cue 2 plays character-bank entry 1 and cue 1 entry 2, with the placement
/// index identifying the sound instance. Pan and depth are narrowed to signed
/// bytes. A missing current record leaves the previous cue bits unchanged.
static void _actor521100PlayFootsteps(Task* task)
{
    enum {
        ACTOR_521100_FOOTSTEP_CUE_2 = SOUND_CHARACTER(0x1C, 1),
        ACTOR_521100_FOOTSTEP_CUE_1 = SOUND_CHARACTER(0x1C, 2),
    };
    Enemy*                 enemy;
    s32                    soundScript;
    s32                    cue2Pan;
    s32                    cue1Pan;
    Actor521100Work*       work;
    GfxCoord*              rootCoord;
    const AnimationRecord* record;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    record    = animationGetCurrentRecord(&work->rig.anim, &work->rig.slots[1]);
    if (record != NULL) {
        if (!(record->flags & ANIMATION_RECORD_CUE_2) && (work->lastCueFlags & ANIMATION_RECORD_CUE_2)) {
            enemy       = task->spawnArg2.pointer;
            soundScript = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_521100_FOOTSTEP_CUE_2;
            cue2Pan     = (s8)worldCoordGetOriginAudioPan(rootCoord);
            sndEvtRequestScriptStart(soundScript, cue2Pan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
        }
        if (!(record->flags & ANIMATION_RECORD_CUE_1) && (work->lastCueFlags & ANIMATION_RECORD_CUE_1)) {
            enemy       = task->spawnArg2.pointer;
            soundScript = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | ACTOR_521100_FOOTSTEP_CUE_1;
            cue1Pan     = (s8)worldCoordGetOriginAudioPan(rootCoord);
            sndEvtRequestScriptStart(soundScript, cue1Pan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
        }
        work->lastCueFlags = (u16)(record->flags & ANIMATION_RECORD_CUE_MASK);
    }
}

#include "../../shared/creatures/golem/no9_golem_aim_head.inc.c"
/// Composes a hit rotation into the chest's local rotation; changes GTE state.
///
/// Postmultiplies chest * hit using Q12 coefficients, shifting each product by
/// twelve and saturating its components to signed halfwords. Both matrices must
/// remain live; the chest is word-aligned and writable. Its loaded basis stays
/// in the GTE while each result column is stored, avoiding compounded columns.
/// Translation and composition stamps are untouched; retains no pointer.
static inline void _actor521100ComposeHitRotation(MATRIX* chestRotation, const MATRIX* hitRotation)
{
    gte_SetRotMatrix(chestRotation);
    gte_ldclmv(hitRotation);
    gte_rtir();
    gte_stclmv(chestRotation);
    gte_ldclmv(&hitRotation->m[0][1]);
    gte_rtir();
    gte_stclmv(&chestRotation->m[0][1]);
    gte_ldclmv(&hitRotation->m[0][2]);
    gte_rtir();
    gte_stclmv(&chestRotation->m[0][2]);
}

/// Applies the residual hit rotation to the chest and decays it toward zero.
///
/// Requires the body's live model and work block. X and Y angles use 4096 units
/// per turn and decay by 32 units per tick; hitTwistActive clears when both
/// reach zero. Reserves and releases one MATRIX block and changes GTE state.
static void _actor521100ApplyHitTwist(Task* task)
{
    enum { ACTOR_521100_HIT_TWIST_DECAY_STEP = 32 };
    Actor521100Work* work;
    GfxCoord*        bodyCoords;
    MATRIX*          twistMatrix;
    s32              angleX;
    s32              angleY;
    s32              absAngleX;
    s32              nextAngleX;
    s32              absAngleY;
    s32              nextAngleY;
    s32              decayActive;

    SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    twistMatrix = SCRATCH_STACK_CURSOR(MATRIX);
    decayActive = 0;
    work        = task->work;
    bodyCoords  = task->extra.tmd->coords;
    RotMatrix(&work->hitTwist, twistMatrix);
    // Compose the hit rotation into the chest before decaying its two angles.
    _actor521100ComposeHitRotation(&bodyCoords[3].coord, twistMatrix);
    angleX = work->hitTwist.vx;
    if (angleX != 0) {
        absAngleX = __builtin_abs(angleX);
        if (absAngleX < ACTOR_521100_HIT_TWIST_DECAY_STEP + 1) {
            work->hitTwist.vx = 0;
        } else {
            nextAngleX = angleX - ACTOR_521100_HIT_TWIST_DECAY_STEP;
            if (angleX <= 0) {
                nextAngleX = angleX + ACTOR_521100_HIT_TWIST_DECAY_STEP;
            }
            work->hitTwist.vx = nextAngleX;
            decayActive       = 1;
        }
    }
    angleY = work->hitTwist.vy;
    if (angleY != 0) {
        absAngleY = __builtin_abs(angleY);
        if (absAngleY < ACTOR_521100_HIT_TWIST_DECAY_STEP + 1) {
            work->hitTwist.vy = 0;
        } else {
            nextAngleY = angleY - ACTOR_521100_HIT_TWIST_DECAY_STEP;
            if (angleY <= 0) {
                nextAngleY = angleY + ACTOR_521100_HIT_TWIST_DECAY_STEP;
            }
            work->hitTwist.vy = nextAngleY;
            decayActive       = 1;
        }
    }
    if (decayActive == 0) {
        work->hitTwistActive = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}
/// Spawns and fades the event's body fire over 420 ticks.
///
/// Call once per event frame with eventBurnStage in 1..3. Bursts occur every
/// 7, 14 or 28 ticks at the chest; stage 1 also picks one of eight body parts.
/// At ticks 240 and 330 the stage advances; tick 420 disables further bursts.
/// The signed-halfword timer comparisons and increments retain their wraps.
static void _actor521100TickEventFire(Task* task)
{
    enum {
        ACTOR_521100_FIRE_NONE       = 0,
        ACTOR_521100_FIRE_FULL       = 1,
        ACTOR_521100_FIRE_FADING     = 2,
        ACTOR_521100_FIRE_LAST       = 3,
        ACTOR_521100_FIRE_FADE_FRAME = 240,
        ACTOR_521100_FIRE_LAST_FRAME = 330,
        ACTOR_521100_FIRE_END_FRAME  = 420,
    };
    Actor521100Work* work;
    u16              burstFrames;
    u16              elapsedFrames;
    const s16*       fireParts;
    s16              partIndex;

    work               = task->work;
    burstFrames        = work->stateCounter + 1;
    work->stateCounter = burstFrames;
    if ((s16)burstFrames >= gActor521100EventFireIntervals[work->eventBurnStage]) {
        work->stateCounter = 0U;
        effectSpawnHit(EFFECT_HIT_KIND_BLAST, &task->extra.tmd->coords[3], NULL, &work->hitEffectArg);
        if (work->eventBurnStage == ACTOR_521100_FIRE_FULL) {
            fireParts       = gActor521100AnimationFireTables.tables.eventFireParts;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            partIndex       = fireParts[(gRandomLcgState >> 16) & (ARRAY_SIZE(gActor521100AnimationFireTables.tables.eventFireParts) - 1)];
            effectSpawnHit(EFFECT_HIT_KIND_BLAST, &task->extra.tmd->coords[partIndex], NULL, &work->hitEffectArg);
        }
    }
    // The overall fire clock advances independently of the burst interval.
    elapsedFrames      = work->stateElapsed + 1;
    work->stateElapsed = elapsedFrames;
    if ((s16)elapsedFrames == ACTOR_521100_FIRE_FADE_FRAME) {
        work->eventBurnStage = ACTOR_521100_FIRE_FADING;
    }
    if (work->stateElapsed == ACTOR_521100_FIRE_LAST_FRAME) {
        work->eventBurnStage = ACTOR_521100_FIRE_LAST;
    }
    if (work->stateElapsed >= ACTOR_521100_FIRE_END_FRAME) {
        work->eventBurnStage = ACTOR_521100_FIRE_NONE;
    }
}

enum {
    ACTOR_521100_GUNBLADE_TASK_ATTACH  = 0,
    ACTOR_521100_GUNBLADE_TASK_UPDATE  = 1,
    ACTOR_521100_GUNBLADE_TASK_DESTROY = 2,
};

/// Child gunblade lifecycle handlers: 0 attach, 1 draw-policy update, 2 destroy.
///
/// Copied by value and indexed by `Task::state` in 0..2, without a terminator
/// or bounds check. Callbacks require a live task; update requires setup
/// work, and teardown may release both the task and its owned `Enemy`.
/// Each callback receives the owned `Enemy` from `Task::spawnArg2` and the task.
/// Attach/update borrow the parent body task, part 8 and its lighting.
static const EnemyTaskFuncTable3 _gActor521100GunbladeTaskStates = {
    .funcs = {
        [ACTOR_521100_GUNBLADE_TASK_ATTACH]  = _actor521100AttachGunblade,
        [ACTOR_521100_GUNBLADE_TASK_UPDATE]  = _actor521100UpdateGunbladeDrawMode,
        [ACTOR_521100_GUNBLADE_TASK_DESTROY] = enemyDestroy,
    },
};

/// Dispatches the GOLEM body's spawn or update callback with its owning Enemy.
///
/// Task.state must be 0 (spawn) or 1 (update); spawnArg2.pointer must name the
/// live enemy. The descriptor table supplies this callback for the body model.
static void _actor521100BodyTask(Task* task)
{
    EnemyTaskFunc stateHandlers[ACTOR_521100_BODY_UPDATE + 1] = {
        _actor521100SpawnBody,
        _actor521100Update,
    };

    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Selects the GOLEM's event or combat update and refreshes its event latch.
static void _actor521100Update(Enemy* enemy, Task* task)
{
    Actor521100Work* work;

    work = task->work;
    if (gGameSession->eventState != 0) {
        work->inEvent = 1;
        _actor521100EventTick(enemy, task);
        return;
    }
    work->inEvent = 0;
    _actor521100FightTick(enemy, task);
}

/// Updates event animation, lighting, shadow and any commanded fire.
///
/// Requires live body work. Disables lock-on before animating and drawing;
/// fire effects run last only while eventBurnStage is nonzero.
static void _actor521100EventTick(Enemy* enemy, Task* task)
{
    Actor521100Work* work;

    work                          = task->work;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    _actor521100TickAnimation(task);
    _actor521100UpdateLighting(task);
    _no9GolemDrawShadow(task);
    if (work->eventBurnStage != 0) {
        _actor521100TickEventFire(task);
    }
}

/// Updates combat contacts, behavior, movement, animation and presentation.
///
/// Requires initialized body work and its Enemy. Pause redraws lighting/shadow;
/// hide suppresses both models and lock-on. Combat starts only after activation.
/// Contacts precede state dispatch; movement precedes animation/head/hit twist;
/// the root is composed before lighting and the shadow are drawn.
static void _actor521100FightTick(Enemy* enemy, Task* task)
{
    GfxCoord*        rootCoord;
    TmdObject*       bodyModel;
    Actor521100Work* work;
    s32              actorControl;
    s32              notLockableFlag;

    bodyModel       = task->extra.tmd;
    actorControl    = gSceneCombatState.actorControl;
    work            = task->work;
    rootCoord       = bodyModel->coords;
    notLockableFlag = WORLD_TARGET_NOT_LOCKABLE;
    switch (actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actor521100UpdateLighting(task);
            _no9GolemDrawShadow(task);
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            bodyModel->flags                   = 0;
            work->weaponTask->extra.tmd->flags = 0;
            enemy->node.state.parts.flags      = WORLD_TARGET_HIDE_HP;
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            bodyModel->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->weaponTask->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags      = notLockableFlag;
            return;
    }
    if (work->activated == 0) {
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        return;
    }
    _actor521100ResolveContacts(task);
    _actor521100TickState(task);
    _actor521100TurnTowardTargetYaw(task);
    _actor521100StepRootPosition(task);
    _actor521100PlayFootsteps(task);
    _actor521100TickAnimation(task);
    _no9GolemAimHead(task);
    if (work->hitTwistActive != 0) {
        _actor521100ApplyHitTwist(task);
    }
    rootCoord->composeStamp                 = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    _actor521100UpdateLighting(task);
    _no9GolemDrawShadow(task);
}

/// Dispatches one frame of the seven ACTOR_521100_STATE_* combat behaviors.
///
/// Requires live body work. Unknown state values leave behavior unchanged.
static void _actor521100TickState(Task* task)
{
    s16 fightState;

    fightState = ((Actor521100Work*)task->work)->state;
    switch (fightState) {
        case ACTOR_521100_STATE_APPROACH:
            _actor521100ApproachState(task);
            return;
        case ACTOR_521100_STATE_ATTACK:
            _actor521100AttackState(task);
            return;
        case ACTOR_521100_STATE_GRAB:
            _actor521100GrabState(task);
            return;
        case ACTOR_521100_STATE_STAGGER:
            _actor521100StaggerState(task);
            return;
        case ACTOR_521100_STATE_GUARD:
            _actor521100GuardState(task);
            return;
        case ACTOR_521100_STATE_FLINCH:
            _actor521100FlinchState(task);
            return;
        case ACTOR_521100_STATE_WALK_ROUTE:
            _actor521100WalkRouteState(task);
        default:
            return;
    }
}

/// Starts an attack when the player is within 3500 units and 45 degrees of facing.
///
/// Requires the approach state's current playerDistance, yaw and targetYaw.
/// Both limits are strict. On entry to attack, clears subState and both speeds;
/// otherwise leaves the work block unchanged. Headings use 4096 units per turn.
static void _actor521100TryStartAttack(Task* task)
{
    enum {
        ACTOR_521100_ATTACK_FACING_LIMIT = ACTOR_TRANSFORM_ANGLE_TURN / 8,
        ACTOR_521100_ATTACK_DISTANCE     = 3500,
    };
    Actor521100Work* work;
    s16              signedDifference;
    s16              facingError;
    s16              wrappedDifference;
    s32              differenceMagnitude;

    work                = task->work;
    signedDifference    = work->targetYaw - work->yaw;
    differenceMagnitude = abs(signedDifference);
    if (differenceMagnitude < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        facingError = differenceMagnitude;
    } else {
        if (signedDifference > 0) {
            wrappedDifference = ACTOR_TRANSFORM_ANGLE_TURN - signedDifference;
        } else {
            wrappedDifference = signedDifference + ACTOR_TRANSFORM_ANGLE_TURN;
        }
        facingError = wrappedDifference;
    }
    if ((facingError < ACTOR_521100_ATTACK_FACING_LIMIT) && (work->playerDistance < ACTOR_521100_ATTACK_DISTANCE)) {
        work->state        = ACTOR_521100_STATE_ATTACK;
        work->subState     = 0;
        work->forwardSpeed = 0;
        work->turnSpeed    = 0;
    }
}

/// Runs the stagger reaction and resumes approach or the interrupted route.
///
/// Entry stops movement and disables both attack spheres. After 55 animation
/// ticks, restores idle and draws the next approach wait in frames.
static void _actor521100StaggerState(Task* task)
{
    enum { ACTOR_521100_STAGGER_FRAMES = 0x37 };
    Actor521100Work* work;

    work = task->work;
    switch (work->subState) {
        case ACTOR_521100_REACTION_BEGIN:
            work->animationId          = ACTOR_521100_ANIM_STAGGER;
            work->subState             = ACTOR_521100_REACTION_WAIT;
            work->forwardSpeed         = 0;
            work->turnSpeed            = 0;
            work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            return;
        case ACTOR_521100_REACTION_WAIT:
            if (work->animationFrame >= ACTOR_521100_STAGGER_FRAMES) {
                ACTOR_521100_RESUME_AFTER_HIT(work, _gActor521100HitRecoveryWaitFrames);
            }
            return;
    }
}
/// Runs the plain-hit flinch and resumes approach or the interrupted route.
///
/// Entry stops movement and disables both attack spheres. After 72 animation
/// ticks, restores idle and draws the next approach wait in frames.
static void _actor521100FlinchState(Task* task)
{
    enum { ACTOR_521100_FLINCH_FRAMES = 0x48 };
    Actor521100Work* work;

    work = task->work;
    switch (work->subState) {
        case ACTOR_521100_REACTION_BEGIN:
            work->animationId          = ACTOR_521100_ANIM_FLINCH;
            work->subState             = ACTOR_521100_REACTION_WAIT;
            work->forwardSpeed         = 0;
            work->turnSpeed            = 0;
            work->weaponAttack.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->forearmAttack.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            return;
        case ACTOR_521100_REACTION_WAIT:
            if (work->animationFrame >= ACTOR_521100_FLINCH_FRAMES) {
                ACTOR_521100_RESUME_AFTER_HIT(work, _gActor521100ApproachWaitFrames);
            }
            return;
    }
}
#undef ACTOR_521100_RESUME_AFTER_HIT

/// Saves the root's pre-move position and applies this tick's translation.
///
/// Requires the body's live root and work block. X and Z move forwardSpeed
/// game units along the root's Q12 facing axis; Y advances by 128 units.
/// The saved translation narrows to signed halfwords for collision rollback;
/// the snapshot's fourth component is untouched. The caller dirties the root.
static void _actor521100StepRootPosition(Task* task)
{
    enum {
        ACTOR_521100_MOVEMENT_FRACTION_BITS = 12,
        ACTOR_521100_ROOT_VERTICAL_STEP     = 128,
    };
    GfxCoord*        rootCoord;
    Actor521100Work* work;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    work->prevRootPos.vx   = rootCoord->coord.t[0];
    work->prevRootPos.vy   = rootCoord->coord.t[1];
    work->prevRootPos.vz   = rootCoord->coord.t[2];
    rootCoord->coord.t[0] += (rootCoord->coord.m[0][2] * work->forwardSpeed) >> ACTOR_521100_MOVEMENT_FRACTION_BITS;
    rootCoord->coord.t[1] += ACTOR_521100_ROOT_VERTICAL_STEP;
    rootCoord->coord.t[2] += (rootCoord->coord.m[2][2] * work->forwardSpeed) >> ACTOR_521100_MOVEMENT_FRACTION_BITS;
}

/// Starts a changed body clip with its blend, or ticks all driven slots.
///
/// Requires a valid nonnegative animationId and the initialized nineteen-slot
/// body rig. Drives slots 1..18, leaving the root slot alone. A new clip resets
/// animationFrame; an unchanged one increments it with halfword wrap before
/// ticking. Native clip ids below 21 use the blend table; other clips use zero.
static void _actor521100TickAnimation(Task* task)
{
    Actor521100Work* work;
    s32              slotIndex;
    s32              blendFrames;

    work        = task->work;
    blendFrames = 0;
    if (work->animationId != work->seededAnimationId) {
        work->seededAnimationId = work->animationId;
        work->animationFrame    = 0;
        if (work->animationId < ARRAY_SIZE(gActor521100AnimationFireTables.blendFrames)) {
            blendFrames = gActor521100AnimationFireTables.blendFrames[work->animationId];
        }
        slotIndex = 1;
        do {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animationId, 0, blendFrames);
            slotIndex++;
        } while (slotIndex < ARRAY_SIZE(work->rig.slots));
        return;
    }
    slotIndex             = 1;
    work->animationFrame += slotIndex;
    do {
        animationTickSlot(&work->rig.anim, slotIndex);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->rig.slots));
}

/// Updates body lighting and colour from model coordinate 1's cached translation.
///
/// Requires a live body Enemy/model and a valid coordinate-1 cache. The lighting
/// query borrows only the three initialized position words during the
/// call; the resulting matrices are also used by the child gunblade model.
static void _actor521100UpdateLighting(Task* task)
{
    GfxCoord* sampleCoord;
    VECTOR3   samplePosition;

    sampleCoord       = &task->extra.tmd->coords[1];
    samplePosition.vx = sampleCoord->workm.t[0];
    samplePosition.vy = sampleCoord->workm.t[1];
    samplePosition.vz = sampleCoord->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &samplePosition, 0, 0);
}

#include "../../shared/creatures/golem/no9_golem_draw_shadow.inc.c"

/// Dispatches the child gunblade's attach, draw-update or destroy task state.
///
/// Requires a live child task and Enemy with Task.state in 0..2. The callback
/// table is copied by value; the destroy state releases the enemy and task.
static void _actor521100GunbladeTask(Task* task)
{
    EnemyTaskFuncTable3 handlers;

    handlers = _gActor521100GunbladeTaskStates;
    handlers.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Attaches the gunblade model to the body's weapon hand and starts its draw update.
///
/// Requires the live parent body task, its nineteen-part model and work block.
/// Borrows parent coordinate 8 and the body's lighting matrices; the task
/// hierarchy keeps that parent alive for the child's lifetime. unusedEnemy
/// is retained for the EnemyTaskFunc callback signature.
static void _actor521100AttachGunblade(Enemy* unusedEnemy, Task* task)
{
    enum {
        ACTOR_521100_GUNBLADE_HAND_COORD = 8,
        ACTOR_521100_GUNBLADE_UPDATE     = 1,
    };
    Task*            bodyTask;
    TmdObject*       gunbladeModel;
    Actor521100Work* bodyWork;
    GfxCoord*        gunbladeRoot;
    GfxCoord*        bodyCoords;

    bodyTask      = task->parent;
    gunbladeModel = task->extra.tmd;
    bodyCoords    = bodyTask->extra.tmd->coords;
    gunbladeRoot  = gunbladeModel->coords;
    bodyWork      = bodyTask->work;

    gunbladeRoot->parent    = &bodyCoords[ACTOR_521100_GUNBLADE_HAND_COORD];
    gunbladeModel->lightMtx = &bodyWork->light;
    gunbladeModel->flags    = 0;
    gunbladeModel->colorMtx = &bodyWork->color;
    task->state             = ACTOR_521100_GUNBLADE_UPDATE;
}

/// Makes the gunblade follow the body's requested draw flags during events.
///
/// Requires the live parent work block and child model. Bit 0 permits active
/// drawing and bit 1 suppresses automatic buffer allocation. weaponHidden
/// overrides both with active-draw exclusion alone. Outside events the flags
/// are unchanged. unusedEnemy is retained for the callback signature.
static void _actor521100UpdateGunbladeDrawMode(Enemy* unusedEnemy, Task* task)
{
    TmdObject*       gunbladeModel;
    Actor521100Work* bodyWork;
    s16              drawFlags;

    bodyWork      = task->parent->work;
    gunbladeModel = task->extra.tmd;
    if (bodyWork->inEvent != 0) {
        drawFlags            = ((bodyWork->modelDrawFlags & ACTOR_MESSAGE_PAIR_SHOW) == 0) * TMD_OBJECT_SKIP_ACTIVE_DRAW;
        gunbladeModel->flags = drawFlags;
        if (bodyWork->modelDrawFlags & ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER) {
            gunbladeModel->flags = drawFlags | TMD_OBJECT_SKIP_AUTO_BUFFER;
        }
        if (bodyWork->weaponHidden != 0) {
            gunbladeModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
}

/// Latches a living player's escape request for No. 9's grab sequence.
///
/// Requires initialized work. The next grab update consumes the latch and
/// chooses release or a counterattack. A dead player leaves it unchanged.
/// Ignores the message ID and both payloads; always returns 0.
static s32 _actor521100ReleasePlayerHold(Task* task, s32 unusedMessageId, s32 unusedArg, s32 unusedSecondArg)
{
    Actor521100Work* work;

    if (gPlayerStatus.hp > 0) {
        work                = task->work;
        work->playerEscaped = 1;
    }
    return 0;
}

/// Restarts No. 9's body tracks from an indexed event-animation request.
///
/// Requires live body work and its initialized nineteen-slot rig. Bank selector
/// zero adds 20 to the requested clip; every nonzero selector adds 29. The sum
/// narrows to s16 and must select a loaded entry in the 36-set body bank.
/// Restarts slots 1..18 at pose zero even for a repeated clip. Nonzero blend uses
/// the requested frame duration; reset uses zero. The request is borrowed only
/// during dispatch. Leaves the animation-frame counter and collision state
/// intact, ignores message ID/second argument, and returns 0.
static s32 _actor521100PlayEventAnimation(Task* task, s32 unusedMessageId, const AnimationPlayRequest* request, s32 unusedSecondArg)
{
    enum {
        ACTOR_521100_EVENT_BANK_ZERO_OFFSET    = 20,
        ACTOR_521100_EVENT_BANK_NONZERO_OFFSET = 29,
    };
    Actor521100Work* work;
    s32              slotIndex;
    s32              blendFrames;
    s16              clipId;
    s16              bankClipOffset;

    blendFrames    = 0;
    work           = task->work;
    bankClipOffset = ACTOR_521100_EVENT_BANK_NONZERO_OFFSET;
    if (request->source.index == 0) {
        bankClipOffset = ACTOR_521100_EVENT_BANK_ZERO_OFFSET;
    }
    clipId                  = request->animationId + bankClipOffset;
    work->animationId       = clipId;
    work->seededAnimationId = clipId;
    if (request->blend != ANIMATION_BLEND_RESET) {
        blendFrames = request->blendFrames;
    }
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animationId, 0, blendFrames);
    }
    return 0;
}

#include "../../shared/actor/actor_messages_place_rot_matrix.inc.c"
