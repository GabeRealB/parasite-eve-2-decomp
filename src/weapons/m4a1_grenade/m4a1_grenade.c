#include "weapons/m4a1_grenade.h"

#include "config.h"

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
#include "weapons/sounds.h"

#include "../../shared/projectiles/launcher_grenade.h"
#include "types.h"
#include "../shared/attack.h"

#include "assets/engine/animation/player_schema.h"

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

static void _launcherGrenadeExit(Task* task);

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
        M4A1_GRENADE_IMPACT_SIGNAL_NOISE = 1,
    };

    if (playerActorSpawnWeaponImpact(weaponContacts, playerCoord, impactCoordOut) != 0) {
        worldCoordPlaySound(impactCoordOut, SOUND_BULLET_IMPACT, M4A1_GRENADE_IMPACT_SIGNAL_NOISE);
    }
}

/// The model `gM4a1GrenadeWeaponModel`: generated from its asset (weapons/m4a1_grenade/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/m4a1_grenade/model.inc"

/// The player's clips this package carries, in the package's order, and its animation bank (weapons/m4a1_grenade/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/m4a1_grenade/clips.inc"
#include "assets/weapons/m4a1_grenade/bank.inc"

/// Blast-sphere radii in game-coordinate units for the three grenade rounds.
///
/// Indexed by the secondary load's round at detonation minus `GRENADE_ROUND_FIRST`:
/// fragmentation 500, airburst 1200 and riot 2000. An empty load uses fragmentation.
/// The fourth stored halfword is zero; its role is unproven and no supported
/// round selects it. Keep the stored span in its original trailing-data position.
static u16 _gM4a1GrenadeBlastRadii[4] = {
    [GRENADE_ROUND_FRAGMENTATION - GRENADE_ROUND_FIRST] = 500,
    [GRENADE_ROUND_AIRBURST -
        GRENADE_ROUND_FIRST]                            = 1200,
    [GRENADE_ROUND_RIOT -
        GRENADE_ROUND_FIRST]                            = 2000,
    0,
};

/// The model `gM4a1GrenadeProjectileModel`: generated from its asset (weapons/m4a1_grenade/grenade_model) by
/// tools/buildsteps/gen_model_source.py. Its binding overrides the slot's placement: the faces read 8-bit texels
/// below the weapon's sheet, not the texture they are bound to.
#include "assets/weapons/m4a1_grenade/grenade_model.inc"

