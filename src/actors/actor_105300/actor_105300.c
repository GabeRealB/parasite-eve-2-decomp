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
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
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
#include "../../shared/generator.h"

extern EnemyParams       gGeneratorLifeSupportParams;
extern GeneratorSpawnPos gGeneratorLifeSupportPos[2];
extern GeneratorClip     gGeneratorIdlePulse[];
extern GeneratorSndRow   gGeneratorViewSound[];
extern u32               gGeneratorPulseSoundId;
extern s32               gGeneratorSoundIds[3];
extern SVECTOR           gGeneratorHitEffectOffsets[];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s16 (*call0)(Task*);
        s32 (*call1)(Task*, s32, ActorCommand* request);
    } handler;
} Actor105300MsgEntry;
STATIC_ASSERT_SIZEOF(Actor105300MsgEntry, 8);

extern Actor105300MsgEntry D_actor_105300_80133A00[];
extern SVECTOR             D_actor_105300_80133A30[2];
extern EnemyParams         D_actor_105300_8013D390;
extern u32                 D_actor_105300_8013D3C0;
extern AnimationSet*       D_actor_105300_8013D414[];
extern TaskDesc            D_actor_105300_8013D3FC[2];

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

extern GeneratorClip gGeneratorHitPulse[];
extern s16           gGeneratorPoseStartFrames[];
extern s16           gGeneratorReleaseIds[];

extern AnimationSet D_actor_105300_8013C9D0;
extern AnimationSet D_actor_105300_8013CE8C;
extern AnimationSet D_actor_105300_8013D368;
extern TmdSource    D_actor_105300_8013C794;

