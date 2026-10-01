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
#include "../../shared/burster.h"
#include "../../shared/glow_pod.h"

/// The 0x2E4-byte work block of the package's first enemy, which both of its
/// spawn handlers allocate with `memCalloc` and park in `Task::work`. After the
/// animation context and its three slots come the colour and light matrices the
/// model is pointed at, then four `WorldCollisionBody` bodies, each followed by the
/// `WorldCollisionContact` table its `context.contacts` names.
typedef struct Actor104600Work {
    /* 0x000 */ AnimationContext      context;
    /* 0x014 */ AnimationSlot         slots[3];
    /* 0x08C */ byte                  field_8C[0x30]; // pose buffer handed to func_800B3F84
    /* 0x0BC */ MATRIX                field_BC;       // colour matrix, TmdObject::colorMtx
    /* 0x0DC */ MATRIX                field_DC;       // light matrix, TmdObject::lightMtx
    /* 0x0FC */ WorldCollisionBody    objFC;
    /* 0x11C */ WorldCollisionContact rec11C;
    /* 0x134 */ WorldCollisionBody    obj134;
    /* 0x154 */ WorldCollisionContact rec154[4]; // the body's contact table; also the enemy's `recs`
    /* 0x1B4 */ WorldCollisionBody    obj1B4;
    /* 0x1D4 */ WorldCollisionContact rec1D4;
    /* 0x1EC */ WorldCollisionBody    obj1EC;
    /* 0x20C */ WorldCollisionContact rec20C;
    /* 0x224 */ byte                  pad_224[0x50];
    /* 0x274 */ VECTOR3               field_274; // root translation before the last step
    /* 0x280 */ byte                  pad_280[4];
    /* 0x284 */ EffectSpawnArg        field_284; // hit-effect coordinate and parameters
    /* 0x28C */ MATRIX                field_28C; // root transform saved when the enemy dies
    /* 0x2AC */ s32                   field_2AC; // scale factor of the model's second part
    /* 0x2B0 */ s16                   field_2B0; // heading, stepped 0x20 a frame toward the player
    /* 0x2B2 */ s16                   field_2B2; // reaction state the per-frame dispatch switches on
    /* 0x2B4 */ s16                   field_2B4; // phase of the death sequence
    /* 0x2B6 */ s16                   field_2B6; // frames spent in the death phase
    /* 0x2B8 */ s16                   field_2B8; // animation id the work is playing
    /* 0x2BA */ s16                   field_2BA; // id the two helper slots last saw
    /* 0x2BC */ u16                   field_2BC; // frames spent on the current id
    /* 0x2BE */ s16                   field_2BE; // step length along the facing
    /* 0x2C0 */ byte                  pad_2C0[6];
    /* 0x2C6 */ s16                   field_2C6;
    /* 0x2C8 */ s16                   field_2C8; // live stage: 1 alive, 2 dying
    /* 0x2CA */ s16                   field_2CA; // Y scale folded onto the saved transform
    /* 0x2CC */ s16                   field_2CC;
    /* 0x2CE */ s16                   field_2CE; // remaining hit cooldown
    /* 0x2D0 */ u16                   field_2D0; // frames until the next idle sound
    /* 0x2D2 */ s16                   field_2D2; // non-zero: the animation rebind is suppressed
    /* 0x2D4 */ u16                   field_2D4; // frame or event counter of the dying stages
    /* 0x2D6 */ s16                   field_2D6; // spawn arg's low half; picks the sound set
    /* 0x2D8 */ s16                   field_2D8; // latched once the dormant enemy is touched
    /* 0x2DA */ s16                   field_2DA; // non-zero: the death spawns a final effect
    /* 0x2DC */ s16                   field_2DC; // spawn arg's high half
    /* 0x2DE */ s16                   field_2DE; // fall speed while dropping into place
    /* 0x2E0 */ s16                   field_2E0; // non-zero once the drop has hit something
    /* 0x2E2 */ s16                   field_2E2; // non-zero: the drop has been armed
} Actor104600Work;
STATIC_ASSERT_SIZEOF(Actor104600Work, 0x2E4);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// The first enemy's attack row, packed into its third body's key, and the
/// enemy parameters whose `attacks` name it; `hpMax` seeds the enemy's HP.
extern DamageAttack gBursterAttack;
extern EnemyParams  gBursterParams;

