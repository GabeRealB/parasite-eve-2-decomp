#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gunblade_private.h"

#include "gameplay/animation.h"
#include "gameplay/effects.h"
#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

/// 0x68-byte scratch `func_gunblade_8011E040` carves off the scratch stack.
/// `coord` is the sound source handed to `Gp_PickNearestRec18` and
/// `Gp_PlayObjSfx` (the lock-on target's position is written into its
/// `workm.t`), `dir` receives the blade's forward column from
/// `Gfx_MatrixCol2`, and `step` is that column scaled down by 136 - the
/// per-axis camera shake added to the muzzle coordinate while the slash's
/// recoil timer runs.
typedef struct _GunbladeScratch {
    /* 0x00 */ GfxCoord coord;
    /* 0x50 */ VECTOR   step;
    /* 0x60 */ SVECTOR  dir;
} GunbladeScratch;
STATIC_ASSERT_SIZEOF(GunbladeScratch, 0x68);

/// `gPlayerStatus.weaponSlotItem`, the encoded primary weapon-slot item, read under
/// its own address wherever the value is wanted once rather than as one of a
/// run of accesses to the config block.

/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId`, the 1-based difficulty/mode row of `D_80112E04`.

static void func_gunblade_8011E040(Task* arg0);

/// Per-frame firing state machine for the gunblade. State 0 arms the shot and
/// raises the weapon (clip 6 instead of 1 when it was already up), state 1
/// waits for that clip. State 2 branches on `field_97F`: the blade swing (1)
/// goes to state 3, seeding the 0x12-frame swing timer and the 0x39-frame
/// recoil counter and re-aiming the muzzle record with the attach-0x17 spread,
/// while anything else fires the gun and drops straight into the lock-on of
/// state 6, spending a magazine round and spawning the muzzle flash. States
/// 3-5 count `field_934` down: at 0 state 3 hands over to state 4 and spawns
/// the beam effect (parented to the weapon task so `func_gunblade_8011E008`
/// can reach it), and any later state falls out to 7. State 4 asks
/// `func_801060E0` where the blade landed; a connecting swing (2) advances to
/// state 5 and, if there is still a round to spend, charges the beam and
/// re-grades the shot from the attachment id. Every frame in this group plays
/// the hit sound once the swing has collided and, while the recoil counter
/// runs, spins the actor 0xC units and flags the shake for the tail. State 6
/// picks the lock-on target - attachment 0xE aims the sound at the target's own
/// position rather than the scratch coordinate - and falls into state 7, which
/// drops out of the firing pose once the aim check fails.
///
/// The tail is common to every state: it reads the blade's forward column out
/// of the muzzle matrix and, only while `shake` is set, adds a 1/136th of it to
/// the muzzle coordinate.
static void func_gunblade_8011E040(Task* arg0)
{
    GameActor*             actor;
    GfxCoord*              coord;
    GunbladeScratch*       blk;
    WorldCollisionCapsule* rec;
    EffectWork*            eff;
    s32                    sfx;
    s32                    anim;
    s32                    hit;
    s32                    lvl;
    s16                    spread;
    s32                    shake;

    shake = 0;
    actor = arg0->work;
    sfx   = (gPlayerStatus.weaponSlotItem - 0xD) << 24;
    rec   = &actor->weaponShape;
    SCRATCH_STACK_RESERVE_BYTES(sizeof(GunbladeScratch));
    blk   = SCRATCH_STACK_CURSOR(GunbladeScratch);
    coord = arg0->extra.tmd->coords;
    if (sfx < 0) {
        sfx = 0;
    }
    switch (actor->statePhase) {
        case 0:
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->state          = 4;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            func_80106238(arg0, 0, 0);
            anim                                                  = 1;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x400;
            actor->statePhase                                    += anim;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                anim = 6;
            }
            Gp_AnimPlayChildSlotsEx(arg0, 9, 0, anim);
            actor->movementMode = 0;
            break;
        case 1:
            if (Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case 2:
            if (actor->attackButton == 1) {
                actor->statePhase        = 3;
                actor->stateTimer        = 0x12;
                actor->actionValue       = 0;
                actor->gunbladeSpinTicks = 0x39;
                func_80106518(0x17);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = 0x2171B;
                {
                    u16 reach       = rec->ends[1].vz + D_80112F60[23];
                    rec->end0Radius = 0x180;
                    rec->end1Radius = 0x180;
                    rec->ends[0].vz = reach;
                }
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= 0xF7FF;
                Gp_AnimPlayChildSlotsEx(arg0, 0xA, 0, 3);
                break;
            }
            actor->statePhase = 6;
            actor->weaponShape.ends[0].vz =
                actor->weaponShape.ends[1].vz + 0x2200;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = gPlayerStatus.weaponSlotItem | 0x21700;
            rec->end1Radius                                    = 0x100;
            rec->ends[0].vz                                    = rec->ends[1].vz + 0x2200;
            spread                                             = 0x900;
            if (gPlayerStatus.weaponSlotItem != 0xD) {
                spread = 0x100;
            }
            rec->end0Radius = spread;
            if (gPlayerStatus.weaponSlotItem == 0xE) {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x800;
            } else {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= 0xF7FF;
            }
            Gp_ConsumeSlotQty(0x96, 1);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0xC000;
            Gp_PlayObjSfx(arg0->extra.tmd->coords, sfx | 0x20170005, 1);
            Gp_SpawnEff(0x600A1, actor->equipmentTasks[1]->extra.tmd->coords,
                        (gPlayerStatus.weaponSlotItem << 16) | 0x17, NULL);
            Gp_AnimPlayChildSlotsEx(arg0, 0xB, 0, 3);
            break;
        case 3:
        case 4:
        case 5:
            actor->stateTimer = actor->stateTimer - 1;
            if (actor->stateTimer == 0) {
                if (actor->statePhase == 3) {
                    actor->statePhase                                     = 4;
                    actor->stateTimer                                     = 8;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0xC000;
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, sfx | 0x20170006, 0);
                    eff = Gp_SpawnEff(0x60186,
                                      actor->equipmentTasks[1]->extra.tmd->coords,
                                      0x17, NULL);
                    if (eff != NULL) {
                        Task_Reparent(actor->equipmentTasks[1], eff->task);
                    }
                } else if (actor->statePhase >= 4) {
                    actor->statePhase = 7;
                }
            }
            if (actor->statePhase == 4 && (s8)func_801060E0(arg0) == 2) {
                actor->statePhase = 5;
                if (func_80106264(1) != 0) {
                    if (gPlayerStatus.weaponSlotItem < 0xF) {
                        lvl = gPlayerStatus.weaponSlotItem + 0xB;
                    } else {
                        lvl = 0x20;
                    }
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = lvl | 0x21700;
                    Gp_ConsumeSlotQty(0x96, 1);
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, sfx | 0x20170008, 1);
                    func_gunblade_8011E008(gPlayerStatus.weaponSlotItem);
                } else {
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, sfx | 0x20170001, 0);
                }
            }
            if (actor->actionValue != 1 && Gp_CountRec18Hi(actor->weaponContacts, 0x30000) != 0) {
                actor->actionValue = 1;
                Gp_PlayObjSfx(arg0->extra.tmd->coords, sfx | 0x20170007, 0);
            }
            if (actor->gunbladeSpinTicks != 0) {
                shake                     = 1;
                actor->rotation.vy       += 0xC;
                actor->gunbladeSpinTicks -= 1;
            }
            break;
        case 6:
            actor->statePhase++;
            if (gPlayerStatus.weaponSlotItem != 0xD) {
                hit = Gp_PickNearestRec18(actor->weaponContacts, coord, &blk->coord);
                if (gPlayerStatus.weaponSlotItem == 0xE) {
                    if (hit != 0 || Gp_CountRec18Hi(actor->weaponContacts, 0x30000) != 0) {
                        blk->coord.workm.t[0] = actor->weaponContacts[0].point.vx;
                        blk->coord.workm.t[1] = actor->weaponContacts[0].point.vy;
                        blk->coord.workm.t[2] = actor->weaponContacts[0].point.vz;
                        Gp_PlayObjSfx(&blk->coord, sfx | 0x20170004, 1);
                    }
                } else if (hit != 0) {
                    Gp_PlayObjSfx(&blk->coord, 0x17, 1);
                }
            }
            /* fallthrough */
        case 7:
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= 0x3FFF;
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0) {
                func_80106550(arg0);
            }
            break;
    }
    Gfx_MatrixCol2(&coord->coord, &blk->dir);
    actor->movementSign = shake;
    blk->step.vx        = (s16)(blk->dir.vx / 136) * shake;
    blk->step.vy        = (s16)(blk->dir.vy / 136) * shake;
    blk->step.vz        = (s16)(blk->dir.vz / 136) * shake;
    coord->coord.t[0]  += blk->step.vx;
    coord->coord.t[1]  += blk->step.vy;
    coord->coord.t[2]  += blk->step.vz;
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GunbladeScratch));
}
