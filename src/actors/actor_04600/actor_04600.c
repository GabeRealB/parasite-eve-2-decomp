#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "actors/actors_shared_801673f8.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
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
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/shelter_b3_dumping_hole.h"

#include "rooms/shelter_b3_garbage_incinerator.h"
#include "../../shared/sucklerceph.h"
#include "../../shared/skull_stalker.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// The first enemy's attack row, packed into its third body's key, and the
/// enemy parameters whose `attacks` name it; `hpMax` seeds the enemy's HP.
extern DamageAttack gSucklercephAttack;
extern EnemyParams  gSucklercephParams;

/// The two script arguments the first enemy's death hands to
/// `Gp_SpawnScript18`.
extern PadScriptCmd              gSucklercephBurstScriptA[];
extern PadScriptVibrationSegment gSucklercephBurstScriptB[];

/// Message table the dropping first enemy's spawn parks in `Task::msgTable`.
// Typed callback views for the task message dispatcher.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, ActorCommand* request);
    } handler;
} Actor04600RecoveredMsgEntry;
STATIC_ASSERT_SIZEOF(Actor04600RecoveredMsgEntry, 8);

extern Actor04600RecoveredMsgEntry gSucklercephDropMsgTable[2];

/// The animation data `func_800B3F84` seeds the first enemy's slots from.
extern AnimationSet* gSucklercephAnimSets[4];

/// Offset of the 0x60030 effect the first enemy's death spawns.
extern SVECTOR gSucklercephBurstFxOffset;

/// Offset of the 0x60080 effect the collapsing first enemy spawns.
extern SVECTOR gSucklercephCollapseFxOffset;

/// The second enemy's record; `hpMax` seeds its HP.
extern EnemyParams gSkullStalkerParams;

/// The animation data `func_800B3F84` seeds the second enemy's slots from.
extern AnimationSet* gSkullStalkerAnimSets[3];

/// Offsets of the spark and hit effects the second enemy's hit handler spawns.
extern SVECTOR gSkullStalkerSparkOffset;
extern SVECTOR gSkullStalkerHitFxOffset;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

extern AnimationSet Actor04600_D05554;
extern AnimationSet Actor04600_D0569C;
extern AnimationSet Actor04600_D05840;
extern TmdSource    Actor04600_D05200;
void                Actor04600_Fn024A4(Task*);
void                Actor04600_Fn02C6C(Task*);
void                Actor04600_Fn03B80(Task*);

DamageAttack gSucklercephAttack = { 30, 7 };

EnemyParams gSucklercephParams = { &gSucklercephAttack, 70, 6, 12, 3, 100, 20, 100, 0 };

PadScriptCmd gSucklercephBurstScriptA[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
};

PadScriptVibrationSegment gSucklercephBurstScriptB[3] = {
    { 0, 0, 9, 0 },
    { 255, 255, 12, 1 },
    { 100, 50, 6, 1 },
};

TmdBone Actor04600_D04188[3] = {
#include "assets/actor_104600_model_05200_skeleton.inc"
};

u32 Actor04600_D041F4[3] = {
#include "assets/actor_104600_model_05200_partVerts.inc"
};

SVECTOR Actor04600_D04200[68] = {
#include "assets/actor_104600_model_05200_verts.inc"
};

SVECTOR Actor04600_D04420[68] = {
#include "assets/actor_104600_model_05200_normals.inc"
};

u32 Actor04600_D04640[752] = {
#include "assets/actor_104600_model_05200_stream.inc"
};

TmdSource Actor04600_D05200 = {
    0,
    4516,
    584,
    3,
    Actor04600_D041F4,
    Actor04600_D04200,
    Actor04600_D04420,
    Actor04600_D04188,
    Actor04600_D04640,
};

AnimationPackedPose Actor04600_D05224[34] = {
#include "assets/actor_104600_animation_05554_bank1.inc"
};

AnimationPackedRotation Actor04600_D053BC[26] = {
#include "assets/actor_104600_animation_05554_bank4.inc"
};

AnimationRecord Actor04600_D05424[74] = {
#include "assets/actor_104600_animation_05554_records.inc"
};

u16 Actor04600_D0554C[4] = {
#include "assets/actor_104600_animation_05554_indices.inc"
};

AnimationSet Actor04600_D05554 = {
    Actor04600_D05424,
    Actor04600_D0554C,
    { NULL, Actor04600_D05224, NULL, NULL, Actor04600_D053BC, NULL, NULL, NULL },
};

AnimationPackedPose Actor04600_D0557C[11] = {
#include "assets/actor_104600_animation_0569C_bank1.inc"
};

AnimationPackedRotation Actor04600_D05600[9] = {
#include "assets/actor_104600_animation_0569C_bank4.inc"
};

AnimationRecord Actor04600_D05624[28] = {
#include "assets/actor_104600_animation_0569C_records.inc"
};

u16 Actor04600_D05694[4] = {
#include "assets/actor_104600_animation_0569C_indices.inc"
};