/// The two script arguments the first enemy's death hands to
/// `Gp_SpawnScript18`.
extern PadScriptCmd              gBursterBurstScriptA[];
extern PadScriptVibrationSegment gBursterBurstScriptB[];

/// Message table the dropping first enemy's spawn parks in `Task::msgTable`.
// Typed callback views for the task message dispatcher.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(Task*, s32, ActorCommand* request);
    } handler;
} Actor04600RecoveredMsgEntry;
STATIC_ASSERT_SIZEOF(Actor04600RecoveredMsgEntry, 8);

extern Actor04600RecoveredMsgEntry gBursterDropMsgTable[2];

/// The animation data `func_800B3F84` seeds the first enemy's slots from.
extern AnimationSet* gBursterAnimSets[4];

/// Offset of the 0x60030 effect the first enemy's death spawns.
extern SVECTOR gBursterBurstFxOffset;

/// Offset of the 0x60080 effect the collapsing first enemy spawns.
extern SVECTOR gBursterCollapseFxOffset;

/// The second enemy's record; `hpMax` seeds its HP.
extern EnemyParams gGlowPodParams;

/// The animation data `func_800B3F84` seeds the second enemy's slots from.
extern AnimationSet* gGlowPodAnimSets[3];

/// Offsets of the spark and hit effects the second enemy's hit handler spawns.
extern SVECTOR gGlowPodSparkOffset;
extern SVECTOR gGlowPodHitFxOffset;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

extern AnimationSet Actor04600_D05554;
extern AnimationSet Actor04600_D0569C;
extern AnimationSet Actor04600_D05840;
extern TmdSource    Actor04600_D05200;
void                Actor04600_Fn024A4(Task*);
void                Actor04600_Fn02C6C(Task*);
void                Actor04600_Fn03B80(Task*);

DamageAttack gBursterAttack = { 30, 7 };

EnemyParams gBursterParams = { &gBursterAttack, 70, 6, 12, 3, 100, 20, 100, 0 };

PadScriptCmd gBursterBurstScriptA[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
};

PadScriptVibrationSegment gBursterBurstScriptB[3] = {
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

Actor04600RecoveredMsgEntry gBursterDropMsgTable[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call0 = bursterMessage } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc Actor04600_D05878 = { { { TASK_BODY_TMD, 96 } }, Actor04600_Fn024A4, { .model = &Actor04600_D05200 } };

TaskDesc Actor04600_D05884 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_MODEL_BUFFER), 96 } }, Actor04600_Fn02C6C, { .model = &Actor04600_D05200 } };

AnimationSet* gBursterAnimSets[4] = {
    NULL,
    &Actor04600_D05554,
    &Actor04600_D0569C,
    &Actor04600_D05840,
};

SVECTOR gBursterBurstFxOffset = { 0, -10, 0, 0 };

SVECTOR gBursterCollapseFxOffset = { 0, -300, 0, 0 };

DamageAttack Actor04600_D058B0[1] = { 0 };

EnemyParams gGlowPodParams = { Actor04600_D058B0, 1, 2, 32, 1, 100, 20, 100, 99 };

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

AnimationSet* gGlowPodAnimSets[3] = {
    NULL,
    &Actor04600_D06220,
    &Actor04600_D06474,
};

SVECTOR gGlowPodSparkOffset = { 0, -100, 0, 0 };

SVECTOR gGlowPodHitFxOffset = { 0, 0, 100, 0 };

static __inline__ void Actor04600_TickAnim(Task* task);

/// Rebinds the first enemy's animation id to its two helper slots unless
/// `field_2D2` suppresses it: a changed id is remembered, its frame count
/// restarts and both slots switch to it; otherwise the count ticks and the
/// slots advance.
static __inline__ void Actor04600_TickAnim(Task* task)
{
    Actor104600Work* work = (Actor104600Work*)task->work;
    s32              i;
    if (work->field_2D2 == 0) {
        if (work->field_2B8 != work->field_2BA) {
            work->field_2BA = work->field_2B8;
            work->field_2BC = 0;
            for (i = 1; i < 3; i++) {
                func_800B4114(&work->context, i, work->field_2B8, 0, 0);
            }
        } else {
            work->field_2BC++;
            for (i = 1; i < 3; i++) {
                Gp_AnimTickIndex(&work->context, i);
            }
        }
    }
}

