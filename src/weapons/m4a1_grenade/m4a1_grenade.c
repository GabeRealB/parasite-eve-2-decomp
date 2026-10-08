#include "weapons/m4a1_grenade.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "m4a1_grenade_private.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_ids.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/weapon.h"

#include "../../shared/grenade_shell.h"
#include "types.h"

/// One frame's M4A1 grenade launch vector and temporary pitched rotation.
///
/// Reserved and released as one complete block during projectile initialization.
/// The launch offset's unused fourth halfword is not initialized.
typedef struct {
    SVECTOR launchOffset;   // Local muzzle-to-projectile offset, in game coordinates
    MATRIX  launchRotation; // Projectile local basis pitched for its initial flight direction
} _M4a1GrenadeSpawnScratch;
STATIC_ASSERT_SIZEOF(_M4a1GrenadeSpawnScratch, 0x28);

static void _grenadeShellExit(Task* task);

/// Scratch-stack block the flight state holds for one frame.
///
/// The block is reserved on entry and released on every way out, so nothing
/// in it survives between frames. `delta` serves two purposes in turn: it
/// first receives the push-back of the grid contact being classified, which
/// is discarded because only the surface mask returned beside it is used, and
/// then holds the frame's step, in whole coordinate units, that is added onto
/// the projectile's coordinate.
typedef struct {
    byte                field_0[0x20];   // No recovered access; role unproven
    WorldCollisionDelta delta;           // Contact push-back output, then this frame's translation
    s32                 ammunitionIndex; // Loaded round on detonation (GRENADE_ROUND_*: 0xA fragmentation, 0xB airburst, 0xC riot)
} _M4a1GrenadeFlightScratch;
STATIC_ASSERT_SIZEOF(_M4a1GrenadeFlightScratch, 0x34);

static void _m4a1GrenadeInitProjectile(Task* task);
static void _m4a1GrenadeFlyProjectile(Task* task);

/// Spawns the selected room impact and plays its sound at the returned position.
///
/// The query initializes only the temporary coordinate's cached translation.
static inline void _m4a1GrenadePlayWeaponImpact(const WorldCollisionContact* contacts,
                                                const GfxCoord* playerCoord, GfxCoord* impactCoord)
{
    if (playerActorSpawnWeaponImpact(contacts, playerCoord, impactCoord) != 0) {
        worldCoordPlaySound(impactCoord, SOUND_COMMON(0x17), 1);
    }
}

void m4a1GrenadeAttackState(Task* playerTask)
{
    enum {
        M4A1_GRENADE_PHASE_PREPARE           = 0,
        M4A1_GRENADE_PHASE_WAIT_READY        = 1,
        M4A1_GRENADE_PHASE_SELECT_ATTACK     = 2,
        M4A1_GRENADE_PHASE_RIFLE_BURST       = 3,
        M4A1_GRENADE_PHASE_GRENADE_IMPACT    = 4,
        M4A1_GRENADE_PHASE_RECOVER           = 5,
        M4A1_GRENADE_PLAYER_ATTACK_STATE     = 4,
        M4A1_GRENADE_WEAPON_INDEX            = 27,
        M4A1_GRENADE_ANIMATION_READY         = 9,
        M4A1_GRENADE_ANIMATION_RIFLE         = 10,
        M4A1_GRENADE_ANIMATION_GRENADE       = 11,
        M4A1_GRENADE_READY_BLEND_FRAMES      = 1,
        M4A1_GRENADE_MOVING_BLEND_FRAMES     = 8,
        M4A1_GRENADE_RIFLE_BLEND_FRAMES      = 2,
        M4A1_GRENADE_LAUNCH_BLEND_FRAMES     = 3,
        M4A1_GRENADE_RIFLE_BURST_ROUNDS      = 3,
        M4A1_GRENADE_RIFLE_SHOT_TICKS        = 3,
        M4A1_GRENADE_RIFLE_IMPACT_TICKS      = 2,
        M4A1_GRENADE_RIFLE_CANCEL_TICKS      = 9,
        M4A1_GRENADE_LAUNCH_CANCEL_TICKS     = 34,
        M4A1_GRENADE_LAUNCH_COOLDOWN_TICKS   = 40,
        M4A1_GRENADE_RECOVERY_COOLDOWN_TICKS = 12,
        M4A1_GRENADE_RIFLE_SOUND             = SOUND_WEAPON(M4A1_GRENADE_WEAPON_INDEX, 4),
        M4A1_GRENADE_LAUNCH_SOUND            = SOUND_WEAPON(M4A1_GRENADE_WEAPON_INDEX, 6),
        M4A1_GRENADE_CUE_SOUND_FIRST         = SOUND_WEAPON(M4A1_GRENADE_WEAPON_INDEX, 8),
    };
    GameActor*                 actor;
    GfxCoord*                  playerCoord;
    GfxCoord*                  impactCoord;
    const AnimationRecord*     cueRecord;
    const EquipmentWeaponLoad* weaponLoad;
    s32                        blendFrames;
    s32                        burstTicksLeft;
    s32                        ammunitionIndex;

    actor       = playerTask->work;
    playerCoord = playerTask->extra.tmd->coords;
    weaponLoad  = equipmentGetWeaponLoad(WEAPON_ITEM(gPlayerStatus.weapon));
    // The impact query supplies only cached translation; the remaining node is untouched.
    impactCoord     = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    ammunitionIndex = WEAPON_AMMUNITION_INDEX(weaponLoad->secondaryItemId);
    if (ammunitionIndex < 0) {
        ammunitionIndex = GRENADE_ROUND_FRAGMENTATION;
    }
    switch (actor->statePhase) {
        case M4A1_GRENADE_PHASE_PREPARE:
            blendFrames                                           = M4A1_GRENADE_READY_BLEND_FRAMES;
            actor->state                                          = M4A1_GRENADE_PLAYER_ATTACK_STATE;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex                                  = 0;
            actor->animationState                                 = 0;
            actor->statePhase                                    += blendFrames;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                blendFrames = M4A1_GRENADE_MOVING_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, M4A1_GRENADE_ANIMATION_READY, 0, blendFrames);
            actor->movementMode = 0;
            break;
        case M4A1_GRENADE_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case M4A1_GRENADE_PHASE_SELECT_ATTACK:
            actor->rumblePosted = 0;
            if (actor->attackButton & PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                actor->statePhase        = M4A1_GRENADE_PHASE_RIFLE_BURST;
                actor->stateTimer        = 0;
                actor->attackCancelTicks = M4A1_GRENADE_RIFLE_CANCEL_TICKS;
                actor->actionValue       = M4A1_GRENADE_RIFLE_BURST_ROUNDS;
                playerActorSetWeaponAttackFlags(playerTask, 0, 1);
            } else if (actor->attackButton & PLAYER_ACTOR_ATTACK_BUTTON_SECONDARY) {
                actor->statePhase                  = M4A1_GRENADE_PHASE_GRENADE_IMPACT;
                actor->attackControl.cooldownTicks = M4A1_GRENADE_LAUNCH_COOLDOWN_TICKS;
                actor->attackCancelTicks           = M4A1_GRENADE_LAUNCH_CANCEL_TICKS;
                equipmentConsumeWeaponLoad(WEAPON_ITEM(M4A1_GRENADE_WEAPON_INDEX), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords,
                                    ((ammunitionIndex - GRENADE_ROUND_FIRST) << 24) | M4A1_GRENADE_LAUNCH_SOUND, 1);
                effectSpawn(EFFECT_GRENADE_MUZZLE_FLASH,
                            actor->equipmentTasks[1]->extra.tmd->coords, M4A1_GRENADE_WEAPON_INDEX,
                            NULL);
                playerActorSpawnGrenadeProjectile(playerTask, PLAYER_ACTOR_GRENADE_PLAYER, PLAYER_ACTOR_GRENADE_M4A1,
                                                  ammunitionIndex | (M4A1_GRENADE_WEAPON_INDEX << PLAYER_ACTOR_GRENADE_WEAPON_SHIFT));
                playerActorPlayChildSlotsWithBlend(playerTask, M4A1_GRENADE_ANIMATION_GRENADE, 0, M4A1_GRENADE_LAUNCH_BLEND_FRAMES);
                break;
            }
            /* fallthrough */
        case M4A1_GRENADE_PHASE_RIFLE_BURST:
            // Enable the rifle capsule for each shot, then resolve its contacts next tick.
            if (actor->actionValue != 0) {
                burstTicksLeft = actor->stateTimer;
                if (burstTicksLeft == 0) {
                    actor->actionValue--;
                    actor->stateTimer                                     = M4A1_GRENADE_RIFLE_SHOT_TICKS;
                    actor->rumblePosted                                   = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM(M4A1_GRENADE_WEAPON_INDEX), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(playerTask->extra.tmd->coords,
                                        ((ammunitionIndex - GRENADE_ROUND_FIRST) << 24) | M4A1_GRENADE_RIFLE_SOUND, 1);
                    effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                M4A1_GRENADE_WEAPON_INDEX, NULL);
                    playerActorPlayChildSlotsWithBlend(playerTask, M4A1_GRENADE_ANIMATION_RIFLE, 0, M4A1_GRENADE_RIFLE_BLEND_FRAMES);
                } else {
                    actor->stateTimer = burstTicksLeft - 1;
                    if (burstTicksLeft - 1 == M4A1_GRENADE_RIFLE_IMPACT_TICKS) {
                        actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                        _m4a1GrenadePlayWeaponImpact(actor->weaponContacts, playerCoord, impactCoord);
                    }
                }
                break;
            }
            /* fallthrough */
        case M4A1_GRENADE_PHASE_GRENADE_IMPACT:
            actor->statePhase                                     = M4A1_GRENADE_PHASE_RECOVER;
            actor->actionValue                                    = 0;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            _m4a1GrenadePlayWeaponImpact(actor->weaponContacts, playerCoord, impactCoord);
            /* fallthrough */
        case M4A1_GRENADE_PHASE_RECOVER:
            // Emit each animation cue once and allow cancellation after the attack delay.
            cueRecord = animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1);
            if (cueRecord != NULL && cueRecord != actor->lastCueRecord) {
                actor->lastCueRecord = cueRecord;
                if ((cueRecord->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                    worldCoordPlaySound(playerTask->extra.tmd->coords,
                                        (actor->actionValue + M4A1_GRENADE_CUE_SOUND_FIRST) | ((ammunitionIndex - GRENADE_ROUND_FIRST) << 24), 0);
                    actor->actionValue++;
                }
            }
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = M4A1_GRENADE_RECOVERY_COOLDOWN_TICKS;
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GfxCoord));
}

