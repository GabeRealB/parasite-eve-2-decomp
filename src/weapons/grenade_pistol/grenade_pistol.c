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

#define GRENADE_WEAPON (0xB + GRENADE_VARIANT)

#define SLOT_FUNC__(prefix, addr) func_##prefix##_##addr

#define SLOT_FUNC_(prefix, addr) SLOT_FUNC__(prefix, addr)

/// Name of a public symbol in a source several packages are built from: the
/// package's SLOT_PREFIX (declared in the overlay manifest) in place of a fixed
/// package name, so each build exports its own. `SLOT_FUNC(8011DBD0)` is
/// `func_mm1_8011DBD0` in the MM1's build.
#define SLOT_FUNC(addr) SLOT_FUNC_(SLOT_PREFIX, addr)

/// Which weapon this build is: 0 for the Grenade Pistol, 1 for the MM1. The two
/// packages are this source built once each, and each declares its variant in
/// the manifest. Both carry the other's row of the per-projectile tables.
#ifndef GRENADE_VARIANT
#error "GRENADE_VARIANT is a per-package build parameter"
#endif

/* gameplay's weapon table names each package's attack handler, so each build of
 * this source gives the handler its own package's name. */
#if GRENADE_VARIANT == 1
#define func_grenade_pistol_8011D1D4 func_mm1_8011D1D4
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
            if (Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case 2:
            actor->statePhase                  = 3;
            actor->rumblePosted                = 0;
            actor->attackControl.cooldownTicks = 0x28;
            Gp_PlayObjSfx(arg0->extra.tmd->coords,
                          ((gPlayerStatus.weaponSlotItem - 0xA) << 24) | 0x20000004 | (GRENADE_WEAPON << 16), 1);
            Gp_SpawnEff(EFFECT_GRENADE_MUZZLE_FLASH,
                        actor->equipmentTasks[1]->extra.tmd->coords, GRENADE_WEAPON,
                        NULL);
            Gp_ConsumeSlotQty(WEAPON_ITEM(GRENADE_WEAPON), 1);
            /* The projectile's kind and its row of the muzzle-offset and speed tables
               (bits 16-19 of its spawn argument) both follow the variant. */
            func_80104490(arg0, 0, 1 + GRENADE_VARIANT,
                          gPlayerStatus.weaponSlotItem | (GRENADE_VARIANT << 16) | (GRENADE_WEAPON << 8));
            playerActorPlayChildSlotsWithBlend(arg0, 0xA, 0, 3);
            break;
        case 3:
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0) {
                func_80106550(arg0);
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
    grenadeShellBlast,
    grenadeShellExit,
} };

void SLOT_FUNC(8011DBD0)(Task* arg0)
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
