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
#include "gameplay/world_coords.h"

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

/// Divisor that turns the model's forward axis into the distance the spinning
/// slash covers per frame: 4096 / 136, about 30 coordinate units.
enum { GUNBLADE_SPIN_ADVANCE_DIVISOR = 136 };

/// One-based weapon index passed to the resident use counter.
enum { GUNBLADE_WEAPON_ID = 23 };

/// Scratch-stack block for the gunblade's attack handler.
///
/// The handler reserves one block each frame and releases it before
/// returning; the block is not cleared. It stages two unrelated things: the
/// node a fired round's impact sound is placed at, and the forward step the
/// spinning slash moves the actor by.
///
/// Only the translation of `impactCoord`'s composed transform is written. A
/// sound's pan and depth come from projecting the node's origin, which reads
/// nothing else, so the rest of the node is whatever the scratch stack last
/// held. The position is a weapon contact point, offset by up to 7 units per
/// axis when the impact picker reports it, in the space the weapon node's
/// composed transform is expressed in.
///
/// `forward` is read from the root coordinate's local matrix and is not
/// normalized. `advance` is each component of it divided by
/// `GUNBLADE_SPIN_ADVANCE_DIVISOR` and multiplied by the frame's
/// `GameActor.movementSign`: 1 while the spin runs, 0 otherwise. It is added
/// to the root coordinate's local translation in every state. Neither
/// vector's `pad` is written.
typedef struct {
    GfxCoord impactCoord; // Node standing at the round's impact point: the source of the impact sound
    VECTOR   advance;     // This frame's displacement of the model's root coordinate, in coordinate units; zero outside the spin
    SVECTOR  forward;     // Model's forward axis: the Z column of its root coordinate's local matrix, 4096 per unit
} _GunbladeAttackScratch;
STATIC_ASSERT_SIZEOF(_GunbladeAttackScratch, 0x68);

/// `gPlayerStatus.weaponSlotItem`, the encoded primary weapon-slot item, read under
/// its own address wherever the value is wanted once rather than as one of a
/// run of accesses to the config block.

