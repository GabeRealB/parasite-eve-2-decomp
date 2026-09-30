#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/damage.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
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

#include "rooms/neo_ark_power_plant_1.h"

#include "rooms/neo_ark_power_plant_2.h"
#include "../../shared/model_placement.h"
#include "../../shared/power_plant_pod.h"

/// Spawn offsets at `D_actor_105400_80133A30`: the spawn reads only the second
/// vector, `field_8`, into the enemy's body position and the second list
/// node's position.
typedef struct Actor05400Pose {
    /* 0x0 */ SVECTOR field_0;
    /* 0x8 */ SVECTOR field_8;
} Actor05400Pose;
STATIC_ASSERT_SIZEOF(Actor05400Pose, 0x10);

extern EnemyParams        gPodWeakPointParams;
extern Actor05300SpawnPos gPodWeakPointPos[2];
extern Actor05300Clip     gPodIdlePulse[];
extern Actor05300SndRow   gPodViewSound[];
extern u32                gPodPulseSoundId;
extern s32                gPodSoundIds[3];
extern SVECTOR            gPodHitEffectOffsets[];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s16 (*call0)(Task*);
        s32 (*call1)(Task*, s32, ActorCommand* request);
    } handler;
} Actor105400MsgEntry;
STATIC_ASSERT_SIZEOF(Actor105400MsgEntry, 8);

extern Actor105400MsgEntry D_actor_105400_80133A00[];
extern Actor05400Pose      D_actor_105400_80133A30;
extern EnemyParams         D_actor_105400_8013CE30;
extern u32                 D_actor_105400_8013CE60;
extern AnimationSet*       D_actor_105400_8013CEB8[];
extern TaskDesc            D_actor_105400_8013CEA0[2];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

extern Actor05300Clip gPodHitPulse[];
extern s16            gPodPoseStartFrames[];
extern s16            gPodReleaseIds[];

extern AnimationSet D_actor_105400_8013C5E0;
extern AnimationSet D_actor_105400_8013CA20;
extern AnimationSet D_actor_105400_8013CE08;
extern TmdSource    D_actor_105400_8013C46C;
void                func_actor_105400_801337DC(Task*);
void                func_actor_105400_801339A4(Task*);

s16 Actor05400_Fn01B70(Task*);