/// Initializes a launched M4A1 grenade and links its owned collision bodies.
///
/// Requires a live projectile TMD task whose root is parented to the muzzle.
/// The low halfword of spawnArg1.value is the weapon/round identity. A failed
/// work allocation kills the task after releasing scratch; successful dispatch
/// installs the shared exit callback and leaves work and collision storage owned
/// by the task until teardown. The launch basis is pitched -1024 angle units
/// before its normalized forward direction is stored at 4096 per unit.
static void _m4a1GrenadeInitProjectile(Task* task)
{
    enum {
        M4A1_GRENADE_LAUNCH_Y             = 0x220,
        M4A1_GRENADE_LAUNCH_Z             = 0x28,
        M4A1_GRENADE_LAUNCH_PITCH         = -0x400,
        M4A1_GRENADE_INITIAL_FLIGHT_TIMER = 10 << 16,
        M4A1_GRENADE_PROJECTILE_RADIUS    = 0x94,
        M4A1_GRENADE_CAPSULE_TIMER_SHIFT  = 10,
    };
    _M4a1GrenadeSpawnScratch* scratchEnd;
    _M4a1GrenadeSpawnScratch* scratch;
    SVECTOR*                  launchOffset;
    SVECTOR*                  gteLaunchOffset;
    MATRIX*                   launchRotation;
    TmdObject*                projectileModel;
    GfxCoord*                 projectileCoord;
    GfxCoord*                 muzzleCoord;
    WeaponGrenadeWork*        grenadeWork;

    projectileModel                                = task->extra.tmd;
    scratchEnd                                     = SCRATCH_STACK_CURSOR(_M4a1GrenadeSpawnScratch);
    projectileCoord                                = projectileModel->coords;
    scratch                                        = scratchEnd - 1;
    launchOffset                                   = &scratch->launchOffset;
    SCRATCH_STACK_CURSOR(_M4a1GrenadeSpawnScratch) = scratch;
    muzzleCoord                                    = projectileCoord->parent;
    grenadeWork                                    = memCalloc(sizeof(WeaponGrenadeWork), 0);
    gteLaunchOffset                                = launchOffset;
    if (grenadeWork == NULL) {
        SCRATCH_STACK_RELEASE_BLOCK(_M4a1GrenadeSpawnScratch);
        taskKill(task);
        return;
    }
    task->work         = grenadeWork;
    task->exitCallback = _grenadeShellExit;
    task->state++;
    memFillBytes(grenadeWork, 0, sizeof(*grenadeWork));
    launchOffset->vx          = 0;
    launchOffset->vy          = M4A1_GRENADE_LAUNCH_Y;
    launchOffset->vz          = M4A1_GRENADE_LAUNCH_Z;
    muzzleCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(muzzleCoord);
    projectileCoord->workm = muzzleCoord->workm;
    gte_SetRotMatrix(&muzzleCoord->workm);
    gte_SetTransMatrix(&muzzleCoord->workm);
    gte_ldv0(gteLaunchOffset);
    gte_rtv0tr();
    gte_stlvnl(projectileCoord->workm.t);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &projectileCoord->workm, &projectileCoord->coord);
    launchRotation                = &(scratchEnd - 1)->launchRotation;
    projectileCoord->parent       = &gGfxViewCoord;
    projectileCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    projectileModel->flags        = 0;
    *launchRotation               = projectileCoord->coord;
    gfxRotMatrixX(launchRotation, M4A1_GRENADE_LAUNCH_PITCH, GRAPHICS_ROTATION_COMPOSE);
    gfxReadMatrixZAxis(launchRotation, &grenadeWork->dir);
    VectorNormalSS(&grenadeWork->dir, &grenadeWork->dir);
    grenadeWork->flightTimer.word = M4A1_GRENADE_INITIAL_FLIGHT_TIMER;
    grenadeWork->smokeInterval    = 1;
    grenadeWork->flightFrame      = 0;
    // The sphere records pair contacts; the trailing capsule probes room geometry.
    grenadeWork->sphereBody.coord            = projectileCoord;
    grenadeWork->sphereBody.context.contacts = grenadeWork->sphereContacts;
    grenadeWork->sphereBody.pos.vx           = 0;
    grenadeWork->sphereBody.pos.vy           = 0;
    grenadeWork->sphereBody.pos.vz           = 0;
    grenadeWork->sphereBody.key              = (u16)task->spawnArg1.value | WORLD_COLLISION_CONTACT_ATTACK;
    grenadeWork->sphereBody.radius           = M4A1_GRENADE_PROJECTILE_RADIUS;
    grenadeWork->sphereBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &grenadeWork->sphereBody);
    worldCollisionInitContacts(grenadeWork->sphereBody.context.contacts, ARRAY_SIZE(grenadeWork->sphereContacts), 0);
    grenadeWork->capsuleBody.context.capsule = &grenadeWork->capsule;
    grenadeWork->capsuleBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    grenadeWork->capsule.contacts            = grenadeWork->capsuleContacts;
    grenadeWork->capsuleBody.coord           = projectileCoord;
    grenadeWork->capsuleBody.pos.vx          = 0;
    grenadeWork->capsuleBody.pos.vy          = 0;
    grenadeWork->capsuleBody.pos.vz          = 0;
    grenadeWork->capsuleBody.key             = 0;
    grenadeWork->capsuleBody.radius          = 0;
    grenadeWork->capsule.ends[0].vx          = 0;
    grenadeWork->capsule.ends[0].vy          = 0;
    grenadeWork->capsule.ends[0].vz          = 0;
    grenadeWork->capsule.ends[1].vx          = 0;
    grenadeWork->capsule.ends[1].vz          = 0;
    grenadeWork->capsule.end0Radius          = 1;
    grenadeWork->capsule.end1Radius          = 1;
    grenadeWork->sphereBody.flags           |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    grenadeWork->capsule.ends[1].vy          = -(grenadeWork->flightTimer.word >> M4A1_GRENADE_CAPSULE_TIMER_SHIFT);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &grenadeWork->capsuleBody);
    worldCollisionInitContacts(grenadeWork->capsule.contacts, ARRAY_SIZE(grenadeWork->capsuleContacts), 0);
    grenadeWork->capsuleBody.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
    SCRATCH_STACK_RELEASE_BLOCK(_M4a1GrenadeSpawnScratch);
}