void m4a1GrenadeAttackState(Task* playerTask)
{
    enum {
        M4A1_GRENADE_PHASE_PREPARE        = 0,
        M4A1_GRENADE_PHASE_WAIT_READY     = 1,
        M4A1_GRENADE_PHASE_SELECT_ATTACK  = 2,
        M4A1_GRENADE_PHASE_RIFLE_BURST    = 3,
        M4A1_GRENADE_PHASE_GRENADE_IMPACT = 4,
        M4A1_GRENADE_PHASE_RECOVER        = 5,
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
            actor->state                                          = PLAYER_ACTOR_STATE_ATTACK;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex                                  = 0;
            actor->animationState                                 = 0;
            actor->statePhase                                    += blendFrames;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                blendFrames = M4A1_GRENADE_MOVING_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, blendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
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
                equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_M4A1_GRENADE), EQUIPMENT_WEAPON_LOAD_CONSUME_SECONDARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords,
                                    WEAPON_SOUND_VARIANT(ammunitionIndex - GRENADE_ROUND_FIRST) | M4A1_GRENADE_SOUND(M4A1_GRENADE_SOUND_LAUNCH), 1);
                effectSpawn(EFFECT_GRENADE_MUZZLE_FLASH,
                            actor->equipmentTasks[1]->extra.tmd->coords, WEAPON_INDEX_M4A1_GRENADE,
                            NULL);
                playerActorSpawnGrenadeProjectile(playerTask, PLAYER_ACTOR_GRENADE_PLAYER, PLAYER_ACTOR_GRENADE_M4A1,
                                                  ammunitionIndex | (WEAPON_INDEX_M4A1_GRENADE << PLAYER_ACTOR_GRENADE_WEAPON_SHIFT));
                playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_SECONDARY_FIRE, 0, M4A1_GRENADE_LAUNCH_BLEND_FRAMES);
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
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_M4A1_GRENADE), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(playerTask->extra.tmd->coords,
                                        WEAPON_SOUND_VARIANT(ammunitionIndex - GRENADE_ROUND_FIRST) | M4A1_GRENADE_SOUND(WEAPON_SOUND_SLOT_SHOT), 1);
                    effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                WEAPON_INDEX_M4A1_GRENADE, NULL);
                    playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_FIRE, 0, M4A1_GRENADE_RIFLE_BLEND_FRAMES);
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
                                        (actor->actionValue + M4A1_GRENADE_SOUND(M4A1_GRENADE_SOUND_GRENADE_RELOAD_FIRST)) | WEAPON_SOUND_VARIANT(ammunitionIndex - GRENADE_ROUND_FIRST), 0);
                    actor->actionValue++;
                }
            }
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (_weaponIsArmSlotAdvancing(playerTask) == 0 ||
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
    task->exitCallback = _launcherGrenadeExit;
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
/// adds `LAUNCHER_GRENADE_FLIGHT_DIVISOR_STEP` and detonates above 0xFFFFF. Enemy contacts
/// detonate immediately. Capsule grid contacts take precedence over sphere
/// grid contacts; blocking surfaces either detonate or select teardown. Class
/// 1 in the day/night Dryfield Water Tower also detonates on pass-through.
/// Detonation samples the current secondary load (fragmentation if empty),
/// widens the sphere and changes the clock to 8 blast ticks, or 1 for airburst.
/// Every path releases the frame's scratch block. A teardown surface still
/// receives this frame's movement before the next dispatch releases the task.
static void _m4a1GrenadeFlyProjectile(Task* task)
{

    enum {
        M4A1_GRENADE_TASK_STATE_BLAST = 2,
        M4A1_GRENADE_TASK_STATE_EXIT  = 3,
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
        explosionSoundBits  = WEAPON_SOUND_VARIANT(scratch->ammunitionIndex - GRENADE_ROUND_FIRST) | SOUND_WEAPON(0, M4A1_GRENADE_SOUND_EXPLOSION);
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
    // One tick of translation, capsule reach and the 16.16 flight clock: the growing flight divisor slows
    // translation while gravity lowers the trajectory.
    scratch->delta.vector.vx        = grenadeWork->dir.vx / grenadeWork->flightTimer.halves.integer;
    scratch->delta.vector.vy        = grenadeWork->dir.vy / grenadeWork->flightTimer.halves.integer;
    scratch->delta.vector.vz        = grenadeWork->dir.vz / grenadeWork->flightTimer.halves.integer;
    projectileCoord->coord.t[0]    += scratch->delta.vector.vx;
    projectileCoord->coord.t[1]    += scratch->delta.vector.vy;
    projectileCoord->coord.t[2]    += scratch->delta.vector.vz;
    grenadeWork->capsule.ends[1].vy = -(grenadeWork->flightTimer.word >> M4A1_GRENADE_CAPSULE_TIMER_SHIFT);
    grenadeWork->flightTimer.word  += LAUNCHER_GRENADE_FLIGHT_DIVISOR_STEP;
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
}

#include "../../shared/projectiles/launcher_grenade.inc.c"

void m4a1GrenadeShellTask(Task* task)
{
    TaskFunc stateHandlers[] = {
        _m4a1GrenadeInitProjectile,
        _m4a1GrenadeFlyProjectile,
        _launcherGrenadeBlast,
        _launcherGrenadeExit,
    };

    stateHandlers[task->state](task);
}
