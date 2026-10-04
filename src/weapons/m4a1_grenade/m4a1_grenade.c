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

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/weapon.h"
#include "../../shared/grenade_shell.h"

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

void        func_m4a1_grenade_8011D1EC(Task* arg0);
static void func_m4a1_grenade_8011D654(Task* arg0);
static void func_m4a1_grenade_8011D994(Task* arg0);

/// Per-frame firing state machine for the M4A1 grenade launcher. State 0 arms
/// the shot and raises the weapon (clip 8 instead of 1 when it was already up),
/// state 1 waits for that clip. State 2 branches on `field_97F`: a held trigger
/// (bit 0) drops into the three-round burst of state 3, a tap (bit 1) fires the
/// single 0x101 grenade of state 4, and anything else falls straight into the
/// burst. State 3 counts `field_934` down to each round, spending one grenade,
/// playing `0x201B0004` and spawning the muzzle flash, and picks the lock-on
/// target on the frame after. States 4/5 pick the target once, then state 5
/// walks the animation, emitting `0x201B0008 + field_93E` on every record whose
/// `flags` has both 0x10 and 0x20, and hands back to `func_80106550` when the
/// clip is done or the recoil timer has run out.
void func_m4a1_grenade_8011D1EC(Task* arg0)
{
    GameActor*             actor;
    GfxCoord*              coord;
    GfxCoord*              spot;
    const AnimationRecord* rec;
    EquipmentWeaponLoad*   slot;
    s32                    anim;
    s32                    delay;
    s32                    sfx;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    slot  = Gp_GetItemSlot(gPlayerStatus.weapon + 0x7F);
    /* Reloaded rather than reused: the store leaves the block address in a
       caller-saved register and the copy into `spot` is a second read of
       `SCRATCH_STACK_CURSOR_SLOT` that CSE folds back onto it, which is what keeps the
       two uses in separate registers. */
    SCRATCH_STACK_RESERVE_BYTES(0x50);
    spot = SCRATCH_STACK_CURSOR(GfxCoord);
    sfx  = slot->secondaryItemId - 0x9F;
    if (sfx < 0) {
        sfx = 0xA;
    }
    switch (actor->statePhase) {
        case 0:
            anim                                                  = 1;
            actor->state                                          = 4;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex                                  = 0;
            actor->animationState                                 = 0;
            actor->statePhase                                    += anim;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0xC00;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                anim = 8;
            }
            playerActorPlayChildSlotsWithBlend(arg0, 9, 0, anim);
            actor->movementMode = 0;
            break;
        case 1:
            if (Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case 2:
            actor->rumblePosted = 0;
            if (actor->attackButton & 1) {
                actor->statePhase        = 3;
                actor->stateTimer        = 0;
                actor->attackCancelTicks = 9;
                actor->actionValue       = 3;
                func_80106238(arg0, 0, 1);
            } else if (actor->attackButton & 2) {
                actor->statePhase                  = 4;
                actor->attackControl.cooldownTicks = 0x28;
                actor->attackCancelTicks           = 0x22;
                Gp_ConsumeSlotQty(0x9A, 0x101);
                Gp_PlayObjSfx(arg0->extra.tmd->coords,
                              ((sfx - 0xA) << 24) | 0x201B0006, 1);
                Gp_SpawnEff(EFFECT_GRENADE_MUZZLE_FLASH,
                            actor->equipmentTasks[1]->extra.tmd->coords, 0x1B,
                            NULL);
                func_80104490(arg0, 0, 0, sfx | 0x1B00);
                playerActorPlayChildSlotsWithBlend(arg0, 0xB, 0, 3);
                break;
            }
            /* fallthrough */
        case 3:
            if (actor->actionValue != 0) {
                delay = actor->stateTimer;
                if (delay == 0) {
                    actor->actionValue--;
                    actor->stateTimer                                     = 3;
                    actor->rumblePosted                                   = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    Gp_ConsumeSlotQty(0x9A, 1);
                    if (func_80106264(1) == 0) {
                        actor->actionValue = 0;
                    }
                    Gp_PlayObjSfx(arg0->extra.tmd->coords,
                                  ((sfx - 0xA) << 24) | 0x201B0004, 1);
                    Gp_SpawnEff(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                0x1B, NULL);
                    playerActorPlayChildSlotsWithBlend(arg0, 0xA, 0, 2);
                } else {
                    actor->stateTimer = delay - 1;
                    if (delay - 1 == 2) {
                        actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                        if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                            Gp_PlayObjSfx(spot, 0x17, 1);
                        }
                    }
                }
                break;
            }
            /* fallthrough */
        case 4:
            actor->statePhase                                     = 5;
            actor->actionValue                                    = 0;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                Gp_PlayObjSfx(spot, 0x17, 1);
            }
            /* fallthrough */
        case 5:
            rec = Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1);
            if (rec != NULL && rec != actor->lastCueRecord) {
                actor->lastCueRecord = rec;
                if ((rec->flags & ANIMATION_RECORD_CUE_MASK) == ANIMATION_RECORD_CUE_MASK) {
                    Gp_PlayObjSfx(arg0->extra.tmd->coords,
                                  (actor->actionValue + 0x201B0008) | ((sfx - 0xA) << 24), 0);
                    actor->actionValue++;
                }
            }
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = 0xC;
                func_80106550(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x50);
}