AnimationSet Actor04600_D0569C = {
    Actor04600_D05624,
    Actor04600_D05694,
    { NULL, Actor04600_D0557C, NULL, NULL, Actor04600_D05600, NULL, NULL, NULL },
};

AnimationPackedPose Actor04600_D056C4[15] = {
#include "assets/actor_104600_animation_05840_bank1.inc"
};

AnimationPackedRotation Actor04600_D05778[12] = {
#include "assets/actor_104600_animation_05840_bank4.inc"
};

AnimationRecord Actor04600_D057A8[36] = {
#include "assets/actor_104600_animation_05840_records.inc"
};

u16 Actor04600_D05838[4] = {
#include "assets/actor_104600_animation_05840_indices.inc"
};

AnimationSet Actor04600_D05840 = {
    Actor04600_D057A8,
    Actor04600_D05838,
    { NULL, Actor04600_D056C4, NULL, NULL, Actor04600_D05778, NULL, NULL, NULL },
};

Actor04600RecoveredMsgEntry gSucklercephDropMsgTable[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call0 = sucklercephMessage } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc Actor04600_D05878 = { { { TASK_BODY_TMD, 96 } }, Actor04600_Fn024A4, { .model = &Actor04600_D05200 } };

TaskDesc Actor04600_D05884 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_MODEL_BUFFER), 96 } }, Actor04600_Fn02C6C, { .model = &Actor04600_D05200 } };

AnimationSet* gSucklercephAnimSets[4] = {
    NULL,
    &Actor04600_D05554,
    &Actor04600_D0569C,
    &Actor04600_D05840,
};

SVECTOR gSucklercephBurstFxOffset = { 0, -10, 0, 0 };

SVECTOR gSucklercephCollapseFxOffset = { 0, -300, 0, 0 };

DamageAttack Actor04600_D058B0[1] = { 0 };

EnemyParams gSkullStalkerParams = { Actor04600_D058B0, 1, 2, 32, 1, 100, 20, 100, 99 };

TmdBone Actor04600_D058C4[3] = {
#include "assets/actor_104600_model_061C0_skeleton.inc"
};

u32 Actor04600_D05930[3] = {
#include "assets/actor_104600_model_061C0_partVerts.inc"
};

SVECTOR Actor04600_D0593C[37] = {
#include "assets/actor_104600_model_061C0_verts.inc"
};

SVECTOR Actor04600_D05A64[47] = {
#include "assets/actor_104600_model_061C0_normals.inc"
};

u32 Actor04600_D05BDC[377] = {
#include "assets/actor_104600_model_061C0_stream.inc"
};

TmdSource Actor04600_D061C0 = {
    0,
    2184,
    332,
    3,
    Actor04600_D05930,
    Actor04600_D0593C,
    Actor04600_D05A64,
    Actor04600_D058C4,
    Actor04600_D05BDC,
};

AnimationPackedPose Actor04600_D061E4[2] = {
#include "assets/actor_104600_animation_06220_bank1.inc"
};

AnimationPackedRotation Actor04600_D061FC[1] = {
#include "assets/actor_104600_animation_06220_bank4.inc"
};

AnimationRecord Actor04600_D06200[6] = {
#include "assets/actor_104600_animation_06220_records.inc"
};

u16 Actor04600_D06218[4] = {
#include "assets/actor_104600_animation_06220_indices.inc"
};

AnimationSet Actor04600_D06220 = {
    Actor04600_D06200,
    Actor04600_D06218,
    { NULL, Actor04600_D061E4, NULL, NULL, Actor04600_D061FC, NULL, NULL, NULL },
};

AnimationPackedPose Actor04600_D06248[24] = {
#include "assets/actor_104600_animation_06474_bank1.inc"
};

AnimationPackedRotation Actor04600_D06368[10] = {
#include "assets/actor_104600_animation_06474_bank4.inc"
};

AnimationRecord Actor04600_D06390[55] = {
#include "assets/actor_104600_animation_06474_records.inc"
};

u16 Actor04600_D0646C[4] = {
#include "assets/actor_104600_animation_06474_indices.inc"
};

AnimationSet Actor04600_D06474 = {
    Actor04600_D06390,
    Actor04600_D0646C,
    { NULL, Actor04600_D06248, NULL, NULL, Actor04600_D06368, NULL, NULL, NULL },
};

TaskDesc Actor04600_D0649C = { { { TASK_BODY_TMD, 96 } }, Actor04600_Fn03B80, { .model = &Actor04600_D061C0 } };

AnimationSet* gSkullStalkerAnimSets[3] = {
    NULL,
    &Actor04600_D06220,
    &Actor04600_D06474,
};

SVECTOR gSkullStalkerSparkOffset = { 0, -100, 0, 0 };

SVECTOR gSkullStalkerHitFxOffset = { 0, 0, 100, 0 };

#include "../../shared/sucklerceph_inlines.inc.c"

#include "../../shared/sucklerceph_spawn_state.inc.c"

