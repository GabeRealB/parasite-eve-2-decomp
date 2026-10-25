#include "enemies/stranger/lesser.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include <psyq/abs.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/enemy_params.h"
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
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "stages/akropolis/rooms/fire_escape.h"

#include "assets/enemies/stranger/lesser/schema.h"
#include "../../../shared/actor/actor_contacts.h"

/// Task phases of this package's stationary enemy.
enum {
    PHASE_INIT  = 0,
    PHASE_IDLE  = 1,
    PHASE_HIT   = 2,
    PHASE_DEATH = 3,
    PHASE_DONE  = 4
};

/// Results of one hit-reaction update, consumed through a signed low halfword.
enum {
    HIT_REACTION_LETHAL   = -1,
    HIT_REACTION_PLAYING  = 0,
    HIT_REACTION_FINISHED = 1
};

/// The lesser Stranger's id: its placement entry, its body's collision key and its battle reward (and its sound
/// bank's, `SOUND_BANK_LESSER_STRANGER`).
enum { LESSER_STRANGER_ID = 10 };

/// The parts it is lit from (the pelvis) and targeted and hit at (the chest).
enum {
    PELVIS_PART = 1,
    CHEST_PART  = 2,
};

/// Values of `_Work::step`, the stage of the phase `Task::state`.
enum {
    IDLE_STEP_CHOOSE = 0, // draws between a pause and a replay of animation 0; two replays in a row force the pause
    IDLE_STEP_PAUSE  = 1, // holds the pose for 32 ticks
    IDLE_STEP_REPLAY = 2, // plays animation 0 through from its start
    HIT_STEP_START   = 0, // blends into animation 1 and spawns the hit effect; ends the phase at once when the hit points are gone
    HIT_STEP_PLAY    = 1, // plays animation 1 through
    DEATH_STEP_CRY   = 0, // queues the death sound
    DEATH_STEP_BURN  = 1  // runs the burn-away sequence that `stepFrame` times
};

/// Work block of the fire escape's lesser Stranger.
typedef struct {
    ActorAnimRig19        rig;              // playback of the model's parts; slot 1 reaching a boundary ends an idle replay and the hit reaction
    WorldCollisionBody    hitBody;          // sphere of radius 400 at model part 2 that takes the hits; unlinked when the death sequence begins
    WorldCollisionContact hitContacts[1];   // the one contact of `hitBody`; also the enemy's hit records until the death sequence begins
    MATRIX                lightMtx;         // storage for the model's `TmdObject::lightMtx`
    MATRIX                colorMtx;         // storage for the model's `TmdObject::colorMtx`
    Task*                 playerTask;       // the player's task as the spawn found it; never read
    MATRIX*               playerCoordMtx;   // the player's root coordinate matrix as the spawn found it; never read
    u32                   savedModelFlags;  // the model's `TmdObject::flags` as actor control 2 found them before hiding it; written back when the control returns to 0 from any other value
    s16                   step;             // `*_STEP_*` of the running phase; every phase change restarts it at 0
    byte                  pad_4C2[2];       // never accessed
    s16                   stepFrame;        // ticks counted by the idle pause and by the burn-away sequence, each of which restarts it at 0
    byte                  pad_4C6[2];       // never accessed
    s16                   idlePlayCount;    // idle replays finished since the last pause
    byte                  pad_4CA[2];       // never accessed
    s32                   hitKey;           // contact key of the category-2 contact found in `hitContacts` this tick; 0 when there is none
    s32                   lastHitKey;       // `hitKey` of the last hit taken; its id parameter 1 selects the hit effect
    u16                   present;          // answer to `ACTOR_MESSAGE_IS_PRESENT` (1 from spawn, 0 once the hit points are gone)
    u16                   prevActorControl; // `gSceneCombatState.actorControl` as the tick last recorded it (0 update, 1 pause, 2 hide); not recorded once the death phase runs
} _Work;
STATIC_ASSERT_SIZEOF(_Work, 0x4D8);

static AnimationSet _gStrangerDownBackClip;
extern AnimationSet gStrangerLesserIdleClip;
static TmdSource    _gAkropolisCafeteriaBossStrangerBody;
static void         _lesserStrangerTask(Task* actorTask);
static s32          _lesserStrangerIsPresentMessage(Task* actorTask, s32 messageId, s32 unusedArg, u32* presenceResult);

