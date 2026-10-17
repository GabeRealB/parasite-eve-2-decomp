#include "weapons/grenade_pistol.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

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
#include "main/sound_ids.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/weapon.h"
#include "../../shared/effects/grenade_shell.h"

static SVECTOR   _gGrenadeShellMuzzleOffsets[2];
static const u16 _gGrenadeShellBlastRadii[4];
static u8        _gGrenadeShellInitialFlightDivisors[4];

static void _grenadeShellExit(Task* task);
static void _grenadeShellSpawn(Task* task);

/// Grenade Pistol discriminator for `GRENADE_PISTOL_VARIANT` and launch-tuning row 0.
///
/// Integer preprocessor constant; selects this package's models and animation bank.
#define GRENADE_PISTOL_VARIANT_PISTOL 0

/// MM1 discriminator for `GRENADE_PISTOL_VARIANT` and launch-tuning row 1.
///
/// Integer preprocessor constant; selects the MM1 models and animation bank.
#define GRENADE_PISTOL_VARIANT_MM1 1

/// Manifest-selected launcher build and projectile launch-tuning row.
///
/// Required integer preprocessor expression: 0 Grenade Pistol or 1 MM1.
/// Selects held/shell models and the player animation bank, derives weapon
/// index 11/12 and projectile launcher index 1/2, and occupies spawn bits 16..19
/// to select muzzle offsets and the initial flight divisor. Both packages
/// retain both tuning rows for the shared projectile implementation.
#ifndef GRENADE_PISTOL_VARIANT
#error "GRENADE_PISTOL_VARIANT is a per-package build parameter"
#elif GRENADE_PISTOL_VARIANT != GRENADE_PISTOL_VARIANT_PISTOL && GRENADE_PISTOL_VARIANT != GRENADE_PISTOL_VARIANT_MM1
#error "GRENADE_PISTOL_VARIANT must select a Grenade Pistol or MM1 build"
#endif

/// One-based weapon index for this launcher build: 11 Grenade Pistol or 12 MM1.
///
/// Parenthesized int constant expression from the validated build selector.
/// Selects the sound bank, muzzle-flash profile and projectile distance row;
/// `WEAPON_ITEM_ID` converts it to the saved weapon-load item ID.
#define GRENADE_PISTOL_WEAPON_INDEX (11 + (GRENADE_PISTOL_VARIANT))

void grenadePistolAttackState(Task* playerTask)
{
    enum {
        GRENADE_PISTOL_PHASE_PREPARE       = 0,
        GRENADE_PISTOL_PHASE_WAIT_READY    = 1,
        GRENADE_PISTOL_PHASE_FIRE          = 2,
        GRENADE_PISTOL_PHASE_RECOVER       = 3,
        GRENADE_PISTOL_PLAYER_ATTACK_STATE = 4,
        GRENADE_PISTOL_ANIMATION_READY     = 9,
        GRENADE_PISTOL_ANIMATION_FIRE      = 10,
        GRENADE_PISTOL_READY_BLEND_FRAMES  = 1,
        GRENADE_PISTOL_MOVING_BLEND_FRAMES = 8,
        GRENADE_PISTOL_FIRE_BLEND_FRAMES   = 3,
        GRENADE_PISTOL_SHOT_COOLDOWN_TICKS = 40,
    };
    GameActor* actor;
    s32        blendFrames;

    actor = playerTask->work;
    switch (actor->statePhase) {
        case GRENADE_PISTOL_PHASE_PREPARE:
            blendFrames           = GRENADE_PISTOL_READY_BLEND_FRAMES;
            actor->state          = GRENADE_PISTOL_PLAYER_ATTACK_STATE;
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            actor->statePhase    += blendFrames;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                blendFrames = GRENADE_PISTOL_MOVING_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, GRENADE_PISTOL_ANIMATION_READY, 0, blendFrames);
            actor->movementMode = 0;
            break;
        case GRENADE_PISTOL_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case GRENADE_PISTOL_PHASE_FIRE:
            actor->statePhase                  = GRENADE_PISTOL_PHASE_RECOVER;
            actor->rumblePosted                = 0;
            actor->attackControl.cooldownTicks = GRENADE_PISTOL_SHOT_COOLDOWN_TICKS;
            worldCoordPlaySound(playerTask->extra.tmd->coords,
                                ((gPlayerStatus.weaponSlotItem - GRENADE_ROUND_FIRST) << 24) | SOUND_WEAPON(GRENADE_PISTOL_WEAPON_INDEX, 4), 1);
            effectSpawn(EFFECT_GRENADE_MUZZLE_FLASH,
                        actor->equipmentTasks[1]->extra.tmd->coords, GRENADE_PISTOL_WEAPON_INDEX,
                        NULL);
            equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(GRENADE_PISTOL_WEAPON_INDEX), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
            // Both the projectile package and its launch-tuning row follow this build.
            playerActorSpawnGrenadeProjectile(playerTask, PLAYER_ACTOR_GRENADE_PLAYER, PLAYER_ACTOR_GRENADE_PISTOL + GRENADE_PISTOL_VARIANT,
                                              gPlayerStatus.weaponSlotItem | (GRENADE_PISTOL_VARIANT << PLAYER_ACTOR_GRENADE_MUZZLE_ROW_SHIFT) | (GRENADE_PISTOL_WEAPON_INDEX << PLAYER_ACTOR_GRENADE_WEAPON_SHIFT));
            playerActorPlayChildSlotsWithBlend(playerTask, GRENADE_PISTOL_ANIMATION_FIRE, 0, GRENADE_PISTOL_FIRE_BLEND_FRAMES);
            break;
        case GRENADE_PISTOL_PHASE_RECOVER:
            if (playerActorIsSlotAdvancingLinearly(playerTask, gPlayerActorAttachmentParentParts[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
}

#include "../../shared/effects/grenade_shell_spawn.inc.c"

#include "../../shared/effects/grenade_shell.inc.c"

/// Projectile state handlers for the Grenade Pistol and MM1 builds.
///
/// `Task::state` selects 0 spawn, 1 flight, 2 blast countdown or 3 teardown.
/// The dispatcher copies all four `void(Task*)` callbacks before calling one;
/// there is no terminator or bounds check. Keep the matching weapon package
/// loaded through dispatch. Spawn allocates work; later states require it live,
/// and teardown releases the task after unlinking its collision bodies.
static const TaskFuncTable4 _gGrenadePistolShellStateHandlers = {
    .funcs = {
        _grenadeShellSpawn,
        _grenadeShellFly,
        _grenadeShellBlast,
        _grenadeShellExit,
    },
};

void grenadePistolShellTask(Task* task)
{
    TaskFuncTable4 stateHandlers;

    stateHandlers = _gGrenadePistolShellStateHandlers;
    stateHandlers.funcs[task->state](task);
}

/* Each package carries its own model. */
#if GRENADE_PISTOL_VARIANT == GRENADE_PISTOL_VARIANT_PISTOL
/// Identity rest transform and self-parented root copied into each held weapon.
static const TmdBone _gGrenadePistolWeaponModelSkeleton[] __attribute__((section(".data"))) = {
#include "assets/grenade_pistol_model_00CDC_skeleton.inc"
};

/// Vertex count of the weapon's single part-local group: 36.
static const u32 _gGrenadePistolWeaponModelPartVerts[ARRAY_SIZE(_gGrenadePistolWeaponModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/grenade_pistol_model_00CDC_partVerts.inc"
};

/// Shared weapon vertices in integer coordinates local to the weapon root.
static SVECTOR _gGrenadePistolWeaponModelVerts[] = {
#include "assets/grenade_pistol_model_00CDC_verts.inc"
};

/// Shared lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gGrenadePistolWeaponModelNormals[] = {
#include "assets/grenade_pistol_model_00CDC_normals.inc"
};

/// Writable commands for two textured Gouraud triangles and 33 textured Gouraud quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// Model creation resolves each command's draw-handler slot in place.
static u32 _gGrenadePistolWeaponModelStream[] = {
#include "assets/grenade_pistol_model_00CDC_stream.inc"
};

TmdSource gGrenadePistolWeaponModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (2 * sizeof(POLY_GT3)) + (33 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gGrenadePistolWeaponModelSkeleton),
    .partVertexCounts    = _gGrenadePistolWeaponModelPartVerts,
    .verts               = _gGrenadePistolWeaponModelVerts,
    .normals             = _gGrenadePistolWeaponModelNormals,
    .skeleton            = _gGrenadePistolWeaponModelSkeleton,
    .stream              = _gGrenadePistolWeaponModelStream,
};
#elif GRENADE_PISTOL_VARIANT == GRENADE_PISTOL_VARIANT_MM1
/// Identity rest transform and self-parented root copied into each held MM1.
static const TmdBone _gMm1WeaponModelSkeleton[] __attribute__((section(".data"))) = {
#include "assets/mm1_model_00DD4_skeleton.inc"
};

/// Vertex count of the held MM1's single part-local group: 52.
static const u32 _gMm1WeaponModelPartVerts[ARRAY_SIZE(_gMm1WeaponModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/mm1_model_00DD4_partVerts.inc"
};

/// Shared held-MM1 vertices in integer coordinates local to the weapon root.
static SVECTOR _gMm1WeaponModelVerts[] = {
#include "assets/mm1_model_00DD4_verts.inc"
};

/// Shared held-MM1 lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gMm1WeaponModelNormals[] = {
#include "assets/mm1_model_00DD4_normals.inc"
};

/// Writable commands for four textured Gouraud triangles and 41 textured Gouraud quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// Model creation resolves each command's draw-handler slot in place.
static u32 _gMm1WeaponModelStream[] = {
#include "assets/mm1_model_00DD4_stream.inc"
};

TmdSource gMm1WeaponModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = (4 * sizeof(POLY_GT3)) + (41 * sizeof(POLY_GT4)),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gMm1WeaponModelSkeleton),
    .partVertexCounts    = _gMm1WeaponModelPartVerts,
    .verts               = _gMm1WeaponModelVerts,
    .normals             = _gMm1WeaponModelNormals,
    .skeleton            = _gMm1WeaponModelSkeleton,
    .stream              = _gMm1WeaponModelStream,
};
#endif

