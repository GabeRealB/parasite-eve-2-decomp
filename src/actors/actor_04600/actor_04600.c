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
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
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

/// The 0x2B0-byte work block of the package's second enemy, allocated by its
/// spawn handler and parked in `Task::work`. It carries three `WorldCollisionBody` bodies:
/// the first points its `context.capsule` at the `GpActorD4Rec` after it, the other
/// two point at their own `WorldCollisionContact` tables.
typedef struct Actor104600Enemy2Work {
    /* 0x000 */ AnimationContext      context;
    /* 0x014 */ AnimationSlot         slots[3];
    /* 0x08C */ byte                  field_8C[0x30]; // pose buffer handed to func_800B3F84
    /* 0x0BC */ MATRIX                field_BC;       // colour matrix, TmdObject::colorMtx
    /* 0x0DC */ MATRIX                field_DC;       // light matrix, TmdObject::lightMtx
    /* 0x0FC */ WorldCollisionBody    field_FC;
    /* 0x11C */ GpActorD4Rec          field_11C;
    /* 0x134 */ WorldCollisionContact field_134[1];
    /* 0x14C */ WorldCollisionBody    field_14C;
    /* 0x16C */ WorldCollisionContact field_16C[1];
    /* 0x184 */ WorldCollisionBody    field_184;
    /* 0x1A4 */ WorldCollisionContact field_1A4[4]; // the enemy's `recs`
    /* 0x204 */ byte                  pad_204[0x50];
    /* 0x254 */ s32                   field_254;    // position restored when the push-back conflicts
    /* 0x258 */ s32                   field_258;
    /* 0x25C */ s32                   field_25C;
    /* 0x260 */ byte                  pad_260[4];
    /* 0x264 */ MATRIX                field_264; // root transform the dying enemy refolds
    /* 0x284 */ byte                  pad_284[2];
    /* 0x286 */ s16                   field_286; // reaction state
    /* 0x288 */ s16                   field_288; // non-zero once the death has unlinked the bodies
    /* 0x28A */ s16                   field_28A; // frames spent in the current state
    /* 0x28C */ s16                   field_28C; // animation id the work is playing
    /* 0x28E */ s16                   field_28E; // id the two helper slots last saw
    /* 0x290 */ s16                   field_290; // frames spent on the current id
    /* 0x292 */ s16                   field_292;
    /* 0x294 */ byte                  pad_294[6];
    /* 0x29A */ s16                   field_29A;
    /* 0x29C */ byte                  pad_29C[4];
    /* 0x2A0 */ s16                   field_2A0; // Y scale folded onto the saved transform
    /* 0x2A2 */ byte                  pad_2A2[2];
    /* 0x2A4 */ s16                   field_2A4; // light blend, 0..0x12
    /* 0x2A6 */ s16                   field_2A6; // non-zero: the blend is rising
    /* 0x2A8 */ s16                   field_2A8; // frames until the next blend turn
    /* 0x2AA */ s16                   field_2AA; // latched by a hit
    /* 0x2AC */ s16                   field_2AC; // placement mode; picks the sound set
    /* 0x2AE */ byte                  pad_2AE[2];
} Actor104600Enemy2Work;
STATIC_ASSERT_SIZEOF(Actor104600Enemy2Work, 0x2B0);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// The first enemy's attack row, packed into its third body's key, and the
/// enemy parameters whose `attacks` name it; `hpMax` seeds the enemy's HP.
extern DamageAttack gBursterAttack;
extern EnemyParams  gBursterParams;

/// The two script arguments the first enemy's death hands to
/// `Gp_SpawnScript18`.
extern u32 gBursterBurstScriptA[];
extern u32 gBursterBurstScriptB[];

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
extern EnemyParams Actor04600_D058B4;

/// The animation data `func_800B3F84` seeds the second enemy's slots from.
extern AnimationSet* Actor04600_D064A8[3];

/// Offsets of the spark and hit effects the second enemy's hit handler spawns.
extern SVECTOR Actor04600_D064B4;
extern SVECTOR Actor04600_D064BC;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

static void Actor04600_Fn03BDC(GpEnemy* arg0, Task* arg1);
static void Actor04600_Fn03CEC(Task* arg0);
static void Actor04600_Fn03D54(Task* task);
static void Actor04600_Fn03E10(Task* arg0);
static void Actor04600_Fn03EC0(GpEnemy* arg0, Task* task);
static void Actor04600_Fn03F30(Task* task);
static void Actor04600_Fn0400C(Task* arg0);
static void Actor04600_Fn04100(Task* task);

