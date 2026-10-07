#include "weapons/m4a1_pyke.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/pyke_flame.h"

/// Translation of the Pyke's effect coordinate frame inside its parent frame
/// (the muzzle), `(0, 0x200, 0x40)`.
static SVECTOR D_m4a1_pyke_8011E90C = { 0, 0x200, 0x40, 0 };

void func_m4a1_pyke_8011E4F8(Task* arg0);

/// Per-frame beam task for the M4A1 Pyke. Nothing runs while the player model
/// is hidden (`field_C & 0x80`) or effects are hidden
/// (`gRoomEffectState->effectControl >= 2`). State 0 hangs the task's own coordinate off
/// `field_8` at the fixed muzzle offset with an identity rotation; state 1 then
/// dispatches on `spawnArg1`:
///
/// - 1 draws the beam head at `workm.t` every frame and refreshes transient light slot
///   1 with narrow (`0x80` / `0x400`) falloff and random red intensity in
///   `0x400..0xB00`, arming the flare width in `scale`.
/// - 2 widens that flare by 0x40 a frame up to 0x180, spawns effect `0x6017F`
///   as a child of this task, and refreshes the light with a much wider
///   (`0x400` / `0x4000`) falloff and red intensity in `0x800..0xF00`.
/// - 3 and 4 switch back to sub-state 1 and 0, and 5 releases the pool block.
///
/// While `gRoomEffectState->effectControl` is non-zero the two drawing sub-states wind
/// `age` back down instead of advancing.
void func_m4a1_pyke_8011D1F8(Task* task)
{
    EffectWork*                    work;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    GfxCoord*                      light;
    EffectWork*                    eff;
    u32                            ang;

    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    lightSlot = &gWorldCoordTransientPointLights[1];
    light     = &lightSlot->light.head.transform.coord;
    slot      = &lightSlot->light;
    if ((gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) != 0) {
        return;
    }
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            coord->parent       = work->parent;
            gfxSetRotIdentity(&coord->coord);
            coord->coord.t[0]   = D_m4a1_pyke_8011E90C.vx;
            coord->coord.t[1]   = D_m4a1_pyke_8011E90C.vy;
            coord->coord.t[2]   = D_m4a1_pyke_8011E90C.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            task->state = 1;
            break;
        case 1:
            switch (task->spawnArg1.value) {
                case 0:
                    break;
                case 1:
                    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                        work->age--;
                        _pykeFlameDrawNozzle(
                            MATRIX_TRANS(&coord->workm), work->age, PYKE_FLAME_NOZZLE_SIZE_SCALE);
                        break;
                    }
                    actorRenderComposeCoord(coord);
                    _pykeFlameDrawNozzle(MATRIX_TRANS(&coord->workm), work->age, PYKE_FLAME_NOZZLE_SIZE_SCALE);
                    lightSlot->framesLeft = 4;
                    slot->inner           = 0x80;
                    slot->outer           = 0x400;
                    ang                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState       = ang;
                    // Green halves the unsigned red halfword; blue quarters its signed value.
                    slot->head.color.r = ((ang >> 16) & 0x700) + 0x400;
                    slot->head.color.g = (u16)slot->head.color.r >> 1;
                    slot->head.color.b = slot->head.color.r >> 2;
                    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &light->coord);
                    light->composeStamp = GRAPHICS_COORD_DIRTY;
                    work->scale         = 0x40;
                    break;
                case 2:
                    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                        work->age--;
                        break;
                    }
                    actorRenderComposeCoord(coord);
                    if (work->scale < 0x180) {
                        work->scale = work->scale + 0x40;
                    }
                    eff = effectSpawn(EFFECT_M4A1_PYKE_FLAME, coord, (s32)(work->scale), NULL);
                    if (eff != NULL) {
                        taskReparent(task, eff->task);
                    }
                    lightSlot->framesLeft = 4;
                    slot->inner           = 0x400;
                    slot->outer           = 0x4000;
                    ang                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState       = ang;
                    slot->head.color.r    = ((ang >> 16) & 0x700) + 0x800;
                    slot->head.color.g    = (u16)slot->head.color.r >> 1;
                    slot->head.color.b    = slot->head.color.r >> 2;
                    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &light->coord);
                    light->composeStamp = GRAPHICS_COORD_DIRTY;
                    break;
                case 3:
                    task->spawnArg1.value = 1;
                    break;
                case 4:
                    task->spawnArg1.value = 0;
                    break;
                case 5:
                    effectKillTask(work, task);
                    break;
            }
            break;
    }
}

#include "../../shared/pyke_flame_nozzle.inc.c"

#define PYKE_FLAME_KEY 0x21C1E
#include "../../shared/pyke_flame_task.inc.c"

void m4a1PykeFlameTask(Task* task)
{
    _pykeFlameTask(task);
}

#include "../../shared/pyke_flame_blob.inc.c"

