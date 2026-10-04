#include "weapons/p229.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "p229_private.h"

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

#include "../../shared/muzzle_flash.h"

/// Muzzle offset of the P229, in the firing hand's coordinate frame.
static SVECTOR _gMuzzleOffset = { 0, 0x140, 0x20, 0 };

void func_p229_8011DDA0(Task* arg0);

#include "../../shared/muzzle_flash_task.inc.c"

/// The P229\'s muzzle-flash task, named by gameplay\'s effect table.
void func_p229_8011D1DC(Task* task)
{
    muzzleFlashTask(task);
}

/// Draws the core of a gun's muzzle flash: one semi-transparent, shade-blended
/// `POLY_FT4` billboarded on `arg0`'s world position. `arg1` is the flash size
/// (scaled down by the projected depth) and `arg2` its spin, so the quad is a
/// square rotated by `arg2` rather than an axis-aligned sprite.
/* `otzp` is a second name for the same block on purpose: `gte_stszotz` takes
   its address in a register of its own, so the ROM keeps a `move` the single
   pointer would have coalesced away. The `gte_ldv0` / `gte_stsxy` addresses
   and every `otz` reload are spelled out from `head` for the same reason -
   off `blk` they would reuse the block register instead. */

/* Every scratch vector address is computed off `head`, not off `blk`, so the
   loads and stores keep spelling the block out from `head` rather than reusing
   the `blk` register the way CSE off `blk` would. */

#include "../../shared/muzzle_flash_core.inc.c"

#include "../../shared/muzzle_flash_streak.inc.c"

/// Per-frame firing state machine for the P229. State 0 arms the shot and
/// starts the raise animation (clip 5 instead of 1 when the weapon was already
/// up), state 1 waits for that clip, and state 2 is the frame the round leaves
/// the barrel. That frame branches on `field_97F`: single fire (`== 1`) spends
/// one round, plays `0x20050004`, spawns the plain muzzle flash and restarts
/// the child slots, while burst fire spends 0x101, plays `0x20050005`, holds
/// the pose with the `0xC00` recoil pulse and reparents the longer flash effect
/// under the weapon task. States 3/4 pick the lock-on target once (only while
/// still in state 3) and state 5 counts `field_979` down, dropping back out of
/// the firing pose once the aim check fails or the trigger has been released.
void func_p229_8011DDA0(Task* arg0)
{
    GameActor*             actor;
    GfxCoord*              coord;
    GfxCoord*              spot;
    WorldCollisionCapsule* rec;
    EffectWork*            eff;
    s32                    anim;
    s16                    frames;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    rec   = &actor->weaponShape;
    /* The push must stay *after* the three loads above, or the `lui`/`ori`
       of the scratch-head address wins the ready list and reschedules the
       entry. */
    SCRATCH_STACK_RESERVE_BYTES(0x50);
    spot = SCRATCH_STACK_CURSOR(GfxCoord);
    switch (actor->statePhase) {
        case 0:
            actor->state                                          = 4;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex                                  = 0;
            actor->animationState                                 = 0;
            actor->rumblePosted                                   = 0;
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
            if (Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case 2:
            if (actor->attackButton == 1) {
                actor->statePhase                                     = 3;
                actor->attackCancelTicks                              = 0xA;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | 0x20500;
                rec->end0Radius                                       = rec->end1Radius;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x800;
                func_80106238(arg0, 0, 0);
                Gp_ConsumeSlotQty(0x84, 1);
                Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20050004, 0);
                Gp_SpawnEff(EFFECT_HANDGUN_MUZZLE_FLASH,
                            actor->equipmentTasks[1]->extra.tmd->coords, 5,
                            NULL);
                Gp_AnimResetChildSlots(arg0, 0xA);
            } else {
                actor->statePhase                                     = 4;
                actor->attackCancelTicks                              = 6;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = 0x20516;
                rec->end0Radius                                       = 0xC00;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= 0xF7FF;
                func_80106238(arg0, 0, 1);
                Gp_ConsumeSlotQty(0x84, 0x101);
                Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20050005, 0);
                eff = Gp_SpawnEff(EFFECT_P229_MUZZLE_FLASH,
                                  actor->equipmentTasks[1]->extra.tmd->coords, 5,
                                  NULL);
                if (eff != NULL) {
                    taskReparent(actor->equipmentTasks[1], eff->task);
                }
            }
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            break;
        case 3:
        case 4:
            if (actor->statePhase == 3 && Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                Gp_PlayObjSfx(spot, 0x17, 0);
            }
            actor->statePhase                                     = 5;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            /* fallthrough */
        case 5:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                frames = 0x12;
                if (actor->attackButton == 1) {
                    frames = 0xC;
                }
                actor->attackControl.cooldownTicks = frames;
                func_80106550(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x50);
}

