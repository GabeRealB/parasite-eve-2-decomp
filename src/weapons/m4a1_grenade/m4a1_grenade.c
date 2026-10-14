#include "weapons/m4a1_grenade.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
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
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/weapon.h"

#include "../../shared/grenade_shell.h"
#include "types.h"

static const u16 _gM4a1GrenadeBlastRadii[4];

/// Position offset and temporary aiming transform for an M4A1 grenade launch.
///
/// Projectile initialization reserves one word-aligned block on the scratch
/// stack and releases it on success or allocation failure; no pointer escapes.
/// The offset's fourth halfword is left unset: the GTE loads its containing
/// word but uses only XYZ. The matrix is copied in full from the projectile's
/// transform relative to `gGfxViewCoord`, then pitched -1024 units (4096 per
/// turn). Its Z axis supplies the flight direction without pitching the model.
typedef struct {
    SVECTOR launchOffset;   // Muzzle-local displacement in whole game units: (0, 544, 40)
    MATRIX  launchRotation; // Pitched local transform in the view coordinate's frame; basis uses 4096 per unit
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

/// Requests the M4A1's room-impact effect, sound and combat-noise signal.
///
/// `weaponContacts` must contain all six initialized player weapon-contact
/// entries, including the LAST marker. `playerCoord`'s cached origin and the
/// contact points must share the composed frame. Enemy-body contacts suppress
/// room impacts; otherwise the nearest eligible grid contact is selected.
///
/// `impactCoordOut` must be writable and non-NULL. A selected contact writes
/// only `workm.t`, in game-coordinate units with 0..7 jitter per axis, then
/// requests spatial sound and signals noise even if effect allocation fails.
/// If no contact is selected, the output is unchanged and no sound is requested.
/// The remaining coordinate state is left intact; the caller owns its storage.
/// Requires the live player, room surface tables and initialized scratch/GTE state.
static inline void _m4a1GrenadePlayWeaponImpact(const WorldCollisionContact* weaponContacts,
                                                const GfxCoord* playerCoord, GfxCoord* impactCoordOut)
{
    enum {
        M4A1_GRENADE_ROOM_IMPACT_SOUND   = SOUND_COMMON(0x17),
        M4A1_GRENADE_IMPACT_SIGNAL_NOISE = 1,
    };

    if (playerActorSpawnWeaponImpact(weaponContacts, playerCoord, impactCoordOut) != 0) {
        worldCoordPlaySound(impactCoordOut, M4A1_GRENADE_ROOM_IMPACT_SOUND, M4A1_GRENADE_IMPACT_SIGNAL_NOISE);
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
    weaponLoad  = equipmentGetWeaponLoad(WEAPON_ITEM_ID(gPlayerStatus.weapon));
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
                equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(M4A1_GRENADE_WEAPON_INDEX), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
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
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(M4A1_GRENADE_WEAPON_INDEX), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
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
            if (playerActorIsSlotAdvancingLinearly(playerTask, gPlayerActorAttachmentParentParts[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
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
    _M4a1GrenadeSpawnScratch* scratch;
    SVECTOR*                  launchOffset;
    SVECTOR*                  gteLaunchOffset;
    MATRIX*                   launchRotation;
    TmdObject*                projectileModel;
    GfxCoord*                 projectileCoord;
    GfxCoord*                 muzzleCoord;
    WeaponGrenadeWork*        grenadeWork;

    projectileModel = task->extra.tmd;
    scratch         = SCRATCH_STACK_RESERVE_BLOCK(_M4a1GrenadeSpawnScratch);
    projectileCoord = projectileModel->coords;
    launchOffset    = &scratch->launchOffset;
    muzzleCoord     = projectileCoord->parent;
    grenadeWork     = memCalloc(sizeof(WeaponGrenadeWork), 0);
    gteLaunchOffset = launchOffset;
    if (grenadeWork == NULL) {
        SCRATCH_STACK_RELEASE_BLOCK(_M4a1GrenadeSpawnScratch);
        taskKill(task);
        return;
    }
    task->work         = grenadeWork;
    task->exitCallback = _grenadeShellExit;
    task->state++;
    memFillBytes(grenadeWork, 0, sizeof(*grenadeWork));
    // Place the muzzle-local offset through the muzzle's composed transform.
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
    launchRotation                = &scratch->launchRotation;
    projectileCoord->parent       = &gGfxViewCoord;
    projectileCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    projectileModel->flags        = 0;
    // Aim with a pitched copy so the rendered grenade keeps the muzzle's basis.
    *launchRotation = projectileCoord->coord;
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
/// adds `GRENADE_SHELL_FLIGHT_DIVISOR_STEP` and detonates above 0xFFFFF. Enemy contacts
/// detonate immediately. Capsule grid contacts take precedence over sphere
/// grid contacts; blocking surfaces either detonate or select teardown. Class
/// 1 in the day/night Dryfield Water Tower also detonates on pass-through.
/// Detonation samples the current secondary load (fragmentation if empty),
/// widens the sphere and changes the clock to 8 blast ticks, or 1 for airburst.
/// Every path releases the frame's scratch block. A teardown surface still
/// receives this frame's movement before the next dispatch releases the task.
static void _m4a1GrenadeFlyProjectile(Task* task)
{
    /// Advances translation, capsule reach and the 16.16 flight clock for one tick.
    ///
    /// All arguments must be stable live pointers without side effects; they are
    /// evaluated repeatedly. `grenade` is writable WeaponGrenadeWork*,
    /// `coordNode` its GfxCoord*, and `deltaOut` disjoint WorldCollisionDelta*.
    /// The clock's integer half must be positive; signed division truncates
    /// each Q12 direction component toward zero into a whole-coordinate step.
    /// Only delta XYZ and local translation XYZ are written, leaving pads and
    /// composition stamp intact. `capsuleTimerShift` is the int constant 10:
    /// the negative shifted pre-increment clock sets the capsule's far Y end.
    /// Finally adds `GRENADE_SHELL_FLIGHT_DIVISOR_STEP` to the clock. Captures
    /// no caller locals and retains no pointer; the caller owns cache invalidation.
    /// Expands to a block with no control transfer; invoke only inside braces.
#define M4A1_GRENADE_STEP_PROJECTILE(grenade, coordNode, deltaOut, capsuleTimerShift)              \
    {                                                                                              \
        (deltaOut)->vector.vx         = (grenade)->dir.vx / (grenade)->flightTimer.halves.integer; \
        (deltaOut)->vector.vy         = (grenade)->dir.vy / (grenade)->flightTimer.halves.integer; \
        (deltaOut)->vector.vz         = (grenade)->dir.vz / (grenade)->flightTimer.halves.integer; \
        (coordNode)->coord.t[0]      += (deltaOut)->vector.vx;                                     \
        (coordNode)->coord.t[1]      += (deltaOut)->vector.vy;                                     \
        (coordNode)->coord.t[2]      += (deltaOut)->vector.vz;                                     \
        (grenade)->capsule.ends[1].vy = -((grenade)->flightTimer.word >> (capsuleTimerShift));     \
        (grenade)->flightTimer.word  += GRENADE_SHELL_FLIGHT_DIVISOR_STEP;                         \
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
    weaponLoad                    = equipmentGetWeaponLoad(WEAPON_ITEM_ID(gPlayerStatus.weapon));
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
        grenadeWork->sphereBody.radius = _gM4a1GrenadeBlastRadii[scratch->ammunitionIndex - GRENADE_ROUND_FIRST];
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
    surface = gWorldCollisionSurfacePropertiesTables[gGameSession->location.stage - 1][gGameSession->location.area - 1][surfaceClass];
    if (surface->probePassThrough == WORLD_COLLISION_SURFACE_BLOCK_PROBES) {
        if (surface->weaponImpactEnabled != WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS) {
            goto detonate;
        }
        task->state = M4A1_GRENADE_TASK_STATE_EXIT;
    } else if (surfaceClass == M4A1_GRENADE_WATER_TOWER_SURFACE_CLASS && gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area == GAME_AREA_DRYFIELD_WATER_TOWER && (u32)(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage - GAME_STAGE_DRYFIELD) < 2U) {
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
    M4A1_GRENADE_STEP_PROJECTILE(grenadeWork, projectileCoord, &scratch->delta, M4A1_GRENADE_CAPSULE_TIMER_SHIFT);
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

/// Initial local transform and self-parented root for `gM4a1GrenadeWeaponModel`.
///
/// Object creation copies this pose into each model instance; parent indices select model parts.
static TmdBone _gM4a1GrenadeWeaponModelSkeleton[] = {
#include "assets/m4a1_grenade_model_01134_skeleton.inc"
};

/// Part-local vertex count for `gM4a1GrenadeWeaponModel`.
///
/// One entry per skeleton part; the sole group spans the complete vertex array.
static u32 _gM4a1GrenadeWeaponModelPartVerts[] = {
#include "assets/m4a1_grenade_model_01134_partVerts.inc"
};

/// Integer part-local vertices of `gM4a1GrenadeWeaponModel`.
///
/// Borrowed by model instances for their lifetime; coordinates use the model's integer units.
static SVECTOR _gM4a1GrenadeWeaponModelVerts[] = {
#include "assets/m4a1_grenade_model_01134_verts.inc"
};

/// Lighting normals of `gM4a1GrenadeWeaponModel`, indexed independently of its vertices.
///
/// Borrowed by model instances for their lifetime; components use 4096 per unit.
static SVECTOR _gM4a1GrenadeWeaponModelNormals[] = {
#include "assets/m4a1_grenade_model_01134_normals.inc"
};

/// Writable draw-command words for `gM4a1GrenadeWeaponModel`, through the stream terminator.
///
/// Model creation resolves draw-handler slots in place. This storage must remain
/// writable and loaded until every referring model instance is released.
static u32 _gM4a1GrenadeWeaponModelStream[] = {
#include "assets/m4a1_grenade_model_01134_stream.inc"
};

TmdSource gM4a1GrenadeWeaponModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = 3356,
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gM4a1GrenadeWeaponModelSkeleton),
    .partVertexCounts    = _gM4a1GrenadeWeaponModelPartVerts,
    .verts               = _gM4a1GrenadeWeaponModelVerts,
    .normals             = _gM4a1GrenadeWeaponModelNormals,
    .skeleton            = _gM4a1GrenadeWeaponModelSkeleton,
    .stream              = _gM4a1GrenadeWeaponModelStream,
};

/// Translation/rotation poses (encoding 1): integer coordinates and 1/4096-turn angles.
///
/// Read-only playback data for `_gM4a1GrenadeIdleAnimation`; borrowed while this package is loaded.
static AnimationPackedPose _gM4a1GrenadeIdleAnimationBank1[] = {
#include "assets/m4a1_grenade_animation_01A20_bank1.inc"
};

/// Packed rotation poses (encoding 4): signed 11/10/11-bit angles in eight-unit steps.
///
/// Read-only playback data for `_gM4a1GrenadeIdleAnimation`; borrowed while this package is loaded.
static AnimationPackedRotation _gM4a1GrenadeIdleAnimationBank4[] = {
#include "assets/m4a1_grenade_animation_01A20_bank4.inc"
};

/// Keyframes and track controls; pose offsets count four-byte words.
///
/// Read-only playback data for `_gM4a1GrenadeIdleAnimation`; borrowed while this package is loaded.
static AnimationRecord _gM4a1GrenadeIdleAnimationRecords[] = {
#include "assets/m4a1_grenade_animation_01A20_records.inc"
};

/// Nineteen track-start record indices followed by one retained export halfword.
///
/// Read-only playback data for `_gM4a1GrenadeIdleAnimation`; borrowed while this package is loaded.
static u16 _gM4a1GrenadeIdleAnimationIndices[] = {
#include "assets/m4a1_grenade_animation_01A20_indices.inc"
};

/// Standing idle pose for the M4A1 grenade-launcher player bank.
///
/// Bank slots 1, 21, 22 and 41 share this constant pose.
/// Supplies 19 model-part tracks. Playback reads the clip and its word-aligned
/// pose banks without modifying them; the package must remain loaded while
/// slots or cached record pointers refer to it.
static AnimationSet _gM4a1GrenadeIdleAnimation = {
    .records           = _gM4a1GrenadeIdleAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeIdleAnimationIndices,
    .poseBanks         = { [1] = _gM4a1GrenadeIdleAnimationBank1, [4] = _gM4a1GrenadeIdleAnimationBank4 },
};

/// Translation/rotation poses (encoding 1): integer coordinates and 1/4096-turn angles.
///
/// Read-only playback data for `_gM4a1GrenadeUnarmedWalkAnimation`; borrowed while this package is loaded.
static AnimationPackedPose _gM4a1GrenadeUnarmedWalkAnimationBank1[] = {
#include "assets/m4a1_grenade_animation_020C4_bank1.inc"
};

/// Packed rotation poses (encoding 4): signed 11/10/11-bit angles in eight-unit steps.
///
/// Read-only playback data for `_gM4a1GrenadeUnarmedWalkAnimation`; borrowed while this package is loaded.
static AnimationPackedRotation _gM4a1GrenadeUnarmedWalkAnimationBank4[] = {
#include "assets/m4a1_grenade_animation_020C4_bank4.inc"
};

/// Keyframes and track controls; pose offsets count four-byte words.
///
/// Read-only playback data for `_gM4a1GrenadeUnarmedWalkAnimation`; borrowed while this package is loaded.
static AnimationRecord _gM4a1GrenadeUnarmedWalkAnimationRecords[] = {
#include "assets/m4a1_grenade_animation_020C4_records.inc"
};

/// Nineteen track-start record indices followed by one retained export halfword.
///
/// Read-only playback data for `_gM4a1GrenadeUnarmedWalkAnimation`; borrowed while this package is loaded.
static u16 _gM4a1GrenadeUnarmedWalkAnimationIndices[] = {
#include "assets/m4a1_grenade_animation_020C4_indices.inc"
};

/// Forward walk while the M4A1 grenade-launcher weapon model is absent.
///
/// Bank slot 19 is selected when equipment slot 1 contains no weapon task.
/// Supplies 19 model-part tracks. Playback reads the clip and its word-aligned
/// pose banks without modifying them; the package must remain loaded while
/// slots or cached record pointers refer to it.
static AnimationSet _gM4a1GrenadeUnarmedWalkAnimation = {
    .records           = _gM4a1GrenadeUnarmedWalkAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeUnarmedWalkAnimationIndices,
    .poseBanks         = { [1] = _gM4a1GrenadeUnarmedWalkAnimationBank1, [4] = _gM4a1GrenadeUnarmedWalkAnimationBank4 },
};

/// Translation/rotation poses (encoding 1): integer coordinates and 1/4096-turn angles.
///
/// Read-only playback data for `_gM4a1GrenadeNegativeTurnAnimation`; borrowed while this package is loaded.
static AnimationPackedPose _gM4a1GrenadeNegativeTurnAnimationBank1[] = {
#include "assets/m4a1_grenade_animation_02924_bank1.inc"
};

/// Packed rotation poses (encoding 4): signed 11/10/11-bit angles in eight-unit steps.
///
/// Read-only playback data for `_gM4a1GrenadeNegativeTurnAnimation`; borrowed while this package is loaded.
static AnimationPackedRotation _gM4a1GrenadeNegativeTurnAnimationBank4[] = {
#include "assets/m4a1_grenade_animation_02924_bank4.inc"
};

/// Keyframes and track controls; pose offsets count four-byte words.
///
/// Read-only playback data for `_gM4a1GrenadeNegativeTurnAnimation`; borrowed while this package is loaded.
static AnimationRecord _gM4a1GrenadeNegativeTurnAnimationRecords[] = {
#include "assets/m4a1_grenade_animation_02924_records.inc"
};

/// Nineteen track-start record indices followed by one retained export halfword.
///
/// Read-only playback data for `_gM4a1GrenadeNegativeTurnAnimation`; borrowed while this package is loaded.
static u16 _gM4a1GrenadeNegativeTurnAnimationIndices[] = {
#include "assets/m4a1_grenade_animation_02924_indices.inc"
};

/// Stationary turn with a negative yaw step in the M4A1 grenade-launcher bank.
///
/// Bank slot 5 accompanies negative stationary turning.
/// Supplies 19 model-part tracks. Playback reads the clip and its word-aligned
/// pose banks without modifying them; the package must remain loaded while
/// slots or cached record pointers refer to it.
static AnimationSet _gM4a1GrenadeNegativeTurnAnimation = {
    .records           = _gM4a1GrenadeNegativeTurnAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeNegativeTurnAnimationIndices,
    .poseBanks         = { [1] = _gM4a1GrenadeNegativeTurnAnimationBank1, [4] = _gM4a1GrenadeNegativeTurnAnimationBank4 },
};

/// Translation/rotation poses (encoding 1): integer coordinates and 1/4096-turn angles.
///
/// Read-only playback data for `_gM4a1GrenadePositiveTurnAnimation`; borrowed while this package is loaded.
static AnimationPackedPose _gM4a1GrenadePositiveTurnAnimationBank1[] = {
#include "assets/m4a1_grenade_animation_03188_bank1.inc"
};

/// Packed rotation poses (encoding 4): signed 11/10/11-bit angles in eight-unit steps.
///
/// Read-only playback data for `_gM4a1GrenadePositiveTurnAnimation`; borrowed while this package is loaded.
static AnimationPackedRotation _gM4a1GrenadePositiveTurnAnimationBank4[] = {
#include "assets/m4a1_grenade_animation_03188_bank4.inc"
};

/// Keyframes and track controls; pose offsets count four-byte words.
///
/// Read-only playback data for `_gM4a1GrenadePositiveTurnAnimation`; borrowed while this package is loaded.
static AnimationRecord _gM4a1GrenadePositiveTurnAnimationRecords[] = {
#include "assets/m4a1_grenade_animation_03188_records.inc"
};

/// Nineteen track-start record indices followed by one retained export halfword.
///
/// Read-only playback data for `_gM4a1GrenadePositiveTurnAnimation`; borrowed while this package is loaded.
static u16 _gM4a1GrenadePositiveTurnAnimationIndices[] = {
#include "assets/m4a1_grenade_animation_03188_indices.inc"
};

/// Stationary turn with a positive yaw step in the M4A1 grenade-launcher bank.
///
/// Bank slot 6 accompanies positive stationary turning.
/// Supplies 19 model-part tracks. Playback reads the clip and its word-aligned
/// pose banks without modifying them; the package must remain loaded while
/// slots or cached record pointers refer to it.
static AnimationSet _gM4a1GrenadePositiveTurnAnimation = {
    .records           = _gM4a1GrenadePositiveTurnAnimationRecords,
    .trackStartIndices = _gM4a1GrenadePositiveTurnAnimationIndices,
    .poseBanks         = { [1] = _gM4a1GrenadePositiveTurnAnimationBank1, [4] = _gM4a1GrenadePositiveTurnAnimationBank4 },
};

/// Encoding indices for the pose banks in these M4A1 grenade player-rig clips.
enum {
    M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK      = 4,
};

/// Translation/rotation poses for `_gM4a1GrenadeItemUseAnimation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadeItemUseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0349C_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeItemUseAnimation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeItemUseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0349C_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadeItemUseAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Jump controls use absolute record indices; stop controls hold the last pose.
static const AnimationRecord _gM4a1GrenadeItemUseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0349C_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeItemUseAnimation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword is outside that table.
static const u16 _gM4a1GrenadeItemUseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0349C_indices.inc"
};

/// Player-rig item-use gesture in the M4A1 grenade-launcher bank.
///
/// Bank slot 40 presents an item-use action; it does not apply the item effect.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadeItemUseAnimation = {
    .records           = _gM4a1GrenadeItemUseAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeItemUseAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeItemUseAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeItemUseAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeStairAscentAnimation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadeStairAscentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_03BF8_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeStairAscentAnimation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeStairAscentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_03BF8_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadeStairAscentAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Jump controls use absolute record indices; stop controls hold the last pose.
static const AnimationRecord _gM4a1GrenadeStairAscentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_03BF8_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeStairAscentAnimation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword is outside that table.
static const u16 _gM4a1GrenadeStairAscentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_03BF8_indices.inc"
};

/// Player-rig stair-ascent cycle in the M4A1 grenade-launcher bank.
///
/// Bank slot 36 accompanies a scripted ascending flight. Track 1 carries
/// footstep cues that gate the root displacement.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadeStairAscentAnimation = {
    .records           = _gM4a1GrenadeStairAscentAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeStairAscentAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeStairAscentAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeStairAscentAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeStairDescentAnimation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadeStairDescentAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_04380_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeStairDescentAnimation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeStairDescentAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_04380_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadeStairDescentAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Jump controls use absolute record indices; stop controls hold the last pose.
static const AnimationRecord _gM4a1GrenadeStairDescentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_04380_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeStairDescentAnimation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword is outside that table.
static const u16 _gM4a1GrenadeStairDescentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_04380_indices.inc"
};

/// Player-rig stair-descent cycle in the M4A1 grenade-launcher bank.
///
/// Bank slot 37 accompanies a scripted descending flight. Track 1 carries
/// footstep cues that gate the root displacement.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadeStairDescentAnimation = {
    .records           = _gM4a1GrenadeStairDescentAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeStairDescentAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeStairDescentAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeStairDescentAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeStairAscentFinishOddAnimation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadeStairAscentFinishOddAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_04654_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeStairAscentFinishOddAnimation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeStairAscentFinishOddAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_04654_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadeStairAscentFinishOddAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Jump controls use absolute record indices; stop controls hold the last pose.
static const AnimationRecord _gM4a1GrenadeStairAscentFinishOddAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_04654_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeStairAscentFinishOddAnimation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword is outside that table.
static const u16 _gM4a1GrenadeStairAscentFinishOddAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_04654_indices.inc"
};

/// Player-rig stair-ascent finish for an odd original step count.
///
/// Bank slot 39 ends ascent in the M4A1 grenade-launcher bank; selection uses
/// the flight's original step-count parity, saved before counting steps down.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadeStairAscentFinishOddAnimation = {
    .records           = _gM4a1GrenadeStairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeStairAscentFinishOddAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeStairAscentFinishOddAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeStairAscentFinishOddAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeStairAscentFinishEvenAnimation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadeStairAscentFinishEvenAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_049E4_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeStairAscentFinishEvenAnimation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeStairAscentFinishEvenAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_049E4_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadeStairAscentFinishEvenAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Jump controls use absolute record indices; stop controls hold the last pose.
static const AnimationRecord _gM4a1GrenadeStairAscentFinishEvenAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_049E4_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeStairAscentFinishEvenAnimation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword is outside that table.
static const u16 _gM4a1GrenadeStairAscentFinishEvenAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_049E4_indices.inc"
};

/// Player-rig stair-ascent finish for an even original step count.
///
/// Bank slot 38 ends ascent in the M4A1 grenade-launcher bank; selection uses
/// the flight's original step-count parity, saved before counting steps down.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadeStairAscentFinishEvenAnimation = {
    .records           = _gM4a1GrenadeStairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeStairAscentFinishEvenAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeStairAscentFinishEvenAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeStairAscentFinishEvenAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadePeReleaseFamily0Animation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadePeReleaseFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_04E6C_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadePeReleaseFamily0Animation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadePeReleaseFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_04E6C_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadePeReleaseFamily0Animation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Jump controls use absolute record indices; stop controls hold the last pose.
static const AnimationRecord _gM4a1GrenadePeReleaseFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_04E6C_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadePeReleaseFamily0Animation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword is outside that table.
static const u16 _gM4a1GrenadePeReleaseFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_04E6C_indices.inc"
};

/// Player-rig Parasite Energy family-0 release in the M4A1 grenade-launcher bank.
///
/// Bank slot 26 starts the release/charge/cast sequence when the attachment
/// selects clip family 0. Charge and cast use separate sets.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadePeReleaseFamily0Animation = {
    .records           = _gM4a1GrenadePeReleaseFamily0AnimationRecords,
    .trackStartIndices = _gM4a1GrenadePeReleaseFamily0AnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadePeReleaseFamily0AnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadePeReleaseFamily0AnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadePeChargeFamily0Animation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadePeChargeFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_05068_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadePeChargeFamily0Animation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadePeChargeFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_05068_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadePeChargeFamily0Animation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Each track jumps to its first of two poses; the final stop is retained.
static const AnimationRecord _gM4a1GrenadePeChargeFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_05068_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadePeChargeFamily0Animation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword is outside that table.
static const u16 _gM4a1GrenadePeChargeFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_05068_indices.inc"
};