/* Each package carries its own animation bank. */
#if GRENADE_PISTOL_VARIANT == GRENADE_PISTOL_VARIANT_PISTOL
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

AnimationSet* gGrenadePistolPlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY] = {
    // Initialize the native clip slots; the writable extension starts cleared.
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
};
#elif GRENADE_PISTOL_VARIANT == GRENADE_PISTOL_VARIANT_MM1
enum {
    MM1_ANIMATION_TRANSLATION_ROTATION_BANK = 1,
    MM1_ANIMATION_PACKED_ROTATION_BANK      = 4
};

/// Idle translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1IdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_01488_bank1.inc"
};

/// Idle rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1IdleAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_01488_bank4.inc"
};

/// Constant-pose keyframes and track-local loop/stop controls for the MM1 idle clip.
///
/// Keyframe durations are whole normal-rate frames; pose offsets count u32
/// words, while control jumps select absolute record indices.
static const AnimationRecord _gMm1IdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_01488_records.inc"
};

/// Idle track starts for tracks 0..18, followed by the exporter's retained final halfword.
static const u16 _gMm1IdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_01488_indices.inc"
};

/// Player idle pose in the MM1 bank, shared by set slots 1, 21, 22 and 41.
///
/// Playback reads 19 tracks: translation/rotation for 0 and 1, packed rotation
/// for 2..18. The clip and its banks remain loaded while any slot borrows them.
static AnimationSet _gMm1IdleAnimation = {
    .records           = _gMm1IdleAnimationRecords,
    .trackStartIndices = _gMm1IdleAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1IdleAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1IdleAnimationBank4,
    },
};

/// Unarmed-walk translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1UnarmedWalkAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_01B2C_bank1.inc"
};

/// Unarmed-walk rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1UnarmedWalkAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_01B2C_bank4.inc"
};

/// Keyframes, footstep cues and track-local loop/stop controls for the MM1 unarmed walk.
///
/// Keyframe durations are whole normal-rate frames; pose offsets count u32
/// words, while control jumps select absolute record indices.
static const AnimationRecord _gMm1UnarmedWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_01B2C_records.inc"
};

/// Unarmed-walk track starts for tracks 0..18, followed by the exporter's retained final halfword.
static const u16 _gMm1UnarmedWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_01B2C_indices.inc"
};

/// Player forward walk without a held weapon, selected by MM1 bank slot 19.
///
/// Playback reads 19 tracks: translation/rotation for 0 and 1, packed rotation
/// for 2..18. The clip and its banks remain loaded while any slot borrows them.
static AnimationSet _gMm1UnarmedWalkAnimation = {
    .records           = _gMm1UnarmedWalkAnimationRecords,
    .trackStartIndices = _gMm1UnarmedWalkAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1UnarmedWalkAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1UnarmedWalkAnimationBank4,
    },
};

/// Translation/rotation poses for tracks 0 and 1 of the unreferenced MM1 clip.
///
/// Translations use integer model units; Euler angles use 4096 units per turn.
static const AnimationPackedPose _gMm1Animation02404Bank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_02404_bank1.inc"
};

/// Rotation-only poses for tracks 2..18 of the unreferenced MM1 clip.
///
/// Signed 11/10/11-bit components encode eight angle units per step.
static const AnimationPackedRotation _gMm1Animation02404Bank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_02404_bank4.inc"
};

/// Keyframes and track-local loop/stop controls of the unreferenced MM1 clip.
///
/// Durations count whole normal-rate frames; pose offsets count u32 words and
/// control jumps select absolute record indices. Track 1 carries paired cues;
/// the clip's intended player action is unproven.
static const AnimationRecord _gMm1Animation02404Records[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_02404_records.inc"
};

/// Record starts for the unreferenced MM1 clip's tracks 0..18 and one retained final halfword.
static const u16 _gMm1Animation02404Indices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_02404_indices.inc"
};

/// Unreferenced nineteen-track MM1 clip whose intended player action is unproven.
///
/// No native bank entry selects it. Tracks 0 and 1 contain translation/rotation;
/// tracks 2..18 contain packed rotation. Every track loops before a retained stop
/// control. The descriptor and its read-only banks remain part of the package.
static AnimationSet _gMm1Animation02404 = {
    .records           = _gMm1Animation02404Records,
    .trackStartIndices = _gMm1Animation02404Indices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1Animation02404Bank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1Animation02404Bank4,
    },
};

/// Negative-turn translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1NegativeTurnAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_02C64_bank1.inc"
};

/// Negative-turn rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1NegativeTurnAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_02C64_bank4.inc"
};

/// Keyframes, footstep cues and track-local loop/stop controls for the MM1 negative turn.
///
/// Keyframe durations are whole normal-rate frames; pose offsets count u32
/// words, while control jumps select absolute record indices.
static const AnimationRecord _gMm1NegativeTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_02C64_records.inc"
};

/// Negative-turn track starts for tracks 0..18, followed by the exporter's retained final halfword.
static const u16 _gMm1NegativeTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_02C64_indices.inc"
};