extern AnimationSet Actor04600_D05554;
extern AnimationSet Actor04600_D0569C;
extern AnimationSet Actor04600_D05840;
extern TmdSource    Actor04600_D05200;
void                Actor04600_Fn024A4(Task*);
void                Actor04600_Fn02C6C(Task*);
void                Actor04600_Fn03B80(Task*);

DamageAttack gBursterAttack = { 30, 7 };

EnemyParams gBursterParams = { &gBursterAttack, 70, 6, 12, 3, 100, 20, 100, 0 };

u32 gBursterBurstScriptA[3] = {
    0x1010001,
    0x2010000,
    0,
};

u32 gBursterBurstScriptB[3] = {
    0x90000,
    0x10CFFFF,
    0x1063264,
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

TaskDesc Actor04600_D05878 = { TASK_BODY_TMD, 96, Actor04600_Fn024A4, { .model = &Actor04600_D05200 } };

TaskDesc Actor04600_D05884 = { (TASK_BODY_TMD | 0x100), 96, Actor04600_Fn02C6C, { .model = &Actor04600_D05200 } };

AnimationSet* gBursterAnimSets[4] = {
    NULL,
    &Actor04600_D05554,
    &Actor04600_D0569C,
    &Actor04600_D05840,
};

SVECTOR gBursterBurstFxOffset = { 0, -10, 0, 0 };

SVECTOR gBursterCollapseFxOffset = { 0, -300, 0, 0 };

DamageAttack Actor04600_D058B0[1] = { 0 };

EnemyParams Actor04600_D058B4 = { Actor04600_D058B0, 1, 2, 32, 1, 100, 20, 100, 99 };

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

TaskDesc Actor04600_D0649C = { TASK_BODY_TMD, 96, Actor04600_Fn03B80, { .model = &Actor04600_D061C0 } };

AnimationSet* Actor04600_D064A8[3] = {
    NULL,
    &Actor04600_D06220,
    &Actor04600_D06474,
};

SVECTOR Actor04600_D064B4 = { 0, -100, 0, 0 };

SVECTOR Actor04600_D064BC = { 0, 0, 100, 0 };

static __inline__ void Actor04600_TickAnim(Task* task);
static void            Actor04600_Fn02D68(GpEnemy* arg0, Task* arg1);
static void            Actor04600_Fn030A8(Task* arg0);
static void            Actor04600_Fn0346C(Task* arg0);
static __inline__ void _actor04600Enemy2TickAnim(Task* task);
static void            Actor04600_Fn03958(GpEnemy* arg0, Task* arg1);

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
void bursterColour(GpEnemy* arg0, Task* task)
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
    vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
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

/// Spawn handler of the second enemy, entry 0 of `Actor04600_D0003C`. It
/// allocates the 0x2B0-byte work block, points the model's light and colour
/// matrices into it, links the enemy's node, seeds the animation context and
/// resets slots 1 and 2, starts animation 1 with the light blend fully up and
/// rolls the first 0x64..0xA3 frame wait, then links the three bodies with
/// their contact tables. The placement's mode is kept in `field_2AC`; mode 1
/// matching the task's `bodyKind` steps the model's texture page and CLUT
/// row and re-streams it twice. `Actor04600_Fn04100` becomes the exit
/// callback.
static void Actor04600_Fn02D68(GpEnemy* arg0, Task* arg1)
{
    Actor104600Enemy2Work* work;
    TmdObject*             obj;
    GfxCoord*              coord;
    GfxCoord*              part;
    u32                    seed;
    WorldCollisionContact* records1;
    WorldCollisionContact* records2;
    WorldCollisionContact* records3;
    s32                    i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    part  = &coord[1];
    work  = memCalloc(0x2B0U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->field_DC;
    obj->colorMtx       = &work->field_BC;
    arg0->field_4       = &coord[1].coord;
    arg0->field_48      = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                  = part;
    arg0->node.state.parts.flags = 0;
    arg0->bodyPos.vx             = 0;
    arg0->bodyPos.vy             = 0;
    arg0->bodyPos.vz             = 0;
    arg0->param                  = &Actor04600_D058B4;
    arg0->recs                   = work->field_1A4;
    arg0->hp                     = Actor04600_D058B4.hpMax;
    func_800B3F84(&work->context, Actor04600_D064A8, obj, work->field_8C, work->slots);
    i = 1;
    do {
        Gp_AnimResetSlot(&work->context, i, 1);
        i += 1;
    } while (i < 3);
    (Gp_IncStateF0Ref)(0);
    work->field_28C              = 1;
    work->field_28E              = 1;
    work->field_2A6              = 1;
    work->field_2A4              = 0x12;
    arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    seed                         = Gp_LcgState * 5 + 0x71357911;
    work->field_2A8              = ((seed >> 16) & 0x3F) + 0x64;
    Gp_LcgState                  = seed;
    Gp_SetLightMode(arg1->spawnArg2.pointer, 2);
    work->field_11C.end0.vz        = 0x1388;
    work->field_11C.end0Radius     = 0xFA0;
    work->field_11C.end1Radius     = 0x7D0;
    records1                       = work->field_134;
    work->field_11C.recs           = records1;
    work->field_FC.context.capsule = &work->field_11C;
    work->field_FC.coord           = coord;
    work->field_FC.pos.vx          = 0;
    work->field_FC.pos.vy          = 0;
    work->field_FC.pos.vz          = 0;
    work->field_FC.key             = 0;
    work->field_FC.radius          = 0;
    work->field_FC.flags           = WORLD_COLLISION_BODY_CAPSULE;
    Gp_LinkObj(3, &work->field_FC);
    Gp_InitRec18Table(records1, 1, 0);
    work->field_14C.coord            = coord;
    records2                         = work->field_16C;
    work->field_14C.context.contacts = records2;
    work->field_14C.pos.vx           = 0;
    work->field_14C.pos.vy           = 0;
    work->field_14C.pos.vz           = 0;
    work->field_14C.key              = 0;
    work->field_14C.radius           = 0x7D0;
    work->field_14C.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->field_FC.flags             = work->field_FC.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_LinkObj(3, &work->field_14C);
    Gp_InitRec18Table(records2, 1, 0);
    records3                         = work->field_1A4;
    work->field_184.coord            = coord;
    work->field_184.context.contacts = records3;
    work->field_184.pos.vx           = 0;
    work->field_184.pos.vy           = -0xC8;
    work->field_184.pos.vz           = 0;
    work->field_184.key              = 0x3002F;
    work->field_184.radius           = 0xC8;
    work->field_184.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->field_14C.flags            = work->field_14C.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_LinkObj(2, &work->field_184);
    Gp_InitRec18Table(records3, 4, 0);
    work->field_184.flags = work->field_184.flags | (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->field_2AC       = arg0->place->mode;
    if (work->field_2AC == 1 && arg1->bodyKind == work->field_2AC) {
        obj->texturePageOffset++;
        obj->clutRowOffset++;
        if (obj->buffer != NULL) {
            tmdProcessStream(obj);
            tmdProcessStream(obj);
        }
    }
    arg1->exitCallback = Actor04600_Fn04100;
    arg1->state++;
}

/// Idle tick of the second enemy. A 0x10000-class hit on either of its two
/// single-record tables sets `Gp_StateF0.prefix.bytes.field_3`, latches `field_2AA` and selects
/// animation 2; if the light blend is fully up, one sound plays, the blend is
/// turned to fall and a new 0x12..0x31 frame wait is rolled. A latched hit
/// plays a second sound, clears the 0x8000 bit of the first two bodies and arms
/// state 0xF0. Under animation 1 the frame count reaching `field_2A8` turns the
/// blend down (with the first sound) when it is fully up, or back up after a
/// new 0x64..0xA3 frame wait once it has bottomed out; under animation 2 the
/// second sound repeats every 0x28 frames. `field_2AC` picks between two sets
/// of sound ids.
static void Actor04600_Fn030A8(Task* arg0)
{
    Actor104600Enemy2Work* work;
    GfxCoord*              obj;
    s32                    snd;
    s16                    mode;
    s32                    id;
    GpEnemy*               ctx;

    work = (Actor104600Enemy2Work*)arg0->work;
    SCRATCH_PUSH_BYTES(8);
    obj = arg0->extra.tmd->coords;
    if (Gp_CountRec18Hi(work->field_16C, 0x10000) != 0 || Gp_CountRec18Hi(work->field_134, 0x10000) != 0) {
        Gp_StateF0.prefix.bytes.field_3 = 1;
        work->field_2AA                 = 1;
        work->field_28C                 = 2;
        if (work->field_2A6 != 0 && work->field_2A4 == 0x12) {
            if (work->field_2AC != 0) {
                ctx = arg0->spawnArg2.pointer;
                id  = 0x40480007;
                snd = ((ctx->placeKey >> 12) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            } else {
                ctx = arg0->spawnArg2.pointer;
                id  = 0x402E0006;
                snd = ((ctx->placeKey >> 12) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            }
            work->field_290 = 0;
            work->field_2A6 = 0;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_2A8 = ((Gp_LcgState >> 16) & 0x1F) + 0x12;
        }
    }
    if (work->field_2AA != 0) {
        if (work->field_2AC != 0) {
            ctx = arg0->spawnArg2.pointer;
            id  = 0x40480008;
            snd = ((ctx->placeKey >> 12) << 8) | id;
            SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
        } else {
            ctx = arg0->spawnArg2.pointer;
            id  = 0x402E0007;
            snd = ((ctx->placeKey >> 12) << 8) | id;
            SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
        }
        work->field_14C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_FC.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ArmStateF0(1);
    }
    Gp_ClearRec18Occupied(work->field_16C);
    mode = work->field_28C;
    if (mode == 1) {
        work->field_29A = 1;
        work->field_292 = 0;
        if (work->field_290 > work->field_2A8) {
            if (work->field_2A6 != 0 && work->field_2A4 == 0x12) {
                if (work->field_2AC != 0) {
                    ctx = arg0->spawnArg2.pointer;
                    id  = 0x40480007;
                    snd = ((ctx->placeKey >> 12) << 8) | id;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
                } else {
                    ctx = arg0->spawnArg2.pointer;
                    id  = 0x402E0006;
                    snd = ((ctx->placeKey >> 12) << 8) | id;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
                }
                work->field_290 = 0;
                work->field_2A6 = 0;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_2A8 = ((Gp_LcgState >> 16) & 0x1F) + 0x12;
            } else if (*(s32*)&work->field_2A4 == 0) {
                work->field_290 = 0;
                work->field_2A6 = 1;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_2A8 = ((Gp_LcgState >> 16) & 0x3F) + 0x64;
            }
        }
    } else if (mode == 2) {
        work->field_2AA = 0;
        if (work->field_290 >= 0x28) {
            if (work->field_2AC != 0) {
                ctx = arg0->spawnArg2.pointer;
                id  = 0x40480008;
                snd = ((ctx->placeKey >> 12) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            } else {
                ctx = arg0->spawnArg2.pointer;
                id  = 0x402E0007;
                snd = ((ctx->placeKey >> 12) << 8) | id;
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            }
            work->field_290 = 0;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// Per-frame hit handler. Applies the `func_800E0C10` push-back from the four
/// `field_1A4` records to the root coordinate (restoring `field_254` when two
/// records conflict), then walks the records: a kind-1 hit or a kind-2 hit
/// whose distance-scaled damage is nonzero plays the hit sound and sparks and
/// puts the task into its death state after 5 frames; a zero-damage kind-2 hit
/// applies the id's side effect instead.
static void Actor04600_Fn0346C(Task* arg0)
{
    Actor104600Enemy2Work* work;
    ActorDeltaFrame38*     sc;
    ActorDeltaFrame38*     head;
    TmdObject*             obj;
    GfxCoord*              coord;
    GpEnemy*               enemy;
    s32                    i;
    s32                    sndHit;
    s32                    sndHit2;
    u32                    damage;
    s32                    snd;

    work                                    = (Actor104600Enemy2Work*)arg0->work;
    head                                    = SCRATCH_STACK_CURSOR(ActorDeltaFrame38);
    SCRATCH_STACK_CURSOR(ActorDeltaFrame38) = head - 1;
    sc                                      = head - 1;
    obj                                     = arg0->extra.tmd;
    coord                                   = obj->coords;
    enemy                                   = arg0->spawnArg2.pointer;

    switch (func_800E0C10(work->field_1A4, &head[-1].delta, 4, NULL)) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += sc->delta.vx.h.hi;
            coord->coord.t[1] += sc->delta.vy.h.hi;
            coord->coord.t[2] += sc->delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_254;
            coord->coord.t[1] = work->field_258;
            coord->coord.t[2] = work->field_25C;
            break;
    }
    i       = 0;
    sndHit  = 0x40480009;
    sndHit2 = 0x402E0008;
    do {
        switch (work->field_1A4[i].key.value & 0xFFFF0000) {
            case 0x10000:
                if (work->field_2AC != 0) {
                    snd = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | sndHit;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                } else {
                    snd = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | sndHit2;
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
                Gp_SpawnEff(0x60030, arg0->extra.tmd->coords, 0x200, &Actor04600_D064B4);
                Gp_SpawnEff(0x60030, arg0->extra.tmd->coords, 0x200, &Actor04600_D064B4);
                Gp_SpawnEff(0x6009E, arg0->extra.tmd->coords, 0, &Actor04600_D064BC);
                Gp_SpawnPadLerp(0xA, 0x60, 0x60);
                obj->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                work->field_2A0     = 0x500;
                work->field_28C     = 1;
                enemy->hp           = 0;
                work->field_2A6     = 1;
                arg0->killCountdown = 5;
                arg0->state         = 2;
                break;
            case 0x20000:
                sc->delta.vx.w = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                sc->delta.vy.w = Player_Status.coordMtx->t[1] - coord->coord.t[1];
                sc->delta.vz.w = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                damage         = Gp_ComputeDamage(work->field_1A4[i].key.value,
                                                  SquareRoot0(sc->delta.vx.w * sc->delta.vx.w +
                                                              sc->delta.vy.w * sc->delta.vy.w +
                                                              sc->delta.vz.w * sc->delta.vz.w),
                                                  0, 0);
                if (Gp_RollEnemyChance(arg0->spawnArg2.pointer, work->field_1A4[i].key.value, 0) != 0) {
                    damage *= 4;
                }
                func_800E2C78(enemy, work->field_1A4[i].key.value, damage, 0);
                func_800DA6E8(&enemy->node, damage, 0);
                if (damage != 0) {
                    if (work->field_2AC != 0) {
                        snd = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | sndHit;
                        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    } else {
                        snd = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8) | sndHit2;
                        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                    }
                    Gp_SpawnEff(0x60030, arg0->extra.tmd->coords, 0x200, &Actor04600_D064B4);
                    Gp_SpawnEff(0x60030, arg0->extra.tmd->coords, 0x200, &Actor04600_D064B4);
                    Gp_SpawnEff(0x6009E, arg0->extra.tmd->coords, 0, &Actor04600_D064BC);
                    obj->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    work->field_2A0     = 0x1000;
                    work->field_2A6     = 1;
                    work->field_28C     = 1;
                    enemy->hp           = 0;
                    arg0->killCountdown = 5;
                    arg0->state         = 2;
                    break;
                }
                switch ((u16)Gp_GetIdParam0(work->field_1A4[i].key.value)) {
                    case 2:
                    case 9:
                        Gp_SetObjFlag2(enemy, work->field_1A4[i].key.value, 0);
                        break;
                    case 8:
                        work->field_2A6 = 1;
                        break;
                }
                break;
        }
        i++;
    } while (i < 4);
    Gp_ClearRec18Occupied(work->field_1A4);
    SCRATCH_STACK_CURSOR(ActorDeltaFrame38) = SCRATCH_STACK_CURSOR(ActorDeltaFrame38) + 1;
}

/// Rebinds the second enemy's animation id `field_28C` to its two helper
/// slots: a changed id is remembered in `field_28E`, its frame count restarts
/// and both slots switch to it with a blend of 8; otherwise the count ticks and
/// the slots advance.
static __inline__ void _actor04600Enemy2TickAnim(Task* task)
{
    Actor104600Enemy2Work* work = task->work;
    s32                    i;

    if (work->field_28C != work->field_28E) {
        work->field_28E = work->field_28C;
        work->field_290 = 0;
        for (i = 1; i < 3; i++) {
            func_800B4114(&work->context, i, work->field_28C, 0, 8);
        }
    } else {
        work->field_290++;
        for (i = 1; i < 3; i++) {
            Gp_AnimTickIndex(&work->context, i);
        }
    }
}

/// Dying-state tick of the second enemy, under the `Gp_StateF0.field_4` mode byte: 1
/// does nothing and 2 hides the model. Otherwise the root's matrix is saved
/// into `field_264` and refolded with the decaying Y scale. Once `field_288` is
/// set the enemy is destroyed after 0x3D frames; before that, the kill
/// countdown running out releases state 0xF0, sets `field_288` and unlinks the
/// enemy's node and its three bodies, and the two animation slots are rebound
/// or advanced.
static void Actor04600_Fn03958(GpEnemy* arg0, Task* arg1)
{
    Actor104600Enemy2Work* work;
    TmdObject*             obj;
    GfxCoord*              coord;

    work  = arg1->work;
    obj   = arg1->extra.tmd;
    coord = obj->coords;
    switch (Gp_StateF0.field_4) {
        case 0:
            break;
        case 1:
            return;
        case 2:
            obj->flags                  |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    if (work->field_288 != 0) {
        work->field_264 = coord->coord;
        Actor04600_Fn0400C(arg1);
        work->field_28A++;
        if (work->field_28A >= 0x3D) {
            Gp_DestroyEnemy(arg0, arg1);
        }
        return;
    }
    work->field_264 = coord->coord;
    Actor04600_Fn0400C(arg1);
    arg1->killCountdown--;
    if (arg1->killCountdown <= 0) {
        Gp_ReleaseStateF0Add(arg1, 0x2F);
        work->field_288 = 1;
        work->field_28A = 0;
        arg0->recs      = 0;
        Gp_UnlinkNode(&arg0->node);
        Gp_UnlinkObj(&work->field_14C);
        Gp_UnlinkObj(&work->field_FC);
        Gp_UnlinkObj(&work->field_184);
    }
    _actor04600Enemy2TickAnim(arg1);
}

/// Task states of the second enemy as `Actor04600_Fn03B80` dispatches them:
/// spawn, per-frame update and the dying tick.
static const GpEnemyTaskFuncTable3 Actor04600_D0003C = {
    { Actor04600_Fn02D68, Actor04600_Fn03BDC, Actor04600_Fn03958 },
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

/// Per-frame handler of the second enemy under the `Gp_StateF0.field_4` mode byte:
/// mode 1 runs only the tail, mode 2 hides the model, sets the node flag and
/// returns, mode 0 clears the node flag before falling into the update, and
/// any other mode updates directly. The update raises the root's Y translation
/// by 0x80, runs the reaction dispatch, the light blend, the flag reactions,
/// the hit handler and the animation, clears the first two parts' flags and
/// recomputes the second one's matrix; the tail colours the enemy.
static void Actor04600_Fn03BDC(GpEnemy* arg0, Task* arg1)
{
    s32 state;
    s32 one;

    state = Gp_StateF0.field_4;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    arg0->node.state.parts.flags = 0;
    goto default_body;
case2:
    arg1->extra.tmd->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    arg0->node.state.parts.flags = one;
    return;
default_body:
    arg1->extra.tmd->coords[0].coord.t[1] += 0x80;
    Actor04600_Fn03D54(arg1);
    Actor04600_Fn03F30(arg1);
    Actor04600_Fn03CEC(arg1);
    Actor04600_Fn0346C(arg1);
    Actor04600_Fn03E10(arg1);
    arg1->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&arg1->extra.tmd->coords[1]);
case1:
    Actor04600_Fn03EC0(arg0, arg1);
}

/// Consumes the pending bits of the second enemy's `reactionFlags`: bit 0x1 is
/// dropped on its own, bit 0x2 puts the work into reaction state 3 with its
/// frame count cleared, and bits 0xC are dropped last, after re-reading the
/// byte.
static void Actor04600_Fn03CEC(Task* arg0)
{
    GpEnemy*               enemy;
    Actor104600Enemy2Work* work;
    u8                     flags;

    enemy = (GpEnemy*)arg0->spawnArg2.pointer;
    work  = arg0->work;
    flags = enemy->reactionFlags;
    if (flags != 0) {
        if (flags & 1) {
            enemy->reactionFlags = flags & 0xFE;
        }
        if (enemy->reactionFlags & 2) {
            enemy->reactionFlags = enemy->reactionFlags & 0xFD;
            work->field_286      = 3;
            work->field_28A      = 0;
        }
        flags = enemy->reactionFlags;
        if (flags & 0xC) {
            enemy->reactionFlags = flags & 0xF3;
        }
    }
}

/// Per-frame dispatch on the second enemy's reaction state `field_286`: state
/// 0 runs the idle tick and state 2 does nothing. State 3 clears `field_292`
/// and turns the light blend down, resets the remembered animation id to 1 and
/// the counters every fourth frame, and returns to state 0 once
/// `Gp_TickObjFlag2` reports the reaction over.
static void Actor04600_Fn03D54(Task* task)
{
    Actor104600Enemy2Work* work;

    work = task->work;
    switch (work->field_286) {
        case 0:
            Actor04600_Fn030A8(task);
            break;
        case 2:
            break;
        case 3:
            work->field_292 = 0;
            work->field_2A6 = 0;
            work->field_28A = work->field_28A + 1;
            if (work->field_28A >= 4) {
                work->field_28E = 1;
                work->field_290 = 0;
                work->field_28A = 0;
            }
            if (Gp_TickObjFlag2(task->spawnArg2.pointer) != 0) {
                work->field_286 = 0;
            }
            break;
    }
}

/// The second enemy's animation rebind, `_actor04600Enemy2TickAnim`, as an
/// out-of-line function.
static void Actor04600_Fn03E10(Task* arg0)
{
    _actor04600Enemy2TickAnim(arg0);
}

/// Colours the second enemy from the world position of its model's second
/// coordinate, staged in a `VECTOR` taken off the scratch stack.
static void Actor04600_Fn03EC0(GpEnemy* arg0, Task* task)
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

/// Ramps the second enemy's light blend `field_2A4` up or down as `field_2A6`
/// says. Rising, the first frame switches the enemy to light mode 2 and the
/// blend saturates at 0x12, where the enemy's node flag and the model's 0x80
/// bit are set. Falling, leaving 0x12 clears the node flag and returns to light
/// mode 0, and the blend bottoms out at 0 with the model bits cleared.
static void Actor04600_Fn03F30(Task* task)
{
    Actor104600Enemy2Work* work;
    GpEnemy*               enemy;
    TmdObject*             obj;

    work  = (Actor104600Enemy2Work*)task->work;
    enemy = (GpEnemy*)task->spawnArg2.pointer;
    obj   = task->extra.tmd;

    if (work->field_2A6 != 0) {
        if (work->field_2A4 == 0) {
            work->field_2A4++;
            obj->flags = TMD_OBJECT_SEMI_TRANS;
            Gp_SetLightMode(task->spawnArg2.pointer, 2);
        } else {
            work->field_2A4++;
            if (work->field_2A4 >= 0x12) {
                work->field_2A4               = 0x12;
                enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                obj->flags                    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
        }
    } else {
        if (work->field_2A4 == 0x12) {
            work->field_2A4--;
            enemy->node.state.parts.flags = 0;
            obj->flags                    = TMD_OBJECT_SEMI_TRANS;
            Gp_SetLightMode(task->spawnArg2.pointer, 0);
        } else {
            work->field_2A4--;
            if (work->field_2A4 <= 0) {
                work->field_2A4 = 0;
                obj->flags      = 0;
            }
        }
    }
}

/// Rebuilds the second enemy's root coordinate from the matrix saved in
/// `field_264`, scaled along Y by `field_2A0`, which decays by 0x50 a frame
/// while above 0x200. The scaling matrix and its vector are staged in 0x30
/// bytes of the scratch stack; the node's `composeStamp` is cleared so the next
/// `Gp_UpdateCoord` recomputes it.
static void Actor04600_Fn0400C(Task* arg0)
{
    GfxCoord*              coord;
    ActorScaleScratch*     head;
    ActorScaleScratch*     scratch;
    Actor104600Enemy2Work* work;

    head                                    = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = arg0->work;
    scratch                                 = head - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    coord                                   = arg0->extra.tmd->coords;
    if (work->field_2A0 >= 0x201) {
        work->field_2A0 = (u16)work->field_2A0 - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_2A0;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_264;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}

/// Exit callback of the second enemy: detaches the enemy's contact records,
/// unlinks its node and the work's three bodies, then runs the common enemy
/// task exit.
static void Actor04600_Fn04100(Task* task)
{
    Actor104600Enemy2Work* work;
    GpEnemy*               enemy;

    enemy = task->spawnArg2.pointer;
    work  = (Actor104600Enemy2Work*)task->work;

    enemy->recs = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->field_14C);
    Gp_UnlinkObj(&work->field_FC);
    Gp_UnlinkObj(&work->field_184);
    Gp_EnemyTaskExit(task);
}
