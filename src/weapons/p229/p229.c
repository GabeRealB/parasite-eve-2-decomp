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
