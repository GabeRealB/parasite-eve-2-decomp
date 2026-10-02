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

/// 0x34-byte scratch the flight state takes from the scratch stack. The
/// `WorldCollisionDelta` at 0x20 is handed to `func_800E0FEC` and also holds the
/// per-frame translation the state adds onto the projectile coordinate;
/// `sfx` is the attachment id the explosion effect and sound are keyed on.
typedef struct M4a1GrenadeScratch {
    /* 0x00 */ byte                pad_0[0x20];
    /* 0x20 */ WorldCollisionDelta delta;
    /* 0x30 */ s32                 sfx;
} M4a1GrenadeScratch;
STATIC_ASSERT_SIZEOF(M4a1GrenadeScratch, 0x34);

static void func_m4a1_grenade_8011D1EC(Task* arg0);
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
static void func_m4a1_grenade_8011D1EC(Task* arg0)
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
    Gfx_MatrixCol2(mtx, &work->dir);
    VectorNormalSS(&work->dir, &work->dir);
    work->field_88.word        = 0xA0000;
    work->field_8C             = 1;
    work->field_90             = 0;
    work->obj.coord            = coord;
    work->obj.context.contacts = work->rec0;
    work->obj.pos.vx           = 0;
    work->obj.pos.vy           = 0;
    work->obj.pos.vz           = 0;
    work->obj.key              = (u16)arg0->spawnArg1.value | 0x20000;
    work->obj.radius           = 0x94;
    work->obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(1, &work->obj);
    Gp_InitRec18Table(work->obj.context.contacts, 1, 0);
    work->obj2.context.capsule = &work->d4rec;
    work->obj2.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->d4rec.contacts       = work->rec1;
    work->obj2.coord           = coord;
    work->obj2.pos.vx          = 0;
    work->obj2.pos.vy          = 0;
    work->obj2.pos.vz          = 0;
    work->obj2.key             = 0;
    work->obj2.radius          = 0;
    work->d4rec.ends[0].vx     = 0;
    work->d4rec.ends[0].vy     = 0;
    work->d4rec.ends[0].vz     = 0;
    work->d4rec.ends[1].vx     = 0;
    work->d4rec.ends[1].vz     = 0;
    work->d4rec.end0Radius     = 1;
    work->d4rec.end1Radius     = 1;
    work->obj.flags           |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->d4rec.ends[1].vy     = -(work->field_88.word >> 10);
    Gp_LinkObj(1, &work->obj2);
    Gp_InitRec18Table(work->d4rec.contacts, 1, 0);
    work->obj2.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
    SCRATCH_STACK_RELEASE_BYTES(0x28);
}

/// Flight state: steps the grenade along `dir`, and detonates when it hits
/// something, when it crosses a room record that blocks it, or when the 16.16
/// flight timer runs past 0xFFFFF. `rec0` collects the solid hits, `rec1` the
/// room-boundary ones; whichever table has a `0x100000` record is handed to
/// `func_800E0FEC` / `func_800E1ACC` to select the surface properties it crossed.
/// A surface with `probePassThrough` set only detonates the grenade in the
/// one scripted case (area 0x14, stages 2 and 3). A blocking surface detonates
/// it when `weaponImpactEnabled` is set, otherwise handing the task to state 3.
static void func_m4a1_grenade_8011D994(Task* arg0)
{
    M4a1GrenadeScratch*              blk;
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
    blk                 = SCRATCH_STACK_RESERVE_BLOCK(M4a1GrenadeScratch);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (Gp_CountRec18Hi(work->rec0, 0x30000) != 0) {
    explode:
        blk->sfx = slot->secondaryItemId - 0x9F;
        if (blk->sfx < 0) {
            blk->sfx = 0xA;
        }
        arg0->state = 2;
        Gp_SpawnEff(EFFECT_GRENADE_EXPLOSION, coord, blk->sfx, NULL);
        sfxbase = gPlayerStatus.weapon << 16;
        sfxarg  = ((blk->sfx - 0xA) << 24) | 0x20000007;
        Gp_PlayObjSfx(coord, sfxbase | sfxarg, 1);
        clip = 8;
        if (blk->sfx == 0xB) {
            clip = 1;
        }
        work->field_88.word = clip;
        work->obj.flags    &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        SCRATCH_STACK_RELEASE_BYTES(sizeof(M4a1GrenadeScratch));
        work->obj.radius = D_m4a1_grenade_8012E08C[blk->sfx - 0xA];
        return;
    }

    if (Gp_CountRec18Hi(work->rec1, 0x100000) == 0) {
        goto try_rec0;
    }
    func_800E0FEC(work->rec1, &blk->delta, 1, &idx);
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
try_rec0:
    if (Gp_CountRec18Hi(work->rec0, 0x100000) != 0) {
        func_800E0FEC(work->rec0, &blk->delta, 1, &idx);
        idx = func_800E1ACC((u8*)&idx);
        goto check;
    }
move:
    blk->delta.vector.vx   = work->dir.vx / work->field_88.halves.integer;
    blk->delta.vector.vy   = work->dir.vy / work->field_88.halves.integer;
    blk->delta.vector.vz   = work->dir.vz / work->field_88.halves.integer;
    coord->coord.t[0]     += blk->delta.vector.vx;
    coord->coord.t[1]     += blk->delta.vector.vy;
    coord->coord.t[2]     += blk->delta.vector.vz;
    work->d4rec.ends[1].vy = -(work->field_88.word >> 10);
    work->field_88.word   += 0x1800;
    if (work->field_88.word > 0xFFFFF) {
        goto explode;
    }
    work->dir.vy   = work->dir.vy + 0x10;
    step           = work->field_90 + 1;
    work->field_90 = step;
    if (work->field_8C < 4 && step % 7 == 0) {
        work->field_8C = work->field_8C + 1;
    }
    if (work->field_90 % work->field_8C == 0) {
        Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0, NULL);
    }
    Gp_ClearRec18Occupied(work->rec0);
    Gp_ClearRec18Occupied(work->rec1);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(M4a1GrenadeScratch));
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