/// Player-rig Parasite Energy family-0 charge in the M4A1 grenade-launcher bank.
///
/// Bank slot 27 repeats its two-pose tracks while the attachment countdown
/// runs. The family-0 cast set replaces it when that countdown expires.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadePeChargeFamily0Animation = {
    .records           = _gM4a1GrenadePeChargeFamily0AnimationRecords,
    .trackStartIndices = _gM4a1GrenadePeChargeFamily0AnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadePeChargeFamily0AnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadePeChargeFamily0AnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadePeCastFamily0Animation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadePeCastFamily0AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_05340_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadePeCastFamily0Animation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadePeCastFamily0AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_05340_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadePeCastFamily0Animation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Each track ends with a stop control that holds its final pose.
static const AnimationRecord _gM4a1GrenadePeCastFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_05340_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadePeCastFamily0Animation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword is outside that table.
static const u16 _gM4a1GrenadePeCastFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_05340_indices.inc"
};

/// Player-rig Parasite Energy family-0 cast in the M4A1 grenade-launcher bank.
///
/// Bank slot 28 follows the family-0 charge when the attachment countdown
/// expires; completion allows the interrupted locomotion or aim to resume.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadePeCastFamily0Animation = {
    .records           = _gM4a1GrenadePeCastFamily0AnimationRecords,
    .trackStartIndices = _gM4a1GrenadePeCastFamily0AnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadePeCastFamily0AnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadePeCastFamily0AnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadePeReleaseFamily1Animation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadePeReleaseFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_055E4_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadePeReleaseFamily1Animation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadePeReleaseFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_055E4_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadePeReleaseFamily1Animation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Each track ends with a stop control that holds its final pose.