/// Player turn toward decreasing yaw, selected by MM1 bank slot 5.
///
/// Playback reads 19 tracks: translation/rotation for 0 and 1, packed rotation
/// for 2..18. The clip and its banks remain loaded while any slot borrows them.
static AnimationSet _gMm1NegativeTurnAnimation = {
    .records           = _gMm1NegativeTurnAnimationRecords,
    .trackStartIndices = _gMm1NegativeTurnAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1NegativeTurnAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1NegativeTurnAnimationBank4,
    },
};

/// Positive-turn translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1PositiveTurnAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_034C8_bank1.inc"
};

/// Positive-turn rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1PositiveTurnAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_034C8_bank4.inc"
};

/// Keyframes, footstep cues and track-local loop/stop controls for the MM1 positive turn.
///
/// Keyframe durations are whole normal-rate frames; pose offsets count u32
/// words, while control jumps select absolute record indices.
static const AnimationRecord _gMm1PositiveTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_034C8_records.inc"
};

/// Positive-turn track starts for tracks 0..18, followed by the exporter's retained final halfword.
static const u16 _gMm1PositiveTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_034C8_indices.inc"
};

/// Player turn toward increasing yaw, selected by MM1 bank slot 6.
///
/// Scripted turns also select this clip for a zero yaw delta. Playback reads
/// 19 tracks: translation/rotation for 0 and 1, packed rotation for 2..18.
/// The clip and its banks remain loaded while any slot borrows them.
static AnimationSet _gMm1PositiveTurnAnimation = {
    .records           = _gMm1PositiveTurnAnimationRecords,
    .trackStartIndices = _gMm1PositiveTurnAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1PositiveTurnAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1PositiveTurnAnimationBank4,
    },
};

/// Item-use translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1ItemUseAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_037DC_bank1.inc"
};

/// Item-use rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1ItemUseAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_037DC_bank4.inc"
};

/// Keyframes and track stops for the MM1 item-use presentation.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words.
static const AnimationRecord _gMm1ItemUseAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_037DC_records.inc"
};

/// Item-use track starts for tracks 0..18, followed by the exporter's retained final halfword.
static const u16 _gMm1ItemUseAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_037DC_indices.inc"
};

/// Player item-use presentation, selected by MM1 bank slot 40.
///
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1ItemUseAnimation = {
    .records           = _gMm1ItemUseAnimationRecords,
    .trackStartIndices = _gMm1ItemUseAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1ItemUseAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1ItemUseAnimationBank4,
    },
};

/// Stair-ascent translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1StairAscentAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_03F38_bank1.inc"
};

/// Stair-ascent rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1StairAscentAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_03F38_bank4.inc"
};

/// Keyframes, footstep cues and track-local loop/stop controls for MM1 stair ascent.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words,
/// while control jumps select absolute record indices.
static const AnimationRecord _gMm1StairAscentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_03F38_records.inc"
};

/// Stair-ascent track starts for tracks 0..18, followed by the exporter's retained final halfword.
static const u16 _gMm1StairAscentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_03F38_indices.inc"
};

/// Looping player stair ascent, selected by MM1 bank slot 36.
///
/// Track-1 footstep cues time the strides. Tracks 0 and 1 carry
/// translation/rotation; 2..18 carry packed rotation. Keep the package loaded
/// while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1StairAscentAnimation = {
    .records           = _gMm1StairAscentAnimationRecords,
    .trackStartIndices = _gMm1StairAscentAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1StairAscentAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1StairAscentAnimationBank4,
    },
};

/// Stair-descent translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1StairDescentAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_046C0_bank1.inc"
};

/// Stair-descent rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1StairDescentAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_046C0_bank4.inc"
};

/// Keyframes, footstep cues and track-local loop/stop controls for MM1 stair descent.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words,
/// while control jumps select absolute record indices.
static const AnimationRecord _gMm1StairDescentAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_046C0_records.inc"
};

/// Stair-descent track starts for tracks 0..18, followed by the exporter's retained final halfword.
static const u16 _gMm1StairDescentAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_046C0_indices.inc"
};

/// Looping player stair descent, selected by MM1 bank slot 37.
///
/// Track-1 footstep cues time the strides. Tracks 0 and 1 carry
/// translation/rotation; 2..18 carry packed rotation. Keep the package loaded
/// while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1StairDescentAnimation = {
    .records           = _gMm1StairDescentAnimationRecords,
    .trackStartIndices = _gMm1StairDescentAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1StairDescentAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1StairDescentAnimationBank4,
    },
};

/// Odd-count stair-finish translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1StairAscentFinishOddAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_04994_bank1.inc"
};

/// Odd-count stair-finish rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1StairAscentFinishOddAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_04994_bank4.inc"
};

/// Keyframes, a footstep cue and track stops for the MM1 odd-count stair-ascent finish.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words.
static const AnimationRecord _gMm1StairAscentFinishOddAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_04994_records.inc"
};

/// Odd-count stair-finish track starts for tracks 0..18, followed by the exporter's retained final halfword.
static const u16 _gMm1StairAscentFinishOddAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_04994_indices.inc"
};

/// Player stair-ascent finish for an odd initial step count, selected by MM1 bank slot 39.
///
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1StairAscentFinishOddAnimation = {
    .records           = _gMm1StairAscentFinishOddAnimationRecords,
    .trackStartIndices = _gMm1StairAscentFinishOddAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1StairAscentFinishOddAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1StairAscentFinishOddAnimationBank4,
    },
};

/// Even-count stair-finish translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1StairAscentFinishEvenAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_04D24_bank1.inc"
};

/// Even-count stair-finish rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1StairAscentFinishEvenAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_04D24_bank4.inc"
};

/// Keyframes, a footstep cue and track stops for the MM1 even-count stair-ascent finish.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words.
static const AnimationRecord _gMm1StairAscentFinishEvenAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_04D24_records.inc"
};

/// Even-count stair-finish track starts for tracks 0..18, followed by the exporter's retained final halfword.
static const u16 _gMm1StairAscentFinishEvenAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_04D24_indices.inc"
};

/// Player stair-ascent finish for an even initial step count, selected by MM1 bank slot 38.
///
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1StairAscentFinishEvenAnimation = {
    .records           = _gMm1StairAscentFinishEvenAnimationRecords,
    .trackStartIndices = _gMm1StairAscentFinishEvenAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1StairAscentFinishEvenAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1StairAscentFinishEvenAnimationBank4,
    },
};

/// Family-0 release translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1ParasiteEnergyReleaseFamily0AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_051AC_bank1.inc"
};

/// Family-0 release rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1ParasiteEnergyReleaseFamily0AnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_051AC_bank4.inc"
};

/// Keyframes and track stops for the MM1 Parasite Energy family-0 release.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words.
static const AnimationRecord _gMm1ParasiteEnergyReleaseFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_051AC_records.inc"
};

/// Family-0 release track starts for tracks 0..18, followed by the exporter's retained final halfword.
static const u16 _gMm1ParasiteEnergyReleaseFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_051AC_indices.inc"
};

/// Player Parasite Energy family-0 release, selected by MM1 bank slot 26.
///
/// Begins the family-0 release/charge/cast sequence. Tracks 0 and 1 carry
/// translation/rotation; 2..18 carry packed rotation. Keep the package loaded
/// while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1ParasiteEnergyReleaseFamily0Animation = {
    .records           = _gMm1ParasiteEnergyReleaseFamily0AnimationRecords,
    .trackStartIndices = _gMm1ParasiteEnergyReleaseFamily0AnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1ParasiteEnergyReleaseFamily0AnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1ParasiteEnergyReleaseFamily0AnimationBank4,
    },
};