#include "assets/rooms/akropolis/cafeteria/boss_stranger_body.inc"

#include "assets/enemies/stranger/lesser/clips.inc"

/// Retained attack rows of the fire-escape Stranger, borrowed by its parameters.
static DamageAttack _gLesserStrangerAttacks[2] = {
    { 18, GAME_ACTOR_REACTION_SPARK_AND_PUFFS },
    { 18, GAME_ACTOR_REACTION_ORDINARY },
};

/// Health, rewards and hit-reaction parameters of the fire-escape Stranger.
static EnemyParams _gLesserStrangerParams = {
    .attacks              = _gLesserStrangerAttacks,
    .hpMax                = 30,
    .exp                  = 42,
    .bp                   = 82,
    .mp                   = 4,
    .critChance           = 250,
    .damageOverTimeChance = 100,
};

/// The Odd Stranger's three combat tunings (down ticks, sidestep angle and delay, notice radius), as actor_401000
/// reads them; this package carries the table but nothing here reads it.
static ActorStrangerVariant _gLesserStrangerUnusedStrangerVariants[3] = {
    { 20, 900, 12, 2000, { 0, 0, 0, 0 } },
    { 10, 800, 12, 2500, { 0, 0, 0, 0 } },
    { 0, 500, 12, 3000, { 0, 0, 0, 0 } },
};

/// The fire-escape Stranger's two body clips, indexed by `LESSER_ANIMATION_*`.
#include "assets/enemies/stranger/lesser/bank.inc"

/// Barrier patch's single Q12 +Z normal, copied into the fire-escape grid.
static SVECTOR _gLesserStrangerBarrierPatchNormals[1] = {
#include "assets/lesser_stranger_collision_074DC.inc"
};

/// Barrier patch's four room-coordinate vertices, copied into grid vertices 0..3.
static SVECTOR _gLesserStrangerBarrierPatchVerts[4] = {
#include "assets/lesser_stranger_collision_074E4.inc"
};

/// Barrier patch's one quad, copied into fire-escape grid face 0.
static WorldCollisionGridFace _gLesserStrangerBarrierPatchFaces[1] = {
#include "assets/lesser_stranger_collision_07504.inc"
};

/// Fire-escape Stranger's sole supported message, `ACTOR_MESSAGE_IS_PRESENT`.
static TaskMessageEntry _gLesserStrangerMessages[1] = {
    { ACTOR_MESSAGE_IS_PRESENT, _lesserStrangerIsPresentMessage },
};

TaskDesc gLesserStrangerTaskDesc = {
    .header   = { .fields = { .flags = (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), .priority = TASK_PRIORITY_CONTROL } },
    .callback = _lesserStrangerTask,
    .data     = { .model = &_gAkropolisCafeteriaBossStrangerBody },
};

static void        _lesserStrangerInit(Task* actorTask);
static inline void _lesserStrangerResetAnim(ActorAnimRig19* rig, u8 requestedRate, s32 slotIndex);
static inline u16  _lesserStrangerTickAnim(Task* task);
static void        _lesserStrangerTickIdle(Task* actorTask);
static s16         _lesserStrangerApplyHit(Task* actorTask);
static inline void _lesserStrangerBlendHitAnimation(Task* task);
static inline void _lesserStrangerSpawnHitEffect(Task* task);
static s32         _lesserStrangerTickHitReaction(Task* actorTask);
static s32         _lesserStrangerTickDeath(Task* actorTask);

#include "../../../shared/actor/actor_contacts_find_push.inc.c"

#include "../../../shared/actor/actor_contacts_steer.inc.c"

#include "../../../shared/actor/actor_contacts_turn_joint.inc.c"

/// Restarts the remaining idle tracks of the package rig at normal playback rate.
static inline void _lesserStrangerResetAnim(ActorAnimRig19* rig, u8 requestedRate, s32 slotIndex)
{
    do {
        rig->slots[slotIndex & 0xFFFF].rate = requestedRate;
        animationResetSlot(&rig->anim, slotIndex & 0xFFFF, LESSER_ANIMATION_IDLE);
        slotIndex += 1;
    } while ((u32)(slotIndex & 0xFFFF) < ARRAY_SIZE(rig->slots));
}

