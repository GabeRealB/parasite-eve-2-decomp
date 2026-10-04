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
    GfxRotationWords*              rot;
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
            rot                 = (GfxRotationWords*)&coord->coord;
            coord->parent       = work->parent;
            rot->m00M01         = ONE;
            rot->m02M10         = 0;
            rot->m11M12         = ONE;
            rot->m20M21         = 0;
            rot->m22            = ONE;
            coord->coord.t[0]   = D_m4a1_pyke_8011E90C.vx;
            coord->coord.t[1]   = D_m4a1_pyke_8011E90C.vy;
            coord->coord.t[2]   = D_m4a1_pyke_8011E90C.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            task->state = 1;
            break;
        case 1:
            switch (task->spawnArg1.value) {
                case 0:
                    break;
                case 1:
                    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                        work->age--;
                        pykeFlameDrawNozzle(
                            MATRIX_TRANS(&coord->workm), work->age, 0x80);
                        break;
                    }
                    Gp_UpdateCoord(coord);
                    pykeFlameDrawNozzle(MATRIX_TRANS(&coord->workm), work->age, 0x80);
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
                    Gp_UpdateCoord(coord);
                    if (work->scale < 0x180) {
                        work->scale = work->scale + 0x40;
                    }
                    eff = Gp_SpawnEff(EFFECT_M4A1_PYKE_FLAME, coord, (s32)(work->scale), NULL);
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

/// Per-frame task for one flame the Pyke throws (see pyke_flame.h).
void func_m4a1_pyke_8011D7D4(Task* task)
{
    pykeFlameTask(task);
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
/// `field_93E` and then asks `func_801060E0` where the dart went: a hit
/// (`2`) with rounds still to spend rearms for another `0x14` frames, anything
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
            if (Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1) !=
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
                func_80106238(arg0, 0, 1);
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
                Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x201C0005, 1);
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
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x201C0004, 1);
                    Gp_SpawnEff(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                0x1C, NULL);
                    playerActorPlayChildSlotsWithBlend(arg0, 0xA, 0, 2);
                    break;
                }
                actor->stateTimer = delay - 1;
                if (delay - 1 == 2) {
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                        Gp_PlayObjSfx(spot, 0x17, 1);
                    }
                }
                break;
            }
            /* fallthrough */
        case 4:
            actor->statePhase                                     = 6;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                Gp_PlayObjSfx(spot, 0x17, 1);
            }
            break;
        case 5:
            if (actor->actionValue == 0) {
                spent = func_80106264(2);
                if ((s8)func_801060E0(arg0) == 2 && spent != 0) {
                    actor->actionValue = 0x14;
                    Gp_ConsumeSlotQty(0x9B, 0x101);
                } else {
                    beam              = actor->weaponEffectTask;
                    actor->statePhase = 6;
                    if (beam != NULL) {
                        beam->spawnArg1.value = spent != 0 ? 3 : 4;
                    }
                    SndEvt_EnqueueType7(SOUND_PYKE_FIRE_TAIL, 1);
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
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = 0xC;
                func_80106550(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x50);
}
