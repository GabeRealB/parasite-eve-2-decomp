#include "weapons/mp5a5.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "mp5a5_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effects.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "weapons/weapon.h"

#include "../../shared/muzzle_flash.h"

/// The weapon's index: 0x1E for the MP5A5, 0x1F and 0x20 for its two upgrades.
/// The three packages are this source built once each, and each declares its
/// index in the manifest. It keys the firing sounds, the shot id and the item.
#ifndef WEAPON_ID
#error "WEAPON_ID is a per-package build parameter"
#endif

/// Muzzle offset of the weapon, in the firing hand's coordinate frame.
static SVECTOR _gMuzzleOffset = { 0, 0x240, 0x40, 0 };

void func_mp5a5_8011DDA4(Task* arg0);

#include "../../shared/muzzle_flash_task.inc.c"

/// The MP5A5 and its upgrades\'s muzzle-flash task, named by gameplay\'s effect table.
void func_mp5a5_8011D1E0(Task* task)
{
    _muzzleFlashTask(task);
}

#include "../../shared/muzzle_flash_core.inc.c"

#include "../../shared/muzzle_flash_streak.inc.c"

/// Per-frame firing state machine for the MP5A5 and its upgrades. State 0 arms the shot and
/// starts the raise animation (clip 5 instead of 1 when the weapon was already
/// up), state 1 waits for that clip, and states 2/3 count `field_934` down to
/// the frame the round leaves the barrel. That frame branches on `field_97F`:
/// single fire (`== 1`) spends one round, plays sound 4 of the weapon's bank, spawns the plain
/// muzzle flash and runs the recoil clip, while burst fire spends 0x101, plays
/// sound 5, holds the pose for 0x12 frames and reparents the longer flash
/// effect under the weapon task. States 4/5 pick the lock-on target once (only
/// while still below 6) and state 6 loops back to `fire` while the trigger is
/// held, the ammo check passes and the burst timer has run out.
void func_mp5a5_8011DDA4(Task* arg0)
{
    GameActor*             actor;
    GfxCoord*              coord;
    GfxCoord*              spot;
    WorldCollisionCapsule* rec;
    EffectWork*            eff;
    s32                    anim;

    SCRATCH_STACK_RESERVE_BYTES(0x50);
    spot  = SCRATCH_STACK_CURSOR(GfxCoord);
    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (actor->statePhase) {
        case 0:
            actor->state                                          = 4;
            actor->turnRateIndex                                  = 2;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->stateTimer                                     = 1;
            actor->statePhase                                    += 1;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x400;
            anim                                                  = 1;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                anim = 5;
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
        fire:
            actor->statePhase   = 3;
            actor->rumblePosted = 0;
            /* fallthrough */
        case 3:
            if (--actor->stateTimer == 0) {
                rec = &actor->weaponShape;
                if (actor->attackButton == 1) {
                    actor->statePhase                                     = 4;
                    actor->stateTimer                                     = 3;
                    actor->attackControl.cooldownTicks                    = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | 0x20000 | (WEAPON_ID << 8);
                    rec->end0Radius                                       = rec->end1Radius;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x800;
                    func_80106238(arg0, 0, 1);
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20000004 | (WEAPON_ID << 16), 1);
                    Gp_SpawnEff(EFFECT_HANDGUN_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                WEAPON_ID, NULL);
                    Gp_ConsumeSlotQty(WEAPON_ITEM(WEAPON_ID), 1);
                    playerActorPlayChildSlotsWithBlend(arg0, 0xA, 0, 2);
                } else {
                    actor->statePhase                                     = 5;
                    actor->attackControl.cooldownTicks                    = 0x12;
                    actor->stateTimer                                     = 0x12;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = 0x20016 | (WEAPON_ID << 8);
                    rec->end0Radius                                       = 0xC00;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= 0xF7FF;
                    func_80106238(arg0, 0, 0);
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20000005 | (WEAPON_ID << 16), 0);
                    Gp_ConsumeSlotQty(WEAPON_ITEM(WEAPON_ID), 0x101);
                    eff = Gp_SpawnEff(EFFECT_MP5A5_ALT_FIRE_MUZZLE_FLASH,
                                      actor->equipmentTasks[1]->extra.tmd->coords,
                                      WEAPON_ID, NULL);
                    if (eff != NULL) {
                        taskReparent(actor->equipmentTasks[1], eff->task);
                    }
                }
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
        case 4:
        case 5:
            if (actor->statePhase < 5 && Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                Gp_PlayObjSfx(spot, 0x17, 1);
            }
            actor->statePhase                                     = 6;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            /* fallthrough */
        case 6:
            if ((s8)func_801060E0(arg0) == 1 && func_80106264(1) > 0 && actor->attackControl.cooldownTicks == 0) {
                goto fire;
            }
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0) {
                func_80106550(arg0);
            }
            break;
    }
    Gp_TrackLockTarget(arg0);
    SCRATCH_STACK_RELEASE_BYTES(0x50);
}