/// Initializes the stationary enemy's playback, targeting and single-contact hit body.
static void _lesserStrangerInit(Task* actorTask)
{
    enum { INITIAL_HP = 50,
           HIT_RADIUS = 400 };
    _Work*         allocatedWork;
    _Work*         work;
    _Work*         animationWork;
    Enemy*         enemy;
    GfxCoord*      rootCoord;
    TmdObject*     model;
    AreaPlacement* placement;
    s32            slotIndex;
    u8             rate;

    rootCoord       = actorTask->extra.tmd->coords;
    enemy           = actorTask->spawnArg2.pointer;
    model           = actorTask->extra.tmd;
    allocatedWork   = memCalloc(sizeof(_Work), 0);
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
    animationInitContext(&work->rig.anim, _gLesserStrangerAnimationSets, model, work->rig.poses,
                         &work->rig.slots[0]);
    work->playerTask     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    work->playerCoordMtx = gPlayerStatus.coordMtx;
    rate                 = ANIMATION_RATE_ONE;
    slotIndex            = 1;
    animationWork        = actorTask->work;
    _lesserStrangerResetAnim(&animationWork->rig, rate, slotIndex);
    // The target and the collision body share the chest; this body retains one hit.
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &actorTask->extra.tmd->coords[CHEST_PART];
    worldTargetLinkNode(&enemy->node);
    enemy->hp                      = INITIAL_HP;
    enemy->node.state.parts.flags  = 0;
    enemy->reactionFlags           = 0;
    enemy->param                   = &_gLesserStrangerParams;
    work->hitBody.coord            = &actorTask->extra.tmd->coords[CHEST_PART];
    work->hitBody.context.contacts = &work->hitContacts[0];
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vy           = 0;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | LESSER_STRANGER_ID;
    work->hitBody.radius           = HIT_RADIUS;
    work->hitBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hitBody);
    work->hitBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(&work->hitContacts[0], ARRAY_SIZE(work->hitContacts), 0);
    enemy->recs         = &work->hitContacts[0];
    actorTask->msgTable = _gLesserStrangerMessages;
    work->present       = 1;
    placement           = areaGetVariant(&gGameSession->location)->placements;
    while (placement->entryId != AREA_PLACEMENT_END && placement->entryId != LESSER_STRANGER_ID) {
        placement++;
    }
    tmdSetTextureOffsets(model, placement->texturePageOffset, placement->clutRowOffset);
}