/// Advances a live M4A1 grenade, resolving contacts or starting its blast.
///
/// Requires task state 1 and initialized `WeaponGrenadeWork`. Flight uses a
/// 16.16 clock whose integer half divides the 4096-scaled direction; each tick
/// adds `GRENADE_SHELL_FLIGHT_STEP` and detonates above 0xFFFFF. Enemy contacts
/// detonate immediately. Capsule grid contacts take precedence over sphere
/// grid contacts; blocking surfaces either detonate or select teardown. Class
/// 1 in the day/night Dryfield Water Tower also detonates on pass-through.
/// Detonation samples the current secondary load (fragmentation if empty),
/// widens the sphere and changes the clock to 8 blast ticks, or 1 for airburst.
/// Every path releases the frame's scratch block. A teardown surface still
/// receives this frame's movement before the next dispatch releases the task.
static void _m4a1GrenadeFlyProjectile(Task* task)
{
    /// Advances translation, capsule length and the 16.16 flight clock for one tick.
    ///
    /// All arguments must be stable live pointers without side effects; they are
    /// evaluated repeatedly. `grenade` is `WeaponGrenadeWork`, `coordNode` is its
    /// `GfxCoord`, and `deltaOut` is writable `WorldCollisionDelta`. The clock's
    /// integer half must be nonzero; direction uses 4096 per unit and the output
    /// step uses whole game-coordinate units. Uses the enclosing
    /// `M4A1_GRENADE_CAPSULE_TIMER_SHIFT` and shared `GRENADE_SHELL_FLIGHT_STEP`.
    /// Expands to a compound statement and is undefined after this function.
#define M4A1_GRENADE_STEP_PROJECTILE(grenade, coordNode, deltaOut)                                          \
    {                                                                                                       \
        (deltaOut)->vector.vx         = (grenade)->dir.vx / (grenade)->flightTimer.halves.integer;          \
        (deltaOut)->vector.vy         = (grenade)->dir.vy / (grenade)->flightTimer.halves.integer;          \
        (deltaOut)->vector.vz         = (grenade)->dir.vz / (grenade)->flightTimer.halves.integer;          \
        (coordNode)->coord.t[0]      += (deltaOut)->vector.vx;                                              \
        (coordNode)->coord.t[1]      += (deltaOut)->vector.vy;                                              \
        (coordNode)->coord.t[2]      += (deltaOut)->vector.vz;                                              \
        (grenade)->capsule.ends[1].vy = -((grenade)->flightTimer.word >> M4A1_GRENADE_CAPSULE_TIMER_SHIFT); \
        (grenade)->flightTimer.word  += GRENADE_SHELL_FLIGHT_STEP;                                          \
    }

    enum {
        M4A1_GRENADE_TASK_STATE_BLAST          = 2,
        M4A1_GRENADE_TASK_STATE_EXIT           = 3,
        M4A1_GRENADE_BLAST_FRAMES              = 8,
        M4A1_GRENADE_AIRBURST_BLAST_FRAMES     = 1,
        M4A1_GRENADE_CAPSULE_TIMER_SHIFT       = 10,
        M4A1_GRENADE_FLIGHT_TIMER_LIMIT        = 0xFFFFF,
        M4A1_GRENADE_GRAVITY_STEP              = 0x10,
        M4A1_GRENADE_SMOKE_INTERVAL_MAX        = 4,
        M4A1_GRENADE_SMOKE_THINNING_TICKS      = 7,
        M4A1_GRENADE_WATER_TOWER_SURFACE_CLASS = 1,
    };
    _M4a1GrenadeFlightScratch*             scratch;
    WeaponGrenadeWork*                     grenadeWork;
    GfxCoord*                              projectileCoord;
    const EquipmentWeaponLoad*             weaponLoad;
    const WorldCollisionSurfaceProperties* surface;
    s32                                    surfaceClass;
    s32                                    blastFramesLeft;
    s32                                    flightFrame;
    s32                                    weaponSoundBankBits;
    s32                                    explosionSoundBits;

    grenadeWork                   = task->work;
    projectileCoord               = task->extra.tmd->coords;
    weaponLoad                    = equipmentGetWeaponLoad(WEAPON_ITEM(gPlayerStatus.weapon));
    scratch                       = SCRATCH_STACK_RESERVE_BLOCK(_M4a1GrenadeFlightScratch);
    projectileCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (worldCollisionCountContactsByKind(grenadeWork->sphereContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
    detonate:
        // The launcher's load is the rifle's secondary one. With no round
        // loaded the index comes out negative and the grenade detonates as
        // a fragmentation round.
        scratch->ammunitionIndex = WEAPON_AMMUNITION_INDEX(weaponLoad->secondaryItemId);
        if (scratch->ammunitionIndex < 0) {
            scratch->ammunitionIndex = GRENADE_ROUND_FRAGMENTATION;
        }
        task->state = M4A1_GRENADE_TASK_STATE_BLAST;
        effectSpawn(EFFECT_GRENADE_EXPLOSION, projectileCoord, scratch->ammunitionIndex, NULL);
        weaponSoundBankBits = gPlayerStatus.weapon << 16;
        explosionSoundBits  = ((scratch->ammunitionIndex - GRENADE_ROUND_FIRST) << 24) | SOUND_WEAPON(0, 7);
        worldCoordPlaySound(projectileCoord, weaponSoundBankBits | explosionSoundBits, 1);
        blastFramesLeft = M4A1_GRENADE_BLAST_FRAMES;
        if (scratch->ammunitionIndex == GRENADE_ROUND_AIRBURST) {
            blastFramesLeft = M4A1_GRENADE_AIRBURST_BLAST_FRAMES;
        }
        grenadeWork->flightTimer.word  = blastFramesLeft;
        grenadeWork->sphereBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        grenadeWork->sphereBody.radius = D_m4a1_grenade_8012E08C[scratch->ammunitionIndex - GRENADE_ROUND_FIRST];
        SCRATCH_STACK_RELEASE_BLOCK(_M4a1GrenadeFlightScratch);
        return;
    }

    // Classify the capsule first; the low-byte mask is replaced by its surface class.
    // Each contact path joins after classification so the result is reloaded here.
    if (worldCollisionCountContactsByKind(grenadeWork->capsuleContacts, WORLD_COLLISION_CONTACT_GRID) == 0) {
        goto trySphereContacts;
    }
    worldCollisionResolveResponsePushback(grenadeWork->capsuleContacts, &scratch->delta, 1, &surfaceClass);
    surfaceClass = worldCollisionSurfaceClassFromMask((const u8*)&surfaceClass);
classified:
    surface = Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1][surfaceClass];
    if (surface->probePassThrough == WORLD_COLLISION_SURFACE_BLOCK_PROBES) {
        if (surface->weaponImpactEnabled != WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS) {
            goto detonate;
        }
        task->state = M4A1_GRENADE_TASK_STATE_EXIT;
    } else if (surfaceClass == M4A1_GRENADE_WATER_TOWER_SURFACE_CLASS && gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area == GAME_AREA_DRYFIELD_WATER_TOWER && (u32)(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage - GAME_STAGE_DRYFIELD) < 2U) {
        goto detonate;
    }
    goto advanceFlight;
trySphereContacts:
    if (worldCollisionCountContactsByKind(grenadeWork->sphereContacts, WORLD_COLLISION_CONTACT_GRID) != 0) {
        worldCollisionResolveResponsePushback(grenadeWork->sphereContacts, &scratch->delta, 1, &surfaceClass);
        surfaceClass = worldCollisionSurfaceClassFromMask((const u8*)&surfaceClass);
        goto classified;
    }
advanceFlight:
    // The growing flight divisor slows translation while gravity lowers the trajectory.
    M4A1_GRENADE_STEP_PROJECTILE(grenadeWork, projectileCoord, &scratch->delta);
    if (grenadeWork->flightTimer.word > M4A1_GRENADE_FLIGHT_TIMER_LIMIT) {
        goto detonate;
    }
    grenadeWork->dir.vy      = grenadeWork->dir.vy + M4A1_GRENADE_GRAVITY_STEP;
    flightFrame              = grenadeWork->flightFrame + 1;
    grenadeWork->flightFrame = flightFrame;
    if (grenadeWork->smokeInterval < M4A1_GRENADE_SMOKE_INTERVAL_MAX && flightFrame % M4A1_GRENADE_SMOKE_THINNING_TICKS == 0) {
        grenadeWork->smokeInterval = grenadeWork->smokeInterval + 1;
    }
    if (grenadeWork->flightFrame % grenadeWork->smokeInterval == 0) {
        effectSpawn(EFFECT_SMOKE_PUFF, projectileCoord, 0, NULL);
    }
    worldCollisionClearContacts(grenadeWork->sphereContacts);
    worldCollisionClearContacts(grenadeWork->capsuleContacts);
    SCRATCH_STACK_RELEASE_BLOCK(_M4a1GrenadeFlightScratch);
#undef M4A1_GRENADE_STEP_PROJECTILE
}