#define PYKE_FLAME_SPLASH_DEAD_BIAS 1
#include "../../shared/pyke_flame_splash.inc.c"

#include "../../shared/pyke_flame_release.inc.c"

/// Per-frame firing state machine for the M4A1 Pyke. State 0 arms the shot and
/// raises the weapon (clip 8 instead of 1 when it was already up), state 1
/// waits for that clip. State 2 branches on `field_97F`: a held trigger (bit 0)
/// drops into the three-round burst of state 3, a tap (bit 1) launches the dart
/// (state 5) after telling the beam task (`field_914`) to go to sub-state 2,
/// and anything else falls straight into the burst. State 3 counts `field_934`
/// down to each round, spending one magazine round, playing `0x201C0004` and
/// spawning the muzzle flash, and picks the lock-on target on the frame after.
/// State 4 picks that target once and hands over to state 6. State 5 waits out
/// `field_93E` and then asks `playerActorReadAttackButton` for held fire input: secondary
/// input (`2`) with rounds still to spend rearms for another `0x14` frames, anything
/// else ends the burst, parks the beam task at sub-state 3 or 4 and plays the
/// `0x201C0005` tail. State 6 counts `field_979` down and drops out of the
/// firing pose once the aim check fails or the trigger has been released.
void func_m4a1_pyke_8011E4F8(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    GfxCoord*  spot;
    Task*      beam;
    s32        anim;
    s32        delay;
    s32        spent;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    SCRATCH_STACK_RESERVE_BYTES(0x50);
    spot = SCRATCH_STACK_CURSOR(GfxCoord);
    switch (actor->statePhase) {
        case 0:
            anim                                                  = 1;
            actor->state                                          = 4;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->statePhase                                    += anim;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x400;
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
                actor->turnRateIndex     = 0;
                actor->stateTimer        = 0;
                actor->attackCancelTicks = 9;
                actor->actionValue       = 3;
                playerActorSetWeaponAttackFlags(arg0, 0, 1);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x800;
            } else if (actor->attackButton & 2) {
                beam                               = actor->weaponEffectTask;
                actor->statePhase                  = 5;
                actor->turnRateIndex               = 2;
                actor->attackControl.cooldownTicks = 0x28;
                actor->attackCancelTicks           = 0x1C;
                actor->actionValue                 = 0x14;
                if (beam != NULL) {
                    beam->spawnArg1.value = 2;
                }
                Gp_ConsumeSlotQty(0x9B, 0x101);
                worldCoordPlaySound(arg0->extra.tmd->coords, 0x201C0005, 1);
                playerActorPlayChildSlotsWithBlend(arg0, 0xB, 0, 2);
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
                    Gp_ConsumeSlotQty(0x9B, 1);
                    if (func_80106264(1) == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(arg0->extra.tmd->coords, 0x201C0004, 1);
                    effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                0x1C, NULL);
                    playerActorPlayChildSlotsWithBlend(arg0, 0xA, 0, 2);
                    break;
                }
                actor->stateTimer = delay - 1;
                if (delay - 1 == 2) {
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    if (playerActorSpawnWeaponImpact(actor->weaponContacts, coord, spot) != 0) {
                        worldCoordPlaySound(spot, 0x17, 1);
                    }
                }
                break;
            }
            /* fallthrough */
        case 4:
            actor->statePhase                                     = 6;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (playerActorSpawnWeaponImpact(actor->weaponContacts, coord, spot) != 0) {
                worldCoordPlaySound(spot, 0x17, 1);
            }
            break;
        case 5:
            if (actor->actionValue == 0) {
                spent = func_80106264(2);
                if (playerActorReadAttackButton(arg0) == PLAYER_ACTOR_ATTACK_BUTTON_SECONDARY && spent != 0) {
                    actor->actionValue = 0x14;
                    Gp_ConsumeSlotQty(0x9B, 0x101);
                } else {
                    beam              = actor->weaponEffectTask;
                    actor->statePhase = 6;
                    if (beam != NULL) {
                        beam->spawnArg1.value = spent != 0 ? 3 : 4;
                    }
                    sndEvtRequestScriptStop(SOUND_PYKE_FIRE_TAIL, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    playerActorPlayChildSlotsWithBlend(arg0, 0xF, 0, 2);
                }
            } else {
                actor->actionValue = (u16)actor->actionValue - 1;
            }
            break;
        case 6:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (playerActorIsSlotAdvancingLinearly(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = 0xC;
                func_80106550(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x50);
}

static TmdBone _gM4a1PykeModel01BCCSkeleton[1] = {
#include "assets/m4a1_pyke_model_01BCC_skeleton.inc"
};

static u32 _gM4a1PykeModel01BCCPartVerts[1] = {
#include "assets/m4a1_pyke_model_01BCC_partVerts.inc"
};

static SVECTOR _gM4a1PykeModel01BCCVerts[74] = {
#include "assets/m4a1_pyke_model_01BCC_verts.inc"
};

static SVECTOR _gM4a1PykeModel01BCCNormals[64] = {
#include "assets/m4a1_pyke_model_01BCC_normals.inc"
};

static u32 _gM4a1PykeModel01BCCStream[504] = {
#include "assets/m4a1_pyke_model_01BCC_stream.inc"
};

TmdSource D_m4a1_pyke_8011F56C = {
    0,
    3668,
    0,
    1,
    _gM4a1PykeModel01BCCPartVerts,
    _gM4a1PykeModel01BCCVerts,
    _gM4a1PykeModel01BCCNormals,
    _gM4a1PykeModel01BCCSkeleton,
    _gM4a1PykeModel01BCCStream,
};

static AnimationPackedPose _gM4a1PykeAnimation02560Bank1[2] = {
#include "assets/m4a1_pyke_animation_02560_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation02560Bank4[8] = {
#include "assets/m4a1_pyke_animation_02560_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation02560Records[76] = {
#include "assets/m4a1_pyke_animation_02560_records.inc"
};

static u16 _gM4a1PykeAnimation02560Indices[20] = {
#include "assets/m4a1_pyke_animation_02560_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation02560 = {
    _gM4a1PykeAnimation02560Records,
    _gM4a1PykeAnimation02560Indices,
    { NULL, _gM4a1PykeAnimation02560Bank1, NULL, NULL, _gM4a1PykeAnimation02560Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation02C04Bank1[12] = {
#include "assets/m4a1_pyke_animation_02C04_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation02C04Bank4[151] = {
#include "assets/m4a1_pyke_animation_02C04_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation02C04Records[218] = {
#include "assets/m4a1_pyke_animation_02C04_records.inc"
};

static u16 _gM4a1PykeAnimation02C04Indices[20] = {
#include "assets/m4a1_pyke_animation_02C04_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation02C04 = {
    _gM4a1PykeAnimation02C04Records,
    _gM4a1PykeAnimation02C04Indices,
    { NULL, _gM4a1PykeAnimation02C04Bank1, NULL, NULL, _gM4a1PykeAnimation02C04Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation03464Bank1[19] = {
#include "assets/m4a1_pyke_animation_03464_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation03464Bank4[169] = {
#include "assets/m4a1_pyke_animation_03464_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation03464Records[290] = {
#include "assets/m4a1_pyke_animation_03464_records.inc"
};

static u16 _gM4a1PykeAnimation03464Indices[20] = {
#include "assets/m4a1_pyke_animation_03464_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation03464 = {
    _gM4a1PykeAnimation03464Records,
    _gM4a1PykeAnimation03464Indices,
    { NULL, _gM4a1PykeAnimation03464Bank1, NULL, NULL, _gM4a1PykeAnimation03464Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation03CC8Bank1[19] = {
#include "assets/m4a1_pyke_animation_03CC8_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation03CC8Bank4[170] = {
#include "assets/m4a1_pyke_animation_03CC8_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation03CC8Records[290] = {
#include "assets/m4a1_pyke_animation_03CC8_records.inc"
};

static u16 _gM4a1PykeAnimation03CC8Indices[20] = {
#include "assets/m4a1_pyke_animation_03CC8_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation03CC8 = {
    _gM4a1PykeAnimation03CC8Records,
    _gM4a1PykeAnimation03CC8Indices,
    { NULL, _gM4a1PykeAnimation03CC8Bank1, NULL, NULL, _gM4a1PykeAnimation03CC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation03FDCBank1[3] = {
#include "assets/m4a1_pyke_animation_03FDC_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation03FDCBank4[69] = {
#include "assets/m4a1_pyke_animation_03FDC_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation03FDCRecords[99] = {
#include "assets/m4a1_pyke_animation_03FDC_records.inc"
};

static u16 _gM4a1PykeAnimation03FDCIndices[20] = {
#include "assets/m4a1_pyke_animation_03FDC_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation03FDC = {
    _gM4a1PykeAnimation03FDCRecords,
    _gM4a1PykeAnimation03FDCIndices,
    { NULL, _gM4a1PykeAnimation03FDCBank1, NULL, NULL, _gM4a1PykeAnimation03FDCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation04738Bank1[14] = {
#include "assets/m4a1_pyke_animation_04738_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation04738Bank4[156] = {
#include "assets/m4a1_pyke_animation_04738_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation04738Records[253] = {
#include "assets/m4a1_pyke_animation_04738_records.inc"
};

static u16 _gM4a1PykeAnimation04738Indices[20] = {
#include "assets/m4a1_pyke_animation_04738_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation04738 = {
    _gM4a1PykeAnimation04738Records,
    _gM4a1PykeAnimation04738Indices,
    { NULL, _gM4a1PykeAnimation04738Bank1, NULL, NULL, _gM4a1PykeAnimation04738Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation04EC0Bank1[16] = {
#include "assets/m4a1_pyke_animation_04EC0_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation04EC0Bank4[167] = {
#include "assets/m4a1_pyke_animation_04EC0_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation04EC0Records[247] = {
#include "assets/m4a1_pyke_animation_04EC0_records.inc"
};

static u16 _gM4a1PykeAnimation04EC0Indices[20] = {
#include "assets/m4a1_pyke_animation_04EC0_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation04EC0 = {
    _gM4a1PykeAnimation04EC0Records,
    _gM4a1PykeAnimation04EC0Indices,
    { NULL, _gM4a1PykeAnimation04EC0Bank1, NULL, NULL, _gM4a1PykeAnimation04EC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation05194Bank1[6] = {
#include "assets/m4a1_pyke_animation_05194_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation05194Bank4[52] = {
#include "assets/m4a1_pyke_animation_05194_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation05194Records[91] = {
#include "assets/m4a1_pyke_animation_05194_records.inc"
};

static u16 _gM4a1PykeAnimation05194Indices[20] = {
#include "assets/m4a1_pyke_animation_05194_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation05194 = {
    _gM4a1PykeAnimation05194Records,
    _gM4a1PykeAnimation05194Indices,
    { NULL, _gM4a1PykeAnimation05194Bank1, NULL, NULL, _gM4a1PykeAnimation05194Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation05524Bank1[7] = {
#include "assets/m4a1_pyke_animation_05524_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation05524Bank4[73] = {
#include "assets/m4a1_pyke_animation_05524_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation05524Records[114] = {
#include "assets/m4a1_pyke_animation_05524_records.inc"
};

static u16 _gM4a1PykeAnimation05524Indices[20] = {
#include "assets/m4a1_pyke_animation_05524_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation05524 = {
    _gM4a1PykeAnimation05524Records,
    _gM4a1PykeAnimation05524Indices,
    { NULL, _gM4a1PykeAnimation05524Bank1, NULL, NULL, _gM4a1PykeAnimation05524Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation059ACBank1[9] = {
#include "assets/m4a1_pyke_animation_059AC_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation059ACBank4[104] = {
#include "assets/m4a1_pyke_animation_059AC_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation059ACRecords[139] = {
#include "assets/m4a1_pyke_animation_059AC_records.inc"
};

static u16 _gM4a1PykeAnimation059ACIndices[20] = {
#include "assets/m4a1_pyke_animation_059AC_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation059AC = {
    _gM4a1PykeAnimation059ACRecords,
    _gM4a1PykeAnimation059ACIndices,
    { NULL, _gM4a1PykeAnimation059ACBank1, NULL, NULL, _gM4a1PykeAnimation059ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation05BA8Bank1[3] = {
#include "assets/m4a1_pyke_animation_05BA8_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation05BA8Bank4[22] = {
#include "assets/m4a1_pyke_animation_05BA8_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation05BA8Records[76] = {
#include "assets/m4a1_pyke_animation_05BA8_records.inc"
};

static u16 _gM4a1PykeAnimation05BA8Indices[20] = {
#include "assets/m4a1_pyke_animation_05BA8_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation05BA8 = {
    _gM4a1PykeAnimation05BA8Records,
    _gM4a1PykeAnimation05BA8Indices,
    { NULL, _gM4a1PykeAnimation05BA8Bank1, NULL, NULL, _gM4a1PykeAnimation05BA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation05E80Bank1[6] = {
#include "assets/m4a1_pyke_animation_05E80_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation05E80Bank4[57] = {
#include "assets/m4a1_pyke_animation_05E80_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation05E80Records[87] = {
#include "assets/m4a1_pyke_animation_05E80_records.inc"
};

static u16 _gM4a1PykeAnimation05E80Indices[20] = {
#include "assets/m4a1_pyke_animation_05E80_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation05E80 = {
    _gM4a1PykeAnimation05E80Records,
    _gM4a1PykeAnimation05E80Indices,
    { NULL, _gM4a1PykeAnimation05E80Bank1, NULL, NULL, _gM4a1PykeAnimation05E80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation06124Bank1[4] = {
#include "assets/m4a1_pyke_animation_06124_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation06124Bank4[55] = {
#include "assets/m4a1_pyke_animation_06124_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation06124Records[82] = {
#include "assets/m4a1_pyke_animation_06124_records.inc"
};

static u16 _gM4a1PykeAnimation06124Indices[20] = {
#include "assets/m4a1_pyke_animation_06124_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation06124 = {
    _gM4a1PykeAnimation06124Records,
    _gM4a1PykeAnimation06124Indices,
    { NULL, _gM4a1PykeAnimation06124Bank1, NULL, NULL, _gM4a1PykeAnimation06124Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation06324Bank1[3] = {
#include "assets/m4a1_pyke_animation_06324_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation06324Bank4[23] = {
#include "assets/m4a1_pyke_animation_06324_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation06324Records[76] = {
#include "assets/m4a1_pyke_animation_06324_records.inc"
};

static u16 _gM4a1PykeAnimation06324Indices[20] = {
#include "assets/m4a1_pyke_animation_06324_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation06324 = {
    _gM4a1PykeAnimation06324Records,
    _gM4a1PykeAnimation06324Indices,
    { NULL, _gM4a1PykeAnimation06324Bank1, NULL, NULL, _gM4a1PykeAnimation06324Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation06678Bank1[8] = {
#include "assets/m4a1_pyke_animation_06678_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation06678Bank4[68] = {
#include "assets/m4a1_pyke_animation_06678_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation06678Records[101] = {
#include "assets/m4a1_pyke_animation_06678_records.inc"
};

static u16 _gM4a1PykeAnimation06678Indices[20] = {
#include "assets/m4a1_pyke_animation_06678_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation06678 = {
    _gM4a1PykeAnimation06678Records,
    _gM4a1PykeAnimation06678Indices,
    { NULL, _gM4a1PykeAnimation06678Bank1, NULL, NULL, _gM4a1PykeAnimation06678Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0692CBank1[5] = {
#include "assets/m4a1_pyke_animation_0692C_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0692CBank4[55] = {
#include "assets/m4a1_pyke_animation_0692C_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0692CRecords[83] = {
#include "assets/m4a1_pyke_animation_0692C_records.inc"
};

static u16 _gM4a1PykeAnimation0692CIndices[20] = {
#include "assets/m4a1_pyke_animation_0692C_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0692C = {
    _gM4a1PykeAnimation0692CRecords,
    _gM4a1PykeAnimation0692CIndices,
    { NULL, _gM4a1PykeAnimation0692CBank1, NULL, NULL, _gM4a1PykeAnimation0692CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation06C4CBank1[6] = {
#include "assets/m4a1_pyke_animation_06C4C_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation06C4CBank4[66] = {
#include "assets/m4a1_pyke_animation_06C4C_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation06C4CRecords[96] = {
#include "assets/m4a1_pyke_animation_06C4C_records.inc"
};

static u16 _gM4a1PykeAnimation06C4CIndices[20] = {
#include "assets/m4a1_pyke_animation_06C4C_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation06C4C = {
    _gM4a1PykeAnimation06C4CRecords,
    _gM4a1PykeAnimation06C4CIndices,
    { NULL, _gM4a1PykeAnimation06C4CBank1, NULL, NULL, _gM4a1PykeAnimation06C4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation07410Bank1[18] = {
#include "assets/m4a1_pyke_animation_07410_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation07410Bank4[184] = {
#include "assets/m4a1_pyke_animation_07410_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation07410Records[239] = {
#include "assets/m4a1_pyke_animation_07410_records.inc"
};

static u16 _gM4a1PykeAnimation07410Indices[20] = {
#include "assets/m4a1_pyke_animation_07410_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation07410 = {
    _gM4a1PykeAnimation07410Records,
    _gM4a1PykeAnimation07410Indices,
    { NULL, _gM4a1PykeAnimation07410Bank1, NULL, NULL, _gM4a1PykeAnimation07410Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation086A8Bank1[29] = {
#include "assets/m4a1_pyke_animation_086A8_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation086A8Bank4[450] = {
#include "assets/m4a1_pyke_animation_086A8_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation086A8Records[633] = {
#include "assets/m4a1_pyke_animation_086A8_records.inc"
};

static u16 _gM4a1PykeAnimation086A8Indices[20] = {
#include "assets/m4a1_pyke_animation_086A8_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation086A8 = {
    _gM4a1PykeAnimation086A8Records,
    _gM4a1PykeAnimation086A8Indices,
    { NULL, _gM4a1PykeAnimation086A8Bank1, NULL, NULL, _gM4a1PykeAnimation086A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation09220Bank1[12] = {
#include "assets/m4a1_pyke_animation_09220_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation09220Bank4[266] = {
#include "assets/m4a1_pyke_animation_09220_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation09220Records[412] = {
#include "assets/m4a1_pyke_animation_09220_records.inc"
};

static u16 _gM4a1PykeAnimation09220Indices[20] = {
#include "assets/m4a1_pyke_animation_09220_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation09220 = {
    _gM4a1PykeAnimation09220Records,
    _gM4a1PykeAnimation09220Indices,
    { NULL, _gM4a1PykeAnimation09220Bank1, NULL, NULL, _gM4a1PykeAnimation09220Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0993CBank1[9] = {
#include "assets/m4a1_pyke_animation_0993C_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0993CBank4[144] = {
#include "assets/m4a1_pyke_animation_0993C_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0993CRecords[264] = {
#include "assets/m4a1_pyke_animation_0993C_records.inc"
};

static u16 _gM4a1PykeAnimation0993CIndices[20] = {
#include "assets/m4a1_pyke_animation_0993C_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0993C = {
    _gM4a1PykeAnimation0993CRecords,
    _gM4a1PykeAnimation0993CIndices,
    { NULL, _gM4a1PykeAnimation0993CBank1, NULL, NULL, _gM4a1PykeAnimation0993CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation09DBCBank1[6] = {
#include "assets/m4a1_pyke_animation_09DBC_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation09DBCBank4[107] = {
#include "assets/m4a1_pyke_animation_09DBC_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation09DBCRecords[143] = {
#include "assets/m4a1_pyke_animation_09DBC_records.inc"
};

static u16 _gM4a1PykeAnimation09DBCIndices[20] = {
#include "assets/m4a1_pyke_animation_09DBC_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation09DBC = {
    _gM4a1PykeAnimation09DBCRecords,
    _gM4a1PykeAnimation09DBCIndices,
    { NULL, _gM4a1PykeAnimation09DBCBank1, NULL, NULL, _gM4a1PykeAnimation09DBCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation09F94Bank1[3] = {
#include "assets/m4a1_pyke_animation_09F94_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation09F94Bank4[32] = {
#include "assets/m4a1_pyke_animation_09F94_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation09F94Records[57] = {
#include "assets/m4a1_pyke_animation_09F94_records.inc"
};

static u16 _gM4a1PykeAnimation09F94Indices[20] = {
#include "assets/m4a1_pyke_animation_09F94_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation09F94 = {
    _gM4a1PykeAnimation09F94Records,
    _gM4a1PykeAnimation09F94Indices,
    { NULL, _gM4a1PykeAnimation09F94Bank1, NULL, NULL, _gM4a1PykeAnimation09F94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0A4F8Bank1[11] = {
#include "assets/m4a1_pyke_animation_0A4F8_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0A4F8Bank4[125] = {
#include "assets/m4a1_pyke_animation_0A4F8_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0A4F8Records[167] = {
#include "assets/m4a1_pyke_animation_0A4F8_records.inc"
};

static u16 _gM4a1PykeAnimation0A4F8Indices[20] = {
#include "assets/m4a1_pyke_animation_0A4F8_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0A4F8 = {
    _gM4a1PykeAnimation0A4F8Records,
    _gM4a1PykeAnimation0A4F8Indices,
    { NULL, _gM4a1PykeAnimation0A4F8Bank1, NULL, NULL, _gM4a1PykeAnimation0A4F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0A6ECBank1[3] = {
#include "assets/m4a1_pyke_animation_0A6EC_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0A6ECBank4[20] = {
#include "assets/m4a1_pyke_animation_0A6EC_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0A6ECRecords[76] = {
#include "assets/m4a1_pyke_animation_0A6EC_records.inc"
};

static u16 _gM4a1PykeAnimation0A6ECIndices[20] = {
#include "assets/m4a1_pyke_animation_0A6EC_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0A6EC = {
    _gM4a1PykeAnimation0A6ECRecords,
    _gM4a1PykeAnimation0A6ECIndices,
    { NULL, _gM4a1PykeAnimation0A6ECBank1, NULL, NULL, _gM4a1PykeAnimation0A6ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0AB6CBank1[8] = {
#include "assets/m4a1_pyke_animation_0AB6C_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0AB6CBank4[105] = {
#include "assets/m4a1_pyke_animation_0AB6C_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0AB6CRecords[139] = {
#include "assets/m4a1_pyke_animation_0AB6C_records.inc"
};

static u16 _gM4a1PykeAnimation0AB6CIndices[20] = {
#include "assets/m4a1_pyke_animation_0AB6C_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0AB6C = {
    _gM4a1PykeAnimation0AB6CRecords,
    _gM4a1PykeAnimation0AB6CIndices,
    { NULL, _gM4a1PykeAnimation0AB6CBank1, NULL, NULL, _gM4a1PykeAnimation0AB6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0AD44Bank1[2] = {
#include "assets/m4a1_pyke_animation_0AD44_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0AD44Bank4[16] = {
#include "assets/m4a1_pyke_animation_0AD44_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0AD44Records[76] = {
#include "assets/m4a1_pyke_animation_0AD44_records.inc"
};

static u16 _gM4a1PykeAnimation0AD44Indices[20] = {
#include "assets/m4a1_pyke_animation_0AD44_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0AD44 = {
    _gM4a1PykeAnimation0AD44Records,
    _gM4a1PykeAnimation0AD44Indices,
    { NULL, _gM4a1PykeAnimation0AD44Bank1, NULL, NULL, _gM4a1PykeAnimation0AD44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0B584Bank1[14] = {
#include "assets/m4a1_pyke_animation_0B584_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0B584Bank4[194] = {
#include "assets/m4a1_pyke_animation_0B584_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0B584Records[272] = {
#include "assets/m4a1_pyke_animation_0B584_records.inc"
};

static u16 _gM4a1PykeAnimation0B584Indices[20] = {
#include "assets/m4a1_pyke_animation_0B584_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0B584 = {
    _gM4a1PykeAnimation0B584Records,
    _gM4a1PykeAnimation0B584Indices,
    { NULL, _gM4a1PykeAnimation0B584Bank1, NULL, NULL, _gM4a1PykeAnimation0B584Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0C02CBank1[19] = {
#include "assets/m4a1_pyke_animation_0C02C_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0C02CBank4[269] = {
#include "assets/m4a1_pyke_animation_0C02C_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0C02CRecords[336] = {
#include "assets/m4a1_pyke_animation_0C02C_records.inc"
};

static u16 _gM4a1PykeAnimation0C02CIndices[20] = {
#include "assets/m4a1_pyke_animation_0C02C_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0C02C = {
    _gM4a1PykeAnimation0C02CRecords,
    _gM4a1PykeAnimation0C02CIndices,
    { NULL, _gM4a1PykeAnimation0C02CBank1, NULL, NULL, _gM4a1PykeAnimation0C02CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0CEC8Bank1[24] = {
#include "assets/m4a1_pyke_animation_0CEC8_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0CEC8Bank4[390] = {
#include "assets/m4a1_pyke_animation_0CEC8_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0CEC8Records[453] = {
#include "assets/m4a1_pyke_animation_0CEC8_records.inc"
};

static u16 _gM4a1PykeAnimation0CEC8Indices[20] = {
#include "assets/m4a1_pyke_animation_0CEC8_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0CEC8 = {
    _gM4a1PykeAnimation0CEC8Records,
    _gM4a1PykeAnimation0CEC8Indices,
    { NULL, _gM4a1PykeAnimation0CEC8Bank1, NULL, NULL, _gM4a1PykeAnimation0CEC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0D36CBank1[8] = {
#include "assets/m4a1_pyke_animation_0D36C_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0D36CBank4[102] = {
#include "assets/m4a1_pyke_animation_0D36C_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0D36CRecords[151] = {
#include "assets/m4a1_pyke_animation_0D36C_records.inc"
};

static u16 _gM4a1PykeAnimation0D36CIndices[20] = {
#include "assets/m4a1_pyke_animation_0D36C_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0D36C = {
    _gM4a1PykeAnimation0D36CRecords,
    _gM4a1PykeAnimation0D36CIndices,
    { NULL, _gM4a1PykeAnimation0D36CBank1, NULL, NULL, _gM4a1PykeAnimation0D36CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0D7F4Bank1[7] = {
#include "assets/m4a1_pyke_animation_0D7F4_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0D7F4Bank4[91] = {
#include "assets/m4a1_pyke_animation_0D7F4_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0D7F4Records[158] = {
#include "assets/m4a1_pyke_animation_0D7F4_records.inc"
};

static u16 _gM4a1PykeAnimation0D7F4Indices[20] = {
#include "assets/m4a1_pyke_animation_0D7F4_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0D7F4 = {
    _gM4a1PykeAnimation0D7F4Records,
    _gM4a1PykeAnimation0D7F4Indices,
    { NULL, _gM4a1PykeAnimation0D7F4Bank1, NULL, NULL, _gM4a1PykeAnimation0D7F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0DC68Bank1[8] = {
#include "assets/m4a1_pyke_animation_0DC68_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0DC68Bank4[104] = {
#include "assets/m4a1_pyke_animation_0DC68_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0DC68Records[137] = {
#include "assets/m4a1_pyke_animation_0DC68_records.inc"
};

static u16 _gM4a1PykeAnimation0DC68Indices[20] = {
#include "assets/m4a1_pyke_animation_0DC68_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0DC68 = {
    _gM4a1PykeAnimation0DC68Records,
    _gM4a1PykeAnimation0DC68Indices,
    { NULL, _gM4a1PykeAnimation0DC68Bank1, NULL, NULL, _gM4a1PykeAnimation0DC68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0E374Bank1[13] = {
#include "assets/m4a1_pyke_animation_0E374_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0E374Bank4[175] = {
#include "assets/m4a1_pyke_animation_0E374_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0E374Records[217] = {
#include "assets/m4a1_pyke_animation_0E374_records.inc"
};

static u16 _gM4a1PykeAnimation0E374Indices[20] = {
#include "assets/m4a1_pyke_animation_0E374_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0E374 = {
    _gM4a1PykeAnimation0E374Records,
    _gM4a1PykeAnimation0E374Indices,
    { NULL, _gM4a1PykeAnimation0E374Bank1, NULL, NULL, _gM4a1PykeAnimation0E374Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0E8ECBank1[10] = {
#include "assets/m4a1_pyke_animation_0E8EC_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0E8ECBank4[131] = {
#include "assets/m4a1_pyke_animation_0E8EC_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0E8ECRecords[169] = {
#include "assets/m4a1_pyke_animation_0E8EC_records.inc"
};

static u16 _gM4a1PykeAnimation0E8ECIndices[20] = {
#include "assets/m4a1_pyke_animation_0E8EC_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0E8EC = {
    _gM4a1PykeAnimation0E8ECRecords,
    _gM4a1PykeAnimation0E8ECIndices,
    { NULL, _gM4a1PykeAnimation0E8ECBank1, NULL, NULL, _gM4a1PykeAnimation0E8ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0F1A4Bank1[18] = {
#include "assets/m4a1_pyke_animation_0F1A4_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0F1A4Bank4[201] = {
#include "assets/m4a1_pyke_animation_0F1A4_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0F1A4Records[283] = {
#include "assets/m4a1_pyke_animation_0F1A4_records.inc"
};

static u16 _gM4a1PykeAnimation0F1A4Indices[20] = {
#include "assets/m4a1_pyke_animation_0F1A4_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0F1A4 = {
    _gM4a1PykeAnimation0F1A4Records,
    _gM4a1PykeAnimation0F1A4Indices,
    { NULL, _gM4a1PykeAnimation0F1A4Bank1, NULL, NULL, _gM4a1PykeAnimation0F1A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation0F944Bank1[16] = {
#include "assets/m4a1_pyke_animation_0F944_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation0F944Bank4[157] = {
#include "assets/m4a1_pyke_animation_0F944_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation0F944Records[263] = {
#include "assets/m4a1_pyke_animation_0F944_records.inc"
};

static u16 _gM4a1PykeAnimation0F944Indices[20] = {
#include "assets/m4a1_pyke_animation_0F944_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation0F944 = {
    _gM4a1PykeAnimation0F944Records,
    _gM4a1PykeAnimation0F944Indices,
    { NULL, _gM4a1PykeAnimation0F944Bank1, NULL, NULL, _gM4a1PykeAnimation0F944Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gM4a1PykeAnimation10318Bank1[25] = {
#include "assets/m4a1_pyke_animation_10318_bank1.inc"
};

static AnimationPackedRotation _gM4a1PykeAnimation10318Bank4[223] = {
#include "assets/m4a1_pyke_animation_10318_bank4.inc"
};

static AnimationRecord _gM4a1PykeAnimation10318Records[311] = {
#include "assets/m4a1_pyke_animation_10318_records.inc"
};

static u16 _gM4a1PykeAnimation10318Indices[20] = {
#include "assets/m4a1_pyke_animation_10318_indices.inc"
};

static AnimationSet _gM4a1PykeAnimation10318 = {
    _gM4a1PykeAnimation10318Records,
    _gM4a1PykeAnimation10318Indices,
    { NULL, _gM4a1PykeAnimation10318Bank1, NULL, NULL, _gM4a1PykeAnimation10318Bank4, NULL, NULL, NULL },
};

AnimationBank D_m4a1_pyke_8012D500 = { { {
    NULL,
    &_gM4a1PykeAnimation02560,
    &_gM4a1PykeAnimation0F1A4,
    &_gM4a1PykeAnimation0F944,
    &_gM4a1PykeAnimation10318,
    &_gM4a1PykeAnimation03464,
    &_gM4a1PykeAnimation03CC8,
    &_gM4a1PykeAnimation0E374,
    &_gM4a1PykeAnimation0E8EC,
    &_gM4a1PykeAnimation0AD44,
    &_gM4a1PykeAnimation0D36C,
    &_gM4a1PykeAnimation0D7F4,
    &_gM4a1PykeAnimation0C02C,
    &_gM4a1PykeAnimation0B584,
    &_gM4a1PykeAnimation0CEC8,
    &_gM4a1PykeAnimation0DC68,
    &_gM4a1PykeAnimation0692C,
    &_gM4a1PykeAnimation06C4C,
    &_gM4a1PykeAnimation07410,
    &_gM4a1PykeAnimation02C04,
    &_gM4a1PykeAnimation0CEC8,
    &_gM4a1PykeAnimation02560,
    &_gM4a1PykeAnimation02560,
    &_gM4a1PykeAnimation086A8,
    &_gM4a1PykeAnimation0993C,
    &_gM4a1PykeAnimation09220,
    &_gM4a1PykeAnimation059AC,
    &_gM4a1PykeAnimation05BA8,
    &_gM4a1PykeAnimation05E80,
    &_gM4a1PykeAnimation06124,
    &_gM4a1PykeAnimation06324,
    &_gM4a1PykeAnimation06678,
    &_gM4a1PykeAnimation09DBC,
    &_gM4a1PykeAnimation09F94,
    &_gM4a1PykeAnimation09DBC,
    &_gM4a1PykeAnimation09F94,
    &_gM4a1PykeAnimation04738,
    &_gM4a1PykeAnimation04EC0,
    &_gM4a1PykeAnimation05524,
    &_gM4a1PykeAnimation05194,
    &_gM4a1PykeAnimation03FDC,
    &_gM4a1PykeAnimation02560,
    &_gM4a1PykeAnimation0A4F8,
    &_gM4a1PykeAnimation0A6EC,
    &_gM4a1PykeAnimation0AB6C,
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
