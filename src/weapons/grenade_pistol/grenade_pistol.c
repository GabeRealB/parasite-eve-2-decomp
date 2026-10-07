#include "weapons/grenade_pistol.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "grenade_pistol_private.h"

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
#include "gameplay/world_coords.h"

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

static void _grenadeShellExit(Task* task);

#define GRENADE_WEAPON (0xB + GRENADE_VARIANT)

/// Which weapon this build is: 0 for the Grenade Pistol, 1 for the MM1. The two
/// packages are this source built once each, and each declares its variant in
/// the manifest. Both carry the other's row of the per-projectile tables.
#ifndef GRENADE_VARIANT
#error "GRENADE_VARIANT is a per-package build parameter"
#endif

/// The weapon's index. It also keys the firing sound and the shot effect.

void func_grenade_pistol_8011D1D4(Task* arg0);

void func_grenade_pistol_8011D1D4(Task* arg0)
{
    GameActor* actor;
    s32        anim;

    actor = arg0->work;
    switch (actor->statePhase) {
        case 0:
            anim                  = 1;
            actor->state          = 4;
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            actor->statePhase    += anim;
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
            actor->statePhase                  = 3;
            actor->rumblePosted                = 0;
            actor->attackControl.cooldownTicks = 0x28;
            worldCoordPlaySound(arg0->extra.tmd->coords,
                                ((gPlayerStatus.weaponSlotItem - 0xA) << 24) | 0x20000004 | (GRENADE_WEAPON << 16), 1);
            effectSpawn(EFFECT_GRENADE_MUZZLE_FLASH,
                        actor->equipmentTasks[1]->extra.tmd->coords, GRENADE_WEAPON,
                        NULL);
            equipmentConsumeWeaponLoad(WEAPON_ITEM(GRENADE_WEAPON), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
            /* The projectile's kind and its row of the muzzle-offset and speed tables
               (bits 16-19 of its spawn argument) both follow the variant. */
            func_80104490(arg0, 0, 1 + GRENADE_VARIANT,
                          gPlayerStatus.weaponSlotItem | (GRENADE_VARIANT << 16) | (GRENADE_WEAPON << 8));
            playerActorPlayChildSlotsWithBlend(arg0, 0xA, 0, 3);
            break;
        case 3:
            if (playerActorIsSlotAdvancingLinearly(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0) {
                playerActorFinishWeaponAttack(arg0);
            }
            break;
    }
}

#include "../../shared/grenade_shell_spawn.inc.c"

#include "../../shared/grenade_shell_fly.inc.c"

#include "../../shared/grenade_shell_blast.inc.c"

#include "../../shared/grenade_shell_exit.inc.c"

static const TaskFuncTable4 D_grenade_pistol_8011D1C4 = { {
    grenadeShellSpawn,
    grenadeShellFly,
    _grenadeShellBlast,
    _grenadeShellExit,
} };

void func_grenade_pistol_8011DBD0(Task* arg0)
{
    TaskFuncTable4 handlers;

    handlers = D_grenade_pistol_8011D1C4;
    handlers.funcs[arg0->state](arg0);
}

/* Each package carries its own model. */
#if GRENADE_VARIANT == 0
static TmdBone _gGrenadePistolModel00CDCSkeleton[1] = {
#include "assets/grenade_pistol_model_00CDC_skeleton.inc"
};

static u32 _gGrenadePistolModel00CDCPartVerts[1] = {
#include "assets/grenade_pistol_model_00CDC_partVerts.inc"
};

static SVECTOR _gGrenadePistolModel00CDCVerts[36] = {
#include "assets/grenade_pistol_model_00CDC_verts.inc"
};

static SVECTOR _gGrenadePistolModel00CDCNormals[36] = {
#include "assets/grenade_pistol_model_00CDC_normals.inc"
};

static u32 _gGrenadePistolModel00CDCStream[252] = {
#include "assets/grenade_pistol_model_00CDC_stream.inc"
};

TmdSource D_grenade_pistol_8011E28C = {
    0,
    1796,
    0,
    1,
    _gGrenadePistolModel00CDCPartVerts,
    _gGrenadePistolModel00CDCVerts,
    _gGrenadePistolModel00CDCNormals,
    _gGrenadePistolModel00CDCSkeleton,
    _gGrenadePistolModel00CDCStream,
};
#elif GRENADE_VARIANT == 1
static TmdBone _gMm1Model00DD4Skeleton[1] = {
#include "assets/mm1_model_00DD4_skeleton.inc"
};

static u32 _gMm1Model00DD4PartVerts[1] = {
#include "assets/mm1_model_00DD4_partVerts.inc"
};

static SVECTOR _gMm1Model00DD4Verts[52] = {
#include "assets/mm1_model_00DD4_verts.inc"
};

static SVECTOR _gMm1Model00DD4Normals[50] = {
#include "assets/mm1_model_00DD4_normals.inc"
};

static u32 _gMm1Model00DD4Stream[320] = {
#include "assets/mm1_model_00DD4_stream.inc"
};

TmdSource D_mm1_8011E494 = {
    0,
    2292,
    0,
    1,
    _gMm1Model00DD4PartVerts,
    _gMm1Model00DD4Verts,
    _gMm1Model00DD4Normals,
    _gMm1Model00DD4Skeleton,
    _gMm1Model00DD4Stream,
};
#endif

/* Each package carries its own animation bank. */
#if GRENADE_VARIANT == 0
static AnimationPackedPose _gGrenadePistolAnimation01280Bank1[2] = {
#include "assets/grenade_pistol_animation_01280_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation01280Bank4[8] = {
#include "assets/grenade_pistol_animation_01280_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation01280Records[76] = {
#include "assets/grenade_pistol_animation_01280_records.inc"
};

static u16 _gGrenadePistolAnimation01280Indices[20] = {
#include "assets/grenade_pistol_animation_01280_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation01280 = {
    _gGrenadePistolAnimation01280Records,
    _gGrenadePistolAnimation01280Indices,
    { NULL, _gGrenadePistolAnimation01280Bank1, NULL, NULL, _gGrenadePistolAnimation01280Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation01924Bank1[12] = {
#include "assets/grenade_pistol_animation_01924_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation01924Bank4[151] = {
#include "assets/grenade_pistol_animation_01924_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation01924Records[218] = {
#include "assets/grenade_pistol_animation_01924_records.inc"
};

static u16 _gGrenadePistolAnimation01924Indices[20] = {
#include "assets/grenade_pistol_animation_01924_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation01924 = {
    _gGrenadePistolAnimation01924Records,
    _gGrenadePistolAnimation01924Indices,
    { NULL, _gGrenadePistolAnimation01924Bank1, NULL, NULL, _gGrenadePistolAnimation01924Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation02184Bank1[19] = {
#include "assets/grenade_pistol_animation_02184_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation02184Bank4[169] = {
#include "assets/grenade_pistol_animation_02184_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation02184Records[290] = {
#include "assets/grenade_pistol_animation_02184_records.inc"
};

static u16 _gGrenadePistolAnimation02184Indices[20] = {
#include "assets/grenade_pistol_animation_02184_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation02184 = {
    _gGrenadePistolAnimation02184Records,
    _gGrenadePistolAnimation02184Indices,
    { NULL, _gGrenadePistolAnimation02184Bank1, NULL, NULL, _gGrenadePistolAnimation02184Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation029E8Bank1[19] = {
#include "assets/grenade_pistol_animation_029E8_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation029E8Bank4[170] = {
#include "assets/grenade_pistol_animation_029E8_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation029E8Records[290] = {
#include "assets/grenade_pistol_animation_029E8_records.inc"
};

static u16 _gGrenadePistolAnimation029E8Indices[20] = {
#include "assets/grenade_pistol_animation_029E8_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation029E8 = {
    _gGrenadePistolAnimation029E8Records,
    _gGrenadePistolAnimation029E8Indices,
    { NULL, _gGrenadePistolAnimation029E8Bank1, NULL, NULL, _gGrenadePistolAnimation029E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation02CFCBank1[3] = {
#include "assets/grenade_pistol_animation_02CFC_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation02CFCBank4[69] = {
#include "assets/grenade_pistol_animation_02CFC_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation02CFCRecords[99] = {
#include "assets/grenade_pistol_animation_02CFC_records.inc"
};

static u16 _gGrenadePistolAnimation02CFCIndices[20] = {
#include "assets/grenade_pistol_animation_02CFC_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation02CFC = {
    _gGrenadePistolAnimation02CFCRecords,
    _gGrenadePistolAnimation02CFCIndices,
    { NULL, _gGrenadePistolAnimation02CFCBank1, NULL, NULL, _gGrenadePistolAnimation02CFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation03458Bank1[14] = {
#include "assets/grenade_pistol_animation_03458_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation03458Bank4[156] = {
#include "assets/grenade_pistol_animation_03458_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation03458Records[253] = {
#include "assets/grenade_pistol_animation_03458_records.inc"
};

static u16 _gGrenadePistolAnimation03458Indices[20] = {
#include "assets/grenade_pistol_animation_03458_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation03458 = {
    _gGrenadePistolAnimation03458Records,
    _gGrenadePistolAnimation03458Indices,
    { NULL, _gGrenadePistolAnimation03458Bank1, NULL, NULL, _gGrenadePistolAnimation03458Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation03BE0Bank1[16] = {
#include "assets/grenade_pistol_animation_03BE0_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation03BE0Bank4[167] = {
#include "assets/grenade_pistol_animation_03BE0_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation03BE0Records[247] = {
#include "assets/grenade_pistol_animation_03BE0_records.inc"
};

static u16 _gGrenadePistolAnimation03BE0Indices[20] = {
#include "assets/grenade_pistol_animation_03BE0_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation03BE0 = {
    _gGrenadePistolAnimation03BE0Records,
    _gGrenadePistolAnimation03BE0Indices,
    { NULL, _gGrenadePistolAnimation03BE0Bank1, NULL, NULL, _gGrenadePistolAnimation03BE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation03EB4Bank1[6] = {
#include "assets/grenade_pistol_animation_03EB4_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation03EB4Bank4[52] = {
#include "assets/grenade_pistol_animation_03EB4_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation03EB4Records[91] = {
#include "assets/grenade_pistol_animation_03EB4_records.inc"
};

static u16 _gGrenadePistolAnimation03EB4Indices[20] = {
#include "assets/grenade_pistol_animation_03EB4_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation03EB4 = {
    _gGrenadePistolAnimation03EB4Records,
    _gGrenadePistolAnimation03EB4Indices,
    { NULL, _gGrenadePistolAnimation03EB4Bank1, NULL, NULL, _gGrenadePistolAnimation03EB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation04244Bank1[7] = {
#include "assets/grenade_pistol_animation_04244_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation04244Bank4[73] = {
#include "assets/grenade_pistol_animation_04244_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation04244Records[114] = {
#include "assets/grenade_pistol_animation_04244_records.inc"
};

static u16 _gGrenadePistolAnimation04244Indices[20] = {
#include "assets/grenade_pistol_animation_04244_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation04244 = {
    _gGrenadePistolAnimation04244Records,
    _gGrenadePistolAnimation04244Indices,
    { NULL, _gGrenadePistolAnimation04244Bank1, NULL, NULL, _gGrenadePistolAnimation04244Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation046CCBank1[9] = {
#include "assets/grenade_pistol_animation_046CC_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation046CCBank4[104] = {
#include "assets/grenade_pistol_animation_046CC_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation046CCRecords[139] = {
#include "assets/grenade_pistol_animation_046CC_records.inc"
};

static u16 _gGrenadePistolAnimation046CCIndices[20] = {
#include "assets/grenade_pistol_animation_046CC_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation046CC = {
    _gGrenadePistolAnimation046CCRecords,
    _gGrenadePistolAnimation046CCIndices,
    { NULL, _gGrenadePistolAnimation046CCBank1, NULL, NULL, _gGrenadePistolAnimation046CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation048C8Bank1[3] = {
#include "assets/grenade_pistol_animation_048C8_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation048C8Bank4[22] = {
#include "assets/grenade_pistol_animation_048C8_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation048C8Records[76] = {
#include "assets/grenade_pistol_animation_048C8_records.inc"
};

static u16 _gGrenadePistolAnimation048C8Indices[20] = {
#include "assets/grenade_pistol_animation_048C8_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation048C8 = {
    _gGrenadePistolAnimation048C8Records,
    _gGrenadePistolAnimation048C8Indices,
    { NULL, _gGrenadePistolAnimation048C8Bank1, NULL, NULL, _gGrenadePistolAnimation048C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation04BA0Bank1[6] = {
#include "assets/grenade_pistol_animation_04BA0_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation04BA0Bank4[57] = {
#include "assets/grenade_pistol_animation_04BA0_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation04BA0Records[87] = {
#include "assets/grenade_pistol_animation_04BA0_records.inc"
};

static u16 _gGrenadePistolAnimation04BA0Indices[20] = {
#include "assets/grenade_pistol_animation_04BA0_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation04BA0 = {
    _gGrenadePistolAnimation04BA0Records,
    _gGrenadePistolAnimation04BA0Indices,
    { NULL, _gGrenadePistolAnimation04BA0Bank1, NULL, NULL, _gGrenadePistolAnimation04BA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation04E44Bank1[4] = {
#include "assets/grenade_pistol_animation_04E44_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation04E44Bank4[55] = {
#include "assets/grenade_pistol_animation_04E44_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation04E44Records[82] = {
#include "assets/grenade_pistol_animation_04E44_records.inc"
};

static u16 _gGrenadePistolAnimation04E44Indices[20] = {
#include "assets/grenade_pistol_animation_04E44_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation04E44 = {
    _gGrenadePistolAnimation04E44Records,
    _gGrenadePistolAnimation04E44Indices,
    { NULL, _gGrenadePistolAnimation04E44Bank1, NULL, NULL, _gGrenadePistolAnimation04E44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation05044Bank1[3] = {
#include "assets/grenade_pistol_animation_05044_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation05044Bank4[23] = {
#include "assets/grenade_pistol_animation_05044_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation05044Records[76] = {
#include "assets/grenade_pistol_animation_05044_records.inc"
};

static u16 _gGrenadePistolAnimation05044Indices[20] = {
#include "assets/grenade_pistol_animation_05044_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation05044 = {
    _gGrenadePistolAnimation05044Records,
    _gGrenadePistolAnimation05044Indices,
    { NULL, _gGrenadePistolAnimation05044Bank1, NULL, NULL, _gGrenadePistolAnimation05044Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation05398Bank1[8] = {
#include "assets/grenade_pistol_animation_05398_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation05398Bank4[68] = {
#include "assets/grenade_pistol_animation_05398_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation05398Records[101] = {
#include "assets/grenade_pistol_animation_05398_records.inc"
};

static u16 _gGrenadePistolAnimation05398Indices[20] = {
#include "assets/grenade_pistol_animation_05398_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation05398 = {
    _gGrenadePistolAnimation05398Records,
    _gGrenadePistolAnimation05398Indices,
    { NULL, _gGrenadePistolAnimation05398Bank1, NULL, NULL, _gGrenadePistolAnimation05398Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation0564CBank1[5] = {
#include "assets/grenade_pistol_animation_0564C_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation0564CBank4[55] = {
#include "assets/grenade_pistol_animation_0564C_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation0564CRecords[83] = {
#include "assets/grenade_pistol_animation_0564C_records.inc"
};

static u16 _gGrenadePistolAnimation0564CIndices[20] = {
#include "assets/grenade_pistol_animation_0564C_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation0564C = {
    _gGrenadePistolAnimation0564CRecords,
    _gGrenadePistolAnimation0564CIndices,
    { NULL, _gGrenadePistolAnimation0564CBank1, NULL, NULL, _gGrenadePistolAnimation0564CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation0596CBank1[6] = {
#include "assets/grenade_pistol_animation_0596C_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation0596CBank4[66] = {
#include "assets/grenade_pistol_animation_0596C_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation0596CRecords[96] = {
#include "assets/grenade_pistol_animation_0596C_records.inc"
};

static u16 _gGrenadePistolAnimation0596CIndices[20] = {
#include "assets/grenade_pistol_animation_0596C_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation0596C = {
    _gGrenadePistolAnimation0596CRecords,
    _gGrenadePistolAnimation0596CIndices,
    { NULL, _gGrenadePistolAnimation0596CBank1, NULL, NULL, _gGrenadePistolAnimation0596CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation06130Bank1[18] = {
#include "assets/grenade_pistol_animation_06130_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation06130Bank4[184] = {
#include "assets/grenade_pistol_animation_06130_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation06130Records[239] = {
#include "assets/grenade_pistol_animation_06130_records.inc"
};

static u16 _gGrenadePistolAnimation06130Indices[20] = {
#include "assets/grenade_pistol_animation_06130_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation06130 = {
    _gGrenadePistolAnimation06130Records,
    _gGrenadePistolAnimation06130Indices,
    { NULL, _gGrenadePistolAnimation06130Bank1, NULL, NULL, _gGrenadePistolAnimation06130Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation073C8Bank1[29] = {
#include "assets/grenade_pistol_animation_073C8_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation073C8Bank4[450] = {
#include "assets/grenade_pistol_animation_073C8_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation073C8Records[633] = {
#include "assets/grenade_pistol_animation_073C8_records.inc"
};

static u16 _gGrenadePistolAnimation073C8Indices[20] = {
#include "assets/grenade_pistol_animation_073C8_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation073C8 = {
    _gGrenadePistolAnimation073C8Records,
    _gGrenadePistolAnimation073C8Indices,
    { NULL, _gGrenadePistolAnimation073C8Bank1, NULL, NULL, _gGrenadePistolAnimation073C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation07F40Bank1[12] = {
#include "assets/grenade_pistol_animation_07F40_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation07F40Bank4[266] = {
#include "assets/grenade_pistol_animation_07F40_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation07F40Records[412] = {
#include "assets/grenade_pistol_animation_07F40_records.inc"
};

static u16 _gGrenadePistolAnimation07F40Indices[20] = {
#include "assets/grenade_pistol_animation_07F40_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation07F40 = {
    _gGrenadePistolAnimation07F40Records,
    _gGrenadePistolAnimation07F40Indices,
    { NULL, _gGrenadePistolAnimation07F40Bank1, NULL, NULL, _gGrenadePistolAnimation07F40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation0865CBank1[9] = {
#include "assets/grenade_pistol_animation_0865C_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation0865CBank4[144] = {
#include "assets/grenade_pistol_animation_0865C_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation0865CRecords[264] = {
#include "assets/grenade_pistol_animation_0865C_records.inc"
};

static u16 _gGrenadePistolAnimation0865CIndices[20] = {
#include "assets/grenade_pistol_animation_0865C_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation0865C = {
    _gGrenadePistolAnimation0865CRecords,
    _gGrenadePistolAnimation0865CIndices,
    { NULL, _gGrenadePistolAnimation0865CBank1, NULL, NULL, _gGrenadePistolAnimation0865CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation08ADCBank1[6] = {
#include "assets/grenade_pistol_animation_08ADC_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation08ADCBank4[107] = {
#include "assets/grenade_pistol_animation_08ADC_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation08ADCRecords[143] = {
#include "assets/grenade_pistol_animation_08ADC_records.inc"
};

static u16 _gGrenadePistolAnimation08ADCIndices[20] = {
#include "assets/grenade_pistol_animation_08ADC_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation08ADC = {
    _gGrenadePistolAnimation08ADCRecords,
    _gGrenadePistolAnimation08ADCIndices,
    { NULL, _gGrenadePistolAnimation08ADCBank1, NULL, NULL, _gGrenadePistolAnimation08ADCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation08CB4Bank1[3] = {
#include "assets/grenade_pistol_animation_08CB4_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation08CB4Bank4[32] = {
#include "assets/grenade_pistol_animation_08CB4_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation08CB4Records[57] = {
#include "assets/grenade_pistol_animation_08CB4_records.inc"
};

static u16 _gGrenadePistolAnimation08CB4Indices[20] = {
#include "assets/grenade_pistol_animation_08CB4_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation08CB4 = {
    _gGrenadePistolAnimation08CB4Records,
    _gGrenadePistolAnimation08CB4Indices,
    { NULL, _gGrenadePistolAnimation08CB4Bank1, NULL, NULL, _gGrenadePistolAnimation08CB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation09218Bank1[11] = {
#include "assets/grenade_pistol_animation_09218_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation09218Bank4[125] = {
#include "assets/grenade_pistol_animation_09218_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation09218Records[167] = {
#include "assets/grenade_pistol_animation_09218_records.inc"
};

static u16 _gGrenadePistolAnimation09218Indices[20] = {
#include "assets/grenade_pistol_animation_09218_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation09218 = {
    _gGrenadePistolAnimation09218Records,
    _gGrenadePistolAnimation09218Indices,
    { NULL, _gGrenadePistolAnimation09218Bank1, NULL, NULL, _gGrenadePistolAnimation09218Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation0940CBank1[3] = {
#include "assets/grenade_pistol_animation_0940C_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation0940CBank4[20] = {
#include "assets/grenade_pistol_animation_0940C_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation0940CRecords[76] = {
#include "assets/grenade_pistol_animation_0940C_records.inc"
};

static u16 _gGrenadePistolAnimation0940CIndices[20] = {
#include "assets/grenade_pistol_animation_0940C_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation0940C = {
    _gGrenadePistolAnimation0940CRecords,
    _gGrenadePistolAnimation0940CIndices,
    { NULL, _gGrenadePistolAnimation0940CBank1, NULL, NULL, _gGrenadePistolAnimation0940CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation0988CBank1[8] = {
#include "assets/grenade_pistol_animation_0988C_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation0988CBank4[105] = {
#include "assets/grenade_pistol_animation_0988C_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation0988CRecords[139] = {
#include "assets/grenade_pistol_animation_0988C_records.inc"
};

static u16 _gGrenadePistolAnimation0988CIndices[20] = {
#include "assets/grenade_pistol_animation_0988C_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation0988C = {
    _gGrenadePistolAnimation0988CRecords,
    _gGrenadePistolAnimation0988CIndices,
    { NULL, _gGrenadePistolAnimation0988CBank1, NULL, NULL, _gGrenadePistolAnimation0988CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation0A3F0Bank1[18] = {
#include "assets/grenade_pistol_animation_0A3F0_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation0A3F0Bank4[293] = {
#include "assets/grenade_pistol_animation_0A3F0_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation0A3F0Records[362] = {
#include "assets/grenade_pistol_animation_0A3F0_records.inc"
};

static u16 _gGrenadePistolAnimation0A3F0Indices[20] = {
#include "assets/grenade_pistol_animation_0A3F0_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation0A3F0 = {
    _gGrenadePistolAnimation0A3F0Records,
    _gGrenadePistolAnimation0A3F0Indices,
    { NULL, _gGrenadePistolAnimation0A3F0Bank1, NULL, NULL, _gGrenadePistolAnimation0A3F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation0A974Bank1[10] = {
#include "assets/grenade_pistol_animation_0A974_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation0A974Bank4[134] = {
#include "assets/grenade_pistol_animation_0A974_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation0A974Records[169] = {
#include "assets/grenade_pistol_animation_0A974_records.inc"
};

static u16 _gGrenadePistolAnimation0A974Indices[20] = {
#include "assets/grenade_pistol_animation_0A974_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation0A974 = {
    _gGrenadePistolAnimation0A974Records,
    _gGrenadePistolAnimation0A974Indices,
    { NULL, _gGrenadePistolAnimation0A974Bank1, NULL, NULL, _gGrenadePistolAnimation0A974Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation0B41CBank1[19] = {
#include "assets/grenade_pistol_animation_0B41C_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation0B41CBank4[269] = {
#include "assets/grenade_pistol_animation_0B41C_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation0B41CRecords[336] = {
#include "assets/grenade_pistol_animation_0B41C_records.inc"
};

static u16 _gGrenadePistolAnimation0B41CIndices[20] = {
#include "assets/grenade_pistol_animation_0B41C_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation0B41C = {
    _gGrenadePistolAnimation0B41CRecords,
    _gGrenadePistolAnimation0B41CIndices,
    { NULL, _gGrenadePistolAnimation0B41CBank1, NULL, NULL, _gGrenadePistolAnimation0B41CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation0B5F4Bank1[2] = {
#include "assets/grenade_pistol_animation_0B5F4_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation0B5F4Bank4[16] = {
#include "assets/grenade_pistol_animation_0B5F4_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation0B5F4Records[76] = {
#include "assets/grenade_pistol_animation_0B5F4_records.inc"
};

static u16 _gGrenadePistolAnimation0B5F4Indices[20] = {
#include "assets/grenade_pistol_animation_0B5F4_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation0B5F4 = {
    _gGrenadePistolAnimation0B5F4Records,
    _gGrenadePistolAnimation0B5F4Indices,
    { NULL, _gGrenadePistolAnimation0B5F4Bank1, NULL, NULL, _gGrenadePistolAnimation0B5F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation0BD28Bank1[12] = {
#include "assets/grenade_pistol_animation_0BD28_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation0BD28Bank4[165] = {
#include "assets/grenade_pistol_animation_0BD28_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation0BD28Records[240] = {
#include "assets/grenade_pistol_animation_0BD28_records.inc"
};

static u16 _gGrenadePistolAnimation0BD28Indices[20] = {
#include "assets/grenade_pistol_animation_0BD28_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation0BD28 = {
    _gGrenadePistolAnimation0BD28Records,
    _gGrenadePistolAnimation0BD28Indices,
    { NULL, _gGrenadePistolAnimation0BD28Bank1, NULL, NULL, _gGrenadePistolAnimation0BD28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation0C218Bank1[9] = {
#include "assets/grenade_pistol_animation_0C218_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation0C218Bank4[116] = {
#include "assets/grenade_pistol_animation_0C218_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation0C218Records[153] = {
#include "assets/grenade_pistol_animation_0C218_records.inc"
};

static u16 _gGrenadePistolAnimation0C218Indices[20] = {
#include "assets/grenade_pistol_animation_0C218_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation0C218 = {
    _gGrenadePistolAnimation0C218Records,
    _gGrenadePistolAnimation0C218Indices,
    { NULL, _gGrenadePistolAnimation0C218Bank1, NULL, NULL, _gGrenadePistolAnimation0C218Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation0C7A4Bank1[10] = {
#include "assets/grenade_pistol_animation_0C7A4_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation0C7A4Bank4[136] = {
#include "assets/grenade_pistol_animation_0C7A4_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation0C7A4Records[169] = {
#include "assets/grenade_pistol_animation_0C7A4_records.inc"
};

static u16 _gGrenadePistolAnimation0C7A4Indices[20] = {
#include "assets/grenade_pistol_animation_0C7A4_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation0C7A4 = {
    _gGrenadePistolAnimation0C7A4Records,
    _gGrenadePistolAnimation0C7A4Indices,
    { NULL, _gGrenadePistolAnimation0C7A4Bank1, NULL, NULL, _gGrenadePistolAnimation0C7A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation0D0F0Bank1[16] = {
#include "assets/grenade_pistol_animation_0D0F0_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation0D0F0Bank4[221] = {
#include "assets/grenade_pistol_animation_0D0F0_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation0D0F0Records[306] = {
#include "assets/grenade_pistol_animation_0D0F0_records.inc"
};

static u16 _gGrenadePistolAnimation0D0F0Indices[20] = {
#include "assets/grenade_pistol_animation_0D0F0_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation0D0F0 = {
    _gGrenadePistolAnimation0D0F0Records,
    _gGrenadePistolAnimation0D0F0Indices,
    { NULL, _gGrenadePistolAnimation0D0F0Bank1, NULL, NULL, _gGrenadePistolAnimation0D0F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation0D9C8Bank1[19] = {
#include "assets/grenade_pistol_animation_0D9C8_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation0D9C8Bank4[193] = {
#include "assets/grenade_pistol_animation_0D9C8_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation0D9C8Records[296] = {
#include "assets/grenade_pistol_animation_0D9C8_records.inc"
};

static u16 _gGrenadePistolAnimation0D9C8Indices[20] = {
#include "assets/grenade_pistol_animation_0D9C8_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation0D9C8 = {
    _gGrenadePistolAnimation0D9C8Records,
    _gGrenadePistolAnimation0D9C8Indices,
    { NULL, _gGrenadePistolAnimation0D9C8Bank1, NULL, NULL, _gGrenadePistolAnimation0D9C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gGrenadePistolAnimation0E0FCBank1[13] = {
#include "assets/grenade_pistol_animation_0E0FC_bank1.inc"
};

static AnimationPackedRotation _gGrenadePistolAnimation0E0FCBank4[167] = {
#include "assets/grenade_pistol_animation_0E0FC_bank4.inc"
};

static AnimationRecord _gGrenadePistolAnimation0E0FCRecords[235] = {
#include "assets/grenade_pistol_animation_0E0FC_records.inc"
};

static u16 _gGrenadePistolAnimation0E0FCIndices[20] = {
#include "assets/grenade_pistol_animation_0E0FC_indices.inc"
};

static AnimationSet _gGrenadePistolAnimation0E0FC = {
    _gGrenadePistolAnimation0E0FCRecords,
    _gGrenadePistolAnimation0E0FCIndices,
    { NULL, _gGrenadePistolAnimation0E0FCBank1, NULL, NULL, _gGrenadePistolAnimation0E0FCBank4, NULL, NULL, NULL },
};

AnimationBank D_grenade_pistol_8012B2E4 = { { {
    NULL,
    &_gGrenadePistolAnimation01280,
    &_gGrenadePistolAnimation0D0F0,
    &_gGrenadePistolAnimation0D9C8,
    &_gGrenadePistolAnimation0E0FC,
    &_gGrenadePistolAnimation02184,
    &_gGrenadePistolAnimation029E8,
    &_gGrenadePistolAnimation0C218,
    &_gGrenadePistolAnimation0C7A4,
    &_gGrenadePistolAnimation0B5F4,
    &_gGrenadePistolAnimation0A974,
    &_gGrenadePistolAnimation0A974,
    &_gGrenadePistolAnimation0B41C,
    &_gGrenadePistolAnimation0BD28,
    &_gGrenadePistolAnimation0A3F0,
    &_gGrenadePistolAnimation0A3F0,
    &_gGrenadePistolAnimation0564C,
    &_gGrenadePistolAnimation0596C,
    &_gGrenadePistolAnimation06130,
    &_gGrenadePistolAnimation01924,
    &_gGrenadePistolAnimation0A3F0,
    &_gGrenadePistolAnimation01280,
    &_gGrenadePistolAnimation01280,
    &_gGrenadePistolAnimation073C8,
    &_gGrenadePistolAnimation0865C,
    &_gGrenadePistolAnimation07F40,
    &_gGrenadePistolAnimation046CC,
    &_gGrenadePistolAnimation048C8,
    &_gGrenadePistolAnimation04BA0,
    &_gGrenadePistolAnimation04E44,
    &_gGrenadePistolAnimation05044,
    &_gGrenadePistolAnimation05398,
    &_gGrenadePistolAnimation08ADC,
    &_gGrenadePistolAnimation08CB4,
    &_gGrenadePistolAnimation08ADC,
    &_gGrenadePistolAnimation08CB4,
    &_gGrenadePistolAnimation03458,
    &_gGrenadePistolAnimation03BE0,
    &_gGrenadePistolAnimation04244,
    &_gGrenadePistolAnimation03EB4,
    &_gGrenadePistolAnimation02CFC,
    &_gGrenadePistolAnimation01280,
    &_gGrenadePistolAnimation09218,
    &_gGrenadePistolAnimation0940C,
    &_gGrenadePistolAnimation0988C,
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
#elif GRENADE_VARIANT == 1
static AnimationPackedPose _gMm1Animation01488Bank1[2] = {
#include "assets/mm1_animation_01488_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation01488Bank4[8] = {
#include "assets/mm1_animation_01488_bank4.inc"
};

static AnimationRecord _gMm1Animation01488Records[76] = {
#include "assets/mm1_animation_01488_records.inc"
};

static u16 _gMm1Animation01488Indices[20] = {
#include "assets/mm1_animation_01488_indices.inc"
};

static AnimationSet _gMm1Animation01488 = {
    _gMm1Animation01488Records,
    _gMm1Animation01488Indices,
    { NULL, _gMm1Animation01488Bank1, NULL, NULL, _gMm1Animation01488Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation01B2CBank1[12] = {
#include "assets/mm1_animation_01B2C_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation01B2CBank4[151] = {
#include "assets/mm1_animation_01B2C_bank4.inc"
};

static AnimationRecord _gMm1Animation01B2CRecords[218] = {
#include "assets/mm1_animation_01B2C_records.inc"
};

static u16 _gMm1Animation01B2CIndices[20] = {
#include "assets/mm1_animation_01B2C_indices.inc"
};

static AnimationSet _gMm1Animation01B2C = {
    _gMm1Animation01B2CRecords,
    _gMm1Animation01B2CIndices,
    { NULL, _gMm1Animation01B2CBank1, NULL, NULL, _gMm1Animation01B2CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation02404Bank1[19] = {
#include "assets/mm1_animation_02404_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation02404Bank4[193] = {
#include "assets/mm1_animation_02404_bank4.inc"
};

static AnimationRecord _gMm1Animation02404Records[296] = {
#include "assets/mm1_animation_02404_records.inc"
};

static u16 _gMm1Animation02404Indices[20] = {
#include "assets/mm1_animation_02404_indices.inc"
};

static AnimationSet _gMm1Animation02404 = {
    _gMm1Animation02404Records,
    _gMm1Animation02404Indices,
    { NULL, _gMm1Animation02404Bank1, NULL, NULL, _gMm1Animation02404Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation02C64Bank1[19] = {
#include "assets/mm1_animation_02C64_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation02C64Bank4[169] = {
#include "assets/mm1_animation_02C64_bank4.inc"
};

static AnimationRecord _gMm1Animation02C64Records[290] = {
#include "assets/mm1_animation_02C64_records.inc"
};

static u16 _gMm1Animation02C64Indices[20] = {
#include "assets/mm1_animation_02C64_indices.inc"
};

static AnimationSet _gMm1Animation02C64 = {
    _gMm1Animation02C64Records,
    _gMm1Animation02C64Indices,
    { NULL, _gMm1Animation02C64Bank1, NULL, NULL, _gMm1Animation02C64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation034C8Bank1[19] = {
#include "assets/mm1_animation_034C8_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation034C8Bank4[170] = {
#include "assets/mm1_animation_034C8_bank4.inc"
};

static AnimationRecord _gMm1Animation034C8Records[290] = {
#include "assets/mm1_animation_034C8_records.inc"
};

static u16 _gMm1Animation034C8Indices[20] = {
#include "assets/mm1_animation_034C8_indices.inc"
};

static AnimationSet _gMm1Animation034C8 = {
    _gMm1Animation034C8Records,
    _gMm1Animation034C8Indices,
    { NULL, _gMm1Animation034C8Bank1, NULL, NULL, _gMm1Animation034C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation037DCBank1[3] = {
#include "assets/mm1_animation_037DC_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation037DCBank4[69] = {
#include "assets/mm1_animation_037DC_bank4.inc"
};

static AnimationRecord _gMm1Animation037DCRecords[99] = {
#include "assets/mm1_animation_037DC_records.inc"
};

static u16 _gMm1Animation037DCIndices[20] = {
#include "assets/mm1_animation_037DC_indices.inc"
};

static AnimationSet _gMm1Animation037DC = {
    _gMm1Animation037DCRecords,
    _gMm1Animation037DCIndices,
    { NULL, _gMm1Animation037DCBank1, NULL, NULL, _gMm1Animation037DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation03F38Bank1[14] = {
#include "assets/mm1_animation_03F38_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation03F38Bank4[156] = {
#include "assets/mm1_animation_03F38_bank4.inc"
};

static AnimationRecord _gMm1Animation03F38Records[253] = {
#include "assets/mm1_animation_03F38_records.inc"
};

static u16 _gMm1Animation03F38Indices[20] = {
#include "assets/mm1_animation_03F38_indices.inc"
};

static AnimationSet _gMm1Animation03F38 = {
    _gMm1Animation03F38Records,
    _gMm1Animation03F38Indices,
    { NULL, _gMm1Animation03F38Bank1, NULL, NULL, _gMm1Animation03F38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation046C0Bank1[16] = {
#include "assets/mm1_animation_046C0_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation046C0Bank4[167] = {
#include "assets/mm1_animation_046C0_bank4.inc"
};

static AnimationRecord _gMm1Animation046C0Records[247] = {
#include "assets/mm1_animation_046C0_records.inc"
};

static u16 _gMm1Animation046C0Indices[20] = {
#include "assets/mm1_animation_046C0_indices.inc"
};

static AnimationSet _gMm1Animation046C0 = {
    _gMm1Animation046C0Records,
    _gMm1Animation046C0Indices,
    { NULL, _gMm1Animation046C0Bank1, NULL, NULL, _gMm1Animation046C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation04994Bank1[6] = {
#include "assets/mm1_animation_04994_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation04994Bank4[52] = {
#include "assets/mm1_animation_04994_bank4.inc"
};

static AnimationRecord _gMm1Animation04994Records[91] = {
#include "assets/mm1_animation_04994_records.inc"
};

static u16 _gMm1Animation04994Indices[20] = {
#include "assets/mm1_animation_04994_indices.inc"
};

static AnimationSet _gMm1Animation04994 = {
    _gMm1Animation04994Records,
    _gMm1Animation04994Indices,
    { NULL, _gMm1Animation04994Bank1, NULL, NULL, _gMm1Animation04994Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation04D24Bank1[7] = {
#include "assets/mm1_animation_04D24_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation04D24Bank4[73] = {
#include "assets/mm1_animation_04D24_bank4.inc"
};

static AnimationRecord _gMm1Animation04D24Records[114] = {
#include "assets/mm1_animation_04D24_records.inc"
};

static u16 _gMm1Animation04D24Indices[20] = {
#include "assets/mm1_animation_04D24_indices.inc"
};

static AnimationSet _gMm1Animation04D24 = {
    _gMm1Animation04D24Records,
    _gMm1Animation04D24Indices,
    { NULL, _gMm1Animation04D24Bank1, NULL, NULL, _gMm1Animation04D24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation051ACBank1[9] = {
#include "assets/mm1_animation_051AC_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation051ACBank4[104] = {
#include "assets/mm1_animation_051AC_bank4.inc"
};

static AnimationRecord _gMm1Animation051ACRecords[139] = {
#include "assets/mm1_animation_051AC_records.inc"
};

static u16 _gMm1Animation051ACIndices[20] = {
#include "assets/mm1_animation_051AC_indices.inc"
};

static AnimationSet _gMm1Animation051AC = {
    _gMm1Animation051ACRecords,
    _gMm1Animation051ACIndices,
    { NULL, _gMm1Animation051ACBank1, NULL, NULL, _gMm1Animation051ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation053A8Bank1[3] = {
#include "assets/mm1_animation_053A8_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation053A8Bank4[22] = {
#include "assets/mm1_animation_053A8_bank4.inc"
};

static AnimationRecord _gMm1Animation053A8Records[76] = {
#include "assets/mm1_animation_053A8_records.inc"
};

static u16 _gMm1Animation053A8Indices[20] = {
#include "assets/mm1_animation_053A8_indices.inc"
};

static AnimationSet _gMm1Animation053A8 = {
    _gMm1Animation053A8Records,
    _gMm1Animation053A8Indices,
    { NULL, _gMm1Animation053A8Bank1, NULL, NULL, _gMm1Animation053A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation05680Bank1[6] = {
#include "assets/mm1_animation_05680_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation05680Bank4[57] = {
#include "assets/mm1_animation_05680_bank4.inc"
};

static AnimationRecord _gMm1Animation05680Records[87] = {
#include "assets/mm1_animation_05680_records.inc"
};

static u16 _gMm1Animation05680Indices[20] = {
#include "assets/mm1_animation_05680_indices.inc"
};

static AnimationSet _gMm1Animation05680 = {
    _gMm1Animation05680Records,
    _gMm1Animation05680Indices,
    { NULL, _gMm1Animation05680Bank1, NULL, NULL, _gMm1Animation05680Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation05924Bank1[4] = {
#include "assets/mm1_animation_05924_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation05924Bank4[55] = {
#include "assets/mm1_animation_05924_bank4.inc"
};

static AnimationRecord _gMm1Animation05924Records[82] = {
#include "assets/mm1_animation_05924_records.inc"
};

static u16 _gMm1Animation05924Indices[20] = {
#include "assets/mm1_animation_05924_indices.inc"
};

static AnimationSet _gMm1Animation05924 = {
    _gMm1Animation05924Records,
    _gMm1Animation05924Indices,
    { NULL, _gMm1Animation05924Bank1, NULL, NULL, _gMm1Animation05924Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation05B24Bank1[3] = {
#include "assets/mm1_animation_05B24_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation05B24Bank4[23] = {
#include "assets/mm1_animation_05B24_bank4.inc"
};

static AnimationRecord _gMm1Animation05B24Records[76] = {
#include "assets/mm1_animation_05B24_records.inc"
};

static u16 _gMm1Animation05B24Indices[20] = {
#include "assets/mm1_animation_05B24_indices.inc"
};

static AnimationSet _gMm1Animation05B24 = {
    _gMm1Animation05B24Records,
    _gMm1Animation05B24Indices,
    { NULL, _gMm1Animation05B24Bank1, NULL, NULL, _gMm1Animation05B24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation05E78Bank1[8] = {
#include "assets/mm1_animation_05E78_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation05E78Bank4[68] = {
#include "assets/mm1_animation_05E78_bank4.inc"
};

static AnimationRecord _gMm1Animation05E78Records[101] = {
#include "assets/mm1_animation_05E78_records.inc"
};

static u16 _gMm1Animation05E78Indices[20] = {
#include "assets/mm1_animation_05E78_indices.inc"
};

static AnimationSet _gMm1Animation05E78 = {
    _gMm1Animation05E78Records,
    _gMm1Animation05E78Indices,
    { NULL, _gMm1Animation05E78Bank1, NULL, NULL, _gMm1Animation05E78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation0612CBank1[5] = {
#include "assets/mm1_animation_0612C_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation0612CBank4[55] = {
#include "assets/mm1_animation_0612C_bank4.inc"
};

static AnimationRecord _gMm1Animation0612CRecords[83] = {
#include "assets/mm1_animation_0612C_records.inc"
};

static u16 _gMm1Animation0612CIndices[20] = {
#include "assets/mm1_animation_0612C_indices.inc"
};

static AnimationSet _gMm1Animation0612C = {
    _gMm1Animation0612CRecords,
    _gMm1Animation0612CIndices,
    { NULL, _gMm1Animation0612CBank1, NULL, NULL, _gMm1Animation0612CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation0644CBank1[6] = {
#include "assets/mm1_animation_0644C_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation0644CBank4[66] = {
#include "assets/mm1_animation_0644C_bank4.inc"
};

static AnimationRecord _gMm1Animation0644CRecords[96] = {
#include "assets/mm1_animation_0644C_records.inc"
};

static u16 _gMm1Animation0644CIndices[20] = {
#include "assets/mm1_animation_0644C_indices.inc"
};

static AnimationSet _gMm1Animation0644C = {
    _gMm1Animation0644CRecords,
    _gMm1Animation0644CIndices,
    { NULL, _gMm1Animation0644CBank1, NULL, NULL, _gMm1Animation0644CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation06C10Bank1[18] = {
#include "assets/mm1_animation_06C10_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation06C10Bank4[184] = {
#include "assets/mm1_animation_06C10_bank4.inc"
};

static AnimationRecord _gMm1Animation06C10Records[239] = {
#include "assets/mm1_animation_06C10_records.inc"
};

static u16 _gMm1Animation06C10Indices[20] = {
#include "assets/mm1_animation_06C10_indices.inc"
};

static AnimationSet _gMm1Animation06C10 = {
    _gMm1Animation06C10Records,
    _gMm1Animation06C10Indices,
    { NULL, _gMm1Animation06C10Bank1, NULL, NULL, _gMm1Animation06C10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation07EA8Bank1[29] = {
#include "assets/mm1_animation_07EA8_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation07EA8Bank4[450] = {
#include "assets/mm1_animation_07EA8_bank4.inc"
};

static AnimationRecord _gMm1Animation07EA8Records[633] = {
#include "assets/mm1_animation_07EA8_records.inc"
};

static u16 _gMm1Animation07EA8Indices[20] = {
#include "assets/mm1_animation_07EA8_indices.inc"
};

static AnimationSet _gMm1Animation07EA8 = {
    _gMm1Animation07EA8Records,
    _gMm1Animation07EA8Indices,
    { NULL, _gMm1Animation07EA8Bank1, NULL, NULL, _gMm1Animation07EA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation08A20Bank1[12] = {
#include "assets/mm1_animation_08A20_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation08A20Bank4[266] = {
#include "assets/mm1_animation_08A20_bank4.inc"
};

static AnimationRecord _gMm1Animation08A20Records[412] = {
#include "assets/mm1_animation_08A20_records.inc"
};

static u16 _gMm1Animation08A20Indices[20] = {
#include "assets/mm1_animation_08A20_indices.inc"
};

static AnimationSet _gMm1Animation08A20 = {
    _gMm1Animation08A20Records,
    _gMm1Animation08A20Indices,
    { NULL, _gMm1Animation08A20Bank1, NULL, NULL, _gMm1Animation08A20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation0913CBank1[9] = {
#include "assets/mm1_animation_0913C_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation0913CBank4[144] = {
#include "assets/mm1_animation_0913C_bank4.inc"
};

static AnimationRecord _gMm1Animation0913CRecords[264] = {
#include "assets/mm1_animation_0913C_records.inc"
};

static u16 _gMm1Animation0913CIndices[20] = {
#include "assets/mm1_animation_0913C_indices.inc"
};

static AnimationSet _gMm1Animation0913C = {
    _gMm1Animation0913CRecords,
    _gMm1Animation0913CIndices,
    { NULL, _gMm1Animation0913CBank1, NULL, NULL, _gMm1Animation0913CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation095BCBank1[6] = {
#include "assets/mm1_animation_095BC_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation095BCBank4[107] = {
#include "assets/mm1_animation_095BC_bank4.inc"
};

static AnimationRecord _gMm1Animation095BCRecords[143] = {
#include "assets/mm1_animation_095BC_records.inc"
};

static u16 _gMm1Animation095BCIndices[20] = {
#include "assets/mm1_animation_095BC_indices.inc"
};

static AnimationSet _gMm1Animation095BC = {
    _gMm1Animation095BCRecords,
    _gMm1Animation095BCIndices,
    { NULL, _gMm1Animation095BCBank1, NULL, NULL, _gMm1Animation095BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation09794Bank1[3] = {
#include "assets/mm1_animation_09794_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation09794Bank4[32] = {
#include "assets/mm1_animation_09794_bank4.inc"
};

static AnimationRecord _gMm1Animation09794Records[57] = {
#include "assets/mm1_animation_09794_records.inc"
};

static u16 _gMm1Animation09794Indices[20] = {
#include "assets/mm1_animation_09794_indices.inc"
};

static AnimationSet _gMm1Animation09794 = {
    _gMm1Animation09794Records,
    _gMm1Animation09794Indices,
    { NULL, _gMm1Animation09794Bank1, NULL, NULL, _gMm1Animation09794Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation09CF8Bank1[11] = {
#include "assets/mm1_animation_09CF8_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation09CF8Bank4[125] = {
#include "assets/mm1_animation_09CF8_bank4.inc"
};

static AnimationRecord _gMm1Animation09CF8Records[167] = {
#include "assets/mm1_animation_09CF8_records.inc"
};

static u16 _gMm1Animation09CF8Indices[20] = {
#include "assets/mm1_animation_09CF8_indices.inc"
};

static AnimationSet _gMm1Animation09CF8 = {
    _gMm1Animation09CF8Records,
    _gMm1Animation09CF8Indices,
    { NULL, _gMm1Animation09CF8Bank1, NULL, NULL, _gMm1Animation09CF8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation09EECBank1[3] = {
#include "assets/mm1_animation_09EEC_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation09EECBank4[20] = {
#include "assets/mm1_animation_09EEC_bank4.inc"
};

static AnimationRecord _gMm1Animation09EECRecords[76] = {
#include "assets/mm1_animation_09EEC_records.inc"
};

static u16 _gMm1Animation09EECIndices[20] = {
#include "assets/mm1_animation_09EEC_indices.inc"
};

static AnimationSet _gMm1Animation09EEC = {
    _gMm1Animation09EECRecords,
    _gMm1Animation09EECIndices,
    { NULL, _gMm1Animation09EECBank1, NULL, NULL, _gMm1Animation09EECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation0A36CBank1[8] = {
#include "assets/mm1_animation_0A36C_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation0A36CBank4[105] = {
#include "assets/mm1_animation_0A36C_bank4.inc"
};

static AnimationRecord _gMm1Animation0A36CRecords[139] = {
#include "assets/mm1_animation_0A36C_records.inc"
};

static u16 _gMm1Animation0A36CIndices[20] = {
#include "assets/mm1_animation_0A36C_indices.inc"
};

static AnimationSet _gMm1Animation0A36C = {
    _gMm1Animation0A36CRecords,
    _gMm1Animation0A36CIndices,
    { NULL, _gMm1Animation0A36CBank1, NULL, NULL, _gMm1Animation0A36CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation0A544Bank1[2] = {
#include "assets/mm1_animation_0A544_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation0A544Bank4[16] = {
#include "assets/mm1_animation_0A544_bank4.inc"
};

static AnimationRecord _gMm1Animation0A544Records[76] = {
#include "assets/mm1_animation_0A544_records.inc"
};

static u16 _gMm1Animation0A544Indices[20] = {
#include "assets/mm1_animation_0A544_indices.inc"
};

static AnimationSet _gMm1Animation0A544 = {
    _gMm1Animation0A544Records,
    _gMm1Animation0A544Indices,
    { NULL, _gMm1Animation0A544Bank1, NULL, NULL, _gMm1Animation0A544Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation0ACFCBank1[13] = {
#include "assets/mm1_animation_0ACFC_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation0ACFCBank4[188] = {
#include "assets/mm1_animation_0ACFC_bank4.inc"
};

static AnimationRecord _gMm1Animation0ACFCRecords[247] = {
#include "assets/mm1_animation_0ACFC_records.inc"
};

static u16 _gMm1Animation0ACFCIndices[20] = {
#include "assets/mm1_animation_0ACFC_indices.inc"
};

static AnimationSet _gMm1Animation0ACFC = {
    _gMm1Animation0ACFCRecords,
    _gMm1Animation0ACFCIndices,
    { NULL, _gMm1Animation0ACFCBank1, NULL, NULL, _gMm1Animation0ACFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation0B7A4Bank1[19] = {
#include "assets/mm1_animation_0B7A4_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation0B7A4Bank4[269] = {
#include "assets/mm1_animation_0B7A4_bank4.inc"
};

static AnimationRecord _gMm1Animation0B7A4Records[336] = {
#include "assets/mm1_animation_0B7A4_records.inc"
};

static u16 _gMm1Animation0B7A4Indices[20] = {
#include "assets/mm1_animation_0B7A4_indices.inc"
};

static AnimationSet _gMm1Animation0B7A4 = {
    _gMm1Animation0B7A4Records,
    _gMm1Animation0B7A4Indices,
    { NULL, _gMm1Animation0B7A4Bank1, NULL, NULL, _gMm1Animation0B7A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation0CE20Bank1[30] = {
#include "assets/mm1_animation_0CE20_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation0CE20Bank4[497] = {
#include "assets/mm1_animation_0CE20_bank4.inc"
};

static AnimationRecord _gMm1Animation0CE20Records[832] = {
#include "assets/mm1_animation_0CE20_records.inc"
};

static u16 _gMm1Animation0CE20Indices[20] = {
#include "assets/mm1_animation_0CE20_indices.inc"
};

static AnimationSet _gMm1Animation0CE20 = {
    _gMm1Animation0CE20Records,
    _gMm1Animation0CE20Indices,
    { NULL, _gMm1Animation0CE20Bank1, NULL, NULL, _gMm1Animation0CE20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation0D4BCBank1[11] = {
#include "assets/mm1_animation_0D4BC_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation0D4BCBank4[160] = {
#include "assets/mm1_animation_0D4BC_bank4.inc"
};

static AnimationRecord _gMm1Animation0D4BCRecords[210] = {
#include "assets/mm1_animation_0D4BC_records.inc"
};

static u16 _gMm1Animation0D4BCIndices[20] = {
#include "assets/mm1_animation_0D4BC_indices.inc"
};

static AnimationSet _gMm1Animation0D4BC = {
    _gMm1Animation0D4BCRecords,
    _gMm1Animation0D4BCIndices,
    { NULL, _gMm1Animation0D4BCBank1, NULL, NULL, _gMm1Animation0D4BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation0DF5CBank1[19] = {
#include "assets/mm1_animation_0DF5C_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation0DF5CBank4[277] = {
#include "assets/mm1_animation_0DF5C_bank4.inc"
};

static AnimationRecord _gMm1Animation0DF5CRecords[326] = {
#include "assets/mm1_animation_0DF5C_records.inc"
};

static u16 _gMm1Animation0DF5CIndices[20] = {
#include "assets/mm1_animation_0DF5C_indices.inc"
};

static AnimationSet _gMm1Animation0DF5C = {
    _gMm1Animation0DF5CRecords,
    _gMm1Animation0DF5CIndices,
    { NULL, _gMm1Animation0DF5CBank1, NULL, NULL, _gMm1Animation0DF5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation0E570Bank1[11] = {
#include "assets/mm1_animation_0E570_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation0E570Bank4[151] = {
#include "assets/mm1_animation_0E570_bank4.inc"
};

static AnimationRecord _gMm1Animation0E570Records[185] = {
#include "assets/mm1_animation_0E570_records.inc"
};

static u16 _gMm1Animation0E570Indices[20] = {
#include "assets/mm1_animation_0E570_indices.inc"
};

static AnimationSet _gMm1Animation0E570 = {
    _gMm1Animation0E570Records,
    _gMm1Animation0E570Indices,
    { NULL, _gMm1Animation0E570Bank1, NULL, NULL, _gMm1Animation0E570Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation0EE28Bank1[18] = {
#include "assets/mm1_animation_0EE28_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation0EE28Bank4[201] = {
#include "assets/mm1_animation_0EE28_bank4.inc"
};

static AnimationRecord _gMm1Animation0EE28Records[283] = {
#include "assets/mm1_animation_0EE28_records.inc"
};

static u16 _gMm1Animation0EE28Indices[20] = {
#include "assets/mm1_animation_0EE28_indices.inc"
};

static AnimationSet _gMm1Animation0EE28 = {
    _gMm1Animation0EE28Records,
    _gMm1Animation0EE28Indices,
    { NULL, _gMm1Animation0EE28Bank1, NULL, NULL, _gMm1Animation0EE28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation0F5C8Bank1[16] = {
#include "assets/mm1_animation_0F5C8_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation0F5C8Bank4[157] = {
#include "assets/mm1_animation_0F5C8_bank4.inc"
};

static AnimationRecord _gMm1Animation0F5C8Records[263] = {
#include "assets/mm1_animation_0F5C8_records.inc"
};

static u16 _gMm1Animation0F5C8Indices[20] = {
#include "assets/mm1_animation_0F5C8_indices.inc"
};

static AnimationSet _gMm1Animation0F5C8 = {
    _gMm1Animation0F5C8Records,
    _gMm1Animation0F5C8Indices,
    { NULL, _gMm1Animation0F5C8Bank1, NULL, NULL, _gMm1Animation0F5C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMm1Animation0FF9CBank1[25] = {
#include "assets/mm1_animation_0FF9C_bank1.inc"
};

static AnimationPackedRotation _gMm1Animation0FF9CBank4[223] = {
#include "assets/mm1_animation_0FF9C_bank4.inc"
};

static AnimationRecord _gMm1Animation0FF9CRecords[311] = {
#include "assets/mm1_animation_0FF9C_records.inc"
};

static u16 _gMm1Animation0FF9CIndices[20] = {
#include "assets/mm1_animation_0FF9C_indices.inc"
};

static AnimationSet _gMm1Animation0FF9C = {
    _gMm1Animation0FF9CRecords,
    _gMm1Animation0FF9CIndices,
    { NULL, _gMm1Animation0FF9CBank1, NULL, NULL, _gMm1Animation0FF9CBank4, NULL, NULL, NULL },
};

AnimationBank D_mm1_8012D184 = { { {
    NULL,
    &_gMm1Animation01488,
    &_gMm1Animation0EE28,
    &_gMm1Animation0F5C8,
    &_gMm1Animation0FF9C,
    &_gMm1Animation02C64,
    &_gMm1Animation034C8,
    &_gMm1Animation0DF5C,
    &_gMm1Animation0E570,
    &_gMm1Animation0A544,
    &_gMm1Animation0D4BC,
    &_gMm1Animation0D4BC,
    &_gMm1Animation0B7A4,
    &_gMm1Animation0ACFC,
    &_gMm1Animation0CE20,
    &_gMm1Animation0CE20,
    &_gMm1Animation0612C,
    &_gMm1Animation0644C,
    &_gMm1Animation06C10,
    &_gMm1Animation01B2C,
    &_gMm1Animation0CE20,
    &_gMm1Animation01488,
    &_gMm1Animation01488,
    &_gMm1Animation07EA8,
    &_gMm1Animation0913C,
    &_gMm1Animation08A20,
    &_gMm1Animation051AC,
    &_gMm1Animation053A8,
    &_gMm1Animation05680,
    &_gMm1Animation05924,
    &_gMm1Animation05B24,
    &_gMm1Animation05E78,
    &_gMm1Animation095BC,
    &_gMm1Animation09794,
    &_gMm1Animation095BC,
    &_gMm1Animation09794,
    &_gMm1Animation03F38,
    &_gMm1Animation046C0,
    &_gMm1Animation04D24,
    &_gMm1Animation04994,
    &_gMm1Animation037DC,
    &_gMm1Animation01488,
    &_gMm1Animation09CF8,
    &_gMm1Animation09EEC,
    &_gMm1Animation0A36C,
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

/// The spawn state's tables, between the weapon's animation bank and the shell's model.
SVECTOR gGrenadeShellMuzzleOffsets[2] = {
    { 0, 0x1E0, 0x80, 0 },
    { 0, 0x220, 0x80, 0 },
};

/// Impact clip id per attachment, indexed by `ammunitionIndex - GRENADE_ROUND_FIRST`.
u16 gGrenadeShellBlastRadii[4] = { 0x1F4, 0x4B0, 0x7D0, 0 };

/// Per-ammo launch speed.
u8 gGrenadeShellSpeeds[4] = { 0x0C, 0x08, 0, 0 };

/* Each package carries its own shell model. */
#if GRENADE_VARIANT == 0
static TmdBone _gGrenadePistolModel0E324Skeleton[1] = {
#include "assets/grenade_pistol_model_0E324_skeleton.inc"
};

static u32 _gGrenadePistolModel0E324PartVerts[1] = {
#include "assets/grenade_pistol_model_0E324_partVerts.inc"
};

static SVECTOR _gGrenadePistolModel0E324Verts[8] = {
#include "assets/grenade_pistol_model_0E324_verts.inc"
};

static SVECTOR _gGrenadePistolModel0E324Normals[8] = {
#include "assets/grenade_pistol_model_0E324_normals.inc"
};

static u32 _gGrenadePistolModel0E324Stream[48] = {
#include "assets/grenade_pistol_model_0E324_stream.inc"
};

TmdSource D_grenade_pistol_8012B5A4 = {
    0,
    312,
    0,
    1,
    _gGrenadePistolModel0E324PartVerts,
    _gGrenadePistolModel0E324Verts,
    _gGrenadePistolModel0E324Normals,
    _gGrenadePistolModel0E324Skeleton,
    _gGrenadePistolModel0E324Stream,
};
#elif GRENADE_VARIANT == 1
static TmdBone _gMm1Model101C4Skeleton[1] = {
#include "assets/mm1_model_101C4_skeleton.inc"
};

static u32 _gMm1Model101C4PartVerts[1] = {
#include "assets/mm1_model_101C4_partVerts.inc"
};

static SVECTOR _gMm1Model101C4Verts[8] = {
#include "assets/mm1_model_101C4_verts.inc"
};

static SVECTOR _gMm1Model101C4Normals[8] = {
#include "assets/mm1_model_101C4_normals.inc"
};

static u32 _gMm1Model101C4Stream[48] = {
#include "assets/mm1_model_101C4_stream.inc"
};

TmdSource D_mm1_8012D444 = {
    0,
    312,
    0,
    1,
    _gMm1Model101C4PartVerts,
    _gMm1Model101C4Verts,
    _gMm1Model101C4Normals,
    _gMm1Model101C4Skeleton,
    _gMm1Model101C4Stream,
};
#endif

PACKAGE_ALIASES