/// Family-0 charge translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1ParasiteEnergyChargeFamily0AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_053A8_bank1.inc"
};

/// Family-0 charge rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1ParasiteEnergyChargeFamily0AnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_053A8_bank4.inc"
};

/// Keyframes, repeat jumps and retained track stops for the MM1 Parasite Energy family-0 charge.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words and jump targets count records.
static const AnimationRecord _gMm1ParasiteEnergyChargeFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_053A8_records.inc"
};

/// Family-0 charge track starts for tracks 0..18, followed by the exporter's retained final halfword.
static const u16 _gMm1ParasiteEnergyChargeFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_053A8_indices.inc"
};

/// Player Parasite Energy family-0 charge loop, selected by MM1 bank slot 27.
///
/// Repeats two one-frame keyframes per track while the attachment countdown runs.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1ParasiteEnergyChargeFamily0Animation = {
    .records           = _gMm1ParasiteEnergyChargeFamily0AnimationRecords,
    .trackStartIndices = _gMm1ParasiteEnergyChargeFamily0AnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1ParasiteEnergyChargeFamily0AnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1ParasiteEnergyChargeFamily0AnimationBank4,
    },
};

/// Family-0 cast translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1ParasiteEnergyCastFamily0AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_05680_bank1.inc"
};

/// Family-0 cast rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1ParasiteEnergyCastFamily0AnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_05680_bank4.inc"
};

/// Keyframes and track stops for the MM1 Parasite Energy family-0 cast.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words.
static const AnimationRecord _gMm1ParasiteEnergyCastFamily0AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_05680_records.inc"
};

/// Family-0 cast track starts for tracks 0..18, followed by the exporter's retained final halfword.
static const u16 _gMm1ParasiteEnergyCastFamily0AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_05680_indices.inc"
};

/// Player Parasite Energy family-0 cast, selected by MM1 bank slot 28.
///
/// Plays once when the attachment countdown expires, then holds its final poses.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1ParasiteEnergyCastFamily0Animation = {
    .records           = _gMm1ParasiteEnergyCastFamily0AnimationRecords,
    .trackStartIndices = _gMm1ParasiteEnergyCastFamily0AnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1ParasiteEnergyCastFamily0AnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1ParasiteEnergyCastFamily0AnimationBank4,
    },
};

/// Family-1 release translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1ParasiteEnergyReleaseFamily1AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_05924_bank1.inc"
};

/// Family-1 release rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1ParasiteEnergyReleaseFamily1AnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_05924_bank4.inc"
};

/// Keyframes and track stops for the MM1 Parasite Energy family-1 release.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words.
static const AnimationRecord _gMm1ParasiteEnergyReleaseFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_05924_records.inc"
};

/// Family-1 release track starts for tracks 0..18, followed by the exporter's retained final halfword.
static const u16 _gMm1ParasiteEnergyReleaseFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_05924_indices.inc"
};

/// Player Parasite Energy family-1 release, selected by MM1 bank slot 29.
///
/// Begins the family-1 release/charge/cast sequence while the attachment countdown starts.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1ParasiteEnergyReleaseFamily1Animation = {
    .records           = _gMm1ParasiteEnergyReleaseFamily1AnimationRecords,
    .trackStartIndices = _gMm1ParasiteEnergyReleaseFamily1AnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1ParasiteEnergyReleaseFamily1AnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1ParasiteEnergyReleaseFamily1AnimationBank4,
    },
};

/// Family-1 charge translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1ParasiteEnergyChargeFamily1AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_05B24_bank1.inc"
};

/// Family-1 charge rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1ParasiteEnergyChargeFamily1AnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_05B24_bank4.inc"
};

/// Keyframes, repeat jumps and retained track stops for the MM1 Parasite Energy family-1 charge.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words and jump targets count records.
static const AnimationRecord _gMm1ParasiteEnergyChargeFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_05B24_records.inc"
};

/// Family-1 charge track starts for tracks 0..18, followed by the exporter's retained final halfword.
static const u16 _gMm1ParasiteEnergyChargeFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_05B24_indices.inc"
};

/// Player Parasite Energy family-1 charge loop, selected by MM1 bank slot 30.
///
/// Repeats two one-frame keyframes per track while the attachment countdown runs.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1ParasiteEnergyChargeFamily1Animation = {
    .records           = _gMm1ParasiteEnergyChargeFamily1AnimationRecords,
    .trackStartIndices = _gMm1ParasiteEnergyChargeFamily1AnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1ParasiteEnergyChargeFamily1AnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1ParasiteEnergyChargeFamily1AnimationBank4,
    },
};

/// Family-1 cast translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1ParasiteEnergyCastFamily1AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_05E78_bank1.inc"
};

/// Family-1 cast rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1ParasiteEnergyCastFamily1AnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_05E78_bank4.inc"
};

/// Keyframes and track stops for the MM1 Parasite Energy family-1 cast.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words.
static const AnimationRecord _gMm1ParasiteEnergyCastFamily1AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_05E78_records.inc"
};

/// Family-1 cast track starts for tracks 0..18, followed by the exporter's retained final halfword.
static const u16 _gMm1ParasiteEnergyCastFamily1AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_05E78_indices.inc"
};

/// Player Parasite Energy family-1 cast, selected by MM1 bank slot 31.
///
/// Plays once when the attachment countdown expires, then holds its final poses.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1ParasiteEnergyCastFamily1Animation = {
    .records           = _gMm1ParasiteEnergyCastFamily1AnimationRecords,
    .trackStartIndices = _gMm1ParasiteEnergyCastFamily1AnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1ParasiteEnergyCastFamily1AnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1ParasiteEnergyCastFamily1AnimationBank4,
    },
};

/// Part-4 hit-reaction translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1Part4HitReactionAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0612C_bank1.inc"
};

/// Part-4 hit-reaction rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1Part4HitReactionAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0612C_bank4.inc"
};

/// Keyframes and track stops for the MM1 player's part-4 hit reaction.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words.
static const AnimationRecord _gMm1Part4HitReactionAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0612C_records.inc"
};

/// Part-4 hit-reaction track starts for tracks 0..18, followed by the exporter's retained final halfword.
static const u16 _gMm1Part4HitReactionAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0612C_indices.inc"
};

/// Player hit reaction for the collision body on model part 4, selected by MM1 bank slot 16.
///
/// Blended into active child slots over three normal-rate frames after a part-4 hit.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1Part4HitReactionAnimation = {
    .records           = _gMm1Part4HitReactionAnimationRecords,
    .trackStartIndices = _gMm1Part4HitReactionAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1Part4HitReactionAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1Part4HitReactionAnimationBank4,
    },
};

/// Other-body hit-reaction translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1OtherBodyHitReactionAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0644C_bank1.inc"
};

/// Other-body hit-reaction rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1OtherBodyHitReactionAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0644C_bank4.inc"
};

/// Keyframes and track stops for the MM1 other-body hit-reaction animation.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words.
static const AnimationRecord _gMm1OtherBodyHitReactionAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0644C_records.inc"
};

/// Other-body hit-reaction track starts for tracks 0..18, followed by the retained final export halfword.
static const u16 _gMm1OtherBodyHitReactionAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0644C_indices.inc"
};

/// Player hit reaction for contacts outside the model-part-4 collision body, selected by MM1 bank slot 17.
///
/// Blends child tracks over three normal-rate frames, then holds their final poses.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1OtherBodyHitReactionAnimation = {
    .records           = _gMm1OtherBodyHitReactionAnimationRecords,
    .trackStartIndices = _gMm1OtherBodyHitReactionAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1OtherBodyHitReactionAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1OtherBodyHitReactionAnimationBank4,
    },
};