Actor105400MsgEntry D_actor_105400_80133A00[3] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = podSetReleaseBits } },
    { 2006, { .call0 = Actor05400_Fn01B70 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

s16 gPodPoseStartFrames[4] = {
    0,
    0,
    4,
    4,
};

Actor05300SpawnPos gPodWeakPointPos[2] = {
    { 6140, -6090, -8500 },
    { 1865, -1095, -4500 },
};

s16 gPodReleaseIds[2] = {
    53,
    54,
};

Actor05400Pose D_actor_105400_80133A30 = { { 0, -500, 1000, 0 }, { 0, -1000, 1500, 0 } };

SVECTOR gPodHitEffectOffsets[2] = {
    { 0, -500, 1400, 0 },
    { 0, -1000, 1600, 0 },
};

TmdBone D_actor_105400_80133A50[10] = {
#include "assets/actor_105400_model_0A64C_skeleton.inc"
};

u32 D_actor_105400_80133BB8[10] = {
#include "assets/actor_105400_model_0A64C_partVerts.inc"
};

SVECTOR D_actor_105400_80133BE0[603] = {
#include "assets/actor_105400_model_0A64C_verts.inc"
};

SVECTOR D_actor_105400_80134EB8[641] = {
#include "assets/actor_105400_model_0A64C_normals.inc"
};

u32 D_actor_105400_801362C0[6251] = {
#include "assets/actor_105400_model_0A64C_stream.inc"
};

TmdSource D_actor_105400_8013C46C = {
    0,
    37128,
    7312,
    10,
    D_actor_105400_80133BB8,
    D_actor_105400_80133BE0,
    D_actor_105400_80134EB8,
    D_actor_105400_80133A50,
    D_actor_105400_801362C0,
};

AnimationPackedPose D_actor_105400_8013C490[4] = {
#include "assets/actor_105400_animation_0A7C0_bank1.inc"
};

AnimationPackedRotation D_actor_105400_8013C4C0[9] = {
#include "assets/actor_105400_animation_0A7C0_bank4.inc"
};

AnimationRecord D_actor_105400_8013C4E4[58] = {
#include "assets/actor_105400_animation_0A7C0_records.inc"
};

u16 D_actor_105400_8013C5CC[10] = {
#include "assets/actor_105400_animation_0A7C0_indices.inc"
};

AnimationSet D_actor_105400_8013C5E0 = {
    D_actor_105400_8013C4E4,
    D_actor_105400_8013C5CC,
    { NULL, D_actor_105400_8013C490, NULL, NULL, D_actor_105400_8013C4C0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105400_8013C608[11] = {
#include "assets/actor_105400_animation_0AC00_bank1.inc"
};

AnimationPackedRotation D_actor_105400_8013C68C[90] = {
#include "assets/actor_105400_animation_0AC00_bank4.inc"
};

AnimationRecord D_actor_105400_8013C7F4[134] = {
#include "assets/actor_105400_animation_0AC00_records.inc"
};

u16 D_actor_105400_8013CA0C[10] = {
#include "assets/actor_105400_animation_0AC00_indices.inc"
};

AnimationSet D_actor_105400_8013CA20 = {
    D_actor_105400_8013C7F4,
    D_actor_105400_8013CA0C,
    { NULL, D_actor_105400_8013C608, NULL, NULL, D_actor_105400_8013C68C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105400_8013CA48[8] = {
#include "assets/actor_105400_animation_0AFE8_bank1.inc"
};

AnimationPackedRotation D_actor_105400_8013CAA8[85] = {
#include "assets/actor_105400_animation_0AFE8_bank4.inc"
};

AnimationRecord D_actor_105400_8013CBFC[126] = {
#include "assets/actor_105400_animation_0AFE8_records.inc"
};

u16 D_actor_105400_8013CDF4[10] = {
#include "assets/actor_105400_animation_0AFE8_indices.inc"
};

AnimationSet D_actor_105400_8013CE08 = {
    D_actor_105400_8013CBFC,
    D_actor_105400_8013CDF4,
    { NULL, D_actor_105400_8013CA48, NULL, NULL, D_actor_105400_8013CAA8, NULL, NULL, NULL },
};

EnemyParams D_actor_105400_8013CE30 = { NULL, 250, 200, 100, 100, 100, 0, 0, 0 };

EnemyParams gPodWeakPointParams = { NULL, 250, 0, 0, 0, 100, 0, 0, 0 };

s32 gPodSoundIds[3] = {
    0x55110003,
    0x55110004,
    0x55110005,
};

u32 gPodPulseSoundId = 0x55110008;

u32 D_actor_105400_8013CE60 = 0x55110009;

Actor05300SndRow gPodViewSound[8] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { -4, -1, 64, 0 },
    { -15, -1, 76, 0 },
    { 15, 0, 76, 0 },
    { -15, -1, 51, 0 },
    { 2, 0, 64, 0 },
    { 12, 0, 32, 0 },
};

Actor05300Clip gPodIdlePulse[3] = {
    { 0, 4032 },
    { 0, 4096 },
    { 1, 4160 },
};

Actor05300Clip gPodHitPulse[4] = {
    { 0, 3968 },
    { 0, 3840 },
    { 0, 4096 },
    { 1, 4224 },
};

TaskDesc D_actor_105400_8013CEA0[2] = {
    { TASK_BODY_TMD, 96, func_actor_105400_801339A4, { .model = &D_actor_105400_8013C46C } },
    { TASK_BODY_COORD, 96, func_actor_105400_801337DC, { .model = NULL } },
};

AnimationSet* D_actor_105400_8013CEB8[4] = {
    NULL,
    &D_actor_105400_8013C5E0,
    &D_actor_105400_8013CA20,
    &D_actor_105400_8013CE08,
};

static void func_actor_105400_8013310C(GpEnemy* arg0, Task* arg1);

#include "../../shared/power_plant_pod_body_hit.inc.c"

#include "../../shared/power_plant_pod_pulse.inc.c"

#include "../../shared/power_plant_pod_inlines.inc.c"

#include "../../shared/power_plant_pod_death.inc.c"

#include "../../shared/power_plant_pod_weak_point_spawn.inc.c"

#include "../../shared/power_plant_pod_weak_point_hit.inc.c"

/// Spawn/setup handler. It allocates the 0x340-byte work block and hangs it on
/// the task, points the model's coordinate and its two matrices (0x244 colour,
/// 0x264 light) at the block, and seeds the enemy's local position and the
/// second `WorldCollisionBody` from the spawn offsets.
///
/// The block's 0x14 prefix becomes the `AnimationContext`: `func_800B3F84` loads the
/// animation bank into it over the ten `AnimationSlot`s and slots 1..9 are reset.
/// The two `WorldCollisionBody` nodes at 0x284 / 0x2A4 are linked onto list 2 with their two
/// `WorldCollisionContact` records (`Gp_InitRec18Table`), each carrying the "last element"
/// flag 0x8000. A child enemy is spawned from `D_actor_105400_8013CEA0` and its
/// model pointed at the placement record's texture page and CLUT row, then the
/// task moves to the tick handler (`state` 1).
///
/// A failed allocation tears the enemy down instead and leaves the task on this
/// handler.
static void func_actor_105400_8013310C(GpEnemy* arg0, Task* arg1)
{
    TmdObject*       obj;
    TmdObject*       model;
    GfxCoord*        coord;
    Actor05300Work*  work;
    GameLocationKey  key;
    GpAreaVariant*   rec;
    AreaPlacement*   place;
    GameLocationKey* sessionKey;
    Actor05400Pose*  pose;
    Actor05400Pose*  pose2;
    s32              idx;
    s32              sound;
    s32              i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(Actor05300Work), 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->field_264;
    obj->colorMtx       = &work->field_244;
    arg0->field_4       = &coord->coord;
    arg0->field_48      = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                = coord;
    pose                       = &D_actor_105400_80133A30;
    arg0->bodyPos.vx           = pose->field_8.vx;
    arg0->bodyPos.vy           = pose->field_8.vy;
    arg0->bodyPos.vz           = pose->field_8.vz;
    arg0->param                = &D_actor_105400_8013CE30;
    arg0->recs                 = work->rec18;
    arg0->hp                   = D_actor_105400_8013CE30.hpMax;
    work->field_2F4.coord      = coord;
    work->field_2F4.spawnArgLo = 0x500;
    work->field_2F4.spawnArgHi = 3;
    func_800B3F84(&work->anim, D_actor_105400_8013CEB8, obj, work->poses,
                  work->slots);
    for (i = 1; i < 0xA; i++) {
        Gp_AnimResetSlot(&work->anim, i, 1);
    }
    (Gp_IncStateF0Ref)(0);
    work->field_334              = 1;
    work->field_326              = 0x1000;
    work->field_2FC              = coord->coord;
    work->field_338              = 1;
    work->field_33C              = D_actor_105400_8013CE30.hpMax;
    work->node0.coord            = coord;
    work->node0.context.contacts = work->rec18;
    work->node0.pos.vx           = 0;
    work->node0.pos.vy           = 0;
    work->node0.pos.vz           = 0;
    work->node0.key              = 0x30036;
    work->node0.radius           = 0x5DC;
    work->node0.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->node0);
    Gp_InitRec18Table(work->rec18, 2, 0);
    work->node1.coord            = coord;
    work->node1.context.contacts = work->rec18;
    work->node0.flags            = (u16)(work->node0.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    pose2                        = &D_actor_105400_80133A30;
    work->node1.pos.vx           = pose2->field_8.vx;
    work->node1.pos.vy           = pose2->field_8.vy;
    work->node1.pos.vz           = pose2->field_8.vz;
    work->node1.key              = 0x30036;
    work->node1.radius           = 0x12C;
    work->node1.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->node1);
    work->node1.flags = (u16)(work->node1.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    model             = Gp_SpawnEnemyFromTable(D_actor_105400_8013CEA0, 1, 0, arg0)->task->extra.tmd;
    idx               = arg0->placeKey >> 12;
    sessionKey        = &gGameSession->location.loc;
    key.stage         = sessionKey->stage;
    key.area          = sessionKey->area;
    key.room          = sessionKey->room;
    key.view          = sessionKey->view;
    areaSyncLocationVariant(&key);
    rec                      = Gp_GetNestedAreaRec(&key);
    place                    = gpAreaPlaceAt(rec->field_0, idx);
    model->texturePageOffset = place->texturePageOffset;
    model->clutRowOffset     = place->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    sound           = D_actor_105400_8013CE60 | ((((GpEnemy*)arg1->spawnArg2.pointer)->placeKey >> 12) << 8);
    work->field_31C = sound;
    SndEvt_EnqueueType6(sound, gPodViewSound[gGameSession->location.loc.view].field_0,
                        gPodViewSound[gGameSession->location.loc.view].field_2);
    arg1->msgTable = D_actor_105400_80133A00;
    arg1->state    = 1;
}

#include "../../shared/power_plant_pod_tick.inc.c"

#include "../../shared/power_plant_pod_regenerate.inc.c"

/// Hands the model's world position (its coordinate's `workm` translation) to
/// `Gp_UpdateActorColor` for the enemy, with no blend parameters.
void podUpdateColor(Task* arg0)
{
    GfxCoord* coord;
    VECTOR    vec;

    coord  = arg0->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2.pointer, &vec, 0, 0);
}

#include "../../shared/power_plant_pod_tick_pose.inc.c"

#include "../../shared/model_placement_scale.inc.c"

/// State handlers of the part task, indexed by `Task::state`: spawn, per-frame
/// hit reaction and teardown.
static const GpEnemyTaskFuncTable3 D_actor_105400_80131E24 = {
    {
        podWeakPointSpawn,
        podWeakPointHit,
        podWeakPointTeardown,
    },
};

/// Task function of the part: runs the state handler `Task::state` selects in
/// `D_actor_105400_80131E24`, handing it the enemy and the task.
void func_actor_105400_801337DC(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105400_80131E24;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

#include "../../shared/power_plant_pod_weak_point_teardown.inc.c"

#include "../../shared/power_plant_pod_release_bits.inc.c"

/// Returns the work block's `field_338`, which the spawn sets to 1 and the hit
/// handler clears when the enemy is killed.
s16 Actor05400_Fn01B70(Task* arg0)
{
    return ((Actor05300Work*)arg0->work)->field_338;
}

/// State handlers of the main task, indexed by `Task::state`: spawn, per-frame
/// tick and death.
static const GpEnemyTaskFuncTable3 D_actor_105400_80131E30 = {
    {
        func_actor_105400_8013310C,
        podTickState,
        podDeathState,
    },
};

/// Task function of the main body: runs the state handler `Task::state`
/// selects in `D_actor_105400_80131E30`, handing it the enemy and the task.
void func_actor_105400_801339A4(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105400_80131E30;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}