static const AnimationRecord _gM4a1GrenadePeReleaseFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_055E4_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadePeReleaseFamily1Animation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword is outside that table.
static const u16 _gM4a1GrenadePeReleaseFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_055E4_indices.inc"
};

/// Player-rig Parasite Energy family-1 release in the M4A1 grenade-launcher bank.
///
/// Bank slot 29 starts the release/charge/cast sequence when the attachment
/// selects clip family 1. Charge and cast use separate sets.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadePeReleaseFamily1Animation = {
    .records           = _gM4a1GrenadePeReleaseFamily1AnimationRecords,
    .trackStartIndices = _gM4a1GrenadePeReleaseFamily1AnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadePeReleaseFamily1AnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadePeReleaseFamily1AnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadePeChargeFamily1Animation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadePeChargeFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_057E4_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadePeChargeFamily1Animation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadePeChargeFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_057E4_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadePeChargeFamily1Animation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Each track jumps to its first of two poses; the final stop is retained.
static const AnimationRecord _gM4a1GrenadePeChargeFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_057E4_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadePeChargeFamily1Animation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword is outside that table.
static const u16 _gM4a1GrenadePeChargeFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_057E4_indices.inc"
};

/// Player-rig Parasite Energy family-1 charge in the M4A1 grenade-launcher bank.
///
/// Bank slot 30 repeats its two-pose tracks while the attachment countdown
/// runs. The family-1 cast set replaces it when that countdown expires.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadePeChargeFamily1Animation = {
    .records           = _gM4a1GrenadePeChargeFamily1AnimationRecords,
    .trackStartIndices = _gM4a1GrenadePeChargeFamily1AnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadePeChargeFamily1AnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadePeChargeFamily1AnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadePeCastFamily1Animation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadePeCastFamily1AnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_05B38_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadePeCastFamily1Animation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadePeCastFamily1AnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_05B38_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadePeCastFamily1Animation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Each track ends with a stop control that holds its final pose.