/// Death translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1DeathAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_06C10_bank1.inc"
};

/// Death rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1DeathAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_06C10_bank4.inc"
};

/// Keyframes and track stops for the MM1 death animation.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words.
static const AnimationRecord _gMm1DeathAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_06C10_records.inc"
};

/// Death track starts for tracks 0..18, followed by the retained final export halfword.
static const u16 _gMm1DeathAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_06C10_indices.inc"
};

/// Player death animation, selected by MM1 bank slot 18.
///
/// Plays at zero HP with movement and turning disabled, then holds its final poses.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1DeathAnimation = {
    .records           = _gMm1DeathAnimationRecords,
    .trackStartIndices = _gMm1DeathAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1DeathAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1DeathAnimationBank4,
    },
};

/// Healthy-idle translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1HealthyIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_07EA8_bank1.inc"
};

/// Healthy-idle rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1HealthyIdleAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_07EA8_bank4.inc"
};

/// Keyframes, repeat jumps and retained track stops for the MM1 healthy-idle loop.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words and jump targets count records.
static const AnimationRecord _gMm1HealthyIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_07EA8_records.inc"
};

/// Healthy-idle track starts for tracks 0..18, followed by the retained final export halfword.
static const u16 _gMm1HealthyIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_07EA8_indices.inc"
};

/// Player idle loop above half maximum HP, selected by MM1 bank slot 23.
///
/// Starts after 300 direction-free idle ticks and blends in over five normal-rate frames.
/// The HP threshold rounds down to a whole point.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1HealthyIdleAnimation = {
    .records           = _gMm1HealthyIdleAnimationRecords,
    .trackStartIndices = _gMm1HealthyIdleAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1HealthyIdleAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1HealthyIdleAnimationBank4,
    },
};

/// Critical-idle translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1CriticalIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_08A20_bank1.inc"
};

/// Critical-idle rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1CriticalIdleAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_08A20_bank4.inc"
};

/// Keyframes, repeat jumps and retained track stops for the MM1 critical-idle loop.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words and jump targets count records.
static const AnimationRecord _gMm1CriticalIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_08A20_records.inc"
};

/// Critical-idle track starts for tracks 0..18, followed by the retained final export halfword.
static const u16 _gMm1CriticalIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_08A20_indices.inc"
};

/// Player idle loop at or below quarter maximum HP, selected by MM1 bank slot 25.
///
/// Starts after 300 direction-free idle ticks with a five-frame blend.
/// Paralysis also selects this clip with a six-frame blend for all child tracks.
/// The HP threshold rounds down to a whole point.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1CriticalIdleAnimation = {
    .records           = _gMm1CriticalIdleAnimationRecords,
    .trackStartIndices = _gMm1CriticalIdleAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1CriticalIdleAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1CriticalIdleAnimationBank4,
    },
};

/// Injured-idle translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1InjuredIdleAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0913C_bank1.inc"
};

/// Injured-idle rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1InjuredIdleAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0913C_bank4.inc"
};

/// Keyframes, repeat jumps and retained track stops for the MM1 injured-idle loop.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words and jump targets count records.
static const AnimationRecord _gMm1InjuredIdleAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0913C_records.inc"
};

/// Injured-idle track starts for tracks 0..18, followed by the retained final export halfword.
static const u16 _gMm1InjuredIdleAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0913C_indices.inc"
};

/// Player idle loop above quarter and at or below half maximum HP, selected by MM1 bank slot 24.
///
/// Starts after 300 direction-free idle ticks and blends in over five normal-rate frames.
/// Both HP thresholds round down to whole points.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1InjuredIdleAnimation = {
    .records           = _gMm1InjuredIdleAnimationRecords,
    .trackStartIndices = _gMm1InjuredIdleAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1InjuredIdleAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1InjuredIdleAnimationBank4,
    },
};

/// Scripted-presentation entry translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1ScriptedPresentationEntryAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_095BC_bank1.inc"
};

/// Scripted-presentation entry rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1ScriptedPresentationEntryAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_095BC_bank4.inc"
};

/// Keyframes and track stops for the MM1 scripted-presentation entry animation.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words.
static const AnimationRecord _gMm1ScriptedPresentationEntryAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_095BC_records.inc"
};

/// Scripted-presentation entry track starts for tracks 0..18, followed by the retained final export halfword.
static const u16 _gMm1ScriptedPresentationEntryAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_095BC_indices.inc"
};

/// Player scripted-presentation entry animation, selected by MM1 bank slots 32 and 34.
///
/// A zero presentation selector plays slot 32 before a CAP weapon event or conversation.
/// Plays once and holds its final poses; the specific visible gesture is unproven.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1ScriptedPresentationEntryAnimation = {
    .records           = _gMm1ScriptedPresentationEntryAnimationRecords,
    .trackStartIndices = _gMm1ScriptedPresentationEntryAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1ScriptedPresentationEntryAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1ScriptedPresentationEntryAnimationBank4,
    },
};

/// Scripted-presentation exit translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1ScriptedPresentationExitAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_09794_bank1.inc"
};

/// Scripted-presentation exit rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1ScriptedPresentationExitAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_09794_bank4.inc"
};

/// Keyframes and track stops for the MM1 scripted-presentation exit animation.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words.
static const AnimationRecord _gMm1ScriptedPresentationExitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_09794_records.inc"
};

/// Scripted-presentation exit track starts for tracks 0..18, followed by the retained final export halfword.
static const u16 _gMm1ScriptedPresentationExitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_09794_indices.inc"
};

/// Player scripted-presentation exit animation, selected by MM1 bank slots 33 and 35.
///
/// A nonzero presentation selector plays slot 33 after a CAP weapon event or conversation.
/// Plays once and holds its final poses.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1ScriptedPresentationExitAnimation = {
    .records           = _gMm1ScriptedPresentationExitAnimationRecords,
    .trackStartIndices = _gMm1ScriptedPresentationExitAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1ScriptedPresentationExitAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1ScriptedPresentationExitAnimationBank4,
    },
};

/// Parasite Energy family-2 release translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1ParasiteEnergyReleaseFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_09CF8_bank1.inc"
};

/// Parasite Energy family-2 release rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1ParasiteEnergyReleaseFamily2AnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_09CF8_bank4.inc"
};

/// Keyframes and track stops for the MM1 Parasite Energy family-2 release animation.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words.
static const AnimationRecord _gMm1ParasiteEnergyReleaseFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_09CF8_records.inc"
};

/// Parasite Energy family-2 release track starts for tracks 0..18, followed by the retained final export halfword.
static const u16 _gMm1ParasiteEnergyReleaseFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_09CF8_indices.inc"
};

/// Player Parasite Energy release animation for family 2, selected by MM1 bank slot 42.
///
/// Spell IDs below 601 with decimal tens digit 3 select this family.
/// Blends in over six normal-rate frames, then plays once before the charge loop.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1ParasiteEnergyReleaseFamily2Animation = {
    .records           = _gMm1ParasiteEnergyReleaseFamily2AnimationRecords,
    .trackStartIndices = _gMm1ParasiteEnergyReleaseFamily2AnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1ParasiteEnergyReleaseFamily2AnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1ParasiteEnergyReleaseFamily2AnimationBank4,
    },
};

/// Parasite Energy family-2 charge translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1ParasiteEnergyChargeFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_09EEC_bank1.inc"
};

/// Parasite Energy family-2 charge rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1ParasiteEnergyChargeFamily2AnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_09EEC_bank4.inc"
};

/// Keyframes, repeat jumps and retained track stops for the MM1 Parasite Energy family-2 charge loop.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words and jump targets count records.
static const AnimationRecord _gMm1ParasiteEnergyChargeFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_09EEC_records.inc"
};