static TmdBone _gMp5a5Model01318Skeleton[1] = {
#include "assets/mp5a5_model_01318_skeleton.inc"
};

static u32 _gMp5a5Model01318PartVerts[1] = {
#include "assets/mp5a5_model_01318_partVerts.inc"
};

static SVECTOR _gMp5a5Model01318Verts[54] = {
#include "assets/mp5a5_model_01318_verts.inc"
};

static SVECTOR _gMp5a5Model01318Normals[58] = {
#include "assets/mp5a5_model_01318_normals.inc"
};

static u32 _gMp5a5Model01318Stream[393] = {
#include "assets/mp5a5_model_01318_stream.inc"
};

TmdSource D_mp5a5_8011EAFC = {
    0,
    2824,
    0,
    1,
    _gMp5a5Model01318PartVerts,
    _gMp5a5Model01318Verts,
    _gMp5a5Model01318Normals,
    _gMp5a5Model01318Skeleton,
    _gMp5a5Model01318Stream,
};

static AnimationPackedPose _gMp5a5Animation01AF0Bank1[2] = {
#include "assets/mp5a5_animation_01AF0_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation01AF0Bank4[8] = {
#include "assets/mp5a5_animation_01AF0_bank4.inc"
};

static AnimationRecord _gMp5a5Animation01AF0Records[76] = {
#include "assets/mp5a5_animation_01AF0_records.inc"
};

static u16 _gMp5a5Animation01AF0Indices[20] = {
#include "assets/mp5a5_animation_01AF0_indices.inc"
};