static const AnimationRecord _gM4a1GrenadePeCastFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_05B38_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadePeCastFamily1Animation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword is outside that table.
static const u16 _gM4a1GrenadePeCastFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_05B38_indices.inc"
};

/// Player-rig Parasite Energy family-1 cast in the M4A1 grenade-launcher bank.
///
/// Bank slot 31 follows the family-1 charge when the attachment countdown
/// expires; completion allows the interrupted locomotion or aim to resume.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadePeCastFamily1Animation = {
    .records           = _gM4a1GrenadePeCastFamily1AnimationRecords,
    .trackStartIndices = _gM4a1GrenadePeCastFamily1AnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadePeCastFamily1AnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadePeCastFamily1AnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadePart4HitAnimation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadePart4HitAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_05DEC_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadePart4HitAnimation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadePart4HitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_05DEC_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadePart4HitAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Each track ends with a stop control that holds its final pose.
static const AnimationRecord _gM4a1GrenadePart4HitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_05DEC_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadePart4HitAnimation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword is outside that table.
static const u16 _gM4a1GrenadePart4HitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_05DEC_indices.inc"
};

/// Player-rig hit reaction for the part-4 collision body in the M4A1 grenade-launcher bank.
///
/// Bank slot 16 is selected for hit region 1, the sphere attached to model
/// part 4. Damage playback blends child tracks 1..18 over three normal-rate
/// frames, leaving the root track unchanged.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadePart4HitAnimation = {
    .records           = _gM4a1GrenadePart4HitAnimationRecords,
    .trackStartIndices = _gM4a1GrenadePart4HitAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadePart4HitAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadePart4HitAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeOtherBodyHitAnimation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadeOtherBodyHitAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0610C_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeOtherBodyHitAnimation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeOtherBodyHitAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0610C_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadeOtherBodyHitAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Each track ends with a stop control that holds its final pose.
static const AnimationRecord _gM4a1GrenadeOtherBodyHitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0610C_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeOtherBodyHitAnimation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadeOtherBodyHitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0610C_indices.inc"
};

/// Player-rig hit reaction for other body regions in the M4A1 grenade-launcher bank.
///
/// Bank slot 17 is selected for hits outside region 1's part-4 collision body.
/// Damage playback blends child tracks 1..18 over three normal-rate frames,
/// leaving the root track unchanged.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadeOtherBodyHitAnimation = {
    .records           = _gM4a1GrenadeOtherBodyHitAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeOtherBodyHitAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeOtherBodyHitAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeOtherBodyHitAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeDeathPoseAnimation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadeDeathPoseAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_068D0_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeDeathPoseAnimation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeDeathPoseAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_068D0_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadeDeathPoseAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Each track ends with a stop control that holds its final pose.
static const AnimationRecord _gM4a1GrenadeDeathPoseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_068D0_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeDeathPoseAnimation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadeDeathPoseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_068D0_indices.inc"
};

/// Player-rig death pose in the M4A1 grenade-launcher bank.
///
/// Bank slot 18 accompanies stopped movement and turning after HP reaches zero.
/// Child tracks 1..18 enter the clip directly or with the requested blend;
/// each track holds its final pose after its stop control.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadeDeathPoseAnimation = {
    .records           = _gM4a1GrenadeDeathPoseAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeDeathPoseAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeDeathPoseAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeDeathPoseAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeHighHealthIdleAnimation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadeHighHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_07B68_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeHighHealthIdleAnimation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeHighHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_07B68_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadeHighHealthIdleAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Each track jumps back into its repeating segment before a retained stop control.
static const AnimationRecord _gM4a1GrenadeHighHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_07B68_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeHighHealthIdleAnimation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadeHighHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_07B68_indices.inc"
};

