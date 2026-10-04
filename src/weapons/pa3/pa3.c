#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/weapon.h"

/// The PA3 and SP12 shotguns are this source built once each, and each declares
/// these values in the manifest. `WEAPON_ID` is the weapon's index (0xD and
/// 0xE), which keys the sound bank, the shot effect and the item.
/// `PA3_FIELD_979` is the value `field_979` is primed with when the shot is
/// armed.
#if !defined(WEAPON_ID) || !defined(PA3_FIELD_979)
#error "WEAPON_ID and PA3_FIELD_979 are per-package build parameters"
#endif

/* gameplay's weapon table names each package's attack handler, so each build of
 * this source gives the handler its own package's name. */
#if WEAPON_ID == 0xE
#define func_pa3_8011D1DC func_sp12_8011D1DC
#endif

void func_pa3_8011D1DC(Task* arg0);

/// Per-frame firing state machine for the shotgun. Case 0 arms the shot -
/// clearing the recoil counters, priming the `field_979` grace at `PA3_FIELD_979` and the
/// `field_934` frame delay at 0x1F - and queues the ready animation, using the
/// long variant when the weapon was left dirty (`field_958`) or the actor is
/// flagged in `field_975`; the muzzle grip bit in `field_12A` is set only for
/// the 0xE weapon variant. Case 1 waits for that animation to reach its second
/// slot. Case 2 fires, consuming the weapon's item, playing the report and spawning the
/// flash. Case 3 re-acquires the lock-on target, sourcing the impact sound from
/// the actor's own contact point on the 0xE variant. Case 4 runs out the
/// `field_934` delay before playing the pump-action sound, and case 5 runs out
/// the `field_979` grace and otherwise hands back to `func_80106550`.
void func_pa3_8011D1DC(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    GfxCoord*  spot;
    s32        anim;
    s32        hit;

    SCRATCH_STACK_RESERVE_BYTES(0x50);
    spot  = SCRATCH_STACK_CURSOR(GfxCoord);
    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (actor->statePhase) {
        case 0:
            actor->state             = 4;
            actor->statePhase        = 1;
            actor->attackCancelTicks = PA3_FIELD_979;
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->rumblePosted      = 0;
            actor->stateTimer        = 0x1F;
            func_80106238(arg0, 0, 0);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x400;
            if (gPlayerStatus.weaponSlotItem == 0xE) {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x800;
            } else {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= ~0x800;
            }
            anim = 1;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                anim = 8;
            }
            playerActorPlayChildSlotsWithBlend(arg0, 9, 0, anim);
            actor->movementMode = 0;
            /* fallthrough */
        case 1:
            if (Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case 2:
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            Gp_ConsumeSlotQty(WEAPON_ITEM(WEAPON_ID), 1);
            Gp_PlayObjSfx(arg0->extra.tmd->coords,
                          ((gPlayerStatus.weaponSlotItem - 0xD) << 0x18) | 0x20000005 | (WEAPON_ID << 16), 1);
            Gp_SpawnEff(EFFECT_SHOTGUN_MUZZLE_FLASH,
                        actor->equipmentTasks[1]->extra.tmd->coords,
                        (gPlayerStatus.weaponSlotItem << 0x10) | WEAPON_ID, NULL);
            playerActorPlayChildSlotsWithBlend(arg0, 0xA, 1, 3);
            break;
        case 3:
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (gPlayerStatus.weaponSlotItem != 0xD) {
                hit = Gp_PickNearestRec18(actor->weaponContacts, coord, spot);
                if (gPlayerStatus.weaponSlotItem == 0xE) {
                    if (hit != 0 || Gp_CountRec18Hi(actor->weaponContacts, 0x30000) != 0) {
                        spot->workm.t[0] = actor->weaponContacts[0].point.vx;
                        spot->workm.t[1] = actor->weaponContacts[0].point.vy;
                        spot->workm.t[2] = actor->weaponContacts[0].point.vz;
                        Gp_PlayObjSfx(spot,
                                      ((gPlayerStatus.weaponSlotItem - 0xD) << 0x18) | 0x20000004 | (WEAPON_ID << 16), 1);
                    }
                } else if (hit != 0) {
                    Gp_PlayObjSfx(spot, 0x17, 1);
                }
            }
            /* fallthrough */
        case 4:
            if (--actor->stateTimer == 0) {
                actor->statePhase++;
                Gp_PlayObjSfx(arg0->extra.tmd->coords,
                              ((gPlayerStatus.weaponSlotItem - 0xD) << 0x18) | 0x20000002 | (WEAPON_ID << 16), 0);
            }
            /* fallthrough */
        case 5:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = 1;
                func_80106550(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x50);
}

/* Each package carries its own model. */
#if WEAPON_ID == 0xD
static TmdBone _gPa3Model0056CSkeleton[1] = {
#include "assets/pa3_model_0056C_skeleton.inc"
};

static u32 _gPa3Model0056CPartVerts[1] = {
#include "assets/pa3_model_0056C_partVerts.inc"
};

static SVECTOR _gPa3Model0056CVerts[26] = {
#include "assets/pa3_model_0056C_verts.inc"
};

static SVECTOR _gPa3Model0056CNormals[26] = {
#include "assets/pa3_model_0056C_normals.inc"
};

static u32 _gPa3Model0056CStream[182] = {
#include "assets/pa3_model_0056C_stream.inc"
};

TmdSource D_pa3_8011DA04 = {
    0,
    1276,
    0,
    1,
    _gPa3Model0056CPartVerts,
    _gPa3Model0056CVerts,
    _gPa3Model0056CNormals,
    _gPa3Model0056CSkeleton,
    _gPa3Model0056CStream,
};
#elif WEAPON_ID == 0xE
static TmdBone _gSp12Model005CCSkeleton[1] = {
#include "assets/sp12_model_005CC_skeleton.inc"
};

static u32 _gSp12Model005CCPartVerts[1] = {
#include "assets/sp12_model_005CC_partVerts.inc"
};

static SVECTOR _gSp12Model005CCVerts[34] = {
#include "assets/sp12_model_005CC_verts.inc"
};

static SVECTOR _gSp12Model005CCNormals[30] = {
#include "assets/sp12_model_005CC_normals.inc"
};

static u32 _gSp12Model005CCStream[238] = {
#include "assets/sp12_model_005CC_stream.inc"
};

TmdSource D_sp12_8011DB44 = {
    0,
    1692,
    0,
    1,
    _gSp12Model005CCPartVerts,
    _gSp12Model005CCVerts,
    _gSp12Model005CCNormals,
    _gSp12Model005CCSkeleton,
    _gSp12Model005CCStream,
};
#endif

/* Each package carries its own animation bank. */
#if WEAPON_ID == 0xD
static AnimationPackedPose _gPa3Animation009F8Bank1[2] = {
#include "assets/pa3_animation_009F8_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation009F8Bank4[8] = {
#include "assets/pa3_animation_009F8_bank4.inc"
};

static AnimationRecord _gPa3Animation009F8Records[76] = {
#include "assets/pa3_animation_009F8_records.inc"
};

static u16 _gPa3Animation009F8Indices[20] = {
#include "assets/pa3_animation_009F8_indices.inc"
};

static AnimationSet _gPa3Animation009F8 = {
    _gPa3Animation009F8Records,
    _gPa3Animation009F8Indices,
    { NULL, _gPa3Animation009F8Bank1, NULL, NULL, _gPa3Animation009F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0109CBank1[12] = {
#include "assets/pa3_animation_0109C_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0109CBank4[151] = {
#include "assets/pa3_animation_0109C_bank4.inc"
};

static AnimationRecord _gPa3Animation0109CRecords[218] = {
#include "assets/pa3_animation_0109C_records.inc"
};

static u16 _gPa3Animation0109CIndices[20] = {
#include "assets/pa3_animation_0109C_indices.inc"
};

static AnimationSet _gPa3Animation0109C = {
    _gPa3Animation0109CRecords,
    _gPa3Animation0109CIndices,
    { NULL, _gPa3Animation0109CBank1, NULL, NULL, _gPa3Animation0109CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation018FCBank1[19] = {
#include "assets/pa3_animation_018FC_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation018FCBank4[169] = {
#include "assets/pa3_animation_018FC_bank4.inc"
};

static AnimationRecord _gPa3Animation018FCRecords[290] = {
#include "assets/pa3_animation_018FC_records.inc"
};

static u16 _gPa3Animation018FCIndices[20] = {
#include "assets/pa3_animation_018FC_indices.inc"
};

static AnimationSet _gPa3Animation018FC = {
    _gPa3Animation018FCRecords,
    _gPa3Animation018FCIndices,
    { NULL, _gPa3Animation018FCBank1, NULL, NULL, _gPa3Animation018FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation02160Bank1[19] = {
#include "assets/pa3_animation_02160_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation02160Bank4[170] = {
#include "assets/pa3_animation_02160_bank4.inc"
};

static AnimationRecord _gPa3Animation02160Records[290] = {
#include "assets/pa3_animation_02160_records.inc"
};

static u16 _gPa3Animation02160Indices[20] = {
#include "assets/pa3_animation_02160_indices.inc"
};

static AnimationSet _gPa3Animation02160 = {
    _gPa3Animation02160Records,
    _gPa3Animation02160Indices,
    { NULL, _gPa3Animation02160Bank1, NULL, NULL, _gPa3Animation02160Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation02474Bank1[3] = {
#include "assets/pa3_animation_02474_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation02474Bank4[69] = {
#include "assets/pa3_animation_02474_bank4.inc"
};

static AnimationRecord _gPa3Animation02474Records[99] = {
#include "assets/pa3_animation_02474_records.inc"
};

static u16 _gPa3Animation02474Indices[20] = {
#include "assets/pa3_animation_02474_indices.inc"
};

static AnimationSet _gPa3Animation02474 = {
    _gPa3Animation02474Records,
    _gPa3Animation02474Indices,
    { NULL, _gPa3Animation02474Bank1, NULL, NULL, _gPa3Animation02474Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation02BD0Bank1[14] = {
#include "assets/pa3_animation_02BD0_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation02BD0Bank4[156] = {
#include "assets/pa3_animation_02BD0_bank4.inc"
};

static AnimationRecord _gPa3Animation02BD0Records[253] = {
#include "assets/pa3_animation_02BD0_records.inc"
};

static u16 _gPa3Animation02BD0Indices[20] = {
#include "assets/pa3_animation_02BD0_indices.inc"
};

static AnimationSet _gPa3Animation02BD0 = {
    _gPa3Animation02BD0Records,
    _gPa3Animation02BD0Indices,
    { NULL, _gPa3Animation02BD0Bank1, NULL, NULL, _gPa3Animation02BD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation03358Bank1[16] = {
#include "assets/pa3_animation_03358_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation03358Bank4[167] = {
#include "assets/pa3_animation_03358_bank4.inc"
};

static AnimationRecord _gPa3Animation03358Records[247] = {
#include "assets/pa3_animation_03358_records.inc"
};

static u16 _gPa3Animation03358Indices[20] = {
#include "assets/pa3_animation_03358_indices.inc"
};

static AnimationSet _gPa3Animation03358 = {
    _gPa3Animation03358Records,
    _gPa3Animation03358Indices,
    { NULL, _gPa3Animation03358Bank1, NULL, NULL, _gPa3Animation03358Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0362CBank1[6] = {
#include "assets/pa3_animation_0362C_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0362CBank4[52] = {
#include "assets/pa3_animation_0362C_bank4.inc"
};

static AnimationRecord _gPa3Animation0362CRecords[91] = {
#include "assets/pa3_animation_0362C_records.inc"
};

static u16 _gPa3Animation0362CIndices[20] = {
#include "assets/pa3_animation_0362C_indices.inc"
};

static AnimationSet _gPa3Animation0362C = {
    _gPa3Animation0362CRecords,
    _gPa3Animation0362CIndices,
    { NULL, _gPa3Animation0362CBank1, NULL, NULL, _gPa3Animation0362CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation039BCBank1[7] = {
#include "assets/pa3_animation_039BC_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation039BCBank4[73] = {
#include "assets/pa3_animation_039BC_bank4.inc"
};

static AnimationRecord _gPa3Animation039BCRecords[114] = {
#include "assets/pa3_animation_039BC_records.inc"
};

static u16 _gPa3Animation039BCIndices[20] = {
#include "assets/pa3_animation_039BC_indices.inc"
};

static AnimationSet _gPa3Animation039BC = {
    _gPa3Animation039BCRecords,
    _gPa3Animation039BCIndices,
    { NULL, _gPa3Animation039BCBank1, NULL, NULL, _gPa3Animation039BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation03E44Bank1[9] = {
#include "assets/pa3_animation_03E44_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation03E44Bank4[104] = {
#include "assets/pa3_animation_03E44_bank4.inc"
};

static AnimationRecord _gPa3Animation03E44Records[139] = {
#include "assets/pa3_animation_03E44_records.inc"
};

static u16 _gPa3Animation03E44Indices[20] = {
#include "assets/pa3_animation_03E44_indices.inc"
};

static AnimationSet _gPa3Animation03E44 = {
    _gPa3Animation03E44Records,
    _gPa3Animation03E44Indices,
    { NULL, _gPa3Animation03E44Bank1, NULL, NULL, _gPa3Animation03E44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation04040Bank1[3] = {
#include "assets/pa3_animation_04040_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation04040Bank4[22] = {
#include "assets/pa3_animation_04040_bank4.inc"
};

static AnimationRecord _gPa3Animation04040Records[76] = {
#include "assets/pa3_animation_04040_records.inc"
};

static u16 _gPa3Animation04040Indices[20] = {
#include "assets/pa3_animation_04040_indices.inc"
};

static AnimationSet _gPa3Animation04040 = {
    _gPa3Animation04040Records,
    _gPa3Animation04040Indices,
    { NULL, _gPa3Animation04040Bank1, NULL, NULL, _gPa3Animation04040Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation04318Bank1[6] = {
#include "assets/pa3_animation_04318_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation04318Bank4[57] = {
#include "assets/pa3_animation_04318_bank4.inc"
};

static AnimationRecord _gPa3Animation04318Records[87] = {
#include "assets/pa3_animation_04318_records.inc"
};

static u16 _gPa3Animation04318Indices[20] = {
#include "assets/pa3_animation_04318_indices.inc"
};

static AnimationSet _gPa3Animation04318 = {
    _gPa3Animation04318Records,
    _gPa3Animation04318Indices,
    { NULL, _gPa3Animation04318Bank1, NULL, NULL, _gPa3Animation04318Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation045BCBank1[4] = {
#include "assets/pa3_animation_045BC_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation045BCBank4[55] = {
#include "assets/pa3_animation_045BC_bank4.inc"
};

static AnimationRecord _gPa3Animation045BCRecords[82] = {
#include "assets/pa3_animation_045BC_records.inc"
};

static u16 _gPa3Animation045BCIndices[20] = {
#include "assets/pa3_animation_045BC_indices.inc"
};

static AnimationSet _gPa3Animation045BC = {
    _gPa3Animation045BCRecords,
    _gPa3Animation045BCIndices,
    { NULL, _gPa3Animation045BCBank1, NULL, NULL, _gPa3Animation045BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation047BCBank1[3] = {
#include "assets/pa3_animation_047BC_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation047BCBank4[23] = {
#include "assets/pa3_animation_047BC_bank4.inc"
};

static AnimationRecord _gPa3Animation047BCRecords[76] = {
#include "assets/pa3_animation_047BC_records.inc"
};

static u16 _gPa3Animation047BCIndices[20] = {
#include "assets/pa3_animation_047BC_indices.inc"
};

static AnimationSet _gPa3Animation047BC = {
    _gPa3Animation047BCRecords,
    _gPa3Animation047BCIndices,
    { NULL, _gPa3Animation047BCBank1, NULL, NULL, _gPa3Animation047BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation04B10Bank1[8] = {
#include "assets/pa3_animation_04B10_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation04B10Bank4[68] = {
#include "assets/pa3_animation_04B10_bank4.inc"
};

static AnimationRecord _gPa3Animation04B10Records[101] = {
#include "assets/pa3_animation_04B10_records.inc"
};

static u16 _gPa3Animation04B10Indices[20] = {
#include "assets/pa3_animation_04B10_indices.inc"
};

static AnimationSet _gPa3Animation04B10 = {
    _gPa3Animation04B10Records,
    _gPa3Animation04B10Indices,
    { NULL, _gPa3Animation04B10Bank1, NULL, NULL, _gPa3Animation04B10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation04DC4Bank1[5] = {
#include "assets/pa3_animation_04DC4_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation04DC4Bank4[55] = {
#include "assets/pa3_animation_04DC4_bank4.inc"
};

static AnimationRecord _gPa3Animation04DC4Records[83] = {
#include "assets/pa3_animation_04DC4_records.inc"
};

static u16 _gPa3Animation04DC4Indices[20] = {
#include "assets/pa3_animation_04DC4_indices.inc"
};

static AnimationSet _gPa3Animation04DC4 = {
    _gPa3Animation04DC4Records,
    _gPa3Animation04DC4Indices,
    { NULL, _gPa3Animation04DC4Bank1, NULL, NULL, _gPa3Animation04DC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation050E4Bank1[6] = {
#include "assets/pa3_animation_050E4_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation050E4Bank4[66] = {
#include "assets/pa3_animation_050E4_bank4.inc"
};

static AnimationRecord _gPa3Animation050E4Records[96] = {
#include "assets/pa3_animation_050E4_records.inc"
};

static u16 _gPa3Animation050E4Indices[20] = {
#include "assets/pa3_animation_050E4_indices.inc"
};

static AnimationSet _gPa3Animation050E4 = {
    _gPa3Animation050E4Records,
    _gPa3Animation050E4Indices,
    { NULL, _gPa3Animation050E4Bank1, NULL, NULL, _gPa3Animation050E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation058A8Bank1[18] = {
#include "assets/pa3_animation_058A8_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation058A8Bank4[184] = {
#include "assets/pa3_animation_058A8_bank4.inc"
};

static AnimationRecord _gPa3Animation058A8Records[239] = {
#include "assets/pa3_animation_058A8_records.inc"
};

static u16 _gPa3Animation058A8Indices[20] = {
#include "assets/pa3_animation_058A8_indices.inc"
};

static AnimationSet _gPa3Animation058A8 = {
    _gPa3Animation058A8Records,
    _gPa3Animation058A8Indices,
    { NULL, _gPa3Animation058A8Bank1, NULL, NULL, _gPa3Animation058A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation06B40Bank1[29] = {
#include "assets/pa3_animation_06B40_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation06B40Bank4[450] = {
#include "assets/pa3_animation_06B40_bank4.inc"
};

static AnimationRecord _gPa3Animation06B40Records[633] = {
#include "assets/pa3_animation_06B40_records.inc"
};

static u16 _gPa3Animation06B40Indices[20] = {
#include "assets/pa3_animation_06B40_indices.inc"
};

static AnimationSet _gPa3Animation06B40 = {
    _gPa3Animation06B40Records,
    _gPa3Animation06B40Indices,
    { NULL, _gPa3Animation06B40Bank1, NULL, NULL, _gPa3Animation06B40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation076B8Bank1[12] = {
#include "assets/pa3_animation_076B8_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation076B8Bank4[266] = {
#include "assets/pa3_animation_076B8_bank4.inc"
};

static AnimationRecord _gPa3Animation076B8Records[412] = {
#include "assets/pa3_animation_076B8_records.inc"
};

static u16 _gPa3Animation076B8Indices[20] = {
#include "assets/pa3_animation_076B8_indices.inc"
};

static AnimationSet _gPa3Animation076B8 = {
    _gPa3Animation076B8Records,
    _gPa3Animation076B8Indices,
    { NULL, _gPa3Animation076B8Bank1, NULL, NULL, _gPa3Animation076B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation07DD4Bank1[9] = {
#include "assets/pa3_animation_07DD4_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation07DD4Bank4[144] = {
#include "assets/pa3_animation_07DD4_bank4.inc"
};

static AnimationRecord _gPa3Animation07DD4Records[264] = {
#include "assets/pa3_animation_07DD4_records.inc"
};

static u16 _gPa3Animation07DD4Indices[20] = {
#include "assets/pa3_animation_07DD4_indices.inc"
};

static AnimationSet _gPa3Animation07DD4 = {
    _gPa3Animation07DD4Records,
    _gPa3Animation07DD4Indices,
    { NULL, _gPa3Animation07DD4Bank1, NULL, NULL, _gPa3Animation07DD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation08254Bank1[6] = {
#include "assets/pa3_animation_08254_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation08254Bank4[107] = {
#include "assets/pa3_animation_08254_bank4.inc"
};

static AnimationRecord _gPa3Animation08254Records[143] = {
#include "assets/pa3_animation_08254_records.inc"
};

static u16 _gPa3Animation08254Indices[20] = {
#include "assets/pa3_animation_08254_indices.inc"
};

static AnimationSet _gPa3Animation08254 = {
    _gPa3Animation08254Records,
    _gPa3Animation08254Indices,
    { NULL, _gPa3Animation08254Bank1, NULL, NULL, _gPa3Animation08254Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0842CBank1[3] = {
#include "assets/pa3_animation_0842C_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0842CBank4[32] = {
#include "assets/pa3_animation_0842C_bank4.inc"
};

static AnimationRecord _gPa3Animation0842CRecords[57] = {
#include "assets/pa3_animation_0842C_records.inc"
};

static u16 _gPa3Animation0842CIndices[20] = {
#include "assets/pa3_animation_0842C_indices.inc"
};

static AnimationSet _gPa3Animation0842C = {
    _gPa3Animation0842CRecords,
    _gPa3Animation0842CIndices,
    { NULL, _gPa3Animation0842CBank1, NULL, NULL, _gPa3Animation0842CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation08990Bank1[11] = {
#include "assets/pa3_animation_08990_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation08990Bank4[125] = {
#include "assets/pa3_animation_08990_bank4.inc"
};

static AnimationRecord _gPa3Animation08990Records[167] = {
#include "assets/pa3_animation_08990_records.inc"
};

static u16 _gPa3Animation08990Indices[20] = {
#include "assets/pa3_animation_08990_indices.inc"
};

static AnimationSet _gPa3Animation08990 = {
    _gPa3Animation08990Records,
    _gPa3Animation08990Indices,
    { NULL, _gPa3Animation08990Bank1, NULL, NULL, _gPa3Animation08990Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation08B84Bank1[3] = {
#include "assets/pa3_animation_08B84_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation08B84Bank4[20] = {
#include "assets/pa3_animation_08B84_bank4.inc"
};

static AnimationRecord _gPa3Animation08B84Records[76] = {
#include "assets/pa3_animation_08B84_records.inc"
};

static u16 _gPa3Animation08B84Indices[20] = {
#include "assets/pa3_animation_08B84_indices.inc"
};

static AnimationSet _gPa3Animation08B84 = {
    _gPa3Animation08B84Records,
    _gPa3Animation08B84Indices,
    { NULL, _gPa3Animation08B84Bank1, NULL, NULL, _gPa3Animation08B84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation09004Bank1[8] = {
#include "assets/pa3_animation_09004_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation09004Bank4[105] = {
#include "assets/pa3_animation_09004_bank4.inc"
};

static AnimationRecord _gPa3Animation09004Records[139] = {
#include "assets/pa3_animation_09004_records.inc"
};

static u16 _gPa3Animation09004Indices[20] = {
#include "assets/pa3_animation_09004_indices.inc"
};

static AnimationSet _gPa3Animation09004 = {
    _gPa3Animation09004Records,
    _gPa3Animation09004Indices,
    { NULL, _gPa3Animation09004Bank1, NULL, NULL, _gPa3Animation09004Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation091E0Bank1[2] = {
#include "assets/pa3_animation_091E0_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation091E0Bank4[17] = {
#include "assets/pa3_animation_091E0_bank4.inc"
};

static AnimationRecord _gPa3Animation091E0Records[76] = {
#include "assets/pa3_animation_091E0_records.inc"
};

static u16 _gPa3Animation091E0Indices[20] = {
#include "assets/pa3_animation_091E0_indices.inc"
};

static AnimationSet _gPa3Animation091E0 = {
    _gPa3Animation091E0Records,
    _gPa3Animation091E0Indices,
    { NULL, _gPa3Animation091E0Bank1, NULL, NULL, _gPa3Animation091E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation09984Bank1[13] = {
#include "assets/pa3_animation_09984_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation09984Bank4[177] = {
#include "assets/pa3_animation_09984_bank4.inc"
};

static AnimationRecord _gPa3Animation09984Records[253] = {
#include "assets/pa3_animation_09984_records.inc"
};

static u16 _gPa3Animation09984Indices[20] = {
#include "assets/pa3_animation_09984_indices.inc"
};

static AnimationSet _gPa3Animation09984 = {
    _gPa3Animation09984Records,
    _gPa3Animation09984Indices,
    { NULL, _gPa3Animation09984Bank1, NULL, NULL, _gPa3Animation09984Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0A42CBank1[19] = {
#include "assets/pa3_animation_0A42C_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0A42CBank4[269] = {
#include "assets/pa3_animation_0A42C_bank4.inc"
};

static AnimationRecord _gPa3Animation0A42CRecords[336] = {
#include "assets/pa3_animation_0A42C_records.inc"
};

static u16 _gPa3Animation0A42CIndices[20] = {
#include "assets/pa3_animation_0A42C_indices.inc"
};

static AnimationSet _gPa3Animation0A42C = {
    _gPa3Animation0A42CRecords,
    _gPa3Animation0A42CIndices,
    { NULL, _gPa3Animation0A42CBank1, NULL, NULL, _gPa3Animation0A42CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0B590Bank1[30] = {
#include "assets/pa3_animation_0B590_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0B590Bank4[461] = {
#include "assets/pa3_animation_0B590_bank4.inc"
};

static AnimationRecord _gPa3Animation0B590Records[542] = {
#include "assets/pa3_animation_0B590_records.inc"
};

static u16 _gPa3Animation0B590Indices[20] = {
#include "assets/pa3_animation_0B590_indices.inc"
};

static AnimationSet _gPa3Animation0B590 = {
    _gPa3Animation0B590Records,
    _gPa3Animation0B590Indices,
    { NULL, _gPa3Animation0B590Bank1, NULL, NULL, _gPa3Animation0B590Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0C348Bank1[23] = {
#include "assets/pa3_animation_0C348_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0C348Bank4[354] = {
#include "assets/pa3_animation_0C348_bank4.inc"
};

static AnimationRecord _gPa3Animation0C348Records[435] = {
#include "assets/pa3_animation_0C348_records.inc"
};

static u16 _gPa3Animation0C348Indices[20] = {
#include "assets/pa3_animation_0C348_indices.inc"
};

static AnimationSet _gPa3Animation0C348 = {
    _gPa3Animation0C348Records,
    _gPa3Animation0C348Indices,
    { NULL, _gPa3Animation0C348Bank1, NULL, NULL, _gPa3Animation0C348Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0CAA0Bank1[13] = {
#include "assets/pa3_animation_0CAA0_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0CAA0Bank4[184] = {
#include "assets/pa3_animation_0CAA0_bank4.inc"
};

static AnimationRecord _gPa3Animation0CAA0Records[227] = {
#include "assets/pa3_animation_0CAA0_records.inc"
};

static u16 _gPa3Animation0CAA0Indices[20] = {
#include "assets/pa3_animation_0CAA0_indices.inc"
};

static AnimationSet _gPa3Animation0CAA0 = {
    _gPa3Animation0CAA0Records,
    _gPa3Animation0CAA0Indices,
    { NULL, _gPa3Animation0CAA0Bank1, NULL, NULL, _gPa3Animation0CAA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0D578Bank1[19] = {
#include "assets/pa3_animation_0D578_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0D578Bank4[288] = {
#include "assets/pa3_animation_0D578_bank4.inc"
};

static AnimationRecord _gPa3Animation0D578Records[329] = {
#include "assets/pa3_animation_0D578_records.inc"
};

static u16 _gPa3Animation0D578Indices[20] = {
#include "assets/pa3_animation_0D578_indices.inc"
};

static AnimationSet _gPa3Animation0D578 = {
    _gPa3Animation0D578Records,
    _gPa3Animation0D578Indices,
    { NULL, _gPa3Animation0D578Bank1, NULL, NULL, _gPa3Animation0D578Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0DE30Bank1[18] = {
#include "assets/pa3_animation_0DE30_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0DE30Bank4[201] = {
#include "assets/pa3_animation_0DE30_bank4.inc"
};

static AnimationRecord _gPa3Animation0DE30Records[283] = {
#include "assets/pa3_animation_0DE30_records.inc"
};

static u16 _gPa3Animation0DE30Indices[20] = {
#include "assets/pa3_animation_0DE30_indices.inc"
};

static AnimationSet _gPa3Animation0DE30 = {
    _gPa3Animation0DE30Records,
    _gPa3Animation0DE30Indices,
    { NULL, _gPa3Animation0DE30Bank1, NULL, NULL, _gPa3Animation0DE30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0E5D0Bank1[16] = {
#include "assets/pa3_animation_0E5D0_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0E5D0Bank4[157] = {
#include "assets/pa3_animation_0E5D0_bank4.inc"
};

static AnimationRecord _gPa3Animation0E5D0Records[263] = {
#include "assets/pa3_animation_0E5D0_records.inc"
};

static u16 _gPa3Animation0E5D0Indices[20] = {
#include "assets/pa3_animation_0E5D0_indices.inc"
};

static AnimationSet _gPa3Animation0E5D0 = {
    _gPa3Animation0E5D0Records,
    _gPa3Animation0E5D0Indices,
    { NULL, _gPa3Animation0E5D0Bank1, NULL, NULL, _gPa3Animation0E5D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gPa3Animation0EFA4Bank1[25] = {
#include "assets/pa3_animation_0EFA4_bank1.inc"
};

static AnimationPackedRotation _gPa3Animation0EFA4Bank4[223] = {
#include "assets/pa3_animation_0EFA4_bank4.inc"
};

static AnimationRecord _gPa3Animation0EFA4Records[311] = {
#include "assets/pa3_animation_0EFA4_records.inc"
};

static u16 _gPa3Animation0EFA4Indices[20] = {
#include "assets/pa3_animation_0EFA4_indices.inc"
};

static AnimationSet _gPa3Animation0EFA4 = {
    _gPa3Animation0EFA4Records,
    _gPa3Animation0EFA4Indices,
    { NULL, _gPa3Animation0EFA4Bank1, NULL, NULL, _gPa3Animation0EFA4Bank4, NULL, NULL, NULL },
};

AnimationBank D_pa3_8012C18C = { { {
    NULL,
    &_gPa3Animation009F8,
    &_gPa3Animation0DE30,
    &_gPa3Animation0E5D0,
    &_gPa3Animation0EFA4,
    &_gPa3Animation018FC,
    &_gPa3Animation02160,
    &_gPa3Animation0CAA0,
    &_gPa3Animation0D578,
    &_gPa3Animation091E0,
    &_gPa3Animation0C348,
    &_gPa3Animation0C348,
    &_gPa3Animation0A42C,
    &_gPa3Animation09984,
    &_gPa3Animation0B590,
    &_gPa3Animation0B590,
    &_gPa3Animation04DC4,
    &_gPa3Animation050E4,
    &_gPa3Animation058A8,
    &_gPa3Animation0109C,
    &_gPa3Animation0B590,
    &_gPa3Animation009F8,
    &_gPa3Animation009F8,
    &_gPa3Animation06B40,
    &_gPa3Animation07DD4,
    &_gPa3Animation076B8,
    &_gPa3Animation03E44,
    &_gPa3Animation04040,
    &_gPa3Animation04318,
    &_gPa3Animation045BC,
    &_gPa3Animation047BC,
    &_gPa3Animation04B10,
    &_gPa3Animation08254,
    &_gPa3Animation0842C,
    &_gPa3Animation08254,
    &_gPa3Animation0842C,
    &_gPa3Animation02BD0,
    &_gPa3Animation03358,
    &_gPa3Animation039BC,
    &_gPa3Animation0362C,
    &_gPa3Animation02474,
    &_gPa3Animation009F8,
    &_gPa3Animation08990,
    &_gPa3Animation08B84,
    &_gPa3Animation09004,
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
#elif WEAPON_ID == 0xE
static AnimationPackedPose _gSp12Animation00B38Bank1[2] = {
#include "assets/sp12_animation_00B38_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation00B38Bank4[8] = {
#include "assets/sp12_animation_00B38_bank4.inc"
};

static AnimationRecord _gSp12Animation00B38Records[76] = {
#include "assets/sp12_animation_00B38_records.inc"
};

static u16 _gSp12Animation00B38Indices[20] = {
#include "assets/sp12_animation_00B38_indices.inc"
};

static AnimationSet _gSp12Animation00B38 = {
    _gSp12Animation00B38Records,
    _gSp12Animation00B38Indices,
    { NULL, _gSp12Animation00B38Bank1, NULL, NULL, _gSp12Animation00B38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation011DCBank1[12] = {
#include "assets/sp12_animation_011DC_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation011DCBank4[151] = {
#include "assets/sp12_animation_011DC_bank4.inc"
};

static AnimationRecord _gSp12Animation011DCRecords[218] = {
#include "assets/sp12_animation_011DC_records.inc"
};

static u16 _gSp12Animation011DCIndices[20] = {
#include "assets/sp12_animation_011DC_indices.inc"
};

static AnimationSet _gSp12Animation011DC = {
    _gSp12Animation011DCRecords,
    _gSp12Animation011DCIndices,
    { NULL, _gSp12Animation011DCBank1, NULL, NULL, _gSp12Animation011DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation01A3CBank1[19] = {
#include "assets/sp12_animation_01A3C_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation01A3CBank4[169] = {
#include "assets/sp12_animation_01A3C_bank4.inc"
};

static AnimationRecord _gSp12Animation01A3CRecords[290] = {
#include "assets/sp12_animation_01A3C_records.inc"
};

static u16 _gSp12Animation01A3CIndices[20] = {
#include "assets/sp12_animation_01A3C_indices.inc"
};

static AnimationSet _gSp12Animation01A3C = {
    _gSp12Animation01A3CRecords,
    _gSp12Animation01A3CIndices,
    { NULL, _gSp12Animation01A3CBank1, NULL, NULL, _gSp12Animation01A3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation022A0Bank1[19] = {
#include "assets/sp12_animation_022A0_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation022A0Bank4[170] = {
#include "assets/sp12_animation_022A0_bank4.inc"
};

static AnimationRecord _gSp12Animation022A0Records[290] = {
#include "assets/sp12_animation_022A0_records.inc"
};

static u16 _gSp12Animation022A0Indices[20] = {
#include "assets/sp12_animation_022A0_indices.inc"
};

static AnimationSet _gSp12Animation022A0 = {
    _gSp12Animation022A0Records,
    _gSp12Animation022A0Indices,
    { NULL, _gSp12Animation022A0Bank1, NULL, NULL, _gSp12Animation022A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation025B4Bank1[3] = {
#include "assets/sp12_animation_025B4_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation025B4Bank4[69] = {
#include "assets/sp12_animation_025B4_bank4.inc"
};

static AnimationRecord _gSp12Animation025B4Records[99] = {
#include "assets/sp12_animation_025B4_records.inc"
};

static u16 _gSp12Animation025B4Indices[20] = {
#include "assets/sp12_animation_025B4_indices.inc"
};

static AnimationSet _gSp12Animation025B4 = {
    _gSp12Animation025B4Records,
    _gSp12Animation025B4Indices,
    { NULL, _gSp12Animation025B4Bank1, NULL, NULL, _gSp12Animation025B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation02D10Bank1[14] = {
#include "assets/sp12_animation_02D10_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation02D10Bank4[156] = {
#include "assets/sp12_animation_02D10_bank4.inc"
};

static AnimationRecord _gSp12Animation02D10Records[253] = {
#include "assets/sp12_animation_02D10_records.inc"
};

static u16 _gSp12Animation02D10Indices[20] = {
#include "assets/sp12_animation_02D10_indices.inc"
};

static AnimationSet _gSp12Animation02D10 = {
    _gSp12Animation02D10Records,
    _gSp12Animation02D10Indices,
    { NULL, _gSp12Animation02D10Bank1, NULL, NULL, _gSp12Animation02D10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation03498Bank1[16] = {
#include "assets/sp12_animation_03498_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation03498Bank4[167] = {
#include "assets/sp12_animation_03498_bank4.inc"
};

static AnimationRecord _gSp12Animation03498Records[247] = {
#include "assets/sp12_animation_03498_records.inc"
};

static u16 _gSp12Animation03498Indices[20] = {
#include "assets/sp12_animation_03498_indices.inc"
};

static AnimationSet _gSp12Animation03498 = {
    _gSp12Animation03498Records,
    _gSp12Animation03498Indices,
    { NULL, _gSp12Animation03498Bank1, NULL, NULL, _gSp12Animation03498Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation0376CBank1[6] = {
#include "assets/sp12_animation_0376C_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation0376CBank4[52] = {
#include "assets/sp12_animation_0376C_bank4.inc"
};

static AnimationRecord _gSp12Animation0376CRecords[91] = {
#include "assets/sp12_animation_0376C_records.inc"
};

static u16 _gSp12Animation0376CIndices[20] = {
#include "assets/sp12_animation_0376C_indices.inc"
};

static AnimationSet _gSp12Animation0376C = {
    _gSp12Animation0376CRecords,
    _gSp12Animation0376CIndices,
    { NULL, _gSp12Animation0376CBank1, NULL, NULL, _gSp12Animation0376CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation03AFCBank1[7] = {
#include "assets/sp12_animation_03AFC_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation03AFCBank4[73] = {
#include "assets/sp12_animation_03AFC_bank4.inc"
};

static AnimationRecord _gSp12Animation03AFCRecords[114] = {
#include "assets/sp12_animation_03AFC_records.inc"
};

static u16 _gSp12Animation03AFCIndices[20] = {
#include "assets/sp12_animation_03AFC_indices.inc"
};

static AnimationSet _gSp12Animation03AFC = {
    _gSp12Animation03AFCRecords,
    _gSp12Animation03AFCIndices,
    { NULL, _gSp12Animation03AFCBank1, NULL, NULL, _gSp12Animation03AFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation03F84Bank1[9] = {
#include "assets/sp12_animation_03F84_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation03F84Bank4[104] = {
#include "assets/sp12_animation_03F84_bank4.inc"
};

static AnimationRecord _gSp12Animation03F84Records[139] = {
#include "assets/sp12_animation_03F84_records.inc"
};

static u16 _gSp12Animation03F84Indices[20] = {
#include "assets/sp12_animation_03F84_indices.inc"
};

static AnimationSet _gSp12Animation03F84 = {
    _gSp12Animation03F84Records,
    _gSp12Animation03F84Indices,
    { NULL, _gSp12Animation03F84Bank1, NULL, NULL, _gSp12Animation03F84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation04180Bank1[3] = {
#include "assets/sp12_animation_04180_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation04180Bank4[22] = {
#include "assets/sp12_animation_04180_bank4.inc"
};

static AnimationRecord _gSp12Animation04180Records[76] = {
#include "assets/sp12_animation_04180_records.inc"
};

static u16 _gSp12Animation04180Indices[20] = {
#include "assets/sp12_animation_04180_indices.inc"
};

static AnimationSet _gSp12Animation04180 = {
    _gSp12Animation04180Records,
    _gSp12Animation04180Indices,
    { NULL, _gSp12Animation04180Bank1, NULL, NULL, _gSp12Animation04180Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation04458Bank1[6] = {
#include "assets/sp12_animation_04458_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation04458Bank4[57] = {
#include "assets/sp12_animation_04458_bank4.inc"
};

static AnimationRecord _gSp12Animation04458Records[87] = {
#include "assets/sp12_animation_04458_records.inc"
};

static u16 _gSp12Animation04458Indices[20] = {
#include "assets/sp12_animation_04458_indices.inc"
};

static AnimationSet _gSp12Animation04458 = {
    _gSp12Animation04458Records,
    _gSp12Animation04458Indices,
    { NULL, _gSp12Animation04458Bank1, NULL, NULL, _gSp12Animation04458Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation046FCBank1[4] = {
#include "assets/sp12_animation_046FC_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation046FCBank4[55] = {
#include "assets/sp12_animation_046FC_bank4.inc"
};

static AnimationRecord _gSp12Animation046FCRecords[82] = {
#include "assets/sp12_animation_046FC_records.inc"
};

static u16 _gSp12Animation046FCIndices[20] = {
#include "assets/sp12_animation_046FC_indices.inc"
};

static AnimationSet _gSp12Animation046FC = {
    _gSp12Animation046FCRecords,
    _gSp12Animation046FCIndices,
    { NULL, _gSp12Animation046FCBank1, NULL, NULL, _gSp12Animation046FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation048FCBank1[3] = {
#include "assets/sp12_animation_048FC_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation048FCBank4[23] = {
#include "assets/sp12_animation_048FC_bank4.inc"
};

static AnimationRecord _gSp12Animation048FCRecords[76] = {
#include "assets/sp12_animation_048FC_records.inc"
};

static u16 _gSp12Animation048FCIndices[20] = {
#include "assets/sp12_animation_048FC_indices.inc"
};

static AnimationSet _gSp12Animation048FC = {
    _gSp12Animation048FCRecords,
    _gSp12Animation048FCIndices,
    { NULL, _gSp12Animation048FCBank1, NULL, NULL, _gSp12Animation048FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation04C50Bank1[8] = {
#include "assets/sp12_animation_04C50_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation04C50Bank4[68] = {
#include "assets/sp12_animation_04C50_bank4.inc"
};

static AnimationRecord _gSp12Animation04C50Records[101] = {
#include "assets/sp12_animation_04C50_records.inc"
};

static u16 _gSp12Animation04C50Indices[20] = {
#include "assets/sp12_animation_04C50_indices.inc"
};

static AnimationSet _gSp12Animation04C50 = {
    _gSp12Animation04C50Records,
    _gSp12Animation04C50Indices,
    { NULL, _gSp12Animation04C50Bank1, NULL, NULL, _gSp12Animation04C50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation04F04Bank1[5] = {
#include "assets/sp12_animation_04F04_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation04F04Bank4[55] = {
#include "assets/sp12_animation_04F04_bank4.inc"
};

static AnimationRecord _gSp12Animation04F04Records[83] = {
#include "assets/sp12_animation_04F04_records.inc"
};

static u16 _gSp12Animation04F04Indices[20] = {
#include "assets/sp12_animation_04F04_indices.inc"
};

static AnimationSet _gSp12Animation04F04 = {
    _gSp12Animation04F04Records,
    _gSp12Animation04F04Indices,
    { NULL, _gSp12Animation04F04Bank1, NULL, NULL, _gSp12Animation04F04Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation05224Bank1[6] = {
#include "assets/sp12_animation_05224_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation05224Bank4[66] = {
#include "assets/sp12_animation_05224_bank4.inc"
};

static AnimationRecord _gSp12Animation05224Records[96] = {
#include "assets/sp12_animation_05224_records.inc"
};

static u16 _gSp12Animation05224Indices[20] = {
#include "assets/sp12_animation_05224_indices.inc"
};

static AnimationSet _gSp12Animation05224 = {
    _gSp12Animation05224Records,
    _gSp12Animation05224Indices,
    { NULL, _gSp12Animation05224Bank1, NULL, NULL, _gSp12Animation05224Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation059E8Bank1[18] = {
#include "assets/sp12_animation_059E8_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation059E8Bank4[184] = {
#include "assets/sp12_animation_059E8_bank4.inc"
};

static AnimationRecord _gSp12Animation059E8Records[239] = {
#include "assets/sp12_animation_059E8_records.inc"
};

static u16 _gSp12Animation059E8Indices[20] = {
#include "assets/sp12_animation_059E8_indices.inc"
};

static AnimationSet _gSp12Animation059E8 = {
    _gSp12Animation059E8Records,
    _gSp12Animation059E8Indices,
    { NULL, _gSp12Animation059E8Bank1, NULL, NULL, _gSp12Animation059E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation06C80Bank1[29] = {
#include "assets/sp12_animation_06C80_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation06C80Bank4[450] = {
#include "assets/sp12_animation_06C80_bank4.inc"
};

static AnimationRecord _gSp12Animation06C80Records[633] = {
#include "assets/sp12_animation_06C80_records.inc"
};

static u16 _gSp12Animation06C80Indices[20] = {
#include "assets/sp12_animation_06C80_indices.inc"
};

static AnimationSet _gSp12Animation06C80 = {
    _gSp12Animation06C80Records,
    _gSp12Animation06C80Indices,
    { NULL, _gSp12Animation06C80Bank1, NULL, NULL, _gSp12Animation06C80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation077F8Bank1[12] = {
#include "assets/sp12_animation_077F8_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation077F8Bank4[266] = {
#include "assets/sp12_animation_077F8_bank4.inc"
};

static AnimationRecord _gSp12Animation077F8Records[412] = {
#include "assets/sp12_animation_077F8_records.inc"
};

static u16 _gSp12Animation077F8Indices[20] = {
#include "assets/sp12_animation_077F8_indices.inc"
};

static AnimationSet _gSp12Animation077F8 = {
    _gSp12Animation077F8Records,
    _gSp12Animation077F8Indices,
    { NULL, _gSp12Animation077F8Bank1, NULL, NULL, _gSp12Animation077F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation07F14Bank1[9] = {
#include "assets/sp12_animation_07F14_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation07F14Bank4[144] = {
#include "assets/sp12_animation_07F14_bank4.inc"
};

static AnimationRecord _gSp12Animation07F14Records[264] = {
#include "assets/sp12_animation_07F14_records.inc"
};

static u16 _gSp12Animation07F14Indices[20] = {
#include "assets/sp12_animation_07F14_indices.inc"
};

static AnimationSet _gSp12Animation07F14 = {
    _gSp12Animation07F14Records,
    _gSp12Animation07F14Indices,
    { NULL, _gSp12Animation07F14Bank1, NULL, NULL, _gSp12Animation07F14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation08394Bank1[6] = {
#include "assets/sp12_animation_08394_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation08394Bank4[107] = {
#include "assets/sp12_animation_08394_bank4.inc"
};

static AnimationRecord _gSp12Animation08394Records[143] = {
#include "assets/sp12_animation_08394_records.inc"
};

static u16 _gSp12Animation08394Indices[20] = {
#include "assets/sp12_animation_08394_indices.inc"
};

static AnimationSet _gSp12Animation08394 = {
    _gSp12Animation08394Records,
    _gSp12Animation08394Indices,
    { NULL, _gSp12Animation08394Bank1, NULL, NULL, _gSp12Animation08394Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation0856CBank1[3] = {
#include "assets/sp12_animation_0856C_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation0856CBank4[32] = {
#include "assets/sp12_animation_0856C_bank4.inc"
};

static AnimationRecord _gSp12Animation0856CRecords[57] = {
#include "assets/sp12_animation_0856C_records.inc"
};

static u16 _gSp12Animation0856CIndices[20] = {
#include "assets/sp12_animation_0856C_indices.inc"
};

static AnimationSet _gSp12Animation0856C = {
    _gSp12Animation0856CRecords,
    _gSp12Animation0856CIndices,
    { NULL, _gSp12Animation0856CBank1, NULL, NULL, _gSp12Animation0856CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation08AD0Bank1[11] = {
#include "assets/sp12_animation_08AD0_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation08AD0Bank4[125] = {
#include "assets/sp12_animation_08AD0_bank4.inc"
};

static AnimationRecord _gSp12Animation08AD0Records[167] = {
#include "assets/sp12_animation_08AD0_records.inc"
};

static u16 _gSp12Animation08AD0Indices[20] = {
#include "assets/sp12_animation_08AD0_indices.inc"
};

static AnimationSet _gSp12Animation08AD0 = {
    _gSp12Animation08AD0Records,
    _gSp12Animation08AD0Indices,
    { NULL, _gSp12Animation08AD0Bank1, NULL, NULL, _gSp12Animation08AD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation08CC4Bank1[3] = {
#include "assets/sp12_animation_08CC4_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation08CC4Bank4[20] = {
#include "assets/sp12_animation_08CC4_bank4.inc"
};

static AnimationRecord _gSp12Animation08CC4Records[76] = {
#include "assets/sp12_animation_08CC4_records.inc"
};

static u16 _gSp12Animation08CC4Indices[20] = {
#include "assets/sp12_animation_08CC4_indices.inc"
};

static AnimationSet _gSp12Animation08CC4 = {
    _gSp12Animation08CC4Records,
    _gSp12Animation08CC4Indices,
    { NULL, _gSp12Animation08CC4Bank1, NULL, NULL, _gSp12Animation08CC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation09144Bank1[8] = {
#include "assets/sp12_animation_09144_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation09144Bank4[105] = {
#include "assets/sp12_animation_09144_bank4.inc"
};

static AnimationRecord _gSp12Animation09144Records[139] = {
#include "assets/sp12_animation_09144_records.inc"
};

static u16 _gSp12Animation09144Indices[20] = {
#include "assets/sp12_animation_09144_indices.inc"
};

static AnimationSet _gSp12Animation09144 = {
    _gSp12Animation09144Records,
    _gSp12Animation09144Indices,
    { NULL, _gSp12Animation09144Bank1, NULL, NULL, _gSp12Animation09144Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation0931CBank1[2] = {
#include "assets/sp12_animation_0931C_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation0931CBank4[16] = {
#include "assets/sp12_animation_0931C_bank4.inc"
};

static AnimationRecord _gSp12Animation0931CRecords[76] = {
#include "assets/sp12_animation_0931C_records.inc"
};

static u16 _gSp12Animation0931CIndices[20] = {
#include "assets/sp12_animation_0931C_indices.inc"
};

static AnimationSet _gSp12Animation0931C = {
    _gSp12Animation0931CRecords,
    _gSp12Animation0931CIndices,
    { NULL, _gSp12Animation0931CBank1, NULL, NULL, _gSp12Animation0931CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation09B5CBank1[14] = {
#include "assets/sp12_animation_09B5C_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation09B5CBank4[194] = {
#include "assets/sp12_animation_09B5C_bank4.inc"
};

static AnimationRecord _gSp12Animation09B5CRecords[272] = {
#include "assets/sp12_animation_09B5C_records.inc"
};

static u16 _gSp12Animation09B5CIndices[20] = {
#include "assets/sp12_animation_09B5C_indices.inc"
};

static AnimationSet _gSp12Animation09B5C = {
    _gSp12Animation09B5CRecords,
    _gSp12Animation09B5CIndices,
    { NULL, _gSp12Animation09B5CBank1, NULL, NULL, _gSp12Animation09B5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation0A604Bank1[19] = {
#include "assets/sp12_animation_0A604_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation0A604Bank4[269] = {
#include "assets/sp12_animation_0A604_bank4.inc"
};

static AnimationRecord _gSp12Animation0A604Records[336] = {
#include "assets/sp12_animation_0A604_records.inc"
};

static u16 _gSp12Animation0A604Indices[20] = {
#include "assets/sp12_animation_0A604_indices.inc"
};

static AnimationSet _gSp12Animation0A604 = {
    _gSp12Animation0A604Records,
    _gSp12Animation0A604Indices,
    { NULL, _gSp12Animation0A604Bank1, NULL, NULL, _gSp12Animation0A604Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation0BA44Bank1[34] = {
#include "assets/sp12_animation_0BA44_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation0BA44Bank4[541] = {
#include "assets/sp12_animation_0BA44_bank4.inc"
};

static AnimationRecord _gSp12Animation0BA44Records[633] = {
#include "assets/sp12_animation_0BA44_records.inc"
};

static u16 _gSp12Animation0BA44Indices[20] = {
#include "assets/sp12_animation_0BA44_indices.inc"
};

static AnimationSet _gSp12Animation0BA44 = {
    _gSp12Animation0BA44Records,
    _gSp12Animation0BA44Indices,
    { NULL, _gSp12Animation0BA44Bank1, NULL, NULL, _gSp12Animation0BA44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation0C4B8Bank1[18] = {
#include "assets/sp12_animation_0C4B8_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation0C4B8Bank4[268] = {
#include "assets/sp12_animation_0C4B8_bank4.inc"
};

static AnimationRecord _gSp12Animation0C4B8Records[327] = {
#include "assets/sp12_animation_0C4B8_records.inc"
};

static u16 _gSp12Animation0C4B8Indices[20] = {
#include "assets/sp12_animation_0C4B8_indices.inc"
};

static AnimationSet _gSp12Animation0C4B8 = {
    _gSp12Animation0C4B8Records,
    _gSp12Animation0C4B8Indices,
    { NULL, _gSp12Animation0C4B8Bank1, NULL, NULL, _gSp12Animation0C4B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation0CB3CBank1[12] = {
#include "assets/sp12_animation_0CB3C_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation0CB3CBank4[160] = {
#include "assets/sp12_animation_0CB3C_bank4.inc"
};

static AnimationRecord _gSp12Animation0CB3CRecords[201] = {
#include "assets/sp12_animation_0CB3C_records.inc"
};

static u16 _gSp12Animation0CB3CIndices[20] = {
#include "assets/sp12_animation_0CB3C_indices.inc"
};

static AnimationSet _gSp12Animation0CB3C = {
    _gSp12Animation0CB3CRecords,
    _gSp12Animation0CB3CIndices,
    { NULL, _gSp12Animation0CB3CBank1, NULL, NULL, _gSp12Animation0CB3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation0D0B4Bank1[10] = {
#include "assets/sp12_animation_0D0B4_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation0D0B4Bank4[131] = {
#include "assets/sp12_animation_0D0B4_bank4.inc"
};

static AnimationRecord _gSp12Animation0D0B4Records[169] = {
#include "assets/sp12_animation_0D0B4_records.inc"
};

static u16 _gSp12Animation0D0B4Indices[20] = {
#include "assets/sp12_animation_0D0B4_indices.inc"
};

static AnimationSet _gSp12Animation0D0B4 = {
    _gSp12Animation0D0B4Records,
    _gSp12Animation0D0B4Indices,
    { NULL, _gSp12Animation0D0B4Bank1, NULL, NULL, _gSp12Animation0D0B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation0D96CBank1[18] = {
#include "assets/sp12_animation_0D96C_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation0D96CBank4[201] = {
#include "assets/sp12_animation_0D96C_bank4.inc"
};

static AnimationRecord _gSp12Animation0D96CRecords[283] = {
#include "assets/sp12_animation_0D96C_records.inc"
};

static u16 _gSp12Animation0D96CIndices[20] = {
#include "assets/sp12_animation_0D96C_indices.inc"
};

static AnimationSet _gSp12Animation0D96C = {
    _gSp12Animation0D96CRecords,
    _gSp12Animation0D96CIndices,
    { NULL, _gSp12Animation0D96CBank1, NULL, NULL, _gSp12Animation0D96CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation0E10CBank1[16] = {
#include "assets/sp12_animation_0E10C_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation0E10CBank4[157] = {
#include "assets/sp12_animation_0E10C_bank4.inc"
};

static AnimationRecord _gSp12Animation0E10CRecords[263] = {
#include "assets/sp12_animation_0E10C_records.inc"
};

static u16 _gSp12Animation0E10CIndices[20] = {
#include "assets/sp12_animation_0E10C_indices.inc"
};

static AnimationSet _gSp12Animation0E10C = {
    _gSp12Animation0E10CRecords,
    _gSp12Animation0E10CIndices,
    { NULL, _gSp12Animation0E10CBank1, NULL, NULL, _gSp12Animation0E10CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gSp12Animation0EAE0Bank1[25] = {
#include "assets/sp12_animation_0EAE0_bank1.inc"
};

static AnimationPackedRotation _gSp12Animation0EAE0Bank4[223] = {
#include "assets/sp12_animation_0EAE0_bank4.inc"
};

static AnimationRecord _gSp12Animation0EAE0Records[311] = {
#include "assets/sp12_animation_0EAE0_records.inc"
};

static u16 _gSp12Animation0EAE0Indices[20] = {
#include "assets/sp12_animation_0EAE0_indices.inc"
};

static AnimationSet _gSp12Animation0EAE0 = {
    _gSp12Animation0EAE0Records,
    _gSp12Animation0EAE0Indices,
    { NULL, _gSp12Animation0EAE0Bank1, NULL, NULL, _gSp12Animation0EAE0Bank4, NULL, NULL, NULL },
};

AnimationBank D_sp12_8012BCC8 = { { {
    NULL,
    &_gSp12Animation00B38,
    &_gSp12Animation0D96C,
    &_gSp12Animation0E10C,
    &_gSp12Animation0EAE0,
    &_gSp12Animation01A3C,
    &_gSp12Animation022A0,
    &_gSp12Animation0CB3C,
    &_gSp12Animation0D0B4,
    &_gSp12Animation0931C,
    &_gSp12Animation0C4B8,
    &_gSp12Animation0C4B8,
    &_gSp12Animation0A604,
    &_gSp12Animation09B5C,
    &_gSp12Animation0BA44,
    &_gSp12Animation0BA44,
    &_gSp12Animation04F04,
    &_gSp12Animation05224,
    &_gSp12Animation059E8,
    &_gSp12Animation011DC,
    &_gSp12Animation0BA44,
    &_gSp12Animation00B38,
    &_gSp12Animation00B38,
    &_gSp12Animation06C80,
    &_gSp12Animation07F14,
    &_gSp12Animation077F8,
    &_gSp12Animation03F84,
    &_gSp12Animation04180,
    &_gSp12Animation04458,
    &_gSp12Animation046FC,
    &_gSp12Animation048FC,
    &_gSp12Animation04C50,
    &_gSp12Animation08394,
    &_gSp12Animation0856C,
    &_gSp12Animation08394,
    &_gSp12Animation0856C,
    &_gSp12Animation02D10,
    &_gSp12Animation03498,
    &_gSp12Animation03AFC,
    &_gSp12Animation0376C,
    &_gSp12Animation025B4,
    &_gSp12Animation00B38,
    &_gSp12Animation08AD0,
    &_gSp12Animation08CC4,
    &_gSp12Animation09144,
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
#endif