/// Spawn state: allocates the grenade's work block, places the projectile a
/// little above and in front of the muzzle coordinate, parents it to world,
/// and links its two collision nodes.
static void func_m4a1_grenade_8011D654(Task* arg0)
{
    u8*                head;
    SVECTOR*           blk;
    SVECTOR*           vec;
    MATRIX*            mtx;
    TmdObject*         extra;
    GfxCoord*          coord;
    GfxCoord*          muzzle;
    WeaponGrenadeWork* work;

    extra                         = arg0->extra.tmd;
    head                          = SCRATCH_STACK_CURSOR(u8);
    coord                         = extra->coords;
    blk                           = (SVECTOR*)(head - 0x28);
    SCRATCH_STACK_CURSOR(SVECTOR) = blk;
    muzzle                        = coord->parent;
    work                          = memCalloc(sizeof(WeaponGrenadeWork), 0);
    vec                           = blk;
    if (work == NULL) {
        SCRATCH_STACK_RELEASE_BYTES(0x28);
        taskKill(arg0);
        return;
    }
    arg0->work         = work;
    arg0->exitCallback = grenadeShellExit;
    arg0->state++;
    memFillBytes(work, 0, sizeof(WeaponGrenadeWork));
    blk->vx              = 0;
    blk->vy              = 0x220;
    blk->vz              = 0x28;
    muzzle->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(muzzle);
    coord->workm = muzzle->workm;
    gte_SetRotMatrix(&muzzle->workm);
    gte_SetTransMatrix(&muzzle->workm);
    gte_ldv0(vec);
    gte_rtv0tr();
    gte_stlvnl(coord->workm.t);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &coord->coord);
    mtx                 = (MATRIX*)(head - 0x20);
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    extra->flags        = 0;
    *mtx                = coord->coord;
    gfxRotMatrixX(mtx, -0x400, GRAPHICS_ROTATION_COMPOSE);
    gfxReadMatrixZAxis(mtx, &work->dir);
    VectorNormalSS(&work->dir, &work->dir);
    work->flightTimer.word            = 0xA0000;
    work->smokeInterval               = 1;
    work->flightFrame                 = 0;
    work->sphereBody.coord            = coord;
    work->sphereBody.context.contacts = work->sphereContacts;
    work->sphereBody.pos.vx           = 0;
    work->sphereBody.pos.vy           = 0;
    work->sphereBody.pos.vz           = 0;
    work->sphereBody.key              = (u16)arg0->spawnArg1.value | 0x20000;
    work->sphereBody.radius           = 0x94;
    work->sphereBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(1, &work->sphereBody);
    Gp_InitRec18Table(work->sphereBody.context.contacts, 1, 0);
    work->capsuleBody.context.capsule = &work->capsule;
    work->capsuleBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->capsule.contacts            = work->capsuleContacts;
    work->capsuleBody.coord           = coord;
    work->capsuleBody.pos.vx          = 0;
    work->capsuleBody.pos.vy          = 0;
    work->capsuleBody.pos.vz          = 0;
    work->capsuleBody.key             = 0;
    work->capsuleBody.radius          = 0;
    work->capsule.ends[0].vx          = 0;
    work->capsule.ends[0].vy          = 0;
    work->capsule.ends[0].vz          = 0;
    work->capsule.ends[1].vx          = 0;
    work->capsule.ends[1].vz          = 0;
    work->capsule.end0Radius          = 1;
    work->capsule.end1Radius          = 1;
    work->sphereBody.flags           |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->capsule.ends[1].vy          = -(work->flightTimer.word >> 10);
    Gp_LinkObj(1, &work->capsuleBody);
    Gp_InitRec18Table(work->capsule.contacts, 1, 0);
    work->capsuleBody.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
    SCRATCH_STACK_RELEASE_BYTES(0x28);
}