/// Player-rig long idle above half maximum HP in the M4A1 grenade-launcher bank.
///
/// Bank slot 23 is selected after 300 direction-free locomotion ticks when
/// HP exceeds the integer half of maximum HP. Child tracks 1..18 blend over
/// five normal-rate frames and repeat within their tracks.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadeHighHealthIdleAnimation = {
    .records           = _gM4a1GrenadeHighHealthIdleAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeHighHealthIdleAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeHighHealthIdleAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeHighHealthIdleAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeLowHealthIdleAnimation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadeLowHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_086E0_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeLowHealthIdleAnimation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeLowHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_086E0_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadeLowHealthIdleAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Each track jumps back into its repeating segment before a retained stop control.
static const AnimationRecord _gM4a1GrenadeLowHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_086E0_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeLowHealthIdleAnimation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadeLowHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_086E0_indices.inc"
};

/// Player-rig low-HP idle and paralysis clip in the M4A1 grenade-launcher bank.
///
/// Bank slot 25 supplies the repeating long idle at or below the integer
/// quarter of maximum HP, selected after 300 direction-free locomotion ticks
/// with a five-frame child-track blend. Paralysis reuses it with a six-frame
/// blend across the same child tracks 1..18.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadeLowHealthIdleAnimation = {
    .records           = _gM4a1GrenadeLowHealthIdleAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeLowHealthIdleAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeLowHealthIdleAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeLowHealthIdleAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeMidHealthIdleAnimation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadeMidHealthIdleAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_08DFC_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeMidHealthIdleAnimation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeMidHealthIdleAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_08DFC_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadeMidHealthIdleAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Each track jumps back into its repeating segment before a retained stop control.
static const AnimationRecord _gM4a1GrenadeMidHealthIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_08DFC_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeMidHealthIdleAnimation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadeMidHealthIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_08DFC_indices.inc"
};

/// Player-rig long idle between quarter and half HP in the M4A1 grenade-launcher bank.
///
/// Bank slot 24 is selected after 300 direction-free locomotion ticks when
/// HP exceeds the integer quarter and is at most the integer half of maximum HP.
/// Child tracks 1..18 blend over five normal-rate frames and repeat within their tracks.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadeMidHealthIdleAnimation = {
    .records           = _gM4a1GrenadeMidHealthIdleAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeMidHealthIdleAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeMidHealthIdleAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeMidHealthIdleAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeScriptedPresentationEntryAnimation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadeScriptedPresentationEntryAnimationBank1[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0927C_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeScriptedPresentationEntryAnimation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeScriptedPresentationEntryAnimationBank4[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0927C_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadeScriptedPresentationEntryAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Each track ends with a stop control that holds its final pose.
static const AnimationRecord _gM4a1GrenadeScriptedPresentationEntryAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0927C_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeScriptedPresentationEntryAnimation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadeScriptedPresentationEntryAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0927C_indices.inc"
};

/// Player-rig scripted-presentation entry clip in the M4A1 grenade-launcher bank.
///
/// Bank slot 32 plays when the scripted presentation selector is zero,
/// including before a weapon CAP event. Slot 34 aliases the same clip.
/// Child tracks 1..18 enter without a timed blend and hold their final poses;
/// the visual motion represented by this entry clip is unproven.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadeScriptedPresentationEntryAnimation = {
    .records           = _gM4a1GrenadeScriptedPresentationEntryAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeScriptedPresentationEntryAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeScriptedPresentationEntryAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeScriptedPresentationEntryAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeScriptedPresentationExitAnimation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadeScriptedPresentationExitAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_09454_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeScriptedPresentationExitAnimation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeScriptedPresentationExitAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_09454_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadeScriptedPresentationExitAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Each track ends with a stop control that holds its final pose.
static const AnimationRecord _gM4a1GrenadeScriptedPresentationExitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_09454_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeScriptedPresentationExitAnimation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadeScriptedPresentationExitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_09454_indices.inc"
};

/// Player-rig scripted-presentation exit clip in the M4A1 grenade-launcher bank.
///
/// Bank slot 33 plays for a nonzero scripted presentation selector,
/// including after a weapon CAP event; slot 35 aliases the same clip.
/// Child tracks 1..18 enter without a timed blend and hold their final poses.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadeScriptedPresentationExitAnimation = {
    .records           = _gM4a1GrenadeScriptedPresentationExitAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeScriptedPresentationExitAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeScriptedPresentationExitAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeScriptedPresentationExitAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadePeReleaseFamily2Animation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadePeReleaseFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_099B8_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadePeReleaseFamily2Animation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadePeReleaseFamily2AnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_099B8_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadePeReleaseFamily2Animation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Each track ends with a stop control that holds its final pose.
static const AnimationRecord _gM4a1GrenadePeReleaseFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_099B8_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadePeReleaseFamily2Animation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadePeReleaseFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_099B8_indices.inc"
};

/// Player-rig Parasite Energy release clip for family 2 in the M4A1 grenade-launcher bank.
///
/// Bank slot 42 starts the release phase for clip family 2, selected by
/// spell attachment IDs whose decimal tens digit is 3.
/// Child tracks 1..18 blend over six normal-rate frames and stop before charging.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadePeReleaseFamily2Animation = {
    .records           = _gM4a1GrenadePeReleaseFamily2AnimationRecords,
    .trackStartIndices = _gM4a1GrenadePeReleaseFamily2AnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadePeReleaseFamily2AnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadePeReleaseFamily2AnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadePeChargeFamily2Animation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadePeChargeFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_09BAC_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadePeChargeFamily2Animation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadePeChargeFamily2AnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_09BAC_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadePeChargeFamily2Animation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Each track jumps back to its first keyframe before its retained stop control.
static const AnimationRecord _gM4a1GrenadePeChargeFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_09BAC_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadePeChargeFamily2Animation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadePeChargeFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_09BAC_indices.inc"
};

/// Player-rig Parasite Energy charge loop for family 2 in the M4A1 grenade-launcher bank.
///
/// Bank slot 43 restarts child tracks 1..18 after the family-2 release.
/// Each track repeats two keyframes while the attachment countdown runs;
/// a zero countdown replaces this loop with the family-2 cast clip.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadePeChargeFamily2Animation = {
    .records           = _gM4a1GrenadePeChargeFamily2AnimationRecords,
    .trackStartIndices = _gM4a1GrenadePeChargeFamily2AnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadePeChargeFamily2AnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadePeChargeFamily2AnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadePeCastFamily2Animation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadePeCastFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0A02C_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadePeCastFamily2Animation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadePeCastFamily2AnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0A02C_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadePeCastFamily2Animation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Each track ends with a stop control that holds its final pose.
static const AnimationRecord _gM4a1GrenadePeCastFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0A02C_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadePeCastFamily2Animation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadePeCastFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0A02C_indices.inc"
};

/// Player-rig Parasite Energy cast clip for family 2 in the M4A1 grenade-launcher bank.
///
/// Bank slot 44 restarts child tracks 1..18 when the family-2 attachment
/// countdown reaches zero. Tracks stop at their final poses before normal
/// control resumes; clip family 2 belongs to spell IDs with decimal tens digit 3.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadePeCastFamily2Animation = {
    .records           = _gM4a1GrenadePeCastFamily2AnimationRecords,
    .trackStartIndices = _gM4a1GrenadePeCastFamily2AnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadePeCastFamily2AnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadePeCastFamily2AnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeAimHoldAnimation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadeAimHoldAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0A204_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeAimHoldAnimation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeAimHoldAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0A204_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadeAimHoldAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Each track repeats its two identical keyframes through an absolute jump.
/// A stop control remains after the jump and is not reached by ordinary forward playback.
static const AnimationRecord _gM4a1GrenadeAimHoldAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0A204_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeAimHoldAnimation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadeAimHoldAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0A204_indices.inc"
};