/// Task states of the first enemy as `Actor04600_Fn024A4` dispatches them:
/// spawn, per-frame update and death.
static const GpEnemyTaskFuncTable3 Actor04600_D00004 = {
    { sucklercephSpawnState, sucklercephUpdateState, sucklercephDeathState },
};

/// Task states of the dropping first enemy as `Actor04600_Fn02C6C` dispatches
/// them: the same update and death after a spawn that parks the enemy hidden,
/// and a fourth state for its drop into place.
static const GpEnemyTaskFuncTable4 Actor04600_D00010 = {
    { sucklercephDropSpawnState, sucklercephUpdateState, sucklercephDeathState, sucklercephDropState },
};

#include "../../shared/sucklerceph_reaction_dispatch.inc.c"

#include "../../shared/sucklerceph_dormant_tick.inc.c"

#include "../../shared/sucklerceph_awake_tick.inc.c"

#include "../../shared/sucklerceph_contacts.inc.c"

#include "../../shared/sucklerceph_take_damage.inc.c"

#include "../../shared/sucklerceph_turn_to_player.inc.c"

#include "../../shared/sucklerceph_death_state.inc.c"

#include "../../shared/sucklerceph_kill.inc.c"

#include "../../shared/sucklerceph_drop_spawn_state.inc.c"

#include "../../shared/sucklerceph_drop_state.inc.c"

#include "../../shared/sucklerceph_drop_collide.inc.c"

#include "../../shared/sucklerceph_message.inc.c"

/// Task handler of the first enemy: runs the entry of `Actor04600_D00004` for
/// the task's state with the enemy and the task, from a copy of the table on
/// the stack.
void Actor04600_Fn024A4(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor04600_D00004;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

#include "../../shared/sucklerceph_update_state.inc.c"

#include "../../shared/sucklerceph_reaction_flags.inc.c"

#include "../../shared/sucklerceph_step.inc.c"

/// The first enemy's animation rebind, `sucklercephTickAnim`, as an
/// out-of-line function.
void sucklercephAnimate(Task* arg0)
{
    sucklercephTickAnim(arg0);
}

/// Colours the first enemy from the world position of its model's second
/// coordinate, staged in a `VECTOR` taken off the scratch stack.
void sucklercephColour(Enemy* arg0, Task* task)
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

/// Draws the first enemy's ground shadow under the model root, at the world
/// translation of the root part staged in a `VECTOR3` on the scratch stack.
void sucklercephDrawShadow(Task* task)
{
    GfxCoord* coord;
    VECTOR3*  vec;

    coord   = task->extra.tmd->coords;
    vec     = (VECTOR3*)SCRATCH_STACK_RESERVE_BYTES(0x18);
    vec->vx = coord->workm.t[0];
    vec->vy = coord->workm.t[1];
    vec->vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(vec, 0x1C0, 0);
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

#include "../../shared/sucklerceph_scale_part.inc.c"

#include "../../shared/sucklerceph_flatten.inc.c"

#include "../../shared/sucklerceph_exit.inc.c"

/// Task handler of the dropping first enemy: runs the entry of
/// `Actor04600_D00010` for the task's state with the enemy and the task, from
/// a copy of the table on the stack.
void Actor04600_Fn02C6C(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = Actor04600_D00010;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

#include "../../shared/sucklerceph_fall_step.inc.c"

#include "../../shared/skull_stalker_spawn_state.inc.c"

#include "../../shared/skull_stalker_idle_tick.inc.c"

#include "../../shared/skull_stalker_hits.inc.c"

#include "../../shared/skull_stalker_inlines.inc.c"

#include "../../shared/skull_stalker_death_state.inc.c"

/// Task states of the second enemy as `Actor04600_Fn03B80` dispatches them:
/// spawn, per-frame update and the dying tick.
static const GpEnemyTaskFuncTable3 Actor04600_D0003C = {
    { skullStalkerSpawnState, skullStalkerUpdateState, skullStalkerDeathState },
};

/// Task handler of the second enemy: runs the entry of `Actor04600_D0003C` for
/// the task's state with the enemy and the task, from a copy of the table on
/// the stack.
void Actor04600_Fn03B80(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor04600_D0003C;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

#include "../../shared/skull_stalker_update_state.inc.c"

#include "../../shared/skull_stalker_reaction_flags.inc.c"

#include "../../shared/skull_stalker_reaction_dispatch.inc.c"

/// The second enemy's animation rebind, `skullStalkerTickAnim`, as an
/// out-of-line function.
void skullStalkerAnimate(Task* arg0)
{
    skullStalkerTickAnim(arg0);
}

/// Colours the second enemy from the world position of its model's second
/// coordinate, staged in a `VECTOR` taken off the scratch stack.
void skullStalkerColour(Enemy* arg0, Task* task)
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

#include "../../shared/skull_stalker_light_ramp.inc.c"

#include "../../shared/skull_stalker_flatten.inc.c"

#include "../../shared/skull_stalker_exit.inc.c"