static AnimationSet _gMp5a5Animation01AF0 = {
    _gMp5a5Animation01AF0Records,
    _gMp5a5Animation01AF0Indices,
    { NULL, _gMp5a5Animation01AF0Bank1, NULL, NULL, _gMp5a5Animation01AF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation02194Bank1[12] = {
#include "assets/mp5a5_animation_02194_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation02194Bank4[151] = {
#include "assets/mp5a5_animation_02194_bank4.inc"
};

static AnimationRecord _gMp5a5Animation02194Records[218] = {
#include "assets/mp5a5_animation_02194_records.inc"
};

static u16 _gMp5a5Animation02194Indices[20] = {
#include "assets/mp5a5_animation_02194_indices.inc"
};

static AnimationSet _gMp5a5Animation02194 = {
    _gMp5a5Animation02194Records,
    _gMp5a5Animation02194Indices,
    { NULL, _gMp5a5Animation02194Bank1, NULL, NULL, _gMp5a5Animation02194Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation02A6CBank1[19] = {
#include "assets/mp5a5_animation_02A6C_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation02A6CBank4[193] = {
#include "assets/mp5a5_animation_02A6C_bank4.inc"
};

static AnimationRecord _gMp5a5Animation02A6CRecords[296] = {
#include "assets/mp5a5_animation_02A6C_records.inc"
};

static u16 _gMp5a5Animation02A6CIndices[20] = {
#include "assets/mp5a5_animation_02A6C_indices.inc"
};

static AnimationSet _gMp5a5Animation02A6C = {
    _gMp5a5Animation02A6CRecords,
    _gMp5a5Animation02A6CIndices,
    { NULL, _gMp5a5Animation02A6CBank1, NULL, NULL, _gMp5a5Animation02A6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation031A0Bank1[13] = {
#include "assets/mp5a5_animation_031A0_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation031A0Bank4[167] = {
#include "assets/mp5a5_animation_031A0_bank4.inc"
};

static AnimationRecord _gMp5a5Animation031A0Records[235] = {
#include "assets/mp5a5_animation_031A0_records.inc"
};

static u16 _gMp5a5Animation031A0Indices[20] = {
#include "assets/mp5a5_animation_031A0_indices.inc"
};

static AnimationSet _gMp5a5Animation031A0 = {
    _gMp5a5Animation031A0Records,
    _gMp5a5Animation031A0Indices,
    { NULL, _gMp5a5Animation031A0Bank1, NULL, NULL, _gMp5a5Animation031A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation03A00Bank1[19] = {
#include "assets/mp5a5_animation_03A00_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation03A00Bank4[169] = {
#include "assets/mp5a5_animation_03A00_bank4.inc"
};

static AnimationRecord _gMp5a5Animation03A00Records[290] = {
#include "assets/mp5a5_animation_03A00_records.inc"
};

static u16 _gMp5a5Animation03A00Indices[20] = {
#include "assets/mp5a5_animation_03A00_indices.inc"
};

static AnimationSet _gMp5a5Animation03A00 = {
    _gMp5a5Animation03A00Records,
    _gMp5a5Animation03A00Indices,
    { NULL, _gMp5a5Animation03A00Bank1, NULL, NULL, _gMp5a5Animation03A00Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation04264Bank1[19] = {
#include "assets/mp5a5_animation_04264_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation04264Bank4[170] = {
#include "assets/mp5a5_animation_04264_bank4.inc"
};

static AnimationRecord _gMp5a5Animation04264Records[290] = {
#include "assets/mp5a5_animation_04264_records.inc"
};

static u16 _gMp5a5Animation04264Indices[20] = {
#include "assets/mp5a5_animation_04264_indices.inc"
};

static AnimationSet _gMp5a5Animation04264 = {
    _gMp5a5Animation04264Records,
    _gMp5a5Animation04264Indices,
    { NULL, _gMp5a5Animation04264Bank1, NULL, NULL, _gMp5a5Animation04264Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation04578Bank1[3] = {
#include "assets/mp5a5_animation_04578_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation04578Bank4[69] = {
#include "assets/mp5a5_animation_04578_bank4.inc"
};

static AnimationRecord _gMp5a5Animation04578Records[99] = {
#include "assets/mp5a5_animation_04578_records.inc"
};

static u16 _gMp5a5Animation04578Indices[20] = {
#include "assets/mp5a5_animation_04578_indices.inc"
};

static AnimationSet _gMp5a5Animation04578 = {
    _gMp5a5Animation04578Records,
    _gMp5a5Animation04578Indices,
    { NULL, _gMp5a5Animation04578Bank1, NULL, NULL, _gMp5a5Animation04578Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation04CD4Bank1[14] = {
#include "assets/mp5a5_animation_04CD4_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation04CD4Bank4[156] = {
#include "assets/mp5a5_animation_04CD4_bank4.inc"
};

static AnimationRecord _gMp5a5Animation04CD4Records[253] = {
#include "assets/mp5a5_animation_04CD4_records.inc"
};

static u16 _gMp5a5Animation04CD4Indices[20] = {
#include "assets/mp5a5_animation_04CD4_indices.inc"
};

static AnimationSet _gMp5a5Animation04CD4 = {
    _gMp5a5Animation04CD4Records,
    _gMp5a5Animation04CD4Indices,
    { NULL, _gMp5a5Animation04CD4Bank1, NULL, NULL, _gMp5a5Animation04CD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0545CBank1[16] = {
#include "assets/mp5a5_animation_0545C_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0545CBank4[167] = {
#include "assets/mp5a5_animation_0545C_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0545CRecords[247] = {
#include "assets/mp5a5_animation_0545C_records.inc"
};

static u16 _gMp5a5Animation0545CIndices[20] = {
#include "assets/mp5a5_animation_0545C_indices.inc"
};

static AnimationSet _gMp5a5Animation0545C = {
    _gMp5a5Animation0545CRecords,
    _gMp5a5Animation0545CIndices,
    { NULL, _gMp5a5Animation0545CBank1, NULL, NULL, _gMp5a5Animation0545CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation05730Bank1[6] = {
#include "assets/mp5a5_animation_05730_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation05730Bank4[52] = {
#include "assets/mp5a5_animation_05730_bank4.inc"
};

static AnimationRecord _gMp5a5Animation05730Records[91] = {
#include "assets/mp5a5_animation_05730_records.inc"
};

static u16 _gMp5a5Animation05730Indices[20] = {
#include "assets/mp5a5_animation_05730_indices.inc"
};

static AnimationSet _gMp5a5Animation05730 = {
    _gMp5a5Animation05730Records,
    _gMp5a5Animation05730Indices,
    { NULL, _gMp5a5Animation05730Bank1, NULL, NULL, _gMp5a5Animation05730Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation05AC0Bank1[7] = {
#include "assets/mp5a5_animation_05AC0_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation05AC0Bank4[73] = {
#include "assets/mp5a5_animation_05AC0_bank4.inc"
};

static AnimationRecord _gMp5a5Animation05AC0Records[114] = {
#include "assets/mp5a5_animation_05AC0_records.inc"
};

static u16 _gMp5a5Animation05AC0Indices[20] = {
#include "assets/mp5a5_animation_05AC0_indices.inc"
};

static AnimationSet _gMp5a5Animation05AC0 = {
    _gMp5a5Animation05AC0Records,
    _gMp5a5Animation05AC0Indices,
    { NULL, _gMp5a5Animation05AC0Bank1, NULL, NULL, _gMp5a5Animation05AC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation05F48Bank1[9] = {
#include "assets/mp5a5_animation_05F48_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation05F48Bank4[104] = {
#include "assets/mp5a5_animation_05F48_bank4.inc"
};

static AnimationRecord _gMp5a5Animation05F48Records[139] = {
#include "assets/mp5a5_animation_05F48_records.inc"
};

static u16 _gMp5a5Animation05F48Indices[20] = {
#include "assets/mp5a5_animation_05F48_indices.inc"
};

static AnimationSet _gMp5a5Animation05F48 = {
    _gMp5a5Animation05F48Records,
    _gMp5a5Animation05F48Indices,
    { NULL, _gMp5a5Animation05F48Bank1, NULL, NULL, _gMp5a5Animation05F48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation06144Bank1[3] = {
#include "assets/mp5a5_animation_06144_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation06144Bank4[22] = {
#include "assets/mp5a5_animation_06144_bank4.inc"
};

static AnimationRecord _gMp5a5Animation06144Records[76] = {
#include "assets/mp5a5_animation_06144_records.inc"
};

static u16 _gMp5a5Animation06144Indices[20] = {
#include "assets/mp5a5_animation_06144_indices.inc"
};

static AnimationSet _gMp5a5Animation06144 = {
    _gMp5a5Animation06144Records,
    _gMp5a5Animation06144Indices,
    { NULL, _gMp5a5Animation06144Bank1, NULL, NULL, _gMp5a5Animation06144Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0641CBank1[6] = {
#include "assets/mp5a5_animation_0641C_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0641CBank4[57] = {
#include "assets/mp5a5_animation_0641C_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0641CRecords[87] = {
#include "assets/mp5a5_animation_0641C_records.inc"
};

static u16 _gMp5a5Animation0641CIndices[20] = {
#include "assets/mp5a5_animation_0641C_indices.inc"
};

static AnimationSet _gMp5a5Animation0641C = {
    _gMp5a5Animation0641CRecords,
    _gMp5a5Animation0641CIndices,
    { NULL, _gMp5a5Animation0641CBank1, NULL, NULL, _gMp5a5Animation0641CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation066C0Bank1[4] = {
#include "assets/mp5a5_animation_066C0_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation066C0Bank4[55] = {
#include "assets/mp5a5_animation_066C0_bank4.inc"
};

static AnimationRecord _gMp5a5Animation066C0Records[82] = {
#include "assets/mp5a5_animation_066C0_records.inc"
};

static u16 _gMp5a5Animation066C0Indices[20] = {
#include "assets/mp5a5_animation_066C0_indices.inc"
};

static AnimationSet _gMp5a5Animation066C0 = {
    _gMp5a5Animation066C0Records,
    _gMp5a5Animation066C0Indices,
    { NULL, _gMp5a5Animation066C0Bank1, NULL, NULL, _gMp5a5Animation066C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation068C0Bank1[3] = {
#include "assets/mp5a5_animation_068C0_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation068C0Bank4[23] = {
#include "assets/mp5a5_animation_068C0_bank4.inc"
};

static AnimationRecord _gMp5a5Animation068C0Records[76] = {
#include "assets/mp5a5_animation_068C0_records.inc"
};

static u16 _gMp5a5Animation068C0Indices[20] = {
#include "assets/mp5a5_animation_068C0_indices.inc"
};

static AnimationSet _gMp5a5Animation068C0 = {
    _gMp5a5Animation068C0Records,
    _gMp5a5Animation068C0Indices,
    { NULL, _gMp5a5Animation068C0Bank1, NULL, NULL, _gMp5a5Animation068C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation06C14Bank1[8] = {
#include "assets/mp5a5_animation_06C14_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation06C14Bank4[68] = {
#include "assets/mp5a5_animation_06C14_bank4.inc"
};

static AnimationRecord _gMp5a5Animation06C14Records[101] = {
#include "assets/mp5a5_animation_06C14_records.inc"
};

static u16 _gMp5a5Animation06C14Indices[20] = {
#include "assets/mp5a5_animation_06C14_indices.inc"
};

static AnimationSet _gMp5a5Animation06C14 = {
    _gMp5a5Animation06C14Records,
    _gMp5a5Animation06C14Indices,
    { NULL, _gMp5a5Animation06C14Bank1, NULL, NULL, _gMp5a5Animation06C14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation06EC8Bank1[5] = {
#include "assets/mp5a5_animation_06EC8_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation06EC8Bank4[55] = {
#include "assets/mp5a5_animation_06EC8_bank4.inc"
};

static AnimationRecord _gMp5a5Animation06EC8Records[83] = {
#include "assets/mp5a5_animation_06EC8_records.inc"
};

static u16 _gMp5a5Animation06EC8Indices[20] = {
#include "assets/mp5a5_animation_06EC8_indices.inc"
};

static AnimationSet _gMp5a5Animation06EC8 = {
    _gMp5a5Animation06EC8Records,
    _gMp5a5Animation06EC8Indices,
    { NULL, _gMp5a5Animation06EC8Bank1, NULL, NULL, _gMp5a5Animation06EC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation071E8Bank1[6] = {
#include "assets/mp5a5_animation_071E8_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation071E8Bank4[66] = {
#include "assets/mp5a5_animation_071E8_bank4.inc"
};

static AnimationRecord _gMp5a5Animation071E8Records[96] = {
#include "assets/mp5a5_animation_071E8_records.inc"
};

static u16 _gMp5a5Animation071E8Indices[20] = {
#include "assets/mp5a5_animation_071E8_indices.inc"
};

static AnimationSet _gMp5a5Animation071E8 = {
    _gMp5a5Animation071E8Records,
    _gMp5a5Animation071E8Indices,
    { NULL, _gMp5a5Animation071E8Bank1, NULL, NULL, _gMp5a5Animation071E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation079ACBank1[18] = {
#include "assets/mp5a5_animation_079AC_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation079ACBank4[184] = {
#include "assets/mp5a5_animation_079AC_bank4.inc"
};

static AnimationRecord _gMp5a5Animation079ACRecords[239] = {
#include "assets/mp5a5_animation_079AC_records.inc"
};

static u16 _gMp5a5Animation079ACIndices[20] = {
#include "assets/mp5a5_animation_079AC_indices.inc"
};

static AnimationSet _gMp5a5Animation079AC = {
    _gMp5a5Animation079ACRecords,
    _gMp5a5Animation079ACIndices,
    { NULL, _gMp5a5Animation079ACBank1, NULL, NULL, _gMp5a5Animation079ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation08C44Bank1[29] = {
#include "assets/mp5a5_animation_08C44_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation08C44Bank4[450] = {
#include "assets/mp5a5_animation_08C44_bank4.inc"
};

static AnimationRecord _gMp5a5Animation08C44Records[633] = {
#include "assets/mp5a5_animation_08C44_records.inc"
};

static u16 _gMp5a5Animation08C44Indices[20] = {
#include "assets/mp5a5_animation_08C44_indices.inc"
};

static AnimationSet _gMp5a5Animation08C44 = {
    _gMp5a5Animation08C44Records,
    _gMp5a5Animation08C44Indices,
    { NULL, _gMp5a5Animation08C44Bank1, NULL, NULL, _gMp5a5Animation08C44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation097BCBank1[12] = {
#include "assets/mp5a5_animation_097BC_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation097BCBank4[266] = {
#include "assets/mp5a5_animation_097BC_bank4.inc"
};

static AnimationRecord _gMp5a5Animation097BCRecords[412] = {
#include "assets/mp5a5_animation_097BC_records.inc"
};

static u16 _gMp5a5Animation097BCIndices[20] = {
#include "assets/mp5a5_animation_097BC_indices.inc"
};

static AnimationSet _gMp5a5Animation097BC = {
    _gMp5a5Animation097BCRecords,
    _gMp5a5Animation097BCIndices,
    { NULL, _gMp5a5Animation097BCBank1, NULL, NULL, _gMp5a5Animation097BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation09ED8Bank1[9] = {
#include "assets/mp5a5_animation_09ED8_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation09ED8Bank4[144] = {
#include "assets/mp5a5_animation_09ED8_bank4.inc"
};

static AnimationRecord _gMp5a5Animation09ED8Records[264] = {
#include "assets/mp5a5_animation_09ED8_records.inc"
};

static u16 _gMp5a5Animation09ED8Indices[20] = {
#include "assets/mp5a5_animation_09ED8_indices.inc"
};

static AnimationSet _gMp5a5Animation09ED8 = {
    _gMp5a5Animation09ED8Records,
    _gMp5a5Animation09ED8Indices,
    { NULL, _gMp5a5Animation09ED8Bank1, NULL, NULL, _gMp5a5Animation09ED8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0A358Bank1[6] = {
#include "assets/mp5a5_animation_0A358_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0A358Bank4[107] = {
#include "assets/mp5a5_animation_0A358_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0A358Records[143] = {
#include "assets/mp5a5_animation_0A358_records.inc"
};

static u16 _gMp5a5Animation0A358Indices[20] = {
#include "assets/mp5a5_animation_0A358_indices.inc"
};

static AnimationSet _gMp5a5Animation0A358 = {
    _gMp5a5Animation0A358Records,
    _gMp5a5Animation0A358Indices,
    { NULL, _gMp5a5Animation0A358Bank1, NULL, NULL, _gMp5a5Animation0A358Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0A530Bank1[3] = {
#include "assets/mp5a5_animation_0A530_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0A530Bank4[32] = {
#include "assets/mp5a5_animation_0A530_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0A530Records[57] = {
#include "assets/mp5a5_animation_0A530_records.inc"
};

static u16 _gMp5a5Animation0A530Indices[20] = {
#include "assets/mp5a5_animation_0A530_indices.inc"
};

static AnimationSet _gMp5a5Animation0A530 = {
    _gMp5a5Animation0A530Records,
    _gMp5a5Animation0A530Indices,
    { NULL, _gMp5a5Animation0A530Bank1, NULL, NULL, _gMp5a5Animation0A530Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0AA94Bank1[11] = {
#include "assets/mp5a5_animation_0AA94_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0AA94Bank4[125] = {
#include "assets/mp5a5_animation_0AA94_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0AA94Records[167] = {
#include "assets/mp5a5_animation_0AA94_records.inc"
};

static u16 _gMp5a5Animation0AA94Indices[20] = {
#include "assets/mp5a5_animation_0AA94_indices.inc"
};

static AnimationSet _gMp5a5Animation0AA94 = {
    _gMp5a5Animation0AA94Records,
    _gMp5a5Animation0AA94Indices,
    { NULL, _gMp5a5Animation0AA94Bank1, NULL, NULL, _gMp5a5Animation0AA94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0AC88Bank1[3] = {
#include "assets/mp5a5_animation_0AC88_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0AC88Bank4[20] = {
#include "assets/mp5a5_animation_0AC88_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0AC88Records[76] = {
#include "assets/mp5a5_animation_0AC88_records.inc"
};

static u16 _gMp5a5Animation0AC88Indices[20] = {
#include "assets/mp5a5_animation_0AC88_indices.inc"
};

static AnimationSet _gMp5a5Animation0AC88 = {
    _gMp5a5Animation0AC88Records,
    _gMp5a5Animation0AC88Indices,
    { NULL, _gMp5a5Animation0AC88Bank1, NULL, NULL, _gMp5a5Animation0AC88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0B108Bank1[8] = {
#include "assets/mp5a5_animation_0B108_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0B108Bank4[105] = {
#include "assets/mp5a5_animation_0B108_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0B108Records[139] = {
#include "assets/mp5a5_animation_0B108_records.inc"
};

static u16 _gMp5a5Animation0B108Indices[20] = {
#include "assets/mp5a5_animation_0B108_indices.inc"
};

static AnimationSet _gMp5a5Animation0B108 = {
    _gMp5a5Animation0B108Records,
    _gMp5a5Animation0B108Indices,
    { NULL, _gMp5a5Animation0B108Bank1, NULL, NULL, _gMp5a5Animation0B108Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0BBB0Bank1[19] = {
#include "assets/mp5a5_animation_0BBB0_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0BBB0Bank4[269] = {
#include "assets/mp5a5_animation_0BBB0_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0BBB0Records[336] = {
#include "assets/mp5a5_animation_0BBB0_records.inc"
};

static u16 _gMp5a5Animation0BBB0Indices[20] = {
#include "assets/mp5a5_animation_0BBB0_indices.inc"
};

static AnimationSet _gMp5a5Animation0BBB0 = {
    _gMp5a5Animation0BBB0Records,
    _gMp5a5Animation0BBB0Indices,
    { NULL, _gMp5a5Animation0BBB0Bank1, NULL, NULL, _gMp5a5Animation0BBB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0BD88Bank1[2] = {
#include "assets/mp5a5_animation_0BD88_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0BD88Bank4[16] = {
#include "assets/mp5a5_animation_0BD88_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0BD88Records[76] = {
#include "assets/mp5a5_animation_0BD88_records.inc"
};

static u16 _gMp5a5Animation0BD88Indices[20] = {
#include "assets/mp5a5_animation_0BD88_indices.inc"
};

static AnimationSet _gMp5a5Animation0BD88 = {
    _gMp5a5Animation0BD88Records,
    _gMp5a5Animation0BD88Indices,
    { NULL, _gMp5a5Animation0BD88Bank1, NULL, NULL, _gMp5a5Animation0BD88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0C4BCBank1[12] = {
#include "assets/mp5a5_animation_0C4BC_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0C4BCBank4[165] = {
#include "assets/mp5a5_animation_0C4BC_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0C4BCRecords[240] = {
#include "assets/mp5a5_animation_0C4BC_records.inc"
};

static u16 _gMp5a5Animation0C4BCIndices[20] = {
#include "assets/mp5a5_animation_0C4BC_indices.inc"
};

static AnimationSet _gMp5a5Animation0C4BC = {
    _gMp5a5Animation0C4BCRecords,
    _gMp5a5Animation0C4BCIndices,
    { NULL, _gMp5a5Animation0C4BCBank1, NULL, NULL, _gMp5a5Animation0C4BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0D268Bank1[23] = {
#include "assets/mp5a5_animation_0D268_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0D268Bank4[369] = {
#include "assets/mp5a5_animation_0D268_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0D268Records[417] = {
#include "assets/mp5a5_animation_0D268_records.inc"
};

static u16 _gMp5a5Animation0D268Indices[20] = {
#include "assets/mp5a5_animation_0D268_indices.inc"
};

static AnimationSet _gMp5a5Animation0D268 = {
    _gMp5a5Animation0D268Records,
    _gMp5a5Animation0D268Indices,
    { NULL, _gMp5a5Animation0D268Bank1, NULL, NULL, _gMp5a5Animation0D268Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0D768Bank1[9] = {
#include "assets/mp5a5_animation_0D768_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0D768Bank4[120] = {
#include "assets/mp5a5_animation_0D768_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0D768Records[153] = {
#include "assets/mp5a5_animation_0D768_records.inc"
};

static u16 _gMp5a5Animation0D768Indices[20] = {
#include "assets/mp5a5_animation_0D768_indices.inc"
};

static AnimationSet _gMp5a5Animation0D768 = {
    _gMp5a5Animation0D768Records,
    _gMp5a5Animation0D768Indices,
    { NULL, _gMp5a5Animation0D768Bank1, NULL, NULL, _gMp5a5Animation0D768Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0DC58Bank1[9] = {
#include "assets/mp5a5_animation_0DC58_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0DC58Bank4[116] = {
#include "assets/mp5a5_animation_0DC58_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0DC58Records[153] = {
#include "assets/mp5a5_animation_0DC58_records.inc"
};

static u16 _gMp5a5Animation0DC58Indices[20] = {
#include "assets/mp5a5_animation_0DC58_indices.inc"
};

static AnimationSet _gMp5a5Animation0DC58 = {
    _gMp5a5Animation0DC58Records,
    _gMp5a5Animation0DC58Indices,
    { NULL, _gMp5a5Animation0DC58Bank1, NULL, NULL, _gMp5a5Animation0DC58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMp5a5Animation0E1E4Bank1[10] = {
#include "assets/mp5a5_animation_0E1E4_bank1.inc"
};

static AnimationPackedRotation _gMp5a5Animation0E1E4Bank4[136] = {
#include "assets/mp5a5_animation_0E1E4_bank4.inc"
};

static AnimationRecord _gMp5a5Animation0E1E4Records[169] = {
#include "assets/mp5a5_animation_0E1E4_records.inc"
};

static u16 _gMp5a5Animation0E1E4Indices[20] = {
#include "assets/mp5a5_animation_0E1E4_indices.inc"
};

static AnimationSet _gMp5a5Animation0E1E4 = {
    _gMp5a5Animation0E1E4Records,
    _gMp5a5Animation0E1E4Indices,
    { NULL, _gMp5a5Animation0E1E4Bank1, NULL, NULL, _gMp5a5Animation0E1E4Bank4, NULL, NULL, NULL },
};

AnimationBank D_mp5a5_8012B3CC = { { {
    NULL,
    &_gMp5a5Animation01AF0,
    &_gMp5a5Animation02194,
    &_gMp5a5Animation02A6C,
    &_gMp5a5Animation031A0,
    &_gMp5a5Animation03A00,
    &_gMp5a5Animation04264,
    &_gMp5a5Animation0DC58,
    &_gMp5a5Animation0E1E4,
    &_gMp5a5Animation0BD88,
    &_gMp5a5Animation0D768,
    &_gMp5a5Animation0D768,
    &_gMp5a5Animation0BBB0,
    &_gMp5a5Animation0C4BC,
    &_gMp5a5Animation0D268,
    &_gMp5a5Animation0D268,
    &_gMp5a5Animation06EC8,
    &_gMp5a5Animation071E8,
    &_gMp5a5Animation079AC,
    &_gMp5a5Animation02194,
    &_gMp5a5Animation0D268,
    &_gMp5a5Animation01AF0,
    &_gMp5a5Animation01AF0,
    &_gMp5a5Animation08C44,
    &_gMp5a5Animation09ED8,
    &_gMp5a5Animation097BC,
    &_gMp5a5Animation05F48,
    &_gMp5a5Animation06144,
    &_gMp5a5Animation0641C,
    &_gMp5a5Animation066C0,
    &_gMp5a5Animation068C0,
    &_gMp5a5Animation06C14,
    &_gMp5a5Animation0A358,
    &_gMp5a5Animation0A530,
    &_gMp5a5Animation0A358,
    &_gMp5a5Animation0A530,
    &_gMp5a5Animation04CD4,
    &_gMp5a5Animation0545C,
    &_gMp5a5Animation05AC0,
    &_gMp5a5Animation05730,
    &_gMp5a5Animation04578,
    &_gMp5a5Animation01AF0,
    &_gMp5a5Animation0AA94,
    &_gMp5a5Animation0AC88,
    &_gMp5a5Animation0B108,
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

s16 gMuzzleFlashAngles[4] = { 0, 0, 0, 0 };

PACKAGE_ALIASES