/// Player-rig stationary aiming pose in the M4A1 grenade-launcher bank.
///
/// Bank slot 9 holds aim without movement or turning and prepares a weapon attack.
/// Each track repeats two identical keyframes, preserving a fixed pose.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadeAimHoldAnimation = {
    .records           = _gM4a1GrenadeAimHoldAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeAimHoldAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeAimHoldAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeAimHoldAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeAimBackwardOrTurnAnimation`.
///
/// Encoding 1 stores integer model coordinates and angles in 1/4096 turns.
/// Each pose occupies three four-byte words; playback only reads this bank.
static const AnimationPackedPose _gM4a1GrenadeAimBackwardOrTurnAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0AA44_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeAimBackwardOrTurnAnimation`.
///
/// Encoding 4 stores signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeAimBackwardOrTurnAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0AA44_bank4.inc"
};

/// Keyframes and track controls for `_gM4a1GrenadeAimBackwardOrTurnAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Root track 0 jumps to its first keyframe; child tracks repeat from interior keyframes.
/// Track 1's ANIMATION_RECORD_CUE_1 and ANIMATION_RECORD_CUE_2 mark paired footsteps.
/// Each track retains a stop control after its loop jump.
static const AnimationRecord _gM4a1GrenadeAimBackwardOrTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0AA44_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeAimBackwardOrTurnAnimation`.
///
/// Nineteen model-part tracks followed by one retained export halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadeAimBackwardOrTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0AA44_indices.inc"
};

/// Player-rig backward or turning aim loop in the M4A1 grenade-launcher bank.
///
/// Bank slot 13 supplies child tracks 1..18 when aiming backward or turning
/// without displacement, including after reload cancellation.
/// Child tracks loop after their opening poses; track 1 carries paired footstep cues.
/// Supplies 19 model-part tracks. The descriptor and its word-aligned,
/// read-only payloads are borrowed: keep the package loaded while playback
/// slots or cached record pointers refer to them.
static AnimationSet _gM4a1GrenadeAimBackwardOrTurnAnimation = {
    .records           = _gM4a1GrenadeAimBackwardOrTurnAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeAimBackwardOrTurnAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeAimBackwardOrTurnAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeAimBackwardOrTurnAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeAimForwardAnimation`.
///
/// Tracks 0 and 1 use encoding 1: integer model coordinates and angles in
/// 1/4096 turns. Each pose occupies three four-byte words, read only by playback.
static const AnimationPackedPose _gM4a1GrenadeAimForwardAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0B4EC_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeAimForwardAnimation`.
///
/// Tracks 2..18 use encoding 4: signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeAimForwardAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0B4EC_bank4.inc"
};

/// Keyframes and loop controls for `_gM4a1GrenadeAimForwardAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Root track 0 jumps to its first keyframe; child tracks loop from interior keyframes.
/// Track 1's separate ANIMATION_RECORD_CUE_1 and ANIMATION_RECORD_CUE_2 mark footsteps.
/// Each track retains a stop control after its loop jump.
static const AnimationRecord _gM4a1GrenadeAimForwardAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0B4EC_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeAimForwardAnimation`.
///
/// Nineteen model-part tracks followed by one retained zero halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadeAimForwardAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0B4EC_indices.inc"
};

/// Player-rig forward aiming loop in the M4A1 grenade-launcher bank.
///
/// Bank slot 12 supplies child tracks 1..18 while moving forward in aim,
/// including after a Parasite Energy action. Track 1 carries paired footstep cues.
/// Supplies 19 model-part tracks. Playback borrows the descriptor and its
/// read-only, word-aligned pose banks; keep the package loaded while any slot
/// or cached record pointer refers to the clip.
static AnimationSet _gM4a1GrenadeAimForwardAnimation = {
    .records           = _gM4a1GrenadeAimForwardAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeAimForwardAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeAimForwardAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeAimForwardAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeRifleReloadAnimation`.
///
/// Tracks 0 and 1 use encoding 1: integer model coordinates and angles in
/// 1/4096 turns. Each pose occupies three four-byte words, read only by playback.
static const AnimationPackedPose _gM4a1GrenadeRifleReloadAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0C388_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeRifleReloadAnimation`.
///
/// Tracks 2..18 use encoding 4: signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeRifleReloadAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0C388_bank4.inc"
};

/// Keyframes and end controls for `_gM4a1GrenadeRifleReloadAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Tracks stop at their final poses. Track 1 has two ANIMATION_RECORD_CUE_MASK
/// keyframes for reload sounds; the second completes the primary load.
static const AnimationRecord _gM4a1GrenadeRifleReloadAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0C388_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeRifleReloadAnimation`.
///
/// Nineteen model-part tracks followed by one retained zero halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadeRifleReloadAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0C388_indices.inc"
};

/// Player-rig rifle reload clip in the M4A1 grenade-launcher bank.
///
/// Bank slot 14 reloads the primary load; slot 20 reuses this clip at battle end.
/// Two full cues on track 1 time loading sounds, with ammunition applied at the second.
/// Supplies 19 model-part tracks. Playback borrows the descriptor and its
/// read-only, word-aligned pose banks; keep the package loaded while any slot
/// or cached record pointer refers to the clip.
static AnimationSet _gM4a1GrenadeRifleReloadAnimation = {
    .records           = _gM4a1GrenadeRifleReloadAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeRifleReloadAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeRifleReloadAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeRifleReloadAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeRifleFireAnimation`.
///
/// Tracks 0 and 1 use encoding 1: integer model coordinates and angles in
/// 1/4096 turns. Each pose occupies three four-byte words, read only by playback.
static const AnimationPackedPose _gM4a1GrenadeRifleFireAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0C82C_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeRifleFireAnimation`.
///
/// Tracks 2..18 use encoding 4: signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeRifleFireAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0C82C_bank4.inc"
};

/// Keyframes and end controls for `_gM4a1GrenadeRifleFireAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Tracks stop at their final poses. No keyframe carries a cue bit; shot sounds
/// and effects are timed by the attack state rather than by the clip.
static const AnimationRecord _gM4a1GrenadeRifleFireAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0C82C_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeRifleFireAnimation`.
///
/// Nineteen model-part tracks followed by one retained zero halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadeRifleFireAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0C82C_indices.inc"
};

/// Player-rig rifle-shot clip in the M4A1 grenade-launcher bank.
///
/// Bank slot 10 restarts child tracks 1..18 for each primary-fire round,
/// blending over two normal-rate frames. The attack may fire up to three rounds.
/// Supplies 19 model-part tracks. Playback borrows the descriptor and its
/// read-only, word-aligned pose banks; keep the package loaded while any slot
/// or cached record pointer refers to the clip.
static AnimationSet _gM4a1GrenadeRifleFireAnimation = {
    .records           = _gM4a1GrenadeRifleFireAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeRifleFireAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeRifleFireAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeRifleFireAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeLaunchAnimation`.
///
/// Tracks 0 and 1 use encoding 1: integer model coordinates and angles in
/// 1/4096 turns. Each pose occupies three four-byte words, read only by playback.
static const AnimationPackedPose _gM4a1GrenadeLaunchAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0CF68_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeLaunchAnimation`.
///
/// Tracks 2..18 use encoding 4: signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeLaunchAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0CF68_bank4.inc"
};

/// Keyframes and end controls for `_gM4a1GrenadeLaunchAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Tracks stop at their final poses. Track 1 has two ANIMATION_RECORD_CUE_MASK
/// keyframes that time weapon sounds during recovery after the launch.
static const AnimationRecord _gM4a1GrenadeLaunchAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0CF68_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeLaunchAnimation`.
///
/// Nineteen model-part tracks followed by one retained zero halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadeLaunchAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0CF68_indices.inc"
};

/// Player-rig grenade-launch clip in the M4A1 grenade-launcher bank.
///
/// Bank slot 11 accompanies secondary fire, blending child tracks 1..18 over
/// three normal-rate frames. Its two full cues time the recovery sounds;
/// projectile creation, the launch sound and the muzzle effect precede playback.
/// Supplies 19 model-part tracks. Playback borrows the descriptor and its
/// read-only, word-aligned pose banks; keep the package loaded while any slot
/// or cached record pointer refers to the clip.
static AnimationSet _gM4a1GrenadeLaunchAnimation = {
    .records           = _gM4a1GrenadeLaunchAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeLaunchAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeLaunchAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeLaunchAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeLauncherReloadAnimation`.