#include "../../shared/grenade_shell_blast.inc.c"

#include "../../shared/grenade_shell_exit.inc.c"

void m4a1GrenadeShellTask(Task* task)
{
    TaskFunc stateHandlers[] = {
        _m4a1GrenadeInitProjectile,
        _m4a1GrenadeFlyProjectile,
        _grenadeShellBlast,
        _grenadeShellExit,
    };

    stateHandlers[task->state](task);
}

static TmdBone _gM4a1GrenadeModel01134Skeleton[1] = {
#include "assets/m4a1_grenade_model_01134_skeleton.inc"
};

static u32 _gM4a1GrenadeModel01134PartVerts[1] = {
#include "assets/m4a1_grenade_model_01134_partVerts.inc"
};

static SVECTOR _gM4a1GrenadeModel01134Verts[66] = {
#include "assets/m4a1_grenade_model_01134_verts.inc"
};

static SVECTOR _gM4a1GrenadeModel01134Normals[62] = {
#include "assets/m4a1_grenade_model_01134_normals.inc"
};

static u32 _gM4a1GrenadeModel01134Stream[462] = {
#include "assets/m4a1_grenade_model_01134_stream.inc"
};

TmdSource D_m4a1_grenade_8011EA2C = {
    0,
    3356,
    0,
    1,
    _gM4a1GrenadeModel01134PartVerts,
    _gM4a1GrenadeModel01134Verts,
    _gM4a1GrenadeModel01134Normals,
    _gM4a1GrenadeModel01134Skeleton,
    _gM4a1GrenadeModel01134Stream,
};

static AnimationPackedPose _gM4a1GrenadeAnimation01A20Bank1[2] = {
#include "assets/m4a1_grenade_animation_01A20_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation01A20Bank4[8] = {
#include "assets/m4a1_grenade_animation_01A20_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation01A20Records[76] = {
#include "assets/m4a1_grenade_animation_01A20_records.inc"
};