static TmdBone _gP229Model01110Skeleton[1] = {
#include "assets/p229_model_01110_skeleton.inc"
};

static u32 _gP229Model01110PartVerts[1] = {
#include "assets/p229_model_01110_partVerts.inc"
};

static SVECTOR _gP229Model01110Verts[30] = {
#include "assets/p229_model_01110_verts.inc"
};

static SVECTOR _gP229Model01110Normals[24] = {
#include "assets/p229_model_01110_normals.inc"
};

static u32 _gP229Model01110Stream[197] = {
#include "assets/p229_model_01110_stream.inc"
};

TmdSource D_p229_8011E5E4 = {
    0,
    1368,
    0,
    1,
    _gP229Model01110PartVerts,
    _gP229Model01110Verts,
    _gP229Model01110Normals,
    _gP229Model01110Skeleton,
    _gP229Model01110Stream,
};

static AnimationPackedPose _gP229Animation015D8Bank1[2] = {
#include "assets/p229_animation_015D8_bank1.inc"
};

static AnimationPackedRotation _gP229Animation015D8Bank4[8] = {
#include "assets/p229_animation_015D8_bank4.inc"
};

static AnimationRecord _gP229Animation015D8Records[76] = {
#include "assets/p229_animation_015D8_records.inc"
};

static u16 _gP229Animation015D8Indices[20] = {
#include "assets/p229_animation_015D8_indices.inc"
};