/// Parasite Energy family-2 charge track starts for tracks 0..18, followed by the retained final export halfword.
static const u16 _gMm1ParasiteEnergyChargeFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_09EEC_indices.inc"
};

/// Player Parasite Energy charge loop for family 2, selected by MM1 bank slot 43.
///
/// Follows the family-2 release and repeats until the attachment countdown reaches zero.
/// The cast animation then replaces it without a transition blend.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1ParasiteEnergyChargeFamily2Animation = {
    .records           = _gMm1ParasiteEnergyChargeFamily2AnimationRecords,
    .trackStartIndices = _gMm1ParasiteEnergyChargeFamily2AnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1ParasiteEnergyChargeFamily2AnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1ParasiteEnergyChargeFamily2AnimationBank4,
    },
};

/// Parasite Energy family-2 cast translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1ParasiteEnergyCastFamily2AnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0A36C_bank1.inc"
};

/// Parasite Energy family-2 cast rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1ParasiteEnergyCastFamily2AnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0A36C_bank4.inc"
};

/// Keyframes and track stops for the MM1 Parasite Energy family-2 cast animation.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words.
static const AnimationRecord _gMm1ParasiteEnergyCastFamily2AnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0A36C_records.inc"
};

/// Parasite Energy family-2 cast track starts for tracks 0..18, followed by the retained final export halfword.
static const u16 _gMm1ParasiteEnergyCastFamily2AnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0A36C_indices.inc"
};

/// Player Parasite Energy cast animation for family 2, selected by MM1 bank slot 44.
///
/// Replaces the charge loop without a transition blend when the attachment countdown reaches zero.
/// Plays once; completion lets the player resume locomotion or aiming.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1ParasiteEnergyCastFamily2Animation = {
    .records           = _gMm1ParasiteEnergyCastFamily2AnimationRecords,
    .trackStartIndices = _gMm1ParasiteEnergyCastFamily2AnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1ParasiteEnergyCastFamily2AnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1ParasiteEnergyCastFamily2AnimationBank4,
    },
};

/// Aim-hold translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1AimHoldAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0A544_bank1.inc"
};

/// Aim-hold rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1AimHoldAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0A544_bank4.inc"
};

/// Keyframe pairs, repeat jumps and retained track stops for the MM1 aim hold.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words and jump targets count records.
static const AnimationRecord _gMm1AimHoldAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0A544_records.inc"
};

/// Aim-hold track starts for tracks 0..18, followed by the retained final export halfword.
static const u16 _gMm1AimHoldAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0A544_indices.inc"
};

/// Player stationary aim hold, selected by MM1 bank slot 9.
///
/// Each track repeats one pose while aim locomotion has no movement or turn input.
/// Aim entry blends child tracks into this set over five normal-rate frames.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1AimHoldAnimation = {
    .records           = _gMm1AimHoldAnimationRecords,
    .trackStartIndices = _gMm1AimHoldAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1AimHoldAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1AimHoldAnimationBank4,
    },
};

/// Aim backward-or-turn translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1AimBackwardOrTurnAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0ACFC_bank1.inc"
};

/// Aim backward-or-turn rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1AimBackwardOrTurnAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0ACFC_bank4.inc"
};

/// Keyframes, footstep cues, repeat jumps and retained track stops for the MM1 aiming step cycle.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words and jump targets count records.
/// Track 1 carries `ANIMATION_RECORD_CUE_2` followed by `ANIMATION_RECORD_CUE_1`.
static const AnimationRecord _gMm1AimBackwardOrTurnAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0ACFC_records.inc"
};

/// Aim backward-or-turn track starts for tracks 0..18, followed by the retained final export halfword.
static const u16 _gMm1AimBackwardOrTurnAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0ACFC_indices.inc"
};

/// Player aiming step cycle for backward movement or turning, selected by MM1 bank slot 13.
///
/// Loops while stepping backward or turning without forward movement.
/// Track 1 carries paired footstep cues; root displacement and turning are applied by the player controller.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the package loaded while slots or cached record pointers borrow the clip.
static AnimationSet _gMm1AimBackwardOrTurnAnimation = {
    .records           = _gMm1AimBackwardOrTurnAnimationRecords,
    .trackStartIndices = _gMm1AimBackwardOrTurnAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1AimBackwardOrTurnAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1AimBackwardOrTurnAnimationBank4,
    },
};

/// Translations in model units and Euler rotations in 1/4096 turns for aim-forward tracks 0 and 1.
static const AnimationPackedPose _gMm1AimForwardAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0B7A4_bank1.inc"
};

/// Aim-forward rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1AimForwardAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0B7A4_bank4.inc"
};

/// Keyframes, footstep cues, repeat jumps and retained track stops for forward movement while aiming with the MM1.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words and jump targets count records.
/// Track 1 carries `ANIMATION_RECORD_CUE_2` followed by `ANIMATION_RECORD_CUE_1`.
static const AnimationRecord _gMm1AimForwardAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0B7A4_records.inc"
};

/// Aim-forward track starts for tracks 0..18, followed by the retained final export halfword.
static const u16 _gMm1AimForwardAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0B7A4_indices.inc"
};

/// Player forward movement cycle while aiming, selected by MM1 bank slot 12.
///
/// Loops while moving forward in aim locomotion; track 1 carries paired footstep cues.
/// The player controller applies root displacement and turning separately.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the MM1 package loaded while slots, borrowed tables or cached records refer to the clip.
static AnimationSet _gMm1AimForwardAnimation = {
    .records           = _gMm1AimForwardAnimationRecords,
    .trackStartIndices = _gMm1AimForwardAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1AimForwardAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1AimForwardAnimationBank4,
    },
};

/// Translations in model units and Euler rotations in 1/4096 turns for reload tracks 0 and 1.
static const AnimationPackedPose _gMm1ReloadAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0CE20_bank1.inc"
};

/// Reload rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1ReloadAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0CE20_bank4.inc"
};

/// Keyframes, six loading cues and track stops for the MM1 reload.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words.
/// Track 1 sets both `ANIMATION_RECORD_CUE_MASK` bits on each loading cue.
static const AnimationRecord _gMm1ReloadAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0CE20_records.inc"
};

/// Reload track starts for tracks 0..18, followed by the retained final export halfword.
static const u16 _gMm1ReloadAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0CE20_indices.inc"
};

/// Player MM1 reload animation shared by bank slots 14, 15 and 20.
///
/// Normal reloads select slot 14 plus the load selector; battle-end reloads select slot 20.
/// Plays once with a three-frame entry blend. Six slot-1 cues time ammunition loading and reload sounds.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the MM1 package loaded while slots, borrowed tables or cached records refer to the clip.
static AnimationSet _gMm1ReloadAnimation = {
    .records           = _gMm1ReloadAnimationRecords,
    .trackStartIndices = _gMm1ReloadAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1ReloadAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1ReloadAnimationBank4,
    },
};

/// Translations in model units and Euler rotations in 1/4096 turns for firing tracks 0 and 1.
static const AnimationPackedPose _gMm1FireAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0D4BC_bank1.inc"
};

/// Firing rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1FireAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0D4BC_bank4.inc"
};

/// Keyframes and track stops for the MM1 firing animation.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words.
/// The shot is triggered by the attack state, without a record cue.
static const AnimationRecord _gMm1FireAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0D4BC_records.inc"
};

/// Firing track starts for tracks 0..18, followed by the retained final export halfword.
static const u16 _gMm1FireAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0D4BC_indices.inc"
};