static u16 _gM4a1GrenadeAnimation01A20Indices[20] = {
#include "assets/m4a1_grenade_animation_01A20_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation01A20 = {
    _gM4a1GrenadeAnimation01A20Records,
    _gM4a1GrenadeAnimation01A20Indices,
    { NULL, _gM4a1GrenadeAnimation01A20Bank1, NULL, NULL, _gM4a1GrenadeAnimation01A20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation020C4Bank1[12] = {
#include "assets/m4a1_grenade_animation_020C4_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation020C4Bank4[151] = {
#include "assets/m4a1_grenade_animation_020C4_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation020C4Records[218] = {
#include "assets/m4a1_grenade_animation_020C4_records.inc"
};

static u16 _gM4a1GrenadeAnimation020C4Indices[20] = {
#include "assets/m4a1_grenade_animation_020C4_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation020C4 = {
    _gM4a1GrenadeAnimation020C4Records,
    _gM4a1GrenadeAnimation020C4Indices,
    { NULL, _gM4a1GrenadeAnimation020C4Bank1, NULL, NULL, _gM4a1GrenadeAnimation020C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation02924Bank1[19] = {
#include "assets/m4a1_grenade_animation_02924_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation02924Bank4[169] = {
#include "assets/m4a1_grenade_animation_02924_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation02924Records[290] = {
#include "assets/m4a1_grenade_animation_02924_records.inc"
};

static u16 _gM4a1GrenadeAnimation02924Indices[20] = {
#include "assets/m4a1_grenade_animation_02924_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation02924 = {
    _gM4a1GrenadeAnimation02924Records,
    _gM4a1GrenadeAnimation02924Indices,
    { NULL, _gM4a1GrenadeAnimation02924Bank1, NULL, NULL, _gM4a1GrenadeAnimation02924Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation03188Bank1[19] = {
#include "assets/m4a1_grenade_animation_03188_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation03188Bank4[170] = {
#include "assets/m4a1_grenade_animation_03188_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation03188Records[290] = {
#include "assets/m4a1_grenade_animation_03188_records.inc"
};

static u16 _gM4a1GrenadeAnimation03188Indices[20] = {
#include "assets/m4a1_grenade_animation_03188_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation03188 = {
    _gM4a1GrenadeAnimation03188Records,
    _gM4a1GrenadeAnimation03188Indices,
    { NULL, _gM4a1GrenadeAnimation03188Bank1, NULL, NULL, _gM4a1GrenadeAnimation03188Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation0349CBank1[3] = {
#include "assets/m4a1_grenade_animation_0349C_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation0349CBank4[69] = {
#include "assets/m4a1_grenade_animation_0349C_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation0349CRecords[99] = {
#include "assets/m4a1_grenade_animation_0349C_records.inc"
};

static u16 _gM4a1GrenadeAnimation0349CIndices[20] = {
#include "assets/m4a1_grenade_animation_0349C_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation0349C = {
    _gM4a1GrenadeAnimation0349CRecords,
    _gM4a1GrenadeAnimation0349CIndices,
    { NULL, _gM4a1GrenadeAnimation0349CBank1, NULL, NULL, _gM4a1GrenadeAnimation0349CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation03BF8Bank1[14] = {
#include "assets/m4a1_grenade_animation_03BF8_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation03BF8Bank4[156] = {
#include "assets/m4a1_grenade_animation_03BF8_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation03BF8Records[253] = {
#include "assets/m4a1_grenade_animation_03BF8_records.inc"
};

static u16 _gM4a1GrenadeAnimation03BF8Indices[20] = {
#include "assets/m4a1_grenade_animation_03BF8_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation03BF8 = {
    _gM4a1GrenadeAnimation03BF8Records,
    _gM4a1GrenadeAnimation03BF8Indices,
    { NULL, _gM4a1GrenadeAnimation03BF8Bank1, NULL, NULL, _gM4a1GrenadeAnimation03BF8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation04380Bank1[16] = {
#include "assets/m4a1_grenade_animation_04380_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation04380Bank4[167] = {
#include "assets/m4a1_grenade_animation_04380_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation04380Records[247] = {
#include "assets/m4a1_grenade_animation_04380_records.inc"
};

static u16 _gM4a1GrenadeAnimation04380Indices[20] = {
#include "assets/m4a1_grenade_animation_04380_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation04380 = {
    _gM4a1GrenadeAnimation04380Records,
    _gM4a1GrenadeAnimation04380Indices,
    { NULL, _gM4a1GrenadeAnimation04380Bank1, NULL, NULL, _gM4a1GrenadeAnimation04380Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation04654Bank1[6] = {
#include "assets/m4a1_grenade_animation_04654_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation04654Bank4[52] = {
#include "assets/m4a1_grenade_animation_04654_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation04654Records[91] = {
#include "assets/m4a1_grenade_animation_04654_records.inc"
};

static u16 _gM4a1GrenadeAnimation04654Indices[20] = {
#include "assets/m4a1_grenade_animation_04654_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation04654 = {
    _gM4a1GrenadeAnimation04654Records,
    _gM4a1GrenadeAnimation04654Indices,
    { NULL, _gM4a1GrenadeAnimation04654Bank1, NULL, NULL, _gM4a1GrenadeAnimation04654Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation049E4Bank1[7] = {
#include "assets/m4a1_grenade_animation_049E4_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation049E4Bank4[73] = {
#include "assets/m4a1_grenade_animation_049E4_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation049E4Records[114] = {
#include "assets/m4a1_grenade_animation_049E4_records.inc"
};

static u16 _gM4a1GrenadeAnimation049E4Indices[20] = {
#include "assets/m4a1_grenade_animation_049E4_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation049E4 = {
    _gM4a1GrenadeAnimation049E4Records,
    _gM4a1GrenadeAnimation049E4Indices,
    { NULL, _gM4a1GrenadeAnimation049E4Bank1, NULL, NULL, _gM4a1GrenadeAnimation049E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation04E6CBank1[9] = {
#include "assets/m4a1_grenade_animation_04E6C_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation04E6CBank4[104] = {
#include "assets/m4a1_grenade_animation_04E6C_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation04E6CRecords[139] = {
#include "assets/m4a1_grenade_animation_04E6C_records.inc"
};

static u16 _gM4a1GrenadeAnimation04E6CIndices[20] = {
#include "assets/m4a1_grenade_animation_04E6C_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation04E6C = {
    _gM4a1GrenadeAnimation04E6CRecords,
    _gM4a1GrenadeAnimation04E6CIndices,
    { NULL, _gM4a1GrenadeAnimation04E6CBank1, NULL, NULL, _gM4a1GrenadeAnimation04E6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation05068Bank1[3] = {
#include "assets/m4a1_grenade_animation_05068_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation05068Bank4[22] = {
#include "assets/m4a1_grenade_animation_05068_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation05068Records[76] = {
#include "assets/m4a1_grenade_animation_05068_records.inc"
};

static u16 _gM4a1GrenadeAnimation05068Indices[20] = {
#include "assets/m4a1_grenade_animation_05068_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation05068 = {
    _gM4a1GrenadeAnimation05068Records,
    _gM4a1GrenadeAnimation05068Indices,
    { NULL, _gM4a1GrenadeAnimation05068Bank1, NULL, NULL, _gM4a1GrenadeAnimation05068Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation05340Bank1[6] = {
#include "assets/m4a1_grenade_animation_05340_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation05340Bank4[57] = {
#include "assets/m4a1_grenade_animation_05340_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation05340Records[87] = {
#include "assets/m4a1_grenade_animation_05340_records.inc"
};

static u16 _gM4a1GrenadeAnimation05340Indices[20] = {
#include "assets/m4a1_grenade_animation_05340_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation05340 = {
    _gM4a1GrenadeAnimation05340Records,
    _gM4a1GrenadeAnimation05340Indices,
    { NULL, _gM4a1GrenadeAnimation05340Bank1, NULL, NULL, _gM4a1GrenadeAnimation05340Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation055E4Bank1[4] = {
#include "assets/m4a1_grenade_animation_055E4_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation055E4Bank4[55] = {
#include "assets/m4a1_grenade_animation_055E4_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation055E4Records[82] = {
#include "assets/m4a1_grenade_animation_055E4_records.inc"
};

static u16 _gM4a1GrenadeAnimation055E4Indices[20] = {
#include "assets/m4a1_grenade_animation_055E4_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation055E4 = {
    _gM4a1GrenadeAnimation055E4Records,
    _gM4a1GrenadeAnimation055E4Indices,
    { NULL, _gM4a1GrenadeAnimation055E4Bank1, NULL, NULL, _gM4a1GrenadeAnimation055E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation057E4Bank1[3] = {
#include "assets/m4a1_grenade_animation_057E4_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation057E4Bank4[23] = {
#include "assets/m4a1_grenade_animation_057E4_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation057E4Records[76] = {
#include "assets/m4a1_grenade_animation_057E4_records.inc"
};

static u16 _gM4a1GrenadeAnimation057E4Indices[20] = {
#include "assets/m4a1_grenade_animation_057E4_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation057E4 = {
    _gM4a1GrenadeAnimation057E4Records,
    _gM4a1GrenadeAnimation057E4Indices,
    { NULL, _gM4a1GrenadeAnimation057E4Bank1, NULL, NULL, _gM4a1GrenadeAnimation057E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation05B38Bank1[8] = {
#include "assets/m4a1_grenade_animation_05B38_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation05B38Bank4[68] = {
#include "assets/m4a1_grenade_animation_05B38_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation05B38Records[101] = {
#include "assets/m4a1_grenade_animation_05B38_records.inc"
};

static u16 _gM4a1GrenadeAnimation05B38Indices[20] = {
#include "assets/m4a1_grenade_animation_05B38_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation05B38 = {
    _gM4a1GrenadeAnimation05B38Records,
    _gM4a1GrenadeAnimation05B38Indices,
    { NULL, _gM4a1GrenadeAnimation05B38Bank1, NULL, NULL, _gM4a1GrenadeAnimation05B38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation05DECBank1[5] = {
#include "assets/m4a1_grenade_animation_05DEC_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation05DECBank4[55] = {
#include "assets/m4a1_grenade_animation_05DEC_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation05DECRecords[83] = {
#include "assets/m4a1_grenade_animation_05DEC_records.inc"
};

static u16 _gM4a1GrenadeAnimation05DECIndices[20] = {
#include "assets/m4a1_grenade_animation_05DEC_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation05DEC = {
    _gM4a1GrenadeAnimation05DECRecords,
    _gM4a1GrenadeAnimation05DECIndices,
    { NULL, _gM4a1GrenadeAnimation05DECBank1, NULL, NULL, _gM4a1GrenadeAnimation05DECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation0610CBank1[6] = {
#include "assets/m4a1_grenade_animation_0610C_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation0610CBank4[66] = {
#include "assets/m4a1_grenade_animation_0610C_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation0610CRecords[96] = {
#include "assets/m4a1_grenade_animation_0610C_records.inc"
};

static u16 _gM4a1GrenadeAnimation0610CIndices[20] = {
#include "assets/m4a1_grenade_animation_0610C_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation0610C = {
    _gM4a1GrenadeAnimation0610CRecords,
    _gM4a1GrenadeAnimation0610CIndices,
    { NULL, _gM4a1GrenadeAnimation0610CBank1, NULL, NULL, _gM4a1GrenadeAnimation0610CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation068D0Bank1[18] = {
#include "assets/m4a1_grenade_animation_068D0_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation068D0Bank4[184] = {
#include "assets/m4a1_grenade_animation_068D0_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation068D0Records[239] = {
#include "assets/m4a1_grenade_animation_068D0_records.inc"
};

static u16 _gM4a1GrenadeAnimation068D0Indices[20] = {
#include "assets/m4a1_grenade_animation_068D0_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation068D0 = {
    _gM4a1GrenadeAnimation068D0Records,
    _gM4a1GrenadeAnimation068D0Indices,
    { NULL, _gM4a1GrenadeAnimation068D0Bank1, NULL, NULL, _gM4a1GrenadeAnimation068D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation07B68Bank1[29] = {
#include "assets/m4a1_grenade_animation_07B68_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation07B68Bank4[450] = {
#include "assets/m4a1_grenade_animation_07B68_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation07B68Records[633] = {
#include "assets/m4a1_grenade_animation_07B68_records.inc"
};

static u16 _gM4a1GrenadeAnimation07B68Indices[20] = {
#include "assets/m4a1_grenade_animation_07B68_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation07B68 = {
    _gM4a1GrenadeAnimation07B68Records,
    _gM4a1GrenadeAnimation07B68Indices,
    { NULL, _gM4a1GrenadeAnimation07B68Bank1, NULL, NULL, _gM4a1GrenadeAnimation07B68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation086E0Bank1[12] = {
#include "assets/m4a1_grenade_animation_086E0_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation086E0Bank4[266] = {
#include "assets/m4a1_grenade_animation_086E0_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation086E0Records[412] = {
#include "assets/m4a1_grenade_animation_086E0_records.inc"
};

static u16 _gM4a1GrenadeAnimation086E0Indices[20] = {
#include "assets/m4a1_grenade_animation_086E0_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation086E0 = {
    _gM4a1GrenadeAnimation086E0Records,
    _gM4a1GrenadeAnimation086E0Indices,
    { NULL, _gM4a1GrenadeAnimation086E0Bank1, NULL, NULL, _gM4a1GrenadeAnimation086E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation08DFCBank1[9] = {
#include "assets/m4a1_grenade_animation_08DFC_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation08DFCBank4[144] = {
#include "assets/m4a1_grenade_animation_08DFC_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation08DFCRecords[264] = {
#include "assets/m4a1_grenade_animation_08DFC_records.inc"
};

static u16 _gM4a1GrenadeAnimation08DFCIndices[20] = {
#include "assets/m4a1_grenade_animation_08DFC_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation08DFC = {
    _gM4a1GrenadeAnimation08DFCRecords,
    _gM4a1GrenadeAnimation08DFCIndices,
    { NULL, _gM4a1GrenadeAnimation08DFCBank1, NULL, NULL, _gM4a1GrenadeAnimation08DFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation0927CBank1[6] = {
#include "assets/m4a1_grenade_animation_0927C_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation0927CBank4[107] = {
#include "assets/m4a1_grenade_animation_0927C_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation0927CRecords[143] = {
#include "assets/m4a1_grenade_animation_0927C_records.inc"
};

static u16 _gM4a1GrenadeAnimation0927CIndices[20] = {
#include "assets/m4a1_grenade_animation_0927C_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation0927C = {
    _gM4a1GrenadeAnimation0927CRecords,
    _gM4a1GrenadeAnimation0927CIndices,
    { NULL, _gM4a1GrenadeAnimation0927CBank1, NULL, NULL, _gM4a1GrenadeAnimation0927CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation09454Bank1[3] = {
#include "assets/m4a1_grenade_animation_09454_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation09454Bank4[32] = {
#include "assets/m4a1_grenade_animation_09454_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation09454Records[57] = {
#include "assets/m4a1_grenade_animation_09454_records.inc"
};

static u16 _gM4a1GrenadeAnimation09454Indices[20] = {
#include "assets/m4a1_grenade_animation_09454_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation09454 = {
    _gM4a1GrenadeAnimation09454Records,
    _gM4a1GrenadeAnimation09454Indices,
    { NULL, _gM4a1GrenadeAnimation09454Bank1, NULL, NULL, _gM4a1GrenadeAnimation09454Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation099B8Bank1[11] = {
#include "assets/m4a1_grenade_animation_099B8_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation099B8Bank4[125] = {
#include "assets/m4a1_grenade_animation_099B8_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation099B8Records[167] = {
#include "assets/m4a1_grenade_animation_099B8_records.inc"
};

static u16 _gM4a1GrenadeAnimation099B8Indices[20] = {
#include "assets/m4a1_grenade_animation_099B8_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation099B8 = {
    _gM4a1GrenadeAnimation099B8Records,
    _gM4a1GrenadeAnimation099B8Indices,
    { NULL, _gM4a1GrenadeAnimation099B8Bank1, NULL, NULL, _gM4a1GrenadeAnimation099B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation09BACBank1[3] = {
#include "assets/m4a1_grenade_animation_09BAC_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation09BACBank4[20] = {
#include "assets/m4a1_grenade_animation_09BAC_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation09BACRecords[76] = {
#include "assets/m4a1_grenade_animation_09BAC_records.inc"
};

static u16 _gM4a1GrenadeAnimation09BACIndices[20] = {
#include "assets/m4a1_grenade_animation_09BAC_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation09BAC = {
    _gM4a1GrenadeAnimation09BACRecords,
    _gM4a1GrenadeAnimation09BACIndices,
    { NULL, _gM4a1GrenadeAnimation09BACBank1, NULL, NULL, _gM4a1GrenadeAnimation09BACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation0A02CBank1[8] = {
#include "assets/m4a1_grenade_animation_0A02C_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation0A02CBank4[105] = {
#include "assets/m4a1_grenade_animation_0A02C_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation0A02CRecords[139] = {
#include "assets/m4a1_grenade_animation_0A02C_records.inc"
};

static u16 _gM4a1GrenadeAnimation0A02CIndices[20] = {
#include "assets/m4a1_grenade_animation_0A02C_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation0A02C = {
    _gM4a1GrenadeAnimation0A02CRecords,
    _gM4a1GrenadeAnimation0A02CIndices,
    { NULL, _gM4a1GrenadeAnimation0A02CBank1, NULL, NULL, _gM4a1GrenadeAnimation0A02CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation0A204Bank1[2] = {
#include "assets/m4a1_grenade_animation_0A204_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation0A204Bank4[16] = {
#include "assets/m4a1_grenade_animation_0A204_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation0A204Records[76] = {
#include "assets/m4a1_grenade_animation_0A204_records.inc"
};

static u16 _gM4a1GrenadeAnimation0A204Indices[20] = {
#include "assets/m4a1_grenade_animation_0A204_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation0A204 = {
    _gM4a1GrenadeAnimation0A204Records,
    _gM4a1GrenadeAnimation0A204Indices,
    { NULL, _gM4a1GrenadeAnimation0A204Bank1, NULL, NULL, _gM4a1GrenadeAnimation0A204Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation0AA44Bank1[14] = {
#include "assets/m4a1_grenade_animation_0AA44_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation0AA44Bank4[194] = {
#include "assets/m4a1_grenade_animation_0AA44_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation0AA44Records[272] = {
#include "assets/m4a1_grenade_animation_0AA44_records.inc"
};

static u16 _gM4a1GrenadeAnimation0AA44Indices[20] = {
#include "assets/m4a1_grenade_animation_0AA44_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation0AA44 = {
    _gM4a1GrenadeAnimation0AA44Records,
    _gM4a1GrenadeAnimation0AA44Indices,
    { NULL, _gM4a1GrenadeAnimation0AA44Bank1, NULL, NULL, _gM4a1GrenadeAnimation0AA44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation0B4ECBank1[19] = {
#include "assets/m4a1_grenade_animation_0B4EC_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation0B4ECBank4[269] = {
#include "assets/m4a1_grenade_animation_0B4EC_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation0B4ECRecords[336] = {
#include "assets/m4a1_grenade_animation_0B4EC_records.inc"
};

static u16 _gM4a1GrenadeAnimation0B4ECIndices[20] = {
#include "assets/m4a1_grenade_animation_0B4EC_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation0B4EC = {
    _gM4a1GrenadeAnimation0B4ECRecords,
    _gM4a1GrenadeAnimation0B4ECIndices,
    { NULL, _gM4a1GrenadeAnimation0B4ECBank1, NULL, NULL, _gM4a1GrenadeAnimation0B4ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation0C388Bank1[24] = {
#include "assets/m4a1_grenade_animation_0C388_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation0C388Bank4[390] = {
#include "assets/m4a1_grenade_animation_0C388_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation0C388Records[453] = {
#include "assets/m4a1_grenade_animation_0C388_records.inc"
};

static u16 _gM4a1GrenadeAnimation0C388Indices[20] = {
#include "assets/m4a1_grenade_animation_0C388_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation0C388 = {
    _gM4a1GrenadeAnimation0C388Records,
    _gM4a1GrenadeAnimation0C388Indices,
    { NULL, _gM4a1GrenadeAnimation0C388Bank1, NULL, NULL, _gM4a1GrenadeAnimation0C388Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation0C82CBank1[8] = {
#include "assets/m4a1_grenade_animation_0C82C_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation0C82CBank4[102] = {
#include "assets/m4a1_grenade_animation_0C82C_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation0C82CRecords[151] = {
#include "assets/m4a1_grenade_animation_0C82C_records.inc"
};

static u16 _gM4a1GrenadeAnimation0C82CIndices[20] = {
#include "assets/m4a1_grenade_animation_0C82C_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation0C82C = {
    _gM4a1GrenadeAnimation0C82CRecords,
    _gM4a1GrenadeAnimation0C82CIndices,
    { NULL, _gM4a1GrenadeAnimation0C82CBank1, NULL, NULL, _gM4a1GrenadeAnimation0C82CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation0CF68Bank1[13] = {
#include "assets/m4a1_grenade_animation_0CF68_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation0CF68Bank4[171] = {
#include "assets/m4a1_grenade_animation_0CF68_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation0CF68Records[233] = {
#include "assets/m4a1_grenade_animation_0CF68_records.inc"
};

static u16 _gM4a1GrenadeAnimation0CF68Indices[20] = {
#include "assets/m4a1_grenade_animation_0CF68_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation0CF68 = {
    _gM4a1GrenadeAnimation0CF68Records,
    _gM4a1GrenadeAnimation0CF68Indices,
    { NULL, _gM4a1GrenadeAnimation0CF68Bank1, NULL, NULL, _gM4a1GrenadeAnimation0CF68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation0E6B8Bank1[39] = {
#include "assets/m4a1_grenade_animation_0E6B8_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation0E6B8Bank4[632] = {
#include "assets/m4a1_grenade_animation_0E6B8_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation0E6B8Records[723] = {
#include "assets/m4a1_grenade_animation_0E6B8_records.inc"
};

static u16 _gM4a1GrenadeAnimation0E6B8Indices[20] = {
#include "assets/m4a1_grenade_animation_0E6B8_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation0E6B8 = {
    _gM4a1GrenadeAnimation0E6B8Records,
    _gM4a1GrenadeAnimation0E6B8Indices,
    { NULL, _gM4a1GrenadeAnimation0E6B8Bank1, NULL, NULL, _gM4a1GrenadeAnimation0E6B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation0EDC4Bank1[13] = {
#include "assets/m4a1_grenade_animation_0EDC4_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation0EDC4Bank4[175] = {
#include "assets/m4a1_grenade_animation_0EDC4_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation0EDC4Records[217] = {
#include "assets/m4a1_grenade_animation_0EDC4_records.inc"
};

static u16 _gM4a1GrenadeAnimation0EDC4Indices[20] = {
#include "assets/m4a1_grenade_animation_0EDC4_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation0EDC4 = {
    _gM4a1GrenadeAnimation0EDC4Records,
    _gM4a1GrenadeAnimation0EDC4Indices,
    { NULL, _gM4a1GrenadeAnimation0EDC4Bank1, NULL, NULL, _gM4a1GrenadeAnimation0EDC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation0F33CBank1[10] = {
#include "assets/m4a1_grenade_animation_0F33C_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation0F33CBank4[131] = {
#include "assets/m4a1_grenade_animation_0F33C_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation0F33CRecords[169] = {
#include "assets/m4a1_grenade_animation_0F33C_records.inc"
};

static u16 _gM4a1GrenadeAnimation0F33CIndices[20] = {
#include "assets/m4a1_grenade_animation_0F33C_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation0F33C = {
    _gM4a1GrenadeAnimation0F33CRecords,
    _gM4a1GrenadeAnimation0F33CIndices,
    { NULL, _gM4a1GrenadeAnimation0F33CBank1, NULL, NULL, _gM4a1GrenadeAnimation0F33CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation0FBF4Bank1[18] = {
#include "assets/m4a1_grenade_animation_0FBF4_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation0FBF4Bank4[201] = {
#include "assets/m4a1_grenade_animation_0FBF4_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation0FBF4Records[283] = {
#include "assets/m4a1_grenade_animation_0FBF4_records.inc"
};

static u16 _gM4a1GrenadeAnimation0FBF4Indices[20] = {
#include "assets/m4a1_grenade_animation_0FBF4_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation0FBF4 = {
    _gM4a1GrenadeAnimation0FBF4Records,
    _gM4a1GrenadeAnimation0FBF4Indices,
    { NULL, _gM4a1GrenadeAnimation0FBF4Bank1, NULL, NULL, _gM4a1GrenadeAnimation0FBF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation10394Bank1[16] = {
#include "assets/m4a1_grenade_animation_10394_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation10394Bank4[157] = {
#include "assets/m4a1_grenade_animation_10394_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation10394Records[263] = {
#include "assets/m4a1_grenade_animation_10394_records.inc"
};

static u16 _gM4a1GrenadeAnimation10394Indices[20] = {
#include "assets/m4a1_grenade_animation_10394_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation10394 = {
    _gM4a1GrenadeAnimation10394Records,
    _gM4a1GrenadeAnimation10394Indices,
    { NULL, _gM4a1GrenadeAnimation10394Bank1, NULL, NULL, _gM4a1GrenadeAnimation10394Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1GrenadeAnimation10D68Bank1[25] = {
#include "assets/m4a1_grenade_animation_10D68_bank1.inc"
};

static AnimationPackedRotation _gM4a1GrenadeAnimation10D68Bank4[223] = {
#include "assets/m4a1_grenade_animation_10D68_bank4.inc"
};

static AnimationRecord _gM4a1GrenadeAnimation10D68Records[311] = {
#include "assets/m4a1_grenade_animation_10D68_records.inc"
};

static u16 _gM4a1GrenadeAnimation10D68Indices[20] = {
#include "assets/m4a1_grenade_animation_10D68_indices.inc"
};

static AnimationSet _gM4a1GrenadeAnimation10D68 = {
    _gM4a1GrenadeAnimation10D68Records,
    _gM4a1GrenadeAnimation10D68Indices,
    { NULL, _gM4a1GrenadeAnimation10D68Bank1, NULL, NULL, _gM4a1GrenadeAnimation10D68Bank4, NULL, NULL, NULL },
};

AnimationBank D_m4a1_grenade_8012DF50 = { { {
    NULL,
    &_gM4a1GrenadeAnimation01A20,
    &_gM4a1GrenadeAnimation0FBF4,
    &_gM4a1GrenadeAnimation10394,
    &_gM4a1GrenadeAnimation10D68,
    &_gM4a1GrenadeAnimation02924,
    &_gM4a1GrenadeAnimation03188,
    &_gM4a1GrenadeAnimation0EDC4,
    &_gM4a1GrenadeAnimation0F33C,
    &_gM4a1GrenadeAnimation0A204,
    &_gM4a1GrenadeAnimation0C82C,
    &_gM4a1GrenadeAnimation0CF68,
    &_gM4a1GrenadeAnimation0B4EC,
    &_gM4a1GrenadeAnimation0AA44,
    &_gM4a1GrenadeAnimation0C388,
    &_gM4a1GrenadeAnimation0E6B8,
    &_gM4a1GrenadeAnimation05DEC,
    &_gM4a1GrenadeAnimation0610C,
    &_gM4a1GrenadeAnimation068D0,
    &_gM4a1GrenadeAnimation020C4,
    &_gM4a1GrenadeAnimation0C388,
    &_gM4a1GrenadeAnimation01A20,
    &_gM4a1GrenadeAnimation01A20,
    &_gM4a1GrenadeAnimation07B68,
    &_gM4a1GrenadeAnimation08DFC,
    &_gM4a1GrenadeAnimation086E0,
    &_gM4a1GrenadeAnimation04E6C,
    &_gM4a1GrenadeAnimation05068,
    &_gM4a1GrenadeAnimation05340,
    &_gM4a1GrenadeAnimation055E4,
    &_gM4a1GrenadeAnimation057E4,
    &_gM4a1GrenadeAnimation05B38,
    &_gM4a1GrenadeAnimation0927C,
    &_gM4a1GrenadeAnimation09454,
    &_gM4a1GrenadeAnimation0927C,
    &_gM4a1GrenadeAnimation09454,
    &_gM4a1GrenadeAnimation03BF8,
    &_gM4a1GrenadeAnimation04380,
    &_gM4a1GrenadeAnimation049E4,
    &_gM4a1GrenadeAnimation04654,
    &_gM4a1GrenadeAnimation0349C,
    &_gM4a1GrenadeAnimation01A20,
    &_gM4a1GrenadeAnimation099B8,
    &_gM4a1GrenadeAnimation09BAC,
    &_gM4a1GrenadeAnimation0A02C,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
} } };

/// Sits in the middle of this package's trailing data, so it is its own unit:
/// splat lists an object in the linker script at its first subsegment, and
/// this has to link between the two runs of split data around it.
u16 D_m4a1_grenade_8012E08C[4] = { 0x1F4, 0x4B0, 0x7D0, 0 };

static TmdBone _gM4a1GrenadeModel10F7CSkeleton[1] = {
#include "assets/m4a1_grenade_model_10F7C_skeleton.inc"
};

static u32 _gM4a1GrenadeModel10F7CPartVerts[1] = {
#include "assets/m4a1_grenade_model_10F7C_partVerts.inc"
};

static SVECTOR _gM4a1GrenadeModel10F7CVerts[8] = {
#include "assets/m4a1_grenade_model_10F7C_verts.inc"
};

static SVECTOR _gM4a1GrenadeModel10F7CNormals[8] = {
#include "assets/m4a1_grenade_model_10F7C_normals.inc"
};

static u32 _gM4a1GrenadeModel10F7CStream[48] = {
#include "assets/m4a1_grenade_model_10F7C_stream.inc"
};

TmdSource D_m4a1_grenade_8012E1FC = {
    0,
    312,
    0,
    1,
    _gM4a1GrenadeModel10F7CPartVerts,
    _gM4a1GrenadeModel10F7CVerts,
    _gM4a1GrenadeModel10F7CNormals,
    _gM4a1GrenadeModel10F7CSkeleton,
    _gM4a1GrenadeModel10F7CStream,
};