/// Flight state: steps the grenade along `dir`. A category-3 contact in
/// `sphereContacts` detonates it, as does `flightTimer` passing 0xFFFFF.
/// Grid contacts on `capsuleContacts` are resolved first, then those on
/// `sphereContacts`; the chosen table is handed to `func_800E0FEC` /
/// `func_800E1ACC` for the surface it crossed. A surface that blocks probes
/// detonates when it accepts weapon impacts, and otherwise advances the task
/// to the exit state. Any other surface detonates only for surface index 1
/// in area 0x14 of stages 2 and 3.
static void func_m4a1_grenade_8011D994(Task* arg0)
{
    _M4a1GrenadeFlightScratch*       scratch;
    WeaponGrenadeWork*               work;
    GfxCoord*                        coord;
    EquipmentWeaponLoad*             slot;
    WorldCollisionSurfaceProperties* surface;
    s32                              idx;
    s32                              clip;
    s32                              step;
    s32                              sfxbase;
    s32                              sfxarg;

    work                = (WeaponGrenadeWork*)arg0->work;
    coord               = arg0->extra.tmd->coords;
    slot                = Gp_GetItemSlot(gPlayerStatus.weapon + 0x7F);
    scratch             = SCRATCH_STACK_RESERVE_BLOCK(_M4a1GrenadeFlightScratch);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (Gp_CountRec18Hi(work->sphereContacts, 0x30000) != 0) {
    explode:
        // The launcher's load is the rifle's secondary one. With no round
        // loaded the index comes out negative and the grenade detonates as
        // a fragmentation round.
        scratch->ammunitionIndex = WEAPON_AMMUNITION_INDEX(slot->secondaryItemId);
        if (scratch->ammunitionIndex < 0) {
            scratch->ammunitionIndex = GRENADE_ROUND_FRAGMENTATION;
        }
        arg0->state = 2;
        Gp_SpawnEff(EFFECT_GRENADE_EXPLOSION, coord, scratch->ammunitionIndex, NULL);
        sfxbase = gPlayerStatus.weapon << 16;
        sfxarg  = ((scratch->ammunitionIndex - GRENADE_ROUND_FIRST) << 24) | 0x20000007;
        Gp_PlayObjSfx(coord, sfxbase | sfxarg, 1);
        clip = 8;
        if (scratch->ammunitionIndex == GRENADE_ROUND_AIRBURST) {
            clip = 1;
        }
        work->flightTimer.word  = clip;
        work->sphereBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        work->sphereBody.radius = D_m4a1_grenade_8012E08C[scratch->ammunitionIndex - GRENADE_ROUND_FIRST];
        SCRATCH_STACK_RELEASE_BLOCK(_M4a1GrenadeFlightScratch);
        return;
    }

    if (Gp_CountRec18Hi(work->capsuleContacts, WORLD_COLLISION_CONTACT_GRID) == 0) {
        goto trySphereContacts;
    }
    func_800E0FEC(work->capsuleContacts, &scratch->delta, 1, &idx);
    idx = func_800E1ACC((u8*)&idx);
check:
    surface = Gp_RoomParamTables[gGameSession->location.loc.stage - 1][gGameSession->location.loc.area - 1][idx];
    if (surface->probePassThrough == WORLD_COLLISION_SURFACE_BLOCK_PROBES) {
        if (surface->weaponImpactEnabled != WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS) {
            goto explode;
        }
        arg0->state = 3;
        goto move;
    }
    if (idx == 1 && gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area == 0x14 && (u32)(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage - 2) < 2U) {
        goto explode;
    }
    goto move;
trySphereContacts:
    if (Gp_CountRec18Hi(work->sphereContacts, WORLD_COLLISION_CONTACT_GRID) != 0) {
        func_800E0FEC(work->sphereContacts, &scratch->delta, 1, &idx);
        idx = func_800E1ACC((u8*)&idx);
        goto check;
    }
move:
    scratch->delta.vector.vx = work->dir.vx / work->flightTimer.halves.integer;
    scratch->delta.vector.vy = work->dir.vy / work->flightTimer.halves.integer;
    scratch->delta.vector.vz = work->dir.vz / work->flightTimer.halves.integer;
    coord->coord.t[0]       += scratch->delta.vector.vx;
    coord->coord.t[1]       += scratch->delta.vector.vy;
    coord->coord.t[2]       += scratch->delta.vector.vz;
    work->capsule.ends[1].vy = -(work->flightTimer.word >> 10);
    work->flightTimer.word  += GRENADE_SHELL_FLIGHT_STEP;
    if (work->flightTimer.word > 0xFFFFF) {
        goto explode;
    }
    work->dir.vy      = work->dir.vy + 0x10;
    step              = work->flightFrame + 1;
    work->flightFrame = step;
    if (work->smokeInterval < 4 && step % 7 == 0) {
        work->smokeInterval = work->smokeInterval + 1;
    }
    if (work->flightFrame % work->smokeInterval == 0) {
        Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0, NULL);
    }
    Gp_ClearRec18Occupied(work->sphereContacts);
    Gp_ClearRec18Occupied(work->capsuleContacts);
    SCRATCH_STACK_RELEASE_BLOCK(_M4a1GrenadeFlightScratch);
}

#include "../../shared/grenade_shell_blast.inc.c"

#include "../../shared/grenade_shell_exit.inc.c"

void func_m4a1_grenade_8011DE68(Task* task)
{
    TaskFunc states[4] = {
        func_m4a1_grenade_8011D654,
        func_m4a1_grenade_8011D994,
        grenadeShellBlast,
        grenadeShellExit,
    };

    states[task->state](task);
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
