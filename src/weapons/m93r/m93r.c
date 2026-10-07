#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

void func_m93r_8011D1C4(Task* arg0);

/// Per-frame firing state machine for the M93R burst pistol. Case 0 arms the
/// shot (four-tick reload window, `field_979` grace of 10) and queues the
/// ready animation, choosing the long variant when the weapon was left dirty
/// (`field_958`) or the actor is flagged in `field_975`; a two-handed grip
/// (`field_97F == 1`) turns the single shot into a three-round burst. Case 1
/// waits for that animation to reach its second slot. Case 2 fires one round
/// per two frames - consuming ammo 0x81, playing the muzzle report and spawning
/// the flash effect - and re-acquires the lock-on target on the off frame and
/// again once the burst runs dry. Case 3 runs out the grace counter and hands
/// back to `func_80106550`, parking `field_940` at 10 when the player is still
/// holding the fire button after the grace expired and at 0 otherwise.
void func_m93r_8011D1C4(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    GfxCoord*  spot;
    s32        anim;
    s32        delay;
    /* Narrower than the field it feeds on purpose: an `s32 shots = 1` would join
       the switch's SImode `1` in the same cse class and steal its register for
       the `field_97F == 1` compare below. */
    s16 shots;
    /* Declared before the switch so it is initialised in the first case test's
       delay slot, as the ROM does; case 3 reads it twice. */
    s32 lockedOut;

    SCRATCH_STACK_RESERVE_BYTES(0x50);
    spot      = SCRATCH_STACK_CURSOR(GfxCoord);
    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    lockedOut = 0;
    switch (actor->statePhase) {
        case 0:
            actor->state             = 4;
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->statePhase        = 1;
            actor->rumblePosted      = 0;
            actor->stateTimer        = 0;
            actor->attackCancelTicks = 0xA;
            shots                    = 1;
            if (actor->attackButton == 1) {
                shots = 3;
            }
            actor->actionValue = shots;
            playerActorSetWeaponAttackFlags(arg0, 0, actor->attackButton == 1);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0xC00;
            anim                                                  = 1;
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
            if (actor->actionValue != 0) {
                delay = actor->stateTimer;
                if (delay == 0) {
                    actor->actionValue--;
                    actor->stateTimer                                     = 1;
                    actor->rumblePosted                                   = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    Gp_ConsumeSlotQty(0x81, 1);
                    if (func_80106264(1) == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(arg0->extra.tmd->coords, 0x20020004, 1);
                    effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords, 2,
                                NULL);
                    playerActorPlayChildSlotsWithBlend(arg0, 0xA, 1, 2);
                    break;
                }
                /* Decrement through the local rather than storing `delay - 1`
                   and re-testing `delay - 1 == 0`: the latter keeps `delay`
                   live and turns the second test into a compare against the
                   switch's `1`. */
                delay--;
                actor->stateTimer = delay;
                if (delay == 0) {
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    if (playerActorSpawnWeaponImpact(actor->weaponContacts, coord, spot) != 0) {
                        worldCoordPlaySound(spot, 0x17, 1);
                    }
                }
            } else {
                /* Spelled out in both arms rather than shared after the `if`;
                   GCC cross-jumps the common tail itself, keeping only the
                   `field_12A` load duplicated, which is what the ROM has. */
                actor->statePhase                                     = 3;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                if (playerActorSpawnWeaponImpact(actor->weaponContacts, coord, spot) != 0) {
                    worldCoordPlaySound(spot, 0x17, 1);
                }
            }
            break;
        case 3:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if ((actor->padHeld & actor->actionPadMask) != 0) {
                lockedOut = actor->attackCancelTicks == 0;
            }
            if (playerActorIsSlotAdvancingLinearly(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 || lockedOut) {
                if (lockedOut) {
                    actor->attackControl.cooldownTicks = 0xA;
                } else {
                    actor->attackControl.cooldownTicks = 0;
                }
                func_80106550(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x50);
}

static TmdBone _gM93rModel00520Skeleton[1] = {
#include "assets/m93r_model_00520_skeleton.inc"
};

static u32 _gM93rModel00520PartVerts[1] = {
#include "assets/m93r_model_00520_partVerts.inc"
};

static SVECTOR _gM93rModel00520Verts[32] = {
#include "assets/m93r_model_00520_verts.inc"
};

static SVECTOR _gM93rModel00520Normals[32] = {
#include "assets/m93r_model_00520_normals.inc"
};

static u32 _gM93rModel00520Stream[229] = {
#include "assets/m93r_model_00520_stream.inc"
};

TmdSource D_m93r_8011DA74 = {
    0,
    1616,
    0,
    1,
    _gM93rModel00520PartVerts,
    _gM93rModel00520Verts,
    _gM93rModel00520Normals,
    _gM93rModel00520Skeleton,
    _gM93rModel00520Stream,
};

static AnimationPackedPose _gM93rAnimation00A68Bank1[2] = {
#include "assets/m93r_animation_00A68_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation00A68Bank4[8] = {
#include "assets/m93r_animation_00A68_bank4.inc"
};

static AnimationRecord _gM93rAnimation00A68Records[76] = {
#include "assets/m93r_animation_00A68_records.inc"
};

static u16 _gM93rAnimation00A68Indices[20] = {
#include "assets/m93r_animation_00A68_indices.inc"
};

static AnimationSet _gM93rAnimation00A68 = {
    _gM93rAnimation00A68Records,
    _gM93rAnimation00A68Indices,
    { NULL, _gM93rAnimation00A68Bank1, NULL, NULL, _gM93rAnimation00A68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0110CBank1[12] = {
#include "assets/m93r_animation_0110C_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0110CBank4[151] = {
#include "assets/m93r_animation_0110C_bank4.inc"
};

static AnimationRecord _gM93rAnimation0110CRecords[218] = {
#include "assets/m93r_animation_0110C_records.inc"
};

static u16 _gM93rAnimation0110CIndices[20] = {
#include "assets/m93r_animation_0110C_indices.inc"
};

static AnimationSet _gM93rAnimation0110C = {
    _gM93rAnimation0110CRecords,
    _gM93rAnimation0110CIndices,
    { NULL, _gM93rAnimation0110CBank1, NULL, NULL, _gM93rAnimation0110CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0196CBank1[19] = {
#include "assets/m93r_animation_0196C_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0196CBank4[169] = {
#include "assets/m93r_animation_0196C_bank4.inc"
};

static AnimationRecord _gM93rAnimation0196CRecords[290] = {
#include "assets/m93r_animation_0196C_records.inc"
};

static u16 _gM93rAnimation0196CIndices[20] = {
#include "assets/m93r_animation_0196C_indices.inc"
};

static AnimationSet _gM93rAnimation0196C = {
    _gM93rAnimation0196CRecords,
    _gM93rAnimation0196CIndices,
    { NULL, _gM93rAnimation0196CBank1, NULL, NULL, _gM93rAnimation0196CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation021D0Bank1[19] = {
#include "assets/m93r_animation_021D0_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation021D0Bank4[170] = {
#include "assets/m93r_animation_021D0_bank4.inc"
};

static AnimationRecord _gM93rAnimation021D0Records[290] = {
#include "assets/m93r_animation_021D0_records.inc"
};

static u16 _gM93rAnimation021D0Indices[20] = {
#include "assets/m93r_animation_021D0_indices.inc"
};

static AnimationSet _gM93rAnimation021D0 = {
    _gM93rAnimation021D0Records,
    _gM93rAnimation021D0Indices,
    { NULL, _gM93rAnimation021D0Bank1, NULL, NULL, _gM93rAnimation021D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation024E4Bank1[3] = {
#include "assets/m93r_animation_024E4_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation024E4Bank4[69] = {
#include "assets/m93r_animation_024E4_bank4.inc"
};

static AnimationRecord _gM93rAnimation024E4Records[99] = {
#include "assets/m93r_animation_024E4_records.inc"
};

static u16 _gM93rAnimation024E4Indices[20] = {
#include "assets/m93r_animation_024E4_indices.inc"
};

static AnimationSet _gM93rAnimation024E4 = {
    _gM93rAnimation024E4Records,
    _gM93rAnimation024E4Indices,
    { NULL, _gM93rAnimation024E4Bank1, NULL, NULL, _gM93rAnimation024E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation02C40Bank1[14] = {
#include "assets/m93r_animation_02C40_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation02C40Bank4[156] = {
#include "assets/m93r_animation_02C40_bank4.inc"
};

static AnimationRecord _gM93rAnimation02C40Records[253] = {
#include "assets/m93r_animation_02C40_records.inc"
};

static u16 _gM93rAnimation02C40Indices[20] = {
#include "assets/m93r_animation_02C40_indices.inc"
};

static AnimationSet _gM93rAnimation02C40 = {
    _gM93rAnimation02C40Records,
    _gM93rAnimation02C40Indices,
    { NULL, _gM93rAnimation02C40Bank1, NULL, NULL, _gM93rAnimation02C40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation033C8Bank1[16] = {
#include "assets/m93r_animation_033C8_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation033C8Bank4[167] = {
#include "assets/m93r_animation_033C8_bank4.inc"
};

static AnimationRecord _gM93rAnimation033C8Records[247] = {
#include "assets/m93r_animation_033C8_records.inc"
};

static u16 _gM93rAnimation033C8Indices[20] = {
#include "assets/m93r_animation_033C8_indices.inc"
};

static AnimationSet _gM93rAnimation033C8 = {
    _gM93rAnimation033C8Records,
    _gM93rAnimation033C8Indices,
    { NULL, _gM93rAnimation033C8Bank1, NULL, NULL, _gM93rAnimation033C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0369CBank1[6] = {
#include "assets/m93r_animation_0369C_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0369CBank4[52] = {
#include "assets/m93r_animation_0369C_bank4.inc"
};

static AnimationRecord _gM93rAnimation0369CRecords[91] = {
#include "assets/m93r_animation_0369C_records.inc"
};

static u16 _gM93rAnimation0369CIndices[20] = {
#include "assets/m93r_animation_0369C_indices.inc"
};

static AnimationSet _gM93rAnimation0369C = {
    _gM93rAnimation0369CRecords,
    _gM93rAnimation0369CIndices,
    { NULL, _gM93rAnimation0369CBank1, NULL, NULL, _gM93rAnimation0369CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation03A2CBank1[7] = {
#include "assets/m93r_animation_03A2C_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation03A2CBank4[73] = {
#include "assets/m93r_animation_03A2C_bank4.inc"
};

static AnimationRecord _gM93rAnimation03A2CRecords[114] = {
#include "assets/m93r_animation_03A2C_records.inc"
};

static u16 _gM93rAnimation03A2CIndices[20] = {
#include "assets/m93r_animation_03A2C_indices.inc"
};

static AnimationSet _gM93rAnimation03A2C = {
    _gM93rAnimation03A2CRecords,
    _gM93rAnimation03A2CIndices,
    { NULL, _gM93rAnimation03A2CBank1, NULL, NULL, _gM93rAnimation03A2CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation03EB4Bank1[9] = {
#include "assets/m93r_animation_03EB4_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation03EB4Bank4[104] = {
#include "assets/m93r_animation_03EB4_bank4.inc"
};

static AnimationRecord _gM93rAnimation03EB4Records[139] = {
#include "assets/m93r_animation_03EB4_records.inc"
};

static u16 _gM93rAnimation03EB4Indices[20] = {
#include "assets/m93r_animation_03EB4_indices.inc"
};

static AnimationSet _gM93rAnimation03EB4 = {
    _gM93rAnimation03EB4Records,
    _gM93rAnimation03EB4Indices,
    { NULL, _gM93rAnimation03EB4Bank1, NULL, NULL, _gM93rAnimation03EB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation040B0Bank1[3] = {
#include "assets/m93r_animation_040B0_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation040B0Bank4[22] = {
#include "assets/m93r_animation_040B0_bank4.inc"
};

static AnimationRecord _gM93rAnimation040B0Records[76] = {
#include "assets/m93r_animation_040B0_records.inc"
};

static u16 _gM93rAnimation040B0Indices[20] = {
#include "assets/m93r_animation_040B0_indices.inc"
};

static AnimationSet _gM93rAnimation040B0 = {
    _gM93rAnimation040B0Records,
    _gM93rAnimation040B0Indices,
    { NULL, _gM93rAnimation040B0Bank1, NULL, NULL, _gM93rAnimation040B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation04388Bank1[6] = {
#include "assets/m93r_animation_04388_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation04388Bank4[57] = {
#include "assets/m93r_animation_04388_bank4.inc"
};

static AnimationRecord _gM93rAnimation04388Records[87] = {
#include "assets/m93r_animation_04388_records.inc"
};

static u16 _gM93rAnimation04388Indices[20] = {
#include "assets/m93r_animation_04388_indices.inc"
};

static AnimationSet _gM93rAnimation04388 = {
    _gM93rAnimation04388Records,
    _gM93rAnimation04388Indices,
    { NULL, _gM93rAnimation04388Bank1, NULL, NULL, _gM93rAnimation04388Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0462CBank1[4] = {
#include "assets/m93r_animation_0462C_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0462CBank4[55] = {
#include "assets/m93r_animation_0462C_bank4.inc"
};

static AnimationRecord _gM93rAnimation0462CRecords[82] = {
#include "assets/m93r_animation_0462C_records.inc"
};

static u16 _gM93rAnimation0462CIndices[20] = {
#include "assets/m93r_animation_0462C_indices.inc"
};

static AnimationSet _gM93rAnimation0462C = {
    _gM93rAnimation0462CRecords,
    _gM93rAnimation0462CIndices,
    { NULL, _gM93rAnimation0462CBank1, NULL, NULL, _gM93rAnimation0462CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0482CBank1[3] = {
#include "assets/m93r_animation_0482C_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0482CBank4[23] = {
#include "assets/m93r_animation_0482C_bank4.inc"
};

static AnimationRecord _gM93rAnimation0482CRecords[76] = {
#include "assets/m93r_animation_0482C_records.inc"
};

static u16 _gM93rAnimation0482CIndices[20] = {
#include "assets/m93r_animation_0482C_indices.inc"
};

static AnimationSet _gM93rAnimation0482C = {
    _gM93rAnimation0482CRecords,
    _gM93rAnimation0482CIndices,
    { NULL, _gM93rAnimation0482CBank1, NULL, NULL, _gM93rAnimation0482CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation04B80Bank1[8] = {
#include "assets/m93r_animation_04B80_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation04B80Bank4[68] = {
#include "assets/m93r_animation_04B80_bank4.inc"
};

static AnimationRecord _gM93rAnimation04B80Records[101] = {
#include "assets/m93r_animation_04B80_records.inc"
};

static u16 _gM93rAnimation04B80Indices[20] = {
#include "assets/m93r_animation_04B80_indices.inc"
};

static AnimationSet _gM93rAnimation04B80 = {
    _gM93rAnimation04B80Records,
    _gM93rAnimation04B80Indices,
    { NULL, _gM93rAnimation04B80Bank1, NULL, NULL, _gM93rAnimation04B80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation04E34Bank1[5] = {
#include "assets/m93r_animation_04E34_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation04E34Bank4[55] = {
#include "assets/m93r_animation_04E34_bank4.inc"
};

static AnimationRecord _gM93rAnimation04E34Records[83] = {
#include "assets/m93r_animation_04E34_records.inc"
};

static u16 _gM93rAnimation04E34Indices[20] = {
#include "assets/m93r_animation_04E34_indices.inc"
};

static AnimationSet _gM93rAnimation04E34 = {
    _gM93rAnimation04E34Records,
    _gM93rAnimation04E34Indices,
    { NULL, _gM93rAnimation04E34Bank1, NULL, NULL, _gM93rAnimation04E34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation05154Bank1[6] = {
#include "assets/m93r_animation_05154_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation05154Bank4[66] = {
#include "assets/m93r_animation_05154_bank4.inc"
};

static AnimationRecord _gM93rAnimation05154Records[96] = {
#include "assets/m93r_animation_05154_records.inc"
};

static u16 _gM93rAnimation05154Indices[20] = {
#include "assets/m93r_animation_05154_indices.inc"
};

static AnimationSet _gM93rAnimation05154 = {
    _gM93rAnimation05154Records,
    _gM93rAnimation05154Indices,
    { NULL, _gM93rAnimation05154Bank1, NULL, NULL, _gM93rAnimation05154Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation05918Bank1[18] = {
#include "assets/m93r_animation_05918_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation05918Bank4[184] = {
#include "assets/m93r_animation_05918_bank4.inc"
};

static AnimationRecord _gM93rAnimation05918Records[239] = {
#include "assets/m93r_animation_05918_records.inc"
};

static u16 _gM93rAnimation05918Indices[20] = {
#include "assets/m93r_animation_05918_indices.inc"
};

static AnimationSet _gM93rAnimation05918 = {
    _gM93rAnimation05918Records,
    _gM93rAnimation05918Indices,
    { NULL, _gM93rAnimation05918Bank1, NULL, NULL, _gM93rAnimation05918Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation06BB0Bank1[29] = {
#include "assets/m93r_animation_06BB0_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation06BB0Bank4[450] = {
#include "assets/m93r_animation_06BB0_bank4.inc"
};

static AnimationRecord _gM93rAnimation06BB0Records[633] = {
#include "assets/m93r_animation_06BB0_records.inc"
};

static u16 _gM93rAnimation06BB0Indices[20] = {
#include "assets/m93r_animation_06BB0_indices.inc"
};

static AnimationSet _gM93rAnimation06BB0 = {
    _gM93rAnimation06BB0Records,
    _gM93rAnimation06BB0Indices,
    { NULL, _gM93rAnimation06BB0Bank1, NULL, NULL, _gM93rAnimation06BB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation07728Bank1[12] = {
#include "assets/m93r_animation_07728_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation07728Bank4[266] = {
#include "assets/m93r_animation_07728_bank4.inc"
};

static AnimationRecord _gM93rAnimation07728Records[412] = {
#include "assets/m93r_animation_07728_records.inc"
};

static u16 _gM93rAnimation07728Indices[20] = {
#include "assets/m93r_animation_07728_indices.inc"
};

static AnimationSet _gM93rAnimation07728 = {
    _gM93rAnimation07728Records,
    _gM93rAnimation07728Indices,
    { NULL, _gM93rAnimation07728Bank1, NULL, NULL, _gM93rAnimation07728Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation07E44Bank1[9] = {
#include "assets/m93r_animation_07E44_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation07E44Bank4[144] = {
#include "assets/m93r_animation_07E44_bank4.inc"
};

static AnimationRecord _gM93rAnimation07E44Records[264] = {
#include "assets/m93r_animation_07E44_records.inc"
};

static u16 _gM93rAnimation07E44Indices[20] = {
#include "assets/m93r_animation_07E44_indices.inc"
};

static AnimationSet _gM93rAnimation07E44 = {
    _gM93rAnimation07E44Records,
    _gM93rAnimation07E44Indices,
    { NULL, _gM93rAnimation07E44Bank1, NULL, NULL, _gM93rAnimation07E44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation082C4Bank1[6] = {
#include "assets/m93r_animation_082C4_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation082C4Bank4[107] = {
#include "assets/m93r_animation_082C4_bank4.inc"
};

static AnimationRecord _gM93rAnimation082C4Records[143] = {
#include "assets/m93r_animation_082C4_records.inc"
};

static u16 _gM93rAnimation082C4Indices[20] = {
#include "assets/m93r_animation_082C4_indices.inc"
};

static AnimationSet _gM93rAnimation082C4 = {
    _gM93rAnimation082C4Records,
    _gM93rAnimation082C4Indices,
    { NULL, _gM93rAnimation082C4Bank1, NULL, NULL, _gM93rAnimation082C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0849CBank1[3] = {
#include "assets/m93r_animation_0849C_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0849CBank4[32] = {
#include "assets/m93r_animation_0849C_bank4.inc"
};

static AnimationRecord _gM93rAnimation0849CRecords[57] = {
#include "assets/m93r_animation_0849C_records.inc"
};

static u16 _gM93rAnimation0849CIndices[20] = {
#include "assets/m93r_animation_0849C_indices.inc"
};

static AnimationSet _gM93rAnimation0849C = {
    _gM93rAnimation0849CRecords,
    _gM93rAnimation0849CIndices,
    { NULL, _gM93rAnimation0849CBank1, NULL, NULL, _gM93rAnimation0849CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation08A00Bank1[11] = {
#include "assets/m93r_animation_08A00_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation08A00Bank4[125] = {
#include "assets/m93r_animation_08A00_bank4.inc"
};

static AnimationRecord _gM93rAnimation08A00Records[167] = {
#include "assets/m93r_animation_08A00_records.inc"
};

static u16 _gM93rAnimation08A00Indices[20] = {
#include "assets/m93r_animation_08A00_indices.inc"
};

static AnimationSet _gM93rAnimation08A00 = {
    _gM93rAnimation08A00Records,
    _gM93rAnimation08A00Indices,
    { NULL, _gM93rAnimation08A00Bank1, NULL, NULL, _gM93rAnimation08A00Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation08BF4Bank1[3] = {
#include "assets/m93r_animation_08BF4_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation08BF4Bank4[20] = {
#include "assets/m93r_animation_08BF4_bank4.inc"
};

static AnimationRecord _gM93rAnimation08BF4Records[76] = {
#include "assets/m93r_animation_08BF4_records.inc"
};

static u16 _gM93rAnimation08BF4Indices[20] = {
#include "assets/m93r_animation_08BF4_indices.inc"
};

static AnimationSet _gM93rAnimation08BF4 = {
    _gM93rAnimation08BF4Records,
    _gM93rAnimation08BF4Indices,
    { NULL, _gM93rAnimation08BF4Bank1, NULL, NULL, _gM93rAnimation08BF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation09074Bank1[8] = {
#include "assets/m93r_animation_09074_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation09074Bank4[105] = {
#include "assets/m93r_animation_09074_bank4.inc"
};

static AnimationRecord _gM93rAnimation09074Records[139] = {
#include "assets/m93r_animation_09074_records.inc"
};

static u16 _gM93rAnimation09074Indices[20] = {
#include "assets/m93r_animation_09074_indices.inc"
};

static AnimationSet _gM93rAnimation09074 = {
    _gM93rAnimation09074Records,
    _gM93rAnimation09074Indices,
    { NULL, _gM93rAnimation09074Bank1, NULL, NULL, _gM93rAnimation09074Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation09250Bank1[2] = {
#include "assets/m93r_animation_09250_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation09250Bank4[17] = {
#include "assets/m93r_animation_09250_bank4.inc"
};

static AnimationRecord _gM93rAnimation09250Records[76] = {
#include "assets/m93r_animation_09250_records.inc"
};

static u16 _gM93rAnimation09250Indices[20] = {
#include "assets/m93r_animation_09250_indices.inc"
};

static AnimationSet _gM93rAnimation09250 = {
    _gM93rAnimation09250Records,
    _gM93rAnimation09250Indices,
    { NULL, _gM93rAnimation09250Bank1, NULL, NULL, _gM93rAnimation09250Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation09900Bank1[11] = {
#include "assets/m93r_animation_09900_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation09900Bank4[151] = {
#include "assets/m93r_animation_09900_bank4.inc"
};

static AnimationRecord _gM93rAnimation09900Records[224] = {
#include "assets/m93r_animation_09900_records.inc"
};

static u16 _gM93rAnimation09900Indices[20] = {
#include "assets/m93r_animation_09900_indices.inc"
};

static AnimationSet _gM93rAnimation09900 = {
    _gM93rAnimation09900Records,
    _gM93rAnimation09900Indices,
    { NULL, _gM93rAnimation09900Bank1, NULL, NULL, _gM93rAnimation09900Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0A4ECBank1[19] = {
#include "assets/m93r_animation_0A4EC_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0A4ECBank4[304] = {
#include "assets/m93r_animation_0A4EC_bank4.inc"
};

static AnimationRecord _gM93rAnimation0A4ECRecords[382] = {
#include "assets/m93r_animation_0A4EC_records.inc"
};

static u16 _gM93rAnimation0A4ECIndices[20] = {
#include "assets/m93r_animation_0A4EC_indices.inc"
};

static AnimationSet _gM93rAnimation0A4EC = {
    _gM93rAnimation0A4ECRecords,
    _gM93rAnimation0A4ECIndices,
    { NULL, _gM93rAnimation0A4ECBank1, NULL, NULL, _gM93rAnimation0A4ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0AF84Bank1[18] = {
#include "assets/m93r_animation_0AF84_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0AF84Bank4[279] = {
#include "assets/m93r_animation_0AF84_bank4.inc"
};

static AnimationRecord _gM93rAnimation0AF84Records[325] = {
#include "assets/m93r_animation_0AF84_records.inc"
};

static u16 _gM93rAnimation0AF84Indices[20] = {
#include "assets/m93r_animation_0AF84_indices.inc"
};

static AnimationSet _gM93rAnimation0AF84 = {
    _gM93rAnimation0AF84Records,
    _gM93rAnimation0AF84Indices,
    { NULL, _gM93rAnimation0AF84Bank1, NULL, NULL, _gM93rAnimation0AF84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0B4D4Bank1[8] = {
#include "assets/m93r_animation_0B4D4_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0B4D4Bank4[120] = {
#include "assets/m93r_animation_0B4D4_bank4.inc"
};

static AnimationRecord _gM93rAnimation0B4D4Records[176] = {
#include "assets/m93r_animation_0B4D4_records.inc"
};

static u16 _gM93rAnimation0B4D4Indices[20] = {
#include "assets/m93r_animation_0B4D4_indices.inc"
};

static AnimationSet _gM93rAnimation0B4D4 = {
    _gM93rAnimation0B4D4Records,
    _gM93rAnimation0B4D4Indices,
    { NULL, _gM93rAnimation0B4D4Bank1, NULL, NULL, _gM93rAnimation0B4D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0BBE8Bank1[12] = {
#include "assets/m93r_animation_0BBE8_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0BBE8Bank4[179] = {
#include "assets/m93r_animation_0BBE8_bank4.inc"
};

static AnimationRecord _gM93rAnimation0BBE8Records[218] = {
#include "assets/m93r_animation_0BBE8_records.inc"
};

static u16 _gM93rAnimation0BBE8Indices[20] = {
#include "assets/m93r_animation_0BBE8_indices.inc"
};

static AnimationSet _gM93rAnimation0BBE8 = {
    _gM93rAnimation0BBE8Records,
    _gM93rAnimation0BBE8Indices,
    { NULL, _gM93rAnimation0BBE8Bank1, NULL, NULL, _gM93rAnimation0BBE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0C2B8Bank1[12] = {
#include "assets/m93r_animation_0C2B8_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0C2B8Bank4[170] = {
#include "assets/m93r_animation_0C2B8_bank4.inc"
};

static AnimationRecord _gM93rAnimation0C2B8Records[210] = {
#include "assets/m93r_animation_0C2B8_records.inc"
};

static u16 _gM93rAnimation0C2B8Indices[20] = {
#include "assets/m93r_animation_0C2B8_indices.inc"
};

static AnimationSet _gM93rAnimation0C2B8 = {
    _gM93rAnimation0C2B8Records,
    _gM93rAnimation0C2B8Indices,
    { NULL, _gM93rAnimation0C2B8Bank1, NULL, NULL, _gM93rAnimation0C2B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0CC04Bank1[16] = {
#include "assets/m93r_animation_0CC04_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0CC04Bank4[221] = {
#include "assets/m93r_animation_0CC04_bank4.inc"
};

static AnimationRecord _gM93rAnimation0CC04Records[306] = {
#include "assets/m93r_animation_0CC04_records.inc"
};

static u16 _gM93rAnimation0CC04Indices[20] = {
#include "assets/m93r_animation_0CC04_indices.inc"
};

static AnimationSet _gM93rAnimation0CC04 = {
    _gM93rAnimation0CC04Records,
    _gM93rAnimation0CC04Indices,
    { NULL, _gM93rAnimation0CC04Bank1, NULL, NULL, _gM93rAnimation0CC04Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0D4DCBank1[19] = {
#include "assets/m93r_animation_0D4DC_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0D4DCBank4[193] = {
#include "assets/m93r_animation_0D4DC_bank4.inc"
};

static AnimationRecord _gM93rAnimation0D4DCRecords[296] = {
#include "assets/m93r_animation_0D4DC_records.inc"
};

static u16 _gM93rAnimation0D4DCIndices[20] = {
#include "assets/m93r_animation_0D4DC_indices.inc"
};

static AnimationSet _gM93rAnimation0D4DC = {
    _gM93rAnimation0D4DCRecords,
    _gM93rAnimation0D4DCIndices,
    { NULL, _gM93rAnimation0D4DCBank1, NULL, NULL, _gM93rAnimation0D4DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM93rAnimation0DC10Bank1[13] = {
#include "assets/m93r_animation_0DC10_bank1.inc"
};

static AnimationPackedRotation _gM93rAnimation0DC10Bank4[167] = {
#include "assets/m93r_animation_0DC10_bank4.inc"
};

static AnimationRecord _gM93rAnimation0DC10Records[235] = {
#include "assets/m93r_animation_0DC10_records.inc"
};

static u16 _gM93rAnimation0DC10Indices[20] = {
#include "assets/m93r_animation_0DC10_indices.inc"
};

static AnimationSet _gM93rAnimation0DC10 = {
    _gM93rAnimation0DC10Records,
    _gM93rAnimation0DC10Indices,
    { NULL, _gM93rAnimation0DC10Bank1, NULL, NULL, _gM93rAnimation0DC10Bank4, NULL, NULL, NULL },
};

AnimationBank D_m93r_8012ADF8 = { { {
    NULL,
    &_gM93rAnimation00A68,
    &_gM93rAnimation0CC04,
    &_gM93rAnimation0D4DC,
    &_gM93rAnimation0DC10,
    &_gM93rAnimation0196C,
    &_gM93rAnimation021D0,
    &_gM93rAnimation0BBE8,
    &_gM93rAnimation0C2B8,
    &_gM93rAnimation09250,
    &_gM93rAnimation0B4D4,
    &_gM93rAnimation00A68,
    &_gM93rAnimation0A4EC,
    &_gM93rAnimation09900,
    &_gM93rAnimation0AF84,
    &_gM93rAnimation00A68,
    &_gM93rAnimation04E34,
    &_gM93rAnimation05154,
    &_gM93rAnimation05918,
    &_gM93rAnimation0110C,
    &_gM93rAnimation0AF84,
    &_gM93rAnimation00A68,
    &_gM93rAnimation00A68,
    &_gM93rAnimation06BB0,
    &_gM93rAnimation07E44,
    &_gM93rAnimation07728,
    &_gM93rAnimation03EB4,
    &_gM93rAnimation040B0,
    &_gM93rAnimation04388,
    &_gM93rAnimation0462C,
    &_gM93rAnimation0482C,
    &_gM93rAnimation04B80,
    &_gM93rAnimation082C4,
    &_gM93rAnimation0849C,
    &_gM93rAnimation0BBE8,
    &_gM93rAnimation0C2B8,
    &_gM93rAnimation02C40,
    &_gM93rAnimation033C8,
    &_gM93rAnimation03A2C,
    &_gM93rAnimation0369C,
    &_gM93rAnimation024E4,
    &_gM93rAnimation00A68,
    &_gM93rAnimation08A00,
    &_gM93rAnimation08BF4,
    &_gM93rAnimation09074,
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
