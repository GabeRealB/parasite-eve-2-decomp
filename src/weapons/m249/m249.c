#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"

#include "main/coord.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// 0x68-byte scratch block `func_m249_8011D1DC` takes from the scratch stack.
/// Only the trailing coordinate is used: `Gp_PickNearestRec18` writes the
/// chosen impact point into its `workm.t`, and that same coordinate is then
/// handed to `Gp_PlayObjSfx` as the sound source.
typedef struct _M249Scratch {
    /* 0x00 */ byte     pad_0[0x18];
    /* 0x18 */ GfxCoord coord;
} M249Scratch;
STATIC_ASSERT_SIZEOF(M249Scratch, 0x68);

static void func_m249_8011D1DC(Task* arg0);

static void func_m249_8011D1DC(Task* arg0)
{
    GameActor*   actor;
    GfxCoord*    coord;
    GfxCoord*    spot;
    M249Scratch* scratch;
    s32          anim;

    SCRATCH_STACK_RESERVE_BYTES(0x68);
    scratch = SCRATCH_STACK_CURSOR(M249Scratch);
    actor   = arg0->work;
    coord   = arg0->extra.tmd->coords;
    switch (actor->statePhase) {
        case 0:
            anim                                                  = 1;
            actor->state                                          = 4;
            actor->turnRateIndex                                  = 2;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->statePhase                                    += anim;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0xC00;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                anim = 8;
            }
            Gp_AnimPlayChildSlotsEx(arg0, 9, 0, anim);
            actor->movementMode = 0;
            break;
        case 1:
            if (Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case 2:
        fire:
            actor->statePhase                  = 3;
            actor->attackControl.cooldownTicks = 0;
            actor->rumblePosted                = 0;
            actor->stateTimer                  = 2;
            func_80106238(arg0, 0, actor->attackButton != 1);
            /* fallthrough */
        case 3:
            if (--actor->stateTimer == 0) {
                actor->statePhase++;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                Gp_ConsumeSlotQty(0x90, 1);
                Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20110004, 1);
                Gp_SpawnEff(EFFECT_RIFLE_MUZZLE_FLASH,
                            actor->equipmentTasks[1]->extra.tmd->coords, 0x11,
                            NULL);
                Gp_AnimPlayChildSlotsEx(arg0, 0xA, 1, 2);
            }
            break;
        case 4:
            spot                = &scratch->coord;
            actor->movementSign = 0;
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                Gp_PlayObjSfx(spot, 0x17, 1);
            }
            /* fallthrough */
        case 5:
            if ((s8)func_801060E0(arg0) != 0 && func_80106264(1) > 0) {
                goto fire;
            }
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0) {
                func_80106550(arg0);
            }
            break;
    }
    Gp_TrackLockTarget(arg0);
    SCRATCH_STACK_RELEASE_BYTES(0x68);
}