#include "../../shared/burster_spawn_state.inc.c"

/// Task states of the first enemy as `Actor04600_Fn024A4` dispatches them:
/// spawn, per-frame update and death.
static const GpEnemyTaskFuncTable3 Actor04600_D00004 = {
    { bursterSpawnState, bursterUpdateState, bursterDeathState },
};

/// Task states of the dropping first enemy as `Actor04600_Fn02C6C` dispatches
/// them: the same update and death after a spawn that parks the enemy hidden,
/// and a fourth state for its drop into place.
static const GpEnemyTaskFuncTable4 Actor04600_D00010 = {
    { bursterDropSpawnState, bursterUpdateState, bursterDeathState, bursterDropState },
};

#include "../../shared/burster_reaction_dispatch.inc.c"

#include "../../shared/burster_dormant_tick.inc.c"

#include "../../shared/burster_awake_tick.inc.c"

#include "../../shared/burster_contacts.inc.c"

#include "../../shared/burster_take_damage.inc.c"

#include "../../shared/burster_turn_to_player.inc.c"

#include "../../shared/burster_death_state.inc.c"

#include "../../shared/burster_kill.inc.c"

#include "../../shared/burster_drop_spawn_state.inc.c"

#include "../../shared/burster_drop_state.inc.c"

#include "../../shared/burster_drop_collide.inc.c"

#include "../../shared/burster_message.inc.c"

/// Task handler of the first enemy: runs the entry of `Actor04600_D00004` for
/// the task's state with the enemy and the task, from a copy of the table on
/// the stack.
void Actor04600_Fn024A4(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor04600_D00004;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

#include "../../shared/burster_update_state.inc.c"

#include "../../shared/burster_reaction_flags.inc.c"

#include "../../shared/burster_step.inc.c"

/// The first enemy's animation rebind, `Actor04600_TickAnim`, as an
/// out-of-line function.
void bursterAnimate(Task* arg0)
{
    Actor04600_TickAnim(arg0);
}

/// Colours the first enemy from the world position of its model's second
/// coordinate, staged in a `VECTOR` taken off the scratch stack.
void bursterColour(Enemy* arg0, Task* task)
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
void bursterDrawShadow(Task* task)
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

#include "../../shared/burster_scale_part.inc.c"

#include "../../shared/burster_flatten.inc.c"

#include "../../shared/burster_exit.inc.c"

/// Task handler of the dropping first enemy: runs the entry of
/// `Actor04600_D00010` for the task's state with the enemy and the task, from
/// a copy of the table on the stack.
void Actor04600_Fn02C6C(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;

    sp = Actor04600_D00010;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

#include "../../shared/burster_fall_step.inc.c"

#include "../../shared/glow_pod_spawn_state.inc.c"

#include "../../shared/glow_pod_idle_tick.inc.c"

#include "../../shared/glow_pod_hits.inc.c"

#include "../../shared/glow_pod_inlines.inc.c"

#include "../../shared/glow_pod_death_state.inc.c"

/// Task states of the second enemy as `Actor04600_Fn03B80` dispatches them:
/// spawn, per-frame update and the dying tick.
static const GpEnemyTaskFuncTable3 Actor04600_D0003C = {
    { glowPodSpawnState, glowPodUpdateState, glowPodDeathState },
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

#include "../../shared/glow_pod_update_state.inc.c"

#include "../../shared/glow_pod_reaction_flags.inc.c"

#include "../../shared/glow_pod_reaction_dispatch.inc.c"

/// The second enemy's animation rebind, `glowEnemy2TickAnim`, as an
/// out-of-line function.
void glowPodAnimate(Task* arg0)
{
    glowEnemy2TickAnim(arg0);
}

/// Colours the second enemy from the world position of its model's second
/// coordinate, staged in a `VECTOR` taken off the scratch stack.
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