/// Advances model slots 1..18 and reports whether slot 1 reached a playback boundary.
static inline u16 _lesserStrangerTickAnim(Task* task)
{
    _Work* work = task->work;
    s32    slotIndex;

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
static void _lesserStrangerTickIdle(Task* actorTask)
{
    enum { PAUSE_FRAMES = 32,
           MAX_REPLAYS  = 2 };
    _Work*  work;
    SVECTOR unusedProbe; // The target reserves this unused stack slot.
    u32     randomChoice;

    work = actorTask->work;

    switch (work->step) {
        case IDLE_STEP_CHOOSE:
            RANDOM_LCG_ADVANCE();
            randomChoice = RANDOM_LCG_DRAW();
            if (work->idlePlayCount >= MAX_REPLAYS) {
                work->step++;
            } else if (randomChoice & 1) {
                work->step++;
            } else {
                _Work* animationWork = actorTask->work;

                _lesserStrangerResetAnim(&animationWork->rig, 2 * ANIMATION_RATE_ONE, 1);
                work->step += 2;
            }
            work->stepFrame = 0;
            break;

        case IDLE_STEP_PAUSE:
            if (work->stepFrame++ >= PAUSE_FRAMES - 1) {
                work->idlePlayCount = 0;
                work->step          = IDLE_STEP_CHOOSE;
            }
            break;

        case IDLE_STEP_REPLAY:
            if (_lesserStrangerTickAnim(actorTask)) {
                work->step = IDLE_STEP_CHOOSE;
                work->idlePlayCount++;
            }
            break;

        default:
            break;
    }
}

/// Finds the first attack contact before an empty record or the contact limit.
static inline s32 _lesserStrangerFindHit(SVECTOR* contactPoint, const WorldCollisionContact* contacts, s16 contactCount)
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
static s16 _lesserStrangerApplyHit(Task* actorTask)
{
    enum { CRITICAL_DAMAGE_MULTIPLIER = 5 };
    _Work*  work = actorTask->work;
    Enemy*  enemy;
    SVECTOR contactPoint;
    s32     damage;

    enemy        = actorTask->spawnArg2.pointer;
    work->hitKey = _lesserStrangerFindHit(&contactPoint, work->hitContacts, ARRAY_SIZE(work->hitContacts));
    if (work->hitKey != 0) {
        work->lastHitKey = work->hitKey;
        // Use zero distance and disable reaction scaling before applying the critical multiplier.
        damage = damageComputePlayerAttack(work->hitKey, 0, 0, 0x1000);
        if (damageRollCriticalHit(enemy, work->hitKey, 0) != 0) {
            damage *= CRITICAL_DAMAGE_MULTIPLIER;
            effectSpawn(EFFECT_CRITICAL_HIT, actorTask->extra.tmd->coords, 0, 0);
        }
        enemy->hp -= damage;
        worldCollisionClearContacts(work->hitContacts);
        worldTargetAddReadoutAmount(&enemy->node, damage, 0);
    }
    return work->hitKey;
}

/// Blends model slots 1..18 into the hit clip over ten normal-rate frames.
static inline void _lesserStrangerBlendHitAnimation(Task* task)
{
    enum { HIT_BLEND_FRAMES = 10 };
    _Work* work = task->work;
    s32    slotIndex;

    slotIndex = 1;
    do {
        animationSeekSlotWithBlend(&work->rig.anim, slotIndex & 0xFFFF, LESSER_ANIMATION_HIT, 0, HIT_BLEND_FRAMES);
        slotIndex += 1;
    } while ((u32)(slotIndex & 0xFFFF) < ARRAY_SIZE(work->rig.slots));
}

/// Spawns the last attack's hit effect at the body's fixed local offset.
static inline void _lesserStrangerSpawnHitEffect(Task* task)
{
    enum { MAGNITUDE = 256,
           COUNT     = 2 };
    _Work*         work = task->work;
    SVECTOR        localOffset;
    EffectSpawnArg spawnArgs;

    spawnArgs.coord      = &task->extra.tmd->coords[CHEST_PART];
    spawnArgs.spawnArgLo = MAGNITUDE;
    spawnArgs.spawnArgHi = COUNT;
    localOffset.vx       = 0x3C;
    localOffset.vy       = -0xC;
    localOffset.vz       = 0x1E;
    effectSpawnHit(damageGetPlayerAttackEffectId(work->lastHitKey), &task->extra.tmd->coords[CHEST_PART], &localOffset, &spawnArgs);
}

/// Starts or advances the hit reaction, returning -1 for death, 1 for a boundary, or 0.
static s32 _lesserStrangerTickHitReaction(Task* actorTask)
{
    _Work* work;
    Enemy* enemy;

    work  = actorTask->work;
    enemy = actorTask->spawnArg2.pointer;

    switch (work->step) {
        case HIT_STEP_START:
            _lesserStrangerBlendHitAnimation(actorTask);
            _lesserStrangerSpawnHitEffect(actorTask);
            if (enemy->hp <= 0) {
                return HIT_REACTION_LETHAL;
            }
            work->step++;
            break;

        case HIT_STEP_PLAY:
            if (_lesserStrangerTickAnim(actorTask)) {
                return HIT_REACTION_FINISHED;
            }
            break;
    }
    return HIT_REACTION_PLAYING;
}

/// Runs the death sound, reward release and timed burn-away of the model.
static s32 _lesserStrangerTickDeath(Task* actorTask)
{
/// Installs the corpse root's nine scaled Q12 basis coefficients.
#define INSTALL_ROOT_ROTATION(rootCoord, rotation, matrixElement) \
    {                                                             \
        (matrixElement)            = (u16)(rotation).m[0][0];     \
        (rootCoord)->coord.m[0][0] = (matrixElement);             \
        (matrixElement)            = (u16)(rotation).m[0][1];     \
        (rootCoord)->coord.m[0][1] = (matrixElement);             \
        (matrixElement)            = (u16)(rotation).m[0][2];     \
        (rootCoord)->coord.m[0][2] = (matrixElement);             \
        (matrixElement)            = (u16)(rotation).m[1][0];     \
        (rootCoord)->coord.m[1][0] = (matrixElement);             \
        (matrixElement)            = (u16)(rotation).m[1][1];     \
        (rootCoord)->coord.m[1][1] = (matrixElement);             \
        (matrixElement)            = (u16)(rotation).m[1][2];     \
        (rootCoord)->coord.m[1][2] = (matrixElement);             \
        (matrixElement)            = (u16)(rotation).m[2][0];     \
        (rootCoord)->coord.m[2][0] = (matrixElement);             \
        (matrixElement)            = (u16)(rotation).m[2][1];     \
        (rootCoord)->coord.m[2][1] = (matrixElement);             \
        (matrixElement)            = (u16)(rotation).m[2][2];     \
        (rootCoord)->composeStamp  = GRAPHICS_COORD_DIRTY;        \
        (rootCoord)->coord.m[2][2] = (matrixElement);             \
    }

    enum {
        BURN_FRAME        = 10,
        BLACK_FRAME       = 22,
        TRANSLUCENT_FRAME = 28,
        HIDE_FRAME        = 80,
        END_FRAME         = 260,
        SCALE_START_FRAME = 6,
        SCALE_UNITY_FRAME = 20,
        FLAME_BURSTS      = 3
    };
    _Work*    work;
    Enemy*    enemy;
    GfxCoord* rootCoord;
    MATRIX    scaledYaw;
    VECTOR    scale;
    u16       matrixElement;
    s32       deathStep;
    s16       frame;
    s32       verticalScale;
    s16       yaw;
    s32       pan;

    work      = actorTask->work;
    enemy     = actorTask->spawnArg2.pointer;
    deathStep = work->step;

    switch (deathStep) {
        case DEATH_STEP_CRY:
            pan = (s8)worldCoordGetOriginAudioPan(actorTask->extra.tmd->coords);
            sndEvtRequestScriptStart(SOUND_LESSER_STRANGER_DEATH, pan, (s8)worldCoordGetOriginAudioDepth(actorTask->extra.tmd->coords));
            work->stepFrame = 0;
            work->step++;
            break;

        case DEATH_STEP_BURN:
            switch (work->stepFrame) {
                case 0:
                    sceneReleaseBattleRefWithRewards(actorTask, LESSER_STRANGER_ID);
                    enemy->recs = NULL;
                    worldCollisionUnlinkBody(&work->hitBody);
                    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                    break;

                case BURN_FRAME:
                    effectSpawn(EFFECT_CORPSE_BURN, &actorTask->extra.tmd->coords[CHEST_PART], FLAME_BURSTS, NULL);
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                    break;

                case BLACK_FRAME:
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                    break;

                case TRANSLUCENT_FRAME:
                    actorTask->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                    break;

                case HIDE_FRAME:
                    actorTask->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    break;

                case END_FRAME:
                    return 1;
            }

            // Rebuild root yaw with a Q12 vertical scale, retaining its translation.
            frame = work->stepFrame;
            if (frame >= SCALE_START_FRAME) {
                rootCoord     = actorTask->extra.tmd->coords;
                verticalScale = ONE - (frame - SCALE_UNITY_FRAME) * 10;
                yaw           = ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]);
                gfxRotMatrixY(&scaledYaw, yaw, 1);
                scale.vx = ONE;
                scale.vy = (s16)verticalScale;
                scale.vz = ONE;
                ScaleMatrix(&scaledYaw, &scale);

                INSTALL_ROOT_ROTATION(rootCoord, scaledYaw, matrixElement);
            }

            work->stepFrame++;
            break;

        default:
            return 0;
    }
    return 0;
}
#undef INSTALL_ROOT_ROTATION