///
/// Tracks 0 and 1 use encoding 1: integer model coordinates and angles in
/// 1/4096 turns. Each pose occupies three four-byte words, read only by playback.
static const AnimationPackedPose _gM4a1GrenadeLauncherReloadAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0E6B8_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeLauncherReloadAnimation`.
///
/// Tracks 2..18 use encoding 4: signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeLauncherReloadAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0E6B8_bank4.inc"
};

/// Keyframes and end controls for `_gM4a1GrenadeLauncherReloadAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Tracks stop at their final poses. Track 1 has three ANIMATION_RECORD_CUE_MASK
/// keyframes: two loading cues, then a sound cue after the load is complete.
static const AnimationRecord _gM4a1GrenadeLauncherReloadAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0E6B8_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeLauncherReloadAnimation`.
///
/// Nineteen model-part tracks followed by one retained zero halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadeLauncherReloadAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0E6B8_indices.inc"
};

/// Player-rig grenade reload clip in the M4A1 grenade-launcher bank.
///
/// Bank slot 15 reloads the secondary load. Track 1's three full cues time the
/// weapon sounds, with the grenade load applied at the second cue and a final
/// sound at the third. Child tracks 1..18 blend over three normal-rate frames.
/// Supplies 19 model-part tracks. Playback borrows the descriptor and its
/// read-only, word-aligned pose banks; keep the package loaded while any slot
/// or cached record pointer refers to the clip.
static AnimationSet _gM4a1GrenadeLauncherReloadAnimation = {
    .records           = _gM4a1GrenadeLauncherReloadAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeLauncherReloadAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeLauncherReloadAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeLauncherReloadAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeAimEntryAnimation`.
///
/// Tracks 0 and 1 use encoding 1: integer model coordinates and angles in
/// 1/4096 turns. Each pose occupies three four-byte words, read only by playback.
static const AnimationPackedPose _gM4a1GrenadeAimEntryAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0EDC4_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeAimEntryAnimation`.
///
/// Tracks 2..18 use encoding 4: signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeAimEntryAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0EDC4_bank4.inc"
};

/// Keyframes and end controls for `_gM4a1GrenadeAimEntryAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Tracks stop at their final poses and carry no cue bits.
static const AnimationRecord _gM4a1GrenadeAimEntryAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0EDC4_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeAimEntryAnimation`.
///
/// Nineteen model-part tracks followed by one retained zero halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadeAimEntryAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0EDC4_indices.inc"
};

/// Player-rig aim-entry clip in the M4A1 grenade-launcher bank.
///
/// Bank slot 7 brings child tracks 1..18 into aim. Enemy-controlled player
/// reactions also borrow this set to recover into the equipped weapon's aim pose.
/// Supplies 19 model-part tracks. Playback borrows the descriptor and its
/// read-only, word-aligned pose banks; keep the package loaded while any slot
/// or cached record pointer refers to the clip, including through another set table.
static AnimationSet _gM4a1GrenadeAimEntryAnimation = {
    .records           = _gM4a1GrenadeAimEntryAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeAimEntryAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeAimEntryAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeAimEntryAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeAimExitAnimation`.
///
/// Tracks 0 and 1 use encoding 1: integer model coordinates and angles in
/// 1/4096 turns. Each pose occupies three four-byte words, read only by playback.
static const AnimationPackedPose _gM4a1GrenadeAimExitAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0F33C_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeAimExitAnimation`.
///
/// Tracks 2..18 use encoding 4: signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeAimExitAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0F33C_bank4.inc"
};

/// Keyframes and end controls for `_gM4a1GrenadeAimExitAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Tracks stop at their final poses. No keyframe carries a cue bit.
static const AnimationRecord _gM4a1GrenadeAimExitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0F33C_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeAimExitAnimation`.
///
/// Nineteen model-part tracks followed by one retained zero halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadeAimExitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0F33C_indices.inc"
};

/// Player-rig aim-exit clip in the M4A1 grenade-launcher bank.
///
/// Bank slot 8 lowers child tracks 1..18 out of aim over six normal-rate
/// blend frames. Its tracks stop at their final poses and carry no cue bits.
/// Supplies 19 model-part tracks. Playback borrows the descriptor and its
/// read-only, word-aligned pose banks; keep the package loaded while any slot
/// or cached record pointer refers to the clip.
static AnimationSet _gM4a1GrenadeAimExitAnimation = {
    .records           = _gM4a1GrenadeAimExitAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeAimExitAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeAimExitAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeAimExitAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeWalkAnimation`.
///
/// Tracks 0 and 1 use encoding 1: integer model coordinates and angles in
/// 1/4096 turns. Each pose occupies three four-byte words, read only by playback.
static const AnimationPackedPose _gM4a1GrenadeWalkAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0FBF4_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeWalkAnimation`.
///
/// Tracks 2..18 use encoding 4: signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeWalkAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_0FBF4_bank4.inc"
};

/// Keyframes and end controls for `_gM4a1GrenadeWalkAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Tracks stop at their final poses. Track 1 carries one cue of each footstep kind.
static const AnimationRecord _gM4a1GrenadeWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0FBF4_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeWalkAnimation`.
///
/// Nineteen model-part tracks followed by one retained zero halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadeWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_0FBF4_indices.inc"
};

/// Player-rig forward-walk clip in the M4A1 grenade-launcher bank.
///
/// Bank slot 2 supplies forward walking while the held weapon model is present.
/// Track 1 carries paired footstep cues; the tracks stop at their final poses.
/// Supplies 19 model-part tracks. Playback borrows the descriptor and its
/// read-only, word-aligned pose banks; keep the package loaded while any slot
/// or cached record pointer refers to the clip.
static AnimationSet _gM4a1GrenadeWalkAnimation = {
    .records           = _gM4a1GrenadeWalkAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeWalkAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeWalkAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeWalkAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeBackwardAnimation`.