static AnimationSet _gP229Animation015D8 = {
    _gP229Animation015D8Records,
    _gP229Animation015D8Indices,
    { NULL, _gP229Animation015D8Bank1, NULL, NULL, _gP229Animation015D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation01C7CBank1[12] = {
#include "assets/p229_animation_01C7C_bank1.inc"
};

static AnimationPackedRotation _gP229Animation01C7CBank4[151] = {
#include "assets/p229_animation_01C7C_bank4.inc"
};

static AnimationRecord _gP229Animation01C7CRecords[218] = {
#include "assets/p229_animation_01C7C_records.inc"
};

static u16 _gP229Animation01C7CIndices[20] = {
#include "assets/p229_animation_01C7C_indices.inc"
};

static AnimationSet _gP229Animation01C7C = {
    _gP229Animation01C7CRecords,
    _gP229Animation01C7CIndices,
    { NULL, _gP229Animation01C7CBank1, NULL, NULL, _gP229Animation01C7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation025C8Bank1[16] = {
#include "assets/p229_animation_025C8_bank1.inc"
};

static AnimationPackedRotation _gP229Animation025C8Bank4[221] = {
#include "assets/p229_animation_025C8_bank4.inc"
};

static AnimationRecord _gP229Animation025C8Records[306] = {
#include "assets/p229_animation_025C8_records.inc"
};

static u16 _gP229Animation025C8Indices[20] = {
#include "assets/p229_animation_025C8_indices.inc"
};

static AnimationSet _gP229Animation025C8 = {
    _gP229Animation025C8Records,
    _gP229Animation025C8Indices,
    { NULL, _gP229Animation025C8Bank1, NULL, NULL, _gP229Animation025C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation02EA0Bank1[19] = {
#include "assets/p229_animation_02EA0_bank1.inc"
};

static AnimationPackedRotation _gP229Animation02EA0Bank4[193] = {
#include "assets/p229_animation_02EA0_bank4.inc"
};

static AnimationRecord _gP229Animation02EA0Records[296] = {
#include "assets/p229_animation_02EA0_records.inc"
};

static u16 _gP229Animation02EA0Indices[20] = {
#include "assets/p229_animation_02EA0_indices.inc"
};

static AnimationSet _gP229Animation02EA0 = {
    _gP229Animation02EA0Records,
    _gP229Animation02EA0Indices,
    { NULL, _gP229Animation02EA0Bank1, NULL, NULL, _gP229Animation02EA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation035D4Bank1[13] = {
#include "assets/p229_animation_035D4_bank1.inc"
};

static AnimationPackedRotation _gP229Animation035D4Bank4[167] = {
#include "assets/p229_animation_035D4_bank4.inc"
};

static AnimationRecord _gP229Animation035D4Records[235] = {
#include "assets/p229_animation_035D4_records.inc"
};

static u16 _gP229Animation035D4Indices[20] = {
#include "assets/p229_animation_035D4_indices.inc"
};

static AnimationSet _gP229Animation035D4 = {
    _gP229Animation035D4Records,
    _gP229Animation035D4Indices,
    { NULL, _gP229Animation035D4Bank1, NULL, NULL, _gP229Animation035D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation03E34Bank1[19] = {
#include "assets/p229_animation_03E34_bank1.inc"
};

static AnimationPackedRotation _gP229Animation03E34Bank4[169] = {
#include "assets/p229_animation_03E34_bank4.inc"
};

static AnimationRecord _gP229Animation03E34Records[290] = {
#include "assets/p229_animation_03E34_records.inc"
};

static u16 _gP229Animation03E34Indices[20] = {
#include "assets/p229_animation_03E34_indices.inc"
};

static AnimationSet _gP229Animation03E34 = {
    _gP229Animation03E34Records,
    _gP229Animation03E34Indices,
    { NULL, _gP229Animation03E34Bank1, NULL, NULL, _gP229Animation03E34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation04698Bank1[19] = {
#include "assets/p229_animation_04698_bank1.inc"
};

static AnimationPackedRotation _gP229Animation04698Bank4[170] = {
#include "assets/p229_animation_04698_bank4.inc"
};

static AnimationRecord _gP229Animation04698Records[290] = {
#include "assets/p229_animation_04698_records.inc"
};

static u16 _gP229Animation04698Indices[20] = {
#include "assets/p229_animation_04698_indices.inc"
};

static AnimationSet _gP229Animation04698 = {
    _gP229Animation04698Records,
    _gP229Animation04698Indices,
    { NULL, _gP229Animation04698Bank1, NULL, NULL, _gP229Animation04698Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation049ACBank1[3] = {
#include "assets/p229_animation_049AC_bank1.inc"
};

static AnimationPackedRotation _gP229Animation049ACBank4[69] = {
#include "assets/p229_animation_049AC_bank4.inc"
};

static AnimationRecord _gP229Animation049ACRecords[99] = {
#include "assets/p229_animation_049AC_records.inc"
};

static u16 _gP229Animation049ACIndices[20] = {
#include "assets/p229_animation_049AC_indices.inc"
};

static AnimationSet _gP229Animation049AC = {
    _gP229Animation049ACRecords,
    _gP229Animation049ACIndices,
    { NULL, _gP229Animation049ACBank1, NULL, NULL, _gP229Animation049ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation05108Bank1[14] = {
#include "assets/p229_animation_05108_bank1.inc"
};

static AnimationPackedRotation _gP229Animation05108Bank4[156] = {
#include "assets/p229_animation_05108_bank4.inc"
};

static AnimationRecord _gP229Animation05108Records[253] = {
#include "assets/p229_animation_05108_records.inc"
};

static u16 _gP229Animation05108Indices[20] = {
#include "assets/p229_animation_05108_indices.inc"
};

static AnimationSet _gP229Animation05108 = {
    _gP229Animation05108Records,
    _gP229Animation05108Indices,
    { NULL, _gP229Animation05108Bank1, NULL, NULL, _gP229Animation05108Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation05890Bank1[16] = {
#include "assets/p229_animation_05890_bank1.inc"
};

static AnimationPackedRotation _gP229Animation05890Bank4[167] = {
#include "assets/p229_animation_05890_bank4.inc"
};

static AnimationRecord _gP229Animation05890Records[247] = {
#include "assets/p229_animation_05890_records.inc"
};

static u16 _gP229Animation05890Indices[20] = {
#include "assets/p229_animation_05890_indices.inc"
};

static AnimationSet _gP229Animation05890 = {
    _gP229Animation05890Records,
    _gP229Animation05890Indices,
    { NULL, _gP229Animation05890Bank1, NULL, NULL, _gP229Animation05890Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation05B64Bank1[6] = {
#include "assets/p229_animation_05B64_bank1.inc"
};

static AnimationPackedRotation _gP229Animation05B64Bank4[52] = {
#include "assets/p229_animation_05B64_bank4.inc"
};

static AnimationRecord _gP229Animation05B64Records[91] = {
#include "assets/p229_animation_05B64_records.inc"
};

static u16 _gP229Animation05B64Indices[20] = {
#include "assets/p229_animation_05B64_indices.inc"
};

static AnimationSet _gP229Animation05B64 = {
    _gP229Animation05B64Records,
    _gP229Animation05B64Indices,
    { NULL, _gP229Animation05B64Bank1, NULL, NULL, _gP229Animation05B64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation05EF4Bank1[7] = {
#include "assets/p229_animation_05EF4_bank1.inc"
};

static AnimationPackedRotation _gP229Animation05EF4Bank4[73] = {
#include "assets/p229_animation_05EF4_bank4.inc"
};

static AnimationRecord _gP229Animation05EF4Records[114] = {
#include "assets/p229_animation_05EF4_records.inc"
};

static u16 _gP229Animation05EF4Indices[20] = {
#include "assets/p229_animation_05EF4_indices.inc"
};

static AnimationSet _gP229Animation05EF4 = {
    _gP229Animation05EF4Records,
    _gP229Animation05EF4Indices,
    { NULL, _gP229Animation05EF4Bank1, NULL, NULL, _gP229Animation05EF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0637CBank1[9] = {
#include "assets/p229_animation_0637C_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0637CBank4[104] = {
#include "assets/p229_animation_0637C_bank4.inc"
};

static AnimationRecord _gP229Animation0637CRecords[139] = {
#include "assets/p229_animation_0637C_records.inc"
};

static u16 _gP229Animation0637CIndices[20] = {
#include "assets/p229_animation_0637C_indices.inc"
};

static AnimationSet _gP229Animation0637C = {
    _gP229Animation0637CRecords,
    _gP229Animation0637CIndices,
    { NULL, _gP229Animation0637CBank1, NULL, NULL, _gP229Animation0637CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation06578Bank1[3] = {
#include "assets/p229_animation_06578_bank1.inc"
};

static AnimationPackedRotation _gP229Animation06578Bank4[22] = {
#include "assets/p229_animation_06578_bank4.inc"
};

static AnimationRecord _gP229Animation06578Records[76] = {
#include "assets/p229_animation_06578_records.inc"
};

static u16 _gP229Animation06578Indices[20] = {
#include "assets/p229_animation_06578_indices.inc"
};

static AnimationSet _gP229Animation06578 = {
    _gP229Animation06578Records,
    _gP229Animation06578Indices,
    { NULL, _gP229Animation06578Bank1, NULL, NULL, _gP229Animation06578Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation06850Bank1[6] = {
#include "assets/p229_animation_06850_bank1.inc"
};

static AnimationPackedRotation _gP229Animation06850Bank4[57] = {
#include "assets/p229_animation_06850_bank4.inc"
};

static AnimationRecord _gP229Animation06850Records[87] = {
#include "assets/p229_animation_06850_records.inc"
};

static u16 _gP229Animation06850Indices[20] = {
#include "assets/p229_animation_06850_indices.inc"
};

static AnimationSet _gP229Animation06850 = {
    _gP229Animation06850Records,
    _gP229Animation06850Indices,
    { NULL, _gP229Animation06850Bank1, NULL, NULL, _gP229Animation06850Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation06AF4Bank1[4] = {
#include "assets/p229_animation_06AF4_bank1.inc"
};

static AnimationPackedRotation _gP229Animation06AF4Bank4[55] = {
#include "assets/p229_animation_06AF4_bank4.inc"
};

static AnimationRecord _gP229Animation06AF4Records[82] = {
#include "assets/p229_animation_06AF4_records.inc"
};

static u16 _gP229Animation06AF4Indices[20] = {
#include "assets/p229_animation_06AF4_indices.inc"
};

static AnimationSet _gP229Animation06AF4 = {
    _gP229Animation06AF4Records,
    _gP229Animation06AF4Indices,
    { NULL, _gP229Animation06AF4Bank1, NULL, NULL, _gP229Animation06AF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation06CF4Bank1[3] = {
#include "assets/p229_animation_06CF4_bank1.inc"
};

static AnimationPackedRotation _gP229Animation06CF4Bank4[23] = {
#include "assets/p229_animation_06CF4_bank4.inc"
};

static AnimationRecord _gP229Animation06CF4Records[76] = {
#include "assets/p229_animation_06CF4_records.inc"
};

static u16 _gP229Animation06CF4Indices[20] = {
#include "assets/p229_animation_06CF4_indices.inc"
};

static AnimationSet _gP229Animation06CF4 = {
    _gP229Animation06CF4Records,
    _gP229Animation06CF4Indices,
    { NULL, _gP229Animation06CF4Bank1, NULL, NULL, _gP229Animation06CF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation07048Bank1[8] = {
#include "assets/p229_animation_07048_bank1.inc"
};

static AnimationPackedRotation _gP229Animation07048Bank4[68] = {
#include "assets/p229_animation_07048_bank4.inc"
};

static AnimationRecord _gP229Animation07048Records[101] = {
#include "assets/p229_animation_07048_records.inc"
};

static u16 _gP229Animation07048Indices[20] = {
#include "assets/p229_animation_07048_indices.inc"
};

static AnimationSet _gP229Animation07048 = {
    _gP229Animation07048Records,
    _gP229Animation07048Indices,
    { NULL, _gP229Animation07048Bank1, NULL, NULL, _gP229Animation07048Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation072FCBank1[5] = {
#include "assets/p229_animation_072FC_bank1.inc"
};

static AnimationPackedRotation _gP229Animation072FCBank4[55] = {
#include "assets/p229_animation_072FC_bank4.inc"
};

static AnimationRecord _gP229Animation072FCRecords[83] = {
#include "assets/p229_animation_072FC_records.inc"
};

static u16 _gP229Animation072FCIndices[20] = {
#include "assets/p229_animation_072FC_indices.inc"
};

static AnimationSet _gP229Animation072FC = {
    _gP229Animation072FCRecords,
    _gP229Animation072FCIndices,
    { NULL, _gP229Animation072FCBank1, NULL, NULL, _gP229Animation072FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0761CBank1[6] = {
#include "assets/p229_animation_0761C_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0761CBank4[66] = {
#include "assets/p229_animation_0761C_bank4.inc"
};

static AnimationRecord _gP229Animation0761CRecords[96] = {
#include "assets/p229_animation_0761C_records.inc"
};

static u16 _gP229Animation0761CIndices[20] = {
#include "assets/p229_animation_0761C_indices.inc"
};

static AnimationSet _gP229Animation0761C = {
    _gP229Animation0761CRecords,
    _gP229Animation0761CIndices,
    { NULL, _gP229Animation0761CBank1, NULL, NULL, _gP229Animation0761CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation07DE0Bank1[18] = {
#include "assets/p229_animation_07DE0_bank1.inc"
};

static AnimationPackedRotation _gP229Animation07DE0Bank4[184] = {
#include "assets/p229_animation_07DE0_bank4.inc"
};

static AnimationRecord _gP229Animation07DE0Records[239] = {
#include "assets/p229_animation_07DE0_records.inc"
};

static u16 _gP229Animation07DE0Indices[20] = {
#include "assets/p229_animation_07DE0_indices.inc"
};

static AnimationSet _gP229Animation07DE0 = {
    _gP229Animation07DE0Records,
    _gP229Animation07DE0Indices,
    { NULL, _gP229Animation07DE0Bank1, NULL, NULL, _gP229Animation07DE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation09078Bank1[29] = {
#include "assets/p229_animation_09078_bank1.inc"
};

static AnimationPackedRotation _gP229Animation09078Bank4[450] = {
#include "assets/p229_animation_09078_bank4.inc"
};

static AnimationRecord _gP229Animation09078Records[633] = {
#include "assets/p229_animation_09078_records.inc"
};

static u16 _gP229Animation09078Indices[20] = {
#include "assets/p229_animation_09078_indices.inc"
};

static AnimationSet _gP229Animation09078 = {
    _gP229Animation09078Records,
    _gP229Animation09078Indices,
    { NULL, _gP229Animation09078Bank1, NULL, NULL, _gP229Animation09078Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation09BF0Bank1[12] = {
#include "assets/p229_animation_09BF0_bank1.inc"
};

static AnimationPackedRotation _gP229Animation09BF0Bank4[266] = {
#include "assets/p229_animation_09BF0_bank4.inc"
};

static AnimationRecord _gP229Animation09BF0Records[412] = {
#include "assets/p229_animation_09BF0_records.inc"
};

static u16 _gP229Animation09BF0Indices[20] = {
#include "assets/p229_animation_09BF0_indices.inc"
};

static AnimationSet _gP229Animation09BF0 = {
    _gP229Animation09BF0Records,
    _gP229Animation09BF0Indices,
    { NULL, _gP229Animation09BF0Bank1, NULL, NULL, _gP229Animation09BF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0A30CBank1[9] = {
#include "assets/p229_animation_0A30C_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0A30CBank4[144] = {
#include "assets/p229_animation_0A30C_bank4.inc"
};

static AnimationRecord _gP229Animation0A30CRecords[264] = {
#include "assets/p229_animation_0A30C_records.inc"
};

static u16 _gP229Animation0A30CIndices[20] = {
#include "assets/p229_animation_0A30C_indices.inc"
};

static AnimationSet _gP229Animation0A30C = {
    _gP229Animation0A30CRecords,
    _gP229Animation0A30CIndices,
    { NULL, _gP229Animation0A30CBank1, NULL, NULL, _gP229Animation0A30CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0A78CBank1[6] = {
#include "assets/p229_animation_0A78C_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0A78CBank4[107] = {
#include "assets/p229_animation_0A78C_bank4.inc"
};

static AnimationRecord _gP229Animation0A78CRecords[143] = {
#include "assets/p229_animation_0A78C_records.inc"
};

static u16 _gP229Animation0A78CIndices[20] = {
#include "assets/p229_animation_0A78C_indices.inc"
};

static AnimationSet _gP229Animation0A78C = {
    _gP229Animation0A78CRecords,
    _gP229Animation0A78CIndices,
    { NULL, _gP229Animation0A78CBank1, NULL, NULL, _gP229Animation0A78CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0A964Bank1[3] = {
#include "assets/p229_animation_0A964_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0A964Bank4[32] = {
#include "assets/p229_animation_0A964_bank4.inc"
};

static AnimationRecord _gP229Animation0A964Records[57] = {
#include "assets/p229_animation_0A964_records.inc"
};

static u16 _gP229Animation0A964Indices[20] = {
#include "assets/p229_animation_0A964_indices.inc"
};

static AnimationSet _gP229Animation0A964 = {
    _gP229Animation0A964Records,
    _gP229Animation0A964Indices,
    { NULL, _gP229Animation0A964Bank1, NULL, NULL, _gP229Animation0A964Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0AEC8Bank1[11] = {
#include "assets/p229_animation_0AEC8_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0AEC8Bank4[125] = {
#include "assets/p229_animation_0AEC8_bank4.inc"
};

static AnimationRecord _gP229Animation0AEC8Records[167] = {
#include "assets/p229_animation_0AEC8_records.inc"
};

static u16 _gP229Animation0AEC8Indices[20] = {
#include "assets/p229_animation_0AEC8_indices.inc"
};

static AnimationSet _gP229Animation0AEC8 = {
    _gP229Animation0AEC8Records,
    _gP229Animation0AEC8Indices,
    { NULL, _gP229Animation0AEC8Bank1, NULL, NULL, _gP229Animation0AEC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0B0BCBank1[3] = {
#include "assets/p229_animation_0B0BC_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0B0BCBank4[20] = {
#include "assets/p229_animation_0B0BC_bank4.inc"
};

static AnimationRecord _gP229Animation0B0BCRecords[76] = {
#include "assets/p229_animation_0B0BC_records.inc"
};

static u16 _gP229Animation0B0BCIndices[20] = {
#include "assets/p229_animation_0B0BC_indices.inc"
};

static AnimationSet _gP229Animation0B0BC = {
    _gP229Animation0B0BCRecords,
    _gP229Animation0B0BCIndices,
    { NULL, _gP229Animation0B0BCBank1, NULL, NULL, _gP229Animation0B0BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0B53CBank1[8] = {
#include "assets/p229_animation_0B53C_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0B53CBank4[105] = {
#include "assets/p229_animation_0B53C_bank4.inc"
};

static AnimationRecord _gP229Animation0B53CRecords[139] = {
#include "assets/p229_animation_0B53C_records.inc"
};

static u16 _gP229Animation0B53CIndices[20] = {
#include "assets/p229_animation_0B53C_indices.inc"
};

static AnimationSet _gP229Animation0B53C = {
    _gP229Animation0B53CRecords,
    _gP229Animation0B53CIndices,
    { NULL, _gP229Animation0B53CBank1, NULL, NULL, _gP229Animation0B53CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0B718Bank1[2] = {
#include "assets/p229_animation_0B718_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0B718Bank4[17] = {
#include "assets/p229_animation_0B718_bank4.inc"
};

static AnimationRecord _gP229Animation0B718Records[76] = {
#include "assets/p229_animation_0B718_records.inc"
};

static u16 _gP229Animation0B718Indices[20] = {
#include "assets/p229_animation_0B718_indices.inc"
};

static AnimationSet _gP229Animation0B718 = {
    _gP229Animation0B718Records,
    _gP229Animation0B718Indices,
    { NULL, _gP229Animation0B718Bank1, NULL, NULL, _gP229Animation0B718Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0BE08Bank1[12] = {
#include "assets/p229_animation_0BE08_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0BE08Bank4[164] = {
#include "assets/p229_animation_0BE08_bank4.inc"
};

static AnimationRecord _gP229Animation0BE08Records[224] = {
#include "assets/p229_animation_0BE08_records.inc"
};

static u16 _gP229Animation0BE08Indices[20] = {
#include "assets/p229_animation_0BE08_indices.inc"
};

static AnimationSet _gP229Animation0BE08 = {
    _gP229Animation0BE08Records,
    _gP229Animation0BE08Indices,
    { NULL, _gP229Animation0BE08Bank1, NULL, NULL, _gP229Animation0BE08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0C9F4Bank1[19] = {
#include "assets/p229_animation_0C9F4_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0C9F4Bank4[304] = {
#include "assets/p229_animation_0C9F4_bank4.inc"
};

static AnimationRecord _gP229Animation0C9F4Records[382] = {
#include "assets/p229_animation_0C9F4_records.inc"
};

static u16 _gP229Animation0C9F4Indices[20] = {
#include "assets/p229_animation_0C9F4_indices.inc"
};

static AnimationSet _gP229Animation0C9F4 = {
    _gP229Animation0C9F4Records,
    _gP229Animation0C9F4Indices,
    { NULL, _gP229Animation0C9F4Bank1, NULL, NULL, _gP229Animation0C9F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0D31CBank1[15] = {
#include "assets/p229_animation_0D31C_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0D31CBank4[231] = {
#include "assets/p229_animation_0D31C_bank4.inc"
};

static AnimationRecord _gP229Animation0D31CRecords[290] = {
#include "assets/p229_animation_0D31C_records.inc"
};

static u16 _gP229Animation0D31CIndices[20] = {
#include "assets/p229_animation_0D31C_indices.inc"
};

static AnimationSet _gP229Animation0D31C = {
    _gP229Animation0D31CRecords,
    _gP229Animation0D31CIndices,
    { NULL, _gP229Animation0D31CBank1, NULL, NULL, _gP229Animation0D31CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0D6ECBank1[6] = {
#include "assets/p229_animation_0D6EC_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0D6ECBank4[81] = {
#include "assets/p229_animation_0D6EC_bank4.inc"
};

static AnimationRecord _gP229Animation0D6ECRecords[125] = {
#include "assets/p229_animation_0D6EC_records.inc"
};

static u16 _gP229Animation0D6ECIndices[20] = {
#include "assets/p229_animation_0D6EC_indices.inc"
};

static AnimationSet _gP229Animation0D6EC = {
    _gP229Animation0D6ECRecords,
    _gP229Animation0D6ECIndices,
    { NULL, _gP229Animation0D6ECBank1, NULL, NULL, _gP229Animation0D6ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0DD68Bank1[11] = {
#include "assets/p229_animation_0DD68_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0DD68Bank4[161] = {
#include "assets/p229_animation_0DD68_bank4.inc"
};

static AnimationRecord _gP229Animation0DD68Records[201] = {
#include "assets/p229_animation_0DD68_records.inc"
};

static u16 _gP229Animation0DD68Indices[20] = {
#include "assets/p229_animation_0DD68_indices.inc"
};

static AnimationSet _gP229Animation0DD68 = {
    _gP229Animation0DD68Records,
    _gP229Animation0DD68Indices,
    { NULL, _gP229Animation0DD68Bank1, NULL, NULL, _gP229Animation0DD68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gP229Animation0E334Bank1[10] = {
#include "assets/p229_animation_0E334_bank1.inc"
};

static AnimationPackedRotation _gP229Animation0E334Bank4[145] = {
#include "assets/p229_animation_0E334_bank4.inc"
};

static AnimationRecord _gP229Animation0E334Records[176] = {
#include "assets/p229_animation_0E334_records.inc"
};

static u16 _gP229Animation0E334Indices[20] = {
#include "assets/p229_animation_0E334_indices.inc"
};

static AnimationSet _gP229Animation0E334 = {
    _gP229Animation0E334Records,
    _gP229Animation0E334Indices,
    { NULL, _gP229Animation0E334Bank1, NULL, NULL, _gP229Animation0E334Bank4, NULL, NULL, NULL },
};

AnimationBank D_p229_8012B51C = { { {
    NULL,
    &_gP229Animation015D8,
    &_gP229Animation025C8,
    &_gP229Animation02EA0,
    &_gP229Animation035D4,
    &_gP229Animation03E34,
    &_gP229Animation04698,
    &_gP229Animation0DD68,
    &_gP229Animation0E334,
    &_gP229Animation0B718,
    &_gP229Animation0D6EC,
    &_gP229Animation0D6EC,
    &_gP229Animation0C9F4,
    &_gP229Animation0BE08,
    &_gP229Animation0D31C,
    &_gP229Animation0D31C,
    &_gP229Animation072FC,
    &_gP229Animation0761C,
    &_gP229Animation07DE0,
    &_gP229Animation01C7C,
    &_gP229Animation0D31C,
    &_gP229Animation015D8,
    &_gP229Animation015D8,
    &_gP229Animation09078,
    &_gP229Animation0A30C,
    &_gP229Animation09BF0,
    &_gP229Animation0637C,
    &_gP229Animation06578,
    &_gP229Animation06850,
    &_gP229Animation06AF4,
    &_gP229Animation06CF4,
    &_gP229Animation07048,
    &_gP229Animation0A78C,
    &_gP229Animation0A964,
    &_gP229Animation0A78C,
    &_gP229Animation0A964,
    &_gP229Animation05108,
    &_gP229Animation05890,
    &_gP229Animation05EF4,
    &_gP229Animation05B64,
    &_gP229Animation049AC,
    &_gP229Animation015D8,
    &_gP229Animation0AEC8,
    &_gP229Animation0B0BC,
    &_gP229Animation0B53C,
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

/// The four flash angles rolled on the frame the shot goes off, one per
/// `muzzleFlashDrawStreak` quad. Each is a fixed quadrant (`i << 10`) plus a
/// 10-bit LCG jitter, so the four quads always fan out around the muzzle.
s16 gMuzzleFlashAngles[4] = { 0, 0, 0, 0 };