/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId`, the 1-based difficulty/mode row of `D_80112E04`.

void func_gunblade_8011E040(Task* arg0);

/// Per-frame firing state machine for the gunblade. State 0 arms the shot and
/// raises the weapon (clip 6 instead of 1 when it was already up), state 1
/// waits for that clip. State 2 branches on `field_97F`: the blade swing (1)
/// goes to state 3, seeding the 0x12-frame swing timer and the 0x39-frame
/// recoil counter and re-aiming the muzzle record with the attach-0x17 spread,
/// while anything else fires the gun and drops straight into the lock-on of
/// state 6, spending a magazine round and spawning the muzzle flash. States
/// 3-5 count `field_934` down: at 0 state 3 hands over to state 4 and spawns
/// the beam effect (parented to the weapon task so `gunbladeRequestChargeFlash`
/// can reach it), and any later state falls out to 7. State 4 asks
/// `playerActorReadAttackButton` for held fire input; secondary input (2) advances to
/// state 5 and, if there is still a round to spend, charges the beam and
/// re-grades the shot from the attachment id. Every frame in this group plays
/// the hit sound once the swing has collided and, while the recoil counter
/// runs, spins the actor 0xC units and flags the shake for the tail. State 6
/// picks the lock-on target - attachment 0xE aims the sound at the target's own
/// position rather than the scratch coordinate - and falls into state 7, which
/// drops out of the firing pose once the aim check fails.
///
/// The tail is common to every state: it reads the model's forward axis out
/// of its root coordinate's matrix and, only while `shake` is set, moves that
/// coordinate forward by a `GUNBLADE_SPIN_ADVANCE_DIVISOR`th of it.
void func_gunblade_8011E040(Task* arg0)
{
    GameActor*              actor;
    GfxCoord*               coord;
    _GunbladeAttackScratch* scratch;
    WorldCollisionCapsule*  rec;
    EffectWork*             eff;
    s32                     sfx;
    s32                     anim;
    s32                     hit;
    s32                     lvl;
    s16                     spread;
    s32                     shake;

    shake   = 0;
    actor   = arg0->work;
    sfx     = (gPlayerStatus.weaponSlotItem - 0xD) << 24;
    rec     = &actor->weaponShape;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_GunbladeAttackScratch);
    coord   = arg0->extra.tmd->coords;
    if (sfx < 0) {
        sfx = 0;
    }
    switch (actor->statePhase) {
        case 0:
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->state          = 4;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            playerActorSetWeaponAttackFlags(arg0, 0, 0);
            anim                                                  = 1;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x400;
            actor->statePhase                                    += anim;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                anim = 6;
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
            if (actor->attackButton == 1) {
                actor->statePhase        = 3;
                actor->stateTimer        = 0x12;
                actor->actionValue       = 0;
                actor->gunbladeSpinTicks = 0x39;
                weaponRecordUse(GUNBLADE_WEAPON_ID);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = 0x2171B;
                {
                    u16 reach       = rec->ends[1].vz + D_80112F60[23];
                    rec->end0Radius = 0x180;
                    rec->end1Radius = 0x180;
                    rec->ends[0].vz = reach;
                }
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= 0xF7FF;
                playerActorPlayChildSlotsWithBlend(arg0, 0xA, 0, 3);
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
            equipmentConsumeWeaponLoad(0x96, EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            worldCoordPlaySound(arg0->extra.tmd->coords, sfx | 0x20170005, 1);
            effectSpawn(EFFECT_SHOTGUN_MUZZLE_FLASH, actor->equipmentTasks[1]->extra.tmd->coords,
                        (gPlayerStatus.weaponSlotItem << 16) | 0x17, NULL);
            playerActorPlayChildSlotsWithBlend(arg0, 0xB, 0, 3);
            break;
        case 3:
        case 4:
        case 5:
            actor->stateTimer = actor->stateTimer - 1;
            if (actor->stateTimer == 0) {
                if (actor->statePhase == 3) {
                    actor->statePhase                                     = 4;
                    actor->stateTimer                                     = 8;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    worldCoordPlaySound(arg0->extra.tmd->coords, sfx | 0x20170006, 0);
                    eff = effectSpawn(EFFECT_GUNBLADE_TRAIL,
                                      actor->equipmentTasks[1]->extra.tmd->coords,
                                      0x17, NULL);
                    if (eff != NULL) {
                        taskReparent(actor->equipmentTasks[1], eff->task);
                    }
                } else if (actor->statePhase >= 4) {
                    actor->statePhase = 7;
                }
            }
            if (actor->statePhase == 4 && playerActorReadAttackButton(arg0) == PLAYER_ACTOR_ATTACK_BUTTON_SECONDARY) {
                actor->statePhase = 5;
                if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) != 0) {
                    if (gPlayerStatus.weaponSlotItem < 0xF) {
                        lvl = gPlayerStatus.weaponSlotItem + 0xB;
                    } else {
                        lvl = 0x20;
                    }
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = lvl | 0x21700;
                    equipmentConsumeWeaponLoad(0x96, EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    worldCoordPlaySound(arg0->extra.tmd->coords, sfx | 0x20170008, 1);
                    gunbladeRequestChargeFlash(gPlayerStatus.weaponSlotItem);
                } else {
                    worldCoordPlaySound(arg0->extra.tmd->coords, sfx | 0x20170001, 0);
                }
            }
            if (actor->actionValue != 1 && worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                actor->actionValue = 1;
                worldCoordPlaySound(arg0->extra.tmd->coords, sfx | 0x20170007, 0);
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
                hit = playerActorSpawnWeaponImpact(actor->weaponContacts, coord, &scratch->impactCoord);
                if (gPlayerStatus.weaponSlotItem == 0xE) {
                    if (hit != 0 || worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                        scratch->impactCoord.workm.t[0] = actor->weaponContacts[0].point.vx;
                        scratch->impactCoord.workm.t[1] = actor->weaponContacts[0].point.vy;
                        scratch->impactCoord.workm.t[2] = actor->weaponContacts[0].point.vz;
                        worldCoordPlaySound(&scratch->impactCoord, sfx | 0x20170004, 1);
                    }
                } else if (hit != 0) {
                    worldCoordPlaySound(&scratch->impactCoord, 0x17, 1);
                }
            }
            /* fallthrough */
        case 7:
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (playerActorIsSlotAdvancingLinearly(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0) {
                playerActorFinishWeaponAttack(arg0);
            }
            break;
    }
    gfxReadMatrixZAxis(&coord->coord, &scratch->forward);
    actor->movementSign = shake;
    scratch->advance.vx = (s16)(scratch->forward.vx / GUNBLADE_SPIN_ADVANCE_DIVISOR) * shake;
    scratch->advance.vy = (s16)(scratch->forward.vy / GUNBLADE_SPIN_ADVANCE_DIVISOR) * shake;
    scratch->advance.vz = (s16)(scratch->forward.vz / GUNBLADE_SPIN_ADVANCE_DIVISOR) * shake;
    coord->coord.t[0]  += scratch->advance.vx;
    coord->coord.t[1]  += scratch->advance.vy;
    coord->coord.t[2]  += scratch->advance.vz;
    SCRATCH_STACK_RELEASE_BLOCK(_GunbladeAttackScratch);
}