Actor105300MsgEntry D_actor_105300_80133A00[3] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call1 = generatorSetReleaseBits } },
    { 2006, { .call0 = generatorIsAlive } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

s16 gGeneratorPoseStartFrames[4] = {
    0,
    0,
    4,
    4,
};

GeneratorSpawnPos gGeneratorLifeSupportPos[2] = {
    { 6140, -6090, -8500 },
    { 1865, -1095, -4500 },
};

s16 gGeneratorReleaseIds[2] = {
    53,
    54,
};

SVECTOR D_actor_105300_80133A30[2] = {
    { 0, -500, 1000, 0 },
    { 0, -1000, 1500, 0 },
};

SVECTOR gGeneratorHitEffectOffsets[2] = {
    { 0, -500, 1400, 0 },
    { 0, -1000, 1600, 0 },
};

TmdBone D_actor_105300_80133A50[10] = {
#include "assets/actor_105300_model_0A974_skeleton.inc"
};

u32 D_actor_105300_80133BB8[10] = {
#include "assets/actor_105300_model_0A974_partVerts.inc"
};

SVECTOR D_actor_105300_80133BE0[610] = {
#include "assets/actor_105300_model_0A974_verts.inc"
};

SVECTOR D_actor_105300_80134EF0[659] = {
#include "assets/actor_105300_model_0A974_normals.inc"
};

u32 D_actor_105300_80136388[6403] = {
#include "assets/actor_105300_model_0A974_stream.inc"
};

TmdSource D_actor_105300_8013C794 = {
    0,
    37792,
    7664,
    10,
    D_actor_105300_80133BB8,
    D_actor_105300_80133BE0,
    D_actor_105300_80134EF0,
    D_actor_105300_80133A50,
    D_actor_105300_80136388,
};

AnimationPackedPose D_actor_105300_8013C7B8[5] = {
#include "assets/actor_105300_animation_0ABB0_bank1.inc"
};

AnimationPackedRotation D_actor_105300_8013C7F4[25] = {
#include "assets/actor_105300_animation_0ABB0_bank4.inc"
};

AnimationRecord D_actor_105300_8013C858[89] = {
#include "assets/actor_105300_animation_0ABB0_records.inc"
};

u16 D_actor_105300_8013C9BC[10] = {
#include "assets/actor_105300_animation_0ABB0_indices.inc"
};

AnimationSet D_actor_105300_8013C9D0 = {
    D_actor_105300_8013C858,
    D_actor_105300_8013C9BC,
    { NULL, D_actor_105300_8013C7B8, NULL, NULL, D_actor_105300_8013C7F4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105300_8013C9F8[4] = {
#include "assets/actor_105300_animation_0B06C_bank1.inc"
};

AnimationPackedRotation D_actor_105300_8013CA28[120] = {
#include "assets/actor_105300_animation_0B06C_bank4.inc"
};

AnimationRecord D_actor_105300_8013CC08[156] = {
#include "assets/actor_105300_animation_0B06C_records.inc"
};

u16 D_actor_105300_8013CE78[10] = {
#include "assets/actor_105300_animation_0B06C_indices.inc"
};

AnimationSet D_actor_105300_8013CE8C = {
    D_actor_105300_8013CC08,
    D_actor_105300_8013CE78,
    { NULL, D_actor_105300_8013C9F8, NULL, NULL, D_actor_105300_8013CA28, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_105300_8013CEB4[4] = {
#include "assets/actor_105300_animation_0B548_bank1.inc"
};

AnimationPackedRotation D_actor_105300_8013CEE4[124] = {
#include "assets/actor_105300_animation_0B548_bank4.inc"
};

AnimationRecord D_actor_105300_8013D0D4[160] = {
#include "assets/actor_105300_animation_0B548_records.inc"
};

u16 D_actor_105300_8013D354[10] = {
#include "assets/actor_105300_animation_0B548_indices.inc"
};

AnimationSet D_actor_105300_8013D368 = {
    D_actor_105300_8013D0D4,
    D_actor_105300_8013D354,
    { NULL, D_actor_105300_8013CEB4, NULL, NULL, D_actor_105300_8013CEE4, NULL, NULL, NULL },
};

EnemyParams D_actor_105300_8013D390 = { NULL, 500, 400, 200, 100, 100, 0, 0, 0 };

EnemyParams gGeneratorLifeSupportParams = { NULL, 250, 0, 0, 0, 100, 0, 0, 0 };

s32 gGeneratorSoundIds[3] = {
    0x55100003,
    0x55100004,
    0x55100005,
};

u32 gGeneratorPulseSoundId = 0x55100008;

u32 D_actor_105300_8013D3C0 = 0x55100009;

GeneratorSndRow gGeneratorViewSound[7] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 15, 0, 76, 0 },
    { -15, -1, 70, 0 },
    { -14, -1, 64, 0 },
    { -15, -1, 38, 0 },
    { 12, 0, 38, 0 },
};

GeneratorClip gGeneratorIdlePulse[3] = {
    { 0, 4032 },
    { 0, 4096 },
    { 1, 4160 },
};

GeneratorClip gGeneratorHitPulse[4] = {
    { 0, 3968 },
    { 0, 3840 },
    { 0, 4096 },
    { 1, 4224 },
};

TaskDesc D_actor_105300_8013D3FC[2] = {
    { { { TASK_BODY_TMD, 96 } }, generatorTask, { .model = &D_actor_105300_8013C794 } },
    { { { TASK_BODY_COORD, 96 } }, generatorLifeSupportTask, { .value = 0 } },
};

AnimationSet* D_actor_105300_8013D414[4] = {
    NULL,
    &D_actor_105300_8013C9D0,
    &D_actor_105300_8013CE8C,
    &D_actor_105300_8013D368,
};

static void func_actor_105300_8013310C(Enemy* arg0, Task* arg1);

#include "../../shared/generator_body_hit.inc.c"

#include "../../shared/generator_pulse.inc.c"

#include "../../shared/generator_inlines.inc.c"

#include "../../shared/generator_death.inc.c"

#include "../../shared/generator_weak_point_spawn.inc.c"

#include "../../shared/generator_weak_point_hit.inc.c"

static void func_actor_105300_8013310C(Enemy* arg0, Task* arg1)
{
    GeneratorWork*   work;
    TmdObject*       obj;
    GfxCoord*        coord;
    GameLocationKey* sessionKey;
    GpAreaVariant*   rec;
    AreaPlacement*   place;
    TmdObject*       model;
    Enemy*           spawned;
    GameLocationKey  key;
    u16              idx;
    s32              sound;
    s32              i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x340, 0);
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
    arg0->bodyPos.vx           = D_actor_105300_80133A30[0].vx;
    arg0->bodyPos.vy           = D_actor_105300_80133A30[0].vy;
    arg0->bodyPos.vz           = D_actor_105300_80133A30[0].vz;
    arg0->param                = &D_actor_105300_8013D390;
    arg0->recs                 = work->rec18;
    arg0->hp                   = D_actor_105300_8013D390.hpMax;
    work->field_2F4.coord      = coord;
    work->field_2F4.spawnArgLo = 0x500;
    work->field_2F4.spawnArgHi = 3;
    func_800B3F84(&work->anim, D_actor_105300_8013D414, obj,
                  work->poses, work->slots);
    for (i = 1; i < 0xA; i++) {
        Gp_AnimResetSlot(&work->anim, i, 1);
    }
    (Gp_IncStateF0Ref)(0);
    work->field_326              = 0x1000;
    work->field_334              = 0;
    work->field_2FC              = coord->coord;
    work->field_338              = 1;
    work->field_33C              = D_actor_105300_8013D390.hpMax;
    work->node0.coord            = coord;
    work->node0.context.contacts = work->rec18;
    work->node0.pos.vx           = 0;
    work->node0.pos.vy           = 0;
    work->node0.pos.vz           = 0;
    work->node0.key              = 0x30035;
    work->node0.radius           = 0x5DC;
    work->node0.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->node0);
    Gp_InitRec18Table(work->rec18, 2, 0);
    work->node0.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->node1.coord            = coord;
    work->node1.context.contacts = work->rec18;
    work->node1.pos.vx           = D_actor_105300_80133A30[0].vx;
    work->node1.pos.vy           = D_actor_105300_80133A30[0].vy;
    work->node1.pos.vz           = D_actor_105300_80133A30[0].vz;
    work->node1.key              = 0x30035;
    work->node1.radius           = 0x12C;
    work->node1.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->node1);
    work->node1.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    spawned            = Gp_SpawnEnemyFromTable(D_actor_105300_8013D3FC, 1, 0, arg0);
    model              = spawned->task->extra.tmd;
    idx                = arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    sessionKey         = &gGameSession->location.loc;
    key.stage          = sessionKey->stage;
    key.area           = sessionKey->area;
    key.room           = sessionKey->room;
    key.view           = sessionKey->view;
    areaSyncLocationVariant(&key);
    rec                      = Gp_GetNestedAreaRec(&key);
    place                    = gpAreaPlaceAt(rec->field_0, idx);
    model->texturePageOffset = place->texturePageOffset;
    model->clutRowOffset     = place->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    sound           = D_actor_105300_8013D3C0 | ((((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
    work->field_31C = sound;
    SndEvt_EnqueueType6(sound, gGeneratorViewSound[gGameSession->location.loc.view].field_0,
                        gGeneratorViewSound[gGameSession->location.loc.view].field_2);
    arg1->msgTable = D_actor_105300_80133A00;
    arg1->state    = 1;
}

#include "../../shared/generator_tick.inc.c"

#include "../../shared/generator_regenerate.inc.c"

#include "../../shared/generator_update_color.inc.c"

#include "../../shared/generator_tick_pose.inc.c"

#include "../../shared/model_placement_scale.inc.c"

/// State handlers of the part task, indexed by `Task::state`: spawn, per-frame
/// hit reaction and teardown.
static const GpEnemyTaskFuncTable3 gGeneratorLifeSupportStates = {
    {
        generatorLifeSupportSpawn,
        generatorLifeSupportHit,
        generatorLifeSupportTeardown,
    },
};

#include "../../shared/generator_life_support_task.inc.c"

#include "../../shared/generator_weak_point_teardown.inc.c"

#include "../../shared/generator_release_bits.inc.c"

#include "../../shared/generator_is_alive.inc.c"

/// State handlers of the main task, indexed by `Task::state`: spawn, per-frame
/// tick and death.
static const GpEnemyTaskFuncTable3 gGeneratorTaskStates = {
    {
        func_actor_105300_8013310C,
        generatorTickState,
        generatorDeathState,
    },
};

#include "../../shared/generator_task.inc.c"
