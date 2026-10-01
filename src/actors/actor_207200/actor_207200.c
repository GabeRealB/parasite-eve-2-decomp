#include "actor_207200_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/enemy.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gamemain.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/glow_pod.h"

extern EnemyParams gGlowPodParams;
extern u8          gGlowPodAnimSets[];
extern SVECTOR     gGlowPodSparkOffset;
extern SVECTOR     gGlowPodHitFxOffset;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

extern TmdSource D_actor_207200_8014E4C8;
void             func_actor_207200_8014AC9C(Task*);

DamageAttack D_actor_207200_8014DBB8[1] = { 0 };

EnemyParams gGlowPodParams = { D_actor_207200_8014DBB8, 1, 2, 32, 1, 100, 20, 100, 99 };

TmdBone D_actor_207200_8014DBCC[3] = {
#include "assets/actor_207200_model_046A8_skeleton.inc"
};

u32 D_actor_207200_8014DC38[3] = {
#include "assets/actor_207200_model_046A8_partVerts.inc"
};

SVECTOR D_actor_207200_8014DC44[37] = {
#include "assets/actor_207200_model_046A8_verts.inc"
};

SVECTOR D_actor_207200_8014DD6C[47] = {
#include "assets/actor_207200_model_046A8_normals.inc"
};

u32 D_actor_207200_8014DEE4[377] = {
#include "assets/actor_207200_model_046A8_stream.inc"
};

TmdSource D_actor_207200_8014E4C8 = {
    0,
    2184,
    332,
    3,
    D_actor_207200_8014DC38,
    D_actor_207200_8014DC44,
    D_actor_207200_8014DD6C,
    D_actor_207200_8014DBCC,
    D_actor_207200_8014DEE4,
};

AnimationPackedPose D_actor_207200_8014E4EC[2] = {
#include "assets/actor_207200_animation_04708_bank1.inc"
};

AnimationPackedRotation D_actor_207200_8014E504[1] = {
#include "assets/actor_207200_animation_04708_bank4.inc"
};

AnimationRecord D_actor_207200_8014E508[6] = {
#include "assets/actor_207200_animation_04708_records.inc"
};

u16 D_actor_207200_8014E520[4] = {
#include "assets/actor_207200_animation_04708_indices.inc"
};

AnimationSet D_actor_207200_8014E528 = {
    D_actor_207200_8014E508,
    D_actor_207200_8014E520,
    { NULL, D_actor_207200_8014E4EC, NULL, NULL, D_actor_207200_8014E504, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_207200_8014E550[24] = {
#include "assets/actor_207200_animation_0495C_bank1.inc"
};

AnimationPackedRotation D_actor_207200_8014E670[10] = {
#include "assets/actor_207200_animation_0495C_bank4.inc"
};

AnimationRecord D_actor_207200_8014E698[55] = {
#include "assets/actor_207200_animation_0495C_records.inc"
};

u16 D_actor_207200_8014E774[4] = {
#include "assets/actor_207200_animation_0495C_indices.inc"
};

AnimationSet D_actor_207200_8014E77C = {
    D_actor_207200_8014E698,
    D_actor_207200_8014E774,
    { NULL, D_actor_207200_8014E550, NULL, NULL, D_actor_207200_8014E670, NULL, NULL, NULL },
};

TaskDesc D_actor_207200_8014E7A4 = { { { TASK_BODY_TMD, 96 } }, func_actor_207200_8014AC9C, { .model = &D_actor_207200_8014E4C8 } };

u8 gGlowPodAnimSets[12] = {
    0,
    0,
    0,
    0,
    40,
    229,
    20,
    128,
    124,
    231,
    20,
    128,
};

SVECTOR gGlowPodSparkOffset = { 0, -100, 0, 0 };

SVECTOR gGlowPodHitFxOffset = { 0, 0, 100, 0 };

DamageAttack D_actor_207200_8014E7CC[2] = { { 25, 11 }, { 10, 0 } };

#include "../../shared/glow_pod_spawn_state.inc.c"

#include "../../shared/glow_pod_idle_tick.inc.c"

#include "../../shared/glow_pod_hits.inc.c"

#include "../../shared/glow_pod_inlines.inc.c"

#include "../../shared/glow_pod_death_state.inc.c"

/// The small enemy's state handlers - spawn, live tick and dying tick - which
/// `func_actor_207200_8014AC9C` dispatches through by task state.
static const GpEnemyTaskFuncTable3 D_actor_207200_80149E24 = {
    { glowPodSpawnState, glowPodUpdateState, glowPodDeathState }
};

void func_actor_207200_8014AC9C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_207200_80149E24;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

#include "../../shared/glow_pod_update_state.inc.c"

#include "../../shared/glow_pod_reaction_flags.inc.c"

#include "../../shared/glow_pod_reaction_dispatch.inc.c"

/// Drives animation slots 1 and 2 from the work's animation id `field_28C`.
/// When it differs from the remembered `field_28E` it is remembered, the
/// frame counter restarts and both slots switch to it with a blend of 8;
/// otherwise the counter ticks and both slots advance.
void glowPodAnimate(Task* arg0)
{
    glowEnemy2TickAnim(arg0);
}

/// Colours the actor from the *second* attach coordinate of its model: takes a
/// 0x10-byte `VECTOR` off the scratch stack, fills it with that coordinate's
/// world position and hands it to `Gp_UpdateActorColor` with no blend
/// parameters. `arg0` is the colour target, passed straight through.
void glowPodColour(Enemy* arg0, Task* task)
{
    GfxCoord* coord;
    void**    scratch;
    u8*       head;
    VECTOR*   block;

    coord                          = &task->extra.tmd->coords[1];
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    Gp_UpdateActorColor(arg0, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

#include "../../shared/glow_pod_light_ramp.inc.c"

#include "../../shared/glow_pod_flatten.inc.c"

#include "../../shared/glow_pod_exit.inc.c"