/// Player MM1 firing animation selected by bank slot 10, also stored in slot 11.
///
/// Starts with a three-frame blend when the attack consumes a round and spawns its projectile.
/// Plays once; completion lets the attack state resume aiming.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the MM1 package loaded while slots, borrowed tables or cached records refer to the clip.
static AnimationSet _gMm1FireAnimation = {
    .records           = _gMm1FireAnimationRecords,
    .trackStartIndices = _gMm1FireAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1FireAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1FireAnimationBank4,
    },
};

/// Translations in model units and Euler rotations in 1/4096 turns for aim-entry tracks 0 and 1.
static const AnimationPackedPose _gMm1AimEntryAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0DF5C_bank1.inc"
};

/// Aim-entry rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1AimEntryAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0DF5C_bank4.inc"
};

/// Keyframes and track stops for the MM1 aim-entry transition.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words.
static const AnimationRecord _gMm1AimEntryAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0DF5C_records.inc"
};

/// Aim-entry track starts for tracks 0..18, followed by the retained final export halfword.
static const u16 _gMm1AimEntryAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0DF5C_indices.inc"
};

/// Player transition into MM1 aiming, selected by bank slot 7.
///
/// Plays once before the controller blends into the aim hold or aim locomotion.
/// Also borrowed as the player's recovery clip after a Desert Chaser grab.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the MM1 package loaded while slots, borrowed tables or cached records refer to the clip.
static AnimationSet _gMm1AimEntryAnimation = {
    .records           = _gMm1AimEntryAnimationRecords,
    .trackStartIndices = _gMm1AimEntryAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1AimEntryAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1AimEntryAnimationBank4,
    },
};

/// Translations in model units and Euler rotations in 1/4096 turns for aim-exit tracks 0 and 1.
static const AnimationPackedPose _gMm1AimExitAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0E570_bank1.inc"
};

/// Aim-exit rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1AimExitAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0E570_bank4.inc"
};

/// Keyframes and track stops for the MM1 aim-exit transition.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words.
static const AnimationRecord _gMm1AimExitAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0E570_records.inc"
};

/// Aim-exit track starts for tracks 0..18, followed by the retained final export halfword.
static const u16 _gMm1AimExitAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0E570_indices.inc"
};

/// Player transition out of MM1 aiming, selected by bank slot 8.
///
/// Starts with a six-frame blend on normal aim exit and on scripted attack completion.
/// Plays once before returning to ordinary locomotion.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the MM1 package loaded while slots, borrowed tables or cached records refer to the clip.
static AnimationSet _gMm1AimExitAnimation = {
    .records           = _gMm1AimExitAnimationRecords,
    .trackStartIndices = _gMm1AimExitAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1AimExitAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1AimExitAnimationBank4,
    },
};

/// Translations in model units and Euler rotations in 1/4096 turns for walk tracks 0 and 1.
static const AnimationPackedPose _gMm1WalkAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0EE28_bank1.inc"
};

/// Walk rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1WalkAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0EE28_bank4.inc"
};

/// Keyframes, footstep cues, repeat jumps and retained track stops for walking with the MM1.
///
/// Durations are whole normal-rate frames; pose offsets count u32 words and jump targets count records.
/// Track 1 carries `ANIMATION_RECORD_CUE_1` followed by `ANIMATION_RECORD_CUE_2`.
static const AnimationRecord _gMm1WalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0EE28_records.inc"
};

/// Walk track starts for tracks 0..18, followed by the retained final export halfword.
static const u16 _gMm1WalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0EE28_indices.inc"
};

/// Player forward walk with the weapon model present, selected by MM1 bank slot 2.
///
/// Loops during ordinary forward walking; track 1 carries paired footstep cues.
/// The player controller applies root displacement and turning separately.
/// Tracks 0 and 1 carry translation/rotation; 2..18 carry packed rotation.
/// Keep the MM1 package loaded while slots, borrowed tables or cached records refer to the clip.
static AnimationSet _gMm1WalkAnimation = {
    .records           = _gMm1WalkAnimationRecords,
    .trackStartIndices = _gMm1WalkAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1WalkAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1WalkAnimationBank4,
    },
};

/// Backward-walk translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1BackwardWalkAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0F5C8_bank1.inc"
};

/// Backward-walk rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1BackwardWalkAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0F5C8_bank4.inc"
};

/// Keyframes, footstep cues and track-local loop/stop controls for walking backward with the MM1.
///
/// Durations count whole normal-rate frames; pose offsets count u32 words and
/// control jumps select absolute record indices. Track 1 carries cue 1 then cue 2.
static const AnimationRecord _gMm1BackwardWalkAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0F5C8_records.inc"
};

/// Backward-walk record starts for tracks 0..18, followed by one retained final halfword.
static const u16 _gMm1BackwardWalkAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0F5C8_indices.inc"
};

/// Player backward walk with the MM1, selected by native bank slot 3.
///
/// Loops during backward locomotion; the controller applies root movement
/// separately. Tracks 0 and 1 carry translation/rotation, and 2..18 carry packed
/// rotation. Keep the MM1 package loaded while playback borrows this clip.
static AnimationSet _gMm1BackwardWalkAnimation = {
    .records           = _gMm1BackwardWalkAnimationRecords,
    .trackStartIndices = _gMm1BackwardWalkAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1BackwardWalkAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1BackwardWalkAnimationBank4,
    },
};

/// Run translations in model units and Euler rotations in 1/4096 turns for tracks 0 and 1.
static const AnimationPackedPose _gMm1RunAnimationBank1[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0FF9C_bank1.inc"
};

/// Run rotations for tracks 2..18, packed in signed 11/10/11-bit components at eight angle units per step.
static const AnimationPackedRotation _gMm1RunAnimationBank4[] __attribute__((section(".data"), aligned(4))) = {
#include "assets/mm1_animation_0FF9C_bank4.inc"
};

/// Keyframes, footstep cues and track-local loop/stop controls for running with the MM1.
///
/// Durations count whole normal-rate frames; pose offsets count u32 words and
/// control jumps select absolute record indices. Track 1 carries cue 2 then cue 1.
static const AnimationRecord _gMm1RunAnimationRecords[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0FF9C_records.inc"
};

/// Run record starts for tracks 0..18, followed by one retained final halfword.
static const u16 _gMm1RunAnimationIndices[] __attribute__((section(".data"))) = {
#include "assets/mm1_animation_0FF9C_indices.inc"
};

/// Player run with the MM1, selected by native bank slot 4.
///
/// Loops during forward running and the default scripted run-to action. The
/// controller applies root movement separately. Tracks 0 and 1 carry
/// translation/rotation, and 2..18 carry packed rotation. Keep the MM1 package
/// loaded while playback borrows this clip.
static AnimationSet _gMm1RunAnimation = {
    .records           = _gMm1RunAnimationRecords,
    .trackStartIndices = _gMm1RunAnimationIndices,
    .poseBanks         = {
        [MM1_ANIMATION_TRANSLATION_ROTATION_BANK] = _gMm1RunAnimationBank1,
        [MM1_ANIMATION_PACKED_ROTATION_BANK]      = _gMm1RunAnimationBank4,
    },
};