/// Composes the body coordinate and refreshes lighting from the root's cached view position.
static inline void _lesserStrangerUpdatePresentation(Task* actor)
{
    Enemy* enemy;
    VECTOR viewPosition;

    enemy = actor->spawnArg2.pointer;
    actorRenderComposeCoord(&actor->extra.tmd->coords[PELVIS_PART]);
    viewPosition.vx = actor->extra.tmd->coords->workm.t[0];
    viewPosition.vy = actor->extra.tmd->coords->workm.t[1];
    viewPosition.vz = actor->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(enemy, &viewPosition, 0, 0);
    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Updates the fire-escape enemy's idle, hit and death phases and presentation.
static void _lesserStrangerTask(Task* actorTask)
{
    _Work*     work;
    TmdObject* model;
    s32        pan;

    work  = actorTask->work;
    model = actorTask->extra.tmd;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->prevActorControl != SCENE_COMBAT_ACTORS_RUNNING) {
                model->flags = work->savedModelFlags;
            }
            switch (actorTask->state) {
                case PHASE_INIT:
                    // Install the opening barrier patch into the loaded fire-escape grid.
                    memCopyBytes(_gLesserStrangerBarrierPatchVerts, gAkropolisFireEscapeCollisionGridVerts, sizeof(_gLesserStrangerBarrierPatchVerts));
                    memCopyBytes(_gLesserStrangerBarrierPatchNormals, gAkropolisFireEscapeCollisionGridNormals, sizeof(_gLesserStrangerBarrierPatchNormals));
                    memCopyBytes(_gLesserStrangerBarrierPatchFaces, gAkropolisFireEscapeCollisionGridFaces, sizeof(_gLesserStrangerBarrierPatchFaces));
                    _lesserStrangerInit(actorTask);
                    work = actorTask->work;
                    _lesserStrangerTickAnim(actorTask);
                    actorTask->state += 1;
                    break;

                case PHASE_IDLE:
                    _lesserStrangerTickIdle(actorTask);
                    if (_lesserStrangerApplyHit(actorTask) != 0) {
                        pan = (s8)worldCoordGetOriginAudioPan(actorTask->extra.tmd->coords);
                        sndEvtRequestScriptStart(SOUND_LESSER_STRANGER_HURT, pan,
                                                 (s8)worldCoordGetOriginAudioDepth(actorTask->extra.tmd->coords));
                        work->step        = HIT_STEP_START;
                        actorTask->state += 1;
                    }
                    worldCollisionClearContacts(work->hitContacts);
                    break;

                case PHASE_HIT:
                    if (_lesserStrangerApplyHit(actorTask) != 0) {
                        work->step = HIT_STEP_START;
                    }
                    // Both calls advance the reaction; a killing hit repeats its start effects.
                    if ((s16)_lesserStrangerTickHitReaction(actorTask) > HIT_REACTION_PLAYING) {
                        work->step        = IDLE_STEP_CHOOSE;
                        actorTask->state -= 1;
                        break;
                    }
                    if ((s16)_lesserStrangerTickHitReaction(actorTask) < HIT_REACTION_PLAYING) {
                        memFillBytes(gAkropolisFireEscapeCollisionGridVerts, 0, sizeof(_gLesserStrangerBarrierPatchVerts));
                        memFillBytes(gAkropolisFireEscapeCollisionGridNormals, 0, sizeof(_gLesserStrangerBarrierPatchNormals));
                        memFillBytes(gAkropolisFireEscapeCollisionGridFaces, 0, sizeof(_gLesserStrangerBarrierPatchFaces));
                        work->present     = 0;
                        work->step        = DEATH_STEP_CRY;
                        actorTask->state += 1;
                    }
                    break;

                case PHASE_DEATH:
                    if ((s16)_lesserStrangerTickDeath(actorTask) != 0) {
                        actorTask->state += 1;
                        return;
                    }
                    _lesserStrangerUpdatePresentation(actorTask);
                    return;

                case PHASE_DONE:
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
    _lesserStrangerUpdatePresentation(actorTask);
}

/// Reports whether the enemy is present, both as the result and through the output word.
static s32 _lesserStrangerIsPresentMessage(Task* actorTask, s32 messageId, s32 unusedArg, u32* presenceResult)
{
    _Work* work    = actorTask->work;
    u32    present = work->present;

    *presenceResult = present;
    return present;
}
