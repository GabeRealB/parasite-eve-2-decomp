#include "types.h"

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

#include "weapons/weapon.h"

/// The P08, the P08 with the snail magazine and the Mongoose are this source
/// built once each, and each declares these values in the manifest.
///
/// `WEAPON_ID` is the weapon's index (4, 1 and 9), which keys the firing sound
/// and the item. `P08_FLASH_EFFECT` is the muzzle-flash effect spawned per shot
/// and `P08_FLASH_WEAPON` the weapon index it is handed - 1 for both P08s, the
/// Mongoose its own. `P08_FIELD_940` is what `field_940` is set to when the
/// firing pose ends.
#if !defined(WEAPON_ID) || !defined(P08_FLASH_EFFECT) || !defined(P08_FLASH_WEAPON) || !defined(P08_FIELD_940)
#error "WEAPON_ID, P08_FLASH_EFFECT, P08_FLASH_WEAPON and P08_FIELD_940 are per-package build parameters"
#endif

/* gameplay's weapon table names each package's attack handler, so each build of
 * this source gives the handler its own package's name. */
#if WEAPON_ID == 0x1
#define func_p08_8011D1D8 func_p08_snail_8011D1D8
#elif WEAPON_ID == 0x9
#define func_p08_8011D1D8 func_mongoose_8011D1D8
#endif

void func_p08_8011D1D8(Task* arg0);

void func_p08_8011D1D8(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    GfxCoord*  spot;
    s32        anim;

    SCRATCH_STACK_RESERVE_BYTES(0x50);
    spot  = SCRATCH_STACK_CURSOR(GfxCoord);
    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (actor->statePhase) {
        case 0:
            actor->state             = 4;
            actor->statePhase        = 1;
            anim                     = 1;
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->rumblePosted      = 0;
            actor->attackCancelTicks = 0xB;
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
            actor->statePhase++;
            func_80106238(arg0, 0, 0);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            Gp_ConsumeSlotQty(WEAPON_ITEM(WEAPON_ID), 1);
            Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20000004 | (WEAPON_ID << 16), 1);
            Gp_SpawnEff(P08_FLASH_EFFECT,
                        actor->equipmentTasks[1]->extra.tmd->coords,
                        P08_FLASH_WEAPON, NULL);
            Gp_AnimResetChildSlots(arg0, 0xA);
            break;
        case 3:
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                Gp_PlayObjSfx(spot, 0x17, 1);
            }
            /* fallthrough */
        case 4:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = P08_FIELD_940;
                func_80106550(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x50);
}

/* Each package carries its own model. */
#if WEAPON_ID == 0x4
static TmdBone _gP08Model00440Skeleton[1] = {
#include "assets/p08_model_00440_skeleton.inc"
};

static u32 _gP08Model00440PartVerts[1] = {
#include "assets/p08_model_00440_partVerts.inc"
};

static SVECTOR _gP08Model00440Verts[28] = {
#include "assets/p08_model_00440_verts.inc"
};

static SVECTOR _gP08Model00440Normals[26] = {
#include "assets/p08_model_00440_normals.inc"
};

static u32 _gP08Model00440Stream[201] = {
#include "assets/p08_model_00440_stream.inc"
};

TmdSource D_p08_8011D924 = {
    0,
    1408,
    0,
    1,
    _gP08Model00440PartVerts,
    _gP08Model00440Verts,
    _gP08Model00440Normals,
    _gP08Model00440Skeleton,
    _gP08Model00440Stream,
};
#elif WEAPON_ID == 0x1
static TmdBone _gP08SnailP08Model00440Skeleton[1] = {
#include "assets/p08_model_00440_skeleton.inc"
};

static u32 _gP08SnailP08Model00440PartVerts[1] = {
#include "assets/p08_model_00440_partVerts.inc"
};

static SVECTOR _gP08SnailP08Model00440Verts[28] = {
#include "assets/p08_model_00440_verts.inc"
};

static SVECTOR _gP08SnailP08Model00440Normals[26] = {
#include "assets/p08_model_00440_normals.inc"
};

static u32 _gP08SnailP08Model00440Stream[201] = {
#include "assets/p08_model_00440_stream.inc"
};

TmdSource D_p08_snail_8011D924 = {
    0,
    1408,
    0,
    1,
    _gP08SnailP08Model00440PartVerts,
    _gP08SnailP08Model00440Verts,
    _gP08SnailP08Model00440Normals,
    _gP08SnailP08Model00440Skeleton,
    _gP08SnailP08Model00440Stream,
};
#elif WEAPON_ID == 0x9
static TmdBone _gMongooseModel00450Skeleton[1] = {
#include "assets/mongoose_model_00450_skeleton.inc"
};

static u32 _gMongooseModel00450PartVerts[1] = {
#include "assets/mongoose_model_00450_partVerts.inc"
};

static SVECTOR _gMongooseModel00450Verts[28] = {
#include "assets/mongoose_model_00450_verts.inc"
};

static SVECTOR _gMongooseModel00450Normals[28] = {
#include "assets/mongoose_model_00450_normals.inc"
};

static u32 _gMongooseModel00450Stream[201] = {
#include "assets/mongoose_model_00450_stream.inc"
};

TmdSource D_mongoose_8011D934 = {
    0,
    1408,
    0,
    1,
    _gMongooseModel00450PartVerts,
    _gMongooseModel00450Verts,
    _gMongooseModel00450Normals,
    _gMongooseModel00450Skeleton,
    _gMongooseModel00450Stream,
};
#endif