AnimationSet* gMm1PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY] = {
    // Initialize native clips; slots 0, 45, 46 and the writable extension start cleared.
    [1]  = &_gMm1IdleAnimation,
    [2]  = &_gMm1WalkAnimation,
    [3]  = &_gMm1BackwardWalkAnimation,
    [4]  = &_gMm1RunAnimation,
    [5]  = &_gMm1NegativeTurnAnimation,
    [6]  = &_gMm1PositiveTurnAnimation,
    [7]  = &_gMm1AimEntryAnimation,
    [8]  = &_gMm1AimExitAnimation,
    [9]  = &_gMm1AimHoldAnimation,
    [10] = &_gMm1FireAnimation,
    [11] = &_gMm1FireAnimation,
    [12] = &_gMm1AimForwardAnimation,
    [13] = &_gMm1AimBackwardOrTurnAnimation,
    [14] = &_gMm1ReloadAnimation,
    [15] = &_gMm1ReloadAnimation,
    [16] = &_gMm1Part4HitReactionAnimation,
    [17] = &_gMm1OtherBodyHitReactionAnimation,
    [18] = &_gMm1DeathAnimation,
    [19] = &_gMm1UnarmedWalkAnimation,
    [20] = &_gMm1ReloadAnimation,
    [21] = &_gMm1IdleAnimation,
    [22] = &_gMm1IdleAnimation,
    [23] = &_gMm1HealthyIdleAnimation,
    [24] = &_gMm1InjuredIdleAnimation,
    [25] = &_gMm1CriticalIdleAnimation,
    [26] = &_gMm1ParasiteEnergyReleaseFamily0Animation,
    [27] = &_gMm1ParasiteEnergyChargeFamily0Animation,
    [28] = &_gMm1ParasiteEnergyCastFamily0Animation,
    [29] = &_gMm1ParasiteEnergyReleaseFamily1Animation,
    [30] = &_gMm1ParasiteEnergyChargeFamily1Animation,
    [31] = &_gMm1ParasiteEnergyCastFamily1Animation,
    [32] = &_gMm1ScriptedPresentationEntryAnimation,
    [33] = &_gMm1ScriptedPresentationExitAnimation,
    [34] = &_gMm1ScriptedPresentationEntryAnimation,
    [35] = &_gMm1ScriptedPresentationExitAnimation,
    [36] = &_gMm1StairAscentAnimation,
    [37] = &_gMm1StairDescentAnimation,
    [38] = &_gMm1StairAscentFinishEvenAnimation,
    [39] = &_gMm1StairAscentFinishOddAnimation,
    [40] = &_gMm1ItemUseAnimation,
    [41] = &_gMm1IdleAnimation,
    [42] = &_gMm1ParasiteEnergyReleaseFamily2Animation,
    [43] = &_gMm1ParasiteEnergyChargeFamily2Animation,
    [44] = &_gMm1ParasiteEnergyCastFamily2Animation,
};
#endif

/// Grenade Pistol/MM1 muzzle offsets in integer launcher-local coordinates.
///
/// Rows 0 and 1 select the Grenade Pistol and MM1 respectively, from bits 16..19
/// of the shot's spawn argument. Kyle's MM1 uses row 1. Only XYZ is consumed.
static SVECTOR _gGrenadeShellMuzzleOffsets[2] = {
    { 0, 480, 128, 0 },
    { 0, 544, 128, 0 },
};

/// Blast-sphere radii in game-coordinate units for the three grenade rounds.
///
/// Indexed by the loaded round minus `GRENADE_ROUND_FIRST`: fragmentation 500,
/// airburst 1200 and riot 2000. The fourth stored halfword is zero; its role is
/// unproven and no supported round selects it.
static const u16 _gGrenadeShellBlastRadii[4] __attribute__((section(".data"))) = {
    [GRENADE_ROUND_FRAGMENTATION - GRENADE_ROUND_FIRST] = 500,
    [GRENADE_ROUND_AIRBURST -
        GRENADE_ROUND_FIRST]                            = 1200,
    [GRENADE_ROUND_RIOT -
        GRENADE_ROUND_FIRST]                            = 2000,
    0,
};

/// Initial positive flight divisors for launcher rows 0 Grenade Pistol and 1 MM1.
///
/// The byte value is shifted left 16 into the Q16.16 flight clock. Flight divides
/// the Q12 direction by its integer half, then increases the divisor each tick.
/// The two trailing stored zero bytes have unproven roles and are never selected
/// by supported launchers; they are not valid initial divisors.
static u8 _gGrenadeShellInitialFlightDivisors[4] = { 12, 8, 0, 0 };

/* Each package carries its own shell model. */
#if GRENADE_PISTOL_VARIANT == GRENADE_PISTOL_VARIANT_PISTOL
/// Identity rest transform and self-parented root copied into each projectile.
static const TmdBone _gGrenadePistolShellModelSkeleton[] __attribute__((section(".data"))) = {
#include "assets/grenade_pistol_model_0E324_skeleton.inc"
};

/// Vertex count of the projectile's single part-local group: eight.
static const u32 _gGrenadePistolShellModelPartVerts[ARRAY_SIZE(_gGrenadePistolShellModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/grenade_pistol_model_0E324_partVerts.inc"
};

/// Shared vertices in integer coordinates local to the projectile root.
static SVECTOR _gGrenadePistolShellModelVerts[] = {
#include "assets/grenade_pistol_model_0E324_verts.inc"
};

/// Shared lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gGrenadePistolShellModelNormals[] = {
#include "assets/grenade_pistol_model_0E324_normals.inc"
};

/// Writable commands for six textured Gouraud quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// Model creation resolves the command's draw-handler slot in place.
static u32 _gGrenadePistolShellModelStream[] = {
#include "assets/grenade_pistol_model_0E324_stream.inc"
};

TmdSource gGrenadePistolShellModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = 6 * sizeof(POLY_GT4),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gGrenadePistolShellModelSkeleton),
    .partVertexCounts    = _gGrenadePistolShellModelPartVerts,
    .verts               = _gGrenadePistolShellModelVerts,
    .normals             = _gGrenadePistolShellModelNormals,
    .skeleton            = _gGrenadePistolShellModelSkeleton,
    .stream              = _gGrenadePistolShellModelStream,
};
#elif GRENADE_PISTOL_VARIANT == GRENADE_PISTOL_VARIANT_MM1
/// Identity rest transform and self-parented root copied into each MM1 shell.
static const TmdBone _gMm1ShellModelSkeleton[] __attribute__((section(".data"))) = {
#include "assets/mm1_model_101C4_skeleton.inc"
};

/// Vertex count of the shell's single part-local group: eight.
static const u32 _gMm1ShellModelPartVerts[ARRAY_SIZE(_gMm1ShellModelSkeleton)] __attribute__((section(".data"))) = {
#include "assets/mm1_model_101C4_partVerts.inc"
};

/// Shared shell vertices in integer coordinates local to the projectile root.
static SVECTOR _gMm1ShellModelVerts[] = {
#include "assets/mm1_model_101C4_verts.inc"
};

/// Shared shell lighting normals with twelve fractional bits, indexed independently of vertices.
static SVECTOR _gMm1ShellModelNormals[] = {
#include "assets/mm1_model_101C4_normals.inc"
};

/// Writable commands for six textured Gouraud quads.
///
/// One skeletal group and an empty final group end in `TMD_STREAM_END`.
/// Model creation resolves the command's draw-handler slot in place.
static u32 _gMm1ShellModelStream[] = {
#include "assets/mm1_model_101C4_stream.inc"
};

TmdSource gMm1ShellModel = {
    .handlersResolved    = false,
    .bufferHalfBytes     = 6 * sizeof(POLY_GT4),
    .preXformRegionBytes = 0,
    .partCount           = ARRAY_SIZE(_gMm1ShellModelSkeleton),
    .partVertexCounts    = _gMm1ShellModelPartVerts,
    .verts               = _gMm1ShellModelVerts,
    .normals             = _gMm1ShellModelNormals,
    .skeleton            = _gMm1ShellModelSkeleton,
    .stream              = _gMm1ShellModelStream,
};
#endif

PACKAGE_ALIASES