///
/// Tracks 0 and 1 use encoding 1: integer model coordinates and angles in
/// 1/4096 turns. Each pose occupies three four-byte words, read only by playback.
static const AnimationPackedPose _gM4a1GrenadeBackwardAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_10394_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeBackwardAnimation`.
///
/// Tracks 2..18 use encoding 4: signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeBackwardAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_10394_bank4.inc"
};

/// Keyframes and end controls for `_gM4a1GrenadeBackwardAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Tracks stop at their final poses. Track 1 carries one cue of each footstep kind.
static const AnimationRecord _gM4a1GrenadeBackwardAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_10394_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeBackwardAnimation`.
///
/// Nineteen model-part tracks followed by one retained zero halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadeBackwardAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_10394_indices.inc"
};

/// Player-rig backward-movement clip in the M4A1 grenade-launcher bank.
///
/// Bank slot 3 supplies backward movement outside aim. Track 1 carries paired
/// footstep cues; the tracks stop at their final poses.
/// Supplies 19 model-part tracks. Playback borrows the descriptor and its
/// read-only, word-aligned pose banks; keep the package loaded while any slot
/// or cached record pointer refers to the clip.
static AnimationSet _gM4a1GrenadeBackwardAnimation = {
    .records           = _gM4a1GrenadeBackwardAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeBackwardAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeBackwardAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeBackwardAnimationBank4,
    },
};

/// Translation/rotation poses for `_gM4a1GrenadeRunAnimation`.
///
/// Tracks 0 and 1 use encoding 1: integer model coordinates and angles in
/// 1/4096 turns. Each pose occupies three four-byte words, read only by playback.
static const AnimationPackedPose _gM4a1GrenadeRunAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_10D68_bank1.inc"
};

/// Packed rotation poses for `_gM4a1GrenadeRunAnimation`.
///
/// Tracks 2..18 use encoding 4: signed 11/10/11-bit angles in eight-unit steps.
/// Each pose occupies one four-byte word and preserves the part's translation.
static const AnimationPackedRotation _gM4a1GrenadeRunAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/m4a1_grenade_animation_10D68_bank4.inc"
};

/// Keyframes and end controls for `_gM4a1GrenadeRunAnimation`.
///
/// Durations count normal-rate frames; pose offsets count four-byte words.
/// Tracks stop at their final poses. Track 1 carries one cue of each footstep kind.
static const AnimationRecord _gM4a1GrenadeRunAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_10D68_records.inc"
};

/// Track-start record indices for `_gM4a1GrenadeRunAnimation`.
///
/// Nineteen model-part tracks followed by one retained zero halfword.
/// Playback indexes tracks 0..18; the final halfword's role is unproven.
static const u16 _gM4a1GrenadeRunAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/m4a1_grenade_animation_10D68_indices.inc"
};

/// Player-rig run clip in the M4A1 grenade-launcher bank.
///
/// Bank slot 4 supplies forward running and the default scripted run-to clip.
/// Track 1 carries paired footstep cues; the tracks stop at their final poses.
/// Supplies 19 model-part tracks. Playback borrows the descriptor and its
/// read-only, word-aligned pose banks; keep the package loaded while any slot
/// or cached record pointer refers to the clip.
static AnimationSet _gM4a1GrenadeRunAnimation = {
    .records           = _gM4a1GrenadeRunAnimationRecords,
    .trackStartIndices = _gM4a1GrenadeRunAnimationIndices,
    .poseBanks         = {
        [M4A1_GRENADE_ANIMATION_TRANSLATION_ROTATION_BANK] = _gM4a1GrenadeRunAnimationBank1,
        [M4A1_GRENADE_ANIMATION_PACKED_ROTATION_BANK]      = _gM4a1GrenadeRunAnimationBank4,
    },
};

AnimationBank gM4a1GrenadePlayerAnimationBank = {
    .table = {
        .sets = {
            [1]  = &_gM4a1GrenadeIdleAnimation,
            [2]  = &_gM4a1GrenadeWalkAnimation,
            [3]  = &_gM4a1GrenadeBackwardAnimation,
            [4]  = &_gM4a1GrenadeRunAnimation,
            [5]  = &_gM4a1GrenadeNegativeTurnAnimation,
            [6]  = &_gM4a1GrenadePositiveTurnAnimation,
            [7]  = &_gM4a1GrenadeAimEntryAnimation,
            [8]  = &_gM4a1GrenadeAimExitAnimation,
            [9]  = &_gM4a1GrenadeAimHoldAnimation,
            [10] = &_gM4a1GrenadeRifleFireAnimation,
            [11] = &_gM4a1GrenadeLaunchAnimation,
            [12] = &_gM4a1GrenadeAimForwardAnimation,
            [13] = &_gM4a1GrenadeAimBackwardOrTurnAnimation,
            [14] = &_gM4a1GrenadeRifleReloadAnimation,
            [15] = &_gM4a1GrenadeLauncherReloadAnimation,
            [16] = &_gM4a1GrenadePart4HitAnimation,
            [17] = &_gM4a1GrenadeOtherBodyHitAnimation,
            [18] = &_gM4a1GrenadeDeathPoseAnimation,
            [19] = &_gM4a1GrenadeUnarmedWalkAnimation,
            [20] = &_gM4a1GrenadeRifleReloadAnimation,
            [21] = &_gM4a1GrenadeIdleAnimation,
            [22] = &_gM4a1GrenadeIdleAnimation,
            [23] = &_gM4a1GrenadeHighHealthIdleAnimation,
            [24] = &_gM4a1GrenadeMidHealthIdleAnimation,
            [25] = &_gM4a1GrenadeLowHealthIdleAnimation,
            [26] = &_gM4a1GrenadePeReleaseFamily0Animation,
            [27] = &_gM4a1GrenadePeChargeFamily0Animation,
            [28] = &_gM4a1GrenadePeCastFamily0Animation,
            [29] = &_gM4a1GrenadePeReleaseFamily1Animation,
            [30] = &_gM4a1GrenadePeChargeFamily1Animation,
            [31] = &_gM4a1GrenadePeCastFamily1Animation,
            [32] = &_gM4a1GrenadeScriptedPresentationEntryAnimation,
            [33] = &_gM4a1GrenadeScriptedPresentationExitAnimation,
            [34] = &_gM4a1GrenadeScriptedPresentationEntryAnimation,
            [35] = &_gM4a1GrenadeScriptedPresentationExitAnimation,
            [36] = &_gM4a1GrenadeStairAscentAnimation,
            [37] = &_gM4a1GrenadeStairDescentAnimation,
            [38] = &_gM4a1GrenadeStairAscentFinishEvenAnimation,
            [39] = &_gM4a1GrenadeStairAscentFinishOddAnimation,
            [40] = &_gM4a1GrenadeItemUseAnimation,
            [41] = &_gM4a1GrenadeIdleAnimation,
            [42] = &_gM4a1GrenadePeReleaseFamily2Animation,
            [43] = &_gM4a1GrenadePeChargeFamily2Animation,
            [44] = &_gM4a1GrenadePeCastFamily2Animation,
        },
    },
};

/// Blast-sphere radii in game-coordinate units for the three grenade rounds.
///
/// Indexed by the secondary load's round at detonation minus `GRENADE_ROUND_FIRST`:
/// fragmentation 500, airburst 1200 and riot 2000. An empty load uses fragmentation.
/// The fourth stored halfword is zero; its role is unproven and no supported
/// round selects it. Keep the stored span in its original trailing-data position.
static const u16 _gM4a1GrenadeBlastRadii[4] __attribute__((section(".data"))) = {
    [GRENADE_ROUND_FRAGMENTATION - GRENADE_ROUND_FIRST] = 500,
    [GRENADE_ROUND_AIRBURST -
        GRENADE_ROUND_FIRST]                            = 1200,
    [GRENADE_ROUND_RIOT -
        GRENADE_ROUND_FIRST]                            = 2000,
    0,
};

/// Initial local transform and self-parented root for `gM4a1GrenadeProjectileModel`.
///
/// Object creation copies this pose into each model instance; parent indices select model parts.
static TmdBone _gM4a1GrenadeProjectileModelSkeleton[] = {
#include "assets/m4a1_grenade_model_10F7C_skeleton.inc"
};

/// Part-local vertex count for `gM4a1GrenadeProjectileModel`.
///
/// One entry per skeleton part; the sole group spans the complete vertex array.
static u32 _gM4a1GrenadeProjectileModelPartVerts[] = {
#include "assets/m4a1_grenade_model_10F7C_partVerts.inc"
};

/// Integer part-local vertices of `gM4a1GrenadeProjectileModel`.
///
/// Borrowed by model instances for their lifetime; coordinates use the model's integer units.
static SVECTOR _gM4a1GrenadeProjectileModelVerts[] = {
#include "assets/m4a1_grenade_model_10F7C_verts.inc"
};

/// Lighting normals of `gM4a1GrenadeProjectileModel`, indexed independently of its vertices.
///
/// Borrowed by model instances for their lifetime; components use 4096 per unit.
static SVECTOR _gM4a1GrenadeProjectileModelNormals[] = {
#include "assets/m4a1_grenade_model_10F7C_normals.inc"
};

/// Writable draw-command words for `gM4a1GrenadeProjectileModel`, through the stream terminator.
///
/// Model creation resolves draw-handler slots in place. This storage must remain
/// writable and loaded until every referring model instance is released.
static u32 _gM4a1GrenadeProjectileModelStream[] = {
#include "assets/m4a1_grenade_model_10F7C_stream.inc"
};

TmdSource gM4a1GrenadeProjectileModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = 312,
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gM4a1GrenadeProjectileModelSkeleton),
    .partVertexCounts    = _gM4a1GrenadeProjectileModelPartVerts,
    .verts               = _gM4a1GrenadeProjectileModelVerts,
    .normals             = _gM4a1GrenadeProjectileModelNormals,
    .skeleton            = _gM4a1GrenadeProjectileModelSkeleton,
    .stream              = _gM4a1GrenadeProjectileModelStream,
};
