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
#include "types.h"

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
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
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
            rec = animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1);
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
    actorRenderComposeCoord(muzzle);
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
    worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &work->sphereBody);
    worldCollisionInitContacts(work->sphereBody.context.contacts, 1, 0);
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
    worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &work->capsuleBody);
    worldCollisionInitContacts(work->capsule.contacts, 1, 0);
    work->capsuleBody.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
    SCRATCH_STACK_RELEASE_BYTES(0x28);
}

/// Flight state: steps the grenade along `dir`. A category-3 contact in
/// `sphereContacts` detonates it, as does `flightTimer` passing 0xFFFFF.
/// Grid contacts on `capsuleContacts` are resolved first, then those on
/// `sphereContacts`; the chosen table is handed to `func_800E0FEC` /
/// `worldCollisionSurfaceClassFromMask` for the surface it crossed. A surface that blocks probes
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
    if (worldCollisionCountContactsByKind(work->sphereContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
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

    if (worldCollisionCountContactsByKind(work->capsuleContacts, WORLD_COLLISION_CONTACT_GRID) == 0) {
        goto trySphereContacts;
    }
    func_800E0FEC(work->capsuleContacts, &scratch->delta, 1, &idx);
    idx = worldCollisionSurfaceClassFromMask((const u8*)&idx);
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
    if (worldCollisionCountContactsByKind(work->sphereContacts, WORLD_COLLISION_CONTACT_GRID) != 0) {
        func_800E0FEC(work->sphereContacts, &scratch->delta, 1, &idx);
        idx = worldCollisionSurfaceClassFromMask((const u8*)&idx);
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
    worldCollisionClearContacts(work->sphereContacts);
    worldCollisionClearContacts(work->capsuleContacts);
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
