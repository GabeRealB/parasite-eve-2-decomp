#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/shelter_b1_control_room.h"

extern TaskDesc D_actor_150400_80132CF0;
extern Task*    D_actor_150400_8013C924;
extern Task*    D_actor_150400_8013C928;

extern TaskDesc D_actor_150400_8013C8F4[];
extern u8       D_actor_150400_8013C90C[];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32 (*call0)(void);
        s32 (*call1)(Task*, s32, AnimationPlayRequest*);
        s32 (*call2)(Task*, s32, ActorTransform*);
        s32 (*call3)(Task*, s32, s32);
    } handler;
} Actor150400MsgEntry;
STATIC_ASSERT_SIZEOF(Actor150400MsgEntry, 8);

extern Actor150400MsgEntry D_actor_150400_8013C8C4[];

/// Scratchpad stack pointer the per-frame helpers carve temporary frames off.

static void func_actor_150400_80132228(Task* task);
static void func_actor_150400_80132434(GpEnemy* enemy, Task* task);
static void func_actor_150400_801324B8(Task* task);
static void func_actor_150400_801324E0(Task* task);
static void func_actor_150400_8013257C(Task* task);
static void func_actor_150400_801325C8(Task* task);
static void func_actor_150400_80132640(Task* task);

extern TmdSource D_actor_150400_80139A64;
extern TmdSource D_actor_150400_8013C8A0;
void             func_actor_150400_801323E0(Task*);
void             func_actor_150400_801328BC(Task*);

s32 func_actor_150400_801326A4(Task*, s32, AnimationPlayRequest*);
s32 func_actor_150400_80132710(Task*, s32, s32);
s32 func_actor_150400_80132774(Task*, s32, ActorTransform* placement);
s32 func_actor_150400_801327EC(void);
s32 func_actor_150400_801327F4(Task*, s32, ActorTransform* target);

void func_actor_150400_80131ECC(void);
void func_actor_150400_80131F6C(void);

void func_actor_150400_80131ECC(void);
void func_actor_150400_80131F6C(void);

void func_actor_150400_80131E24(Task*);
void func_actor_150400_80131ECC(void);
void func_actor_150400_80131F6C(void);
void func_actor_150400_80131F9C(s32);

TmdBone D_actor_150400_8013292C[1] = {
#include "assets/actor_150400_model_00EAC_skeleton.inc"
};

u32 D_actor_150400_80132950[1] = {
#include "assets/actor_150400_model_00EAC_partVerts.inc"
};

SVECTOR D_actor_150400_80132954[34] = {
#include "assets/actor_150400_model_00EAC_verts.inc"
};

u32 D_actor_150400_80132A64[154] = {
#include "assets/actor_150400_model_00EAC_stream.inc"
};

TmdSource D_actor_150400_80132CCC = {
    0,
    1144,
    0,
    1,
    D_actor_150400_80132950,
    D_actor_150400_80132954,
    &D_actor_150400_80132954[34],
    D_actor_150400_8013292C,
    D_actor_150400_80132A64,
};

TaskDesc D_actor_150400_80132CF0 = { (TASK_BODY_TMD | 0x100), 32, func_actor_150400_80131E24, { .model = &D_actor_150400_80132CCC } };

AnimationPlayRequest D_actor_150400_80132CFC = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_150400_80132D10 = { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_150400_80132D24 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_150400_80132D38 = { { 6705, -500, -3316, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_150400_80132D50 = { { 1535, -500, -3316, 0 }, { 0, 1024, 0, 0 } };

GpOverlayIds D_actor_150400_80132D68 = { 5, 4, 11 };

GpEvsCmd D_actor_150400_80132D70[33] = {
    { 12, { .overlays = &D_actor_150400_80132D68 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 31, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_150400_80132D24 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_150400_80131F9C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 30, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_150400_80132D38 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_150400_80132D10 }, { .value = 0 } },
    { 36, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2013 }, { .storage = &D_actor_150400_80132D50 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_150400_80131F9C }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 46, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_150400_80131F6C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_150400_80131ECC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 37, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_actor_150400_80133088[14] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_150400_80131F6C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_150400_80131ECC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TmdBone D_actor_150400_801331D8[19] = {
#include "assets/actor_150400_model_07C44_skeleton.inc"
};

u32 D_actor_150400_80133484[19] = {
#include "assets/actor_150400_model_07C44_partVerts.inc"
};

SVECTOR D_actor_150400_801334D0[432] = {
#include "assets/actor_150400_model_07C44_verts.inc"
};

SVECTOR D_actor_150400_80134250[444] = {
#include "assets/actor_150400_model_07C44_normals.inc"
};

u32 D_actor_150400_80135030[4749] = {
#include "assets/actor_150400_model_07C44_stream.inc"
};

TmdSource D_actor_150400_80139A64 = {
    0,
    26564,
    6624,
    19,
    D_actor_150400_80133484,
    D_actor_150400_801334D0,
    D_actor_150400_80134250,
    D_actor_150400_801331D8,
    D_actor_150400_80135030,
};

AnimationPackedPose D_actor_150400_80139A88[21] = {
#include "assets/actor_150400_animation_08878_bank1.inc"
};

AnimationPackedRotation D_actor_150400_80139B84[317] = {
#include "assets/actor_150400_animation_08878_bank4.inc"
};

AnimationRecord D_actor_150400_8013A078[382] = {
#include "assets/actor_150400_animation_08878_records.inc"
};

u16 D_actor_150400_8013A670[20] = {
#include "assets/actor_150400_animation_08878_indices.inc"
};

AnimationSet D_actor_150400_8013A698 = {
    D_actor_150400_8013A078,
    D_actor_150400_8013A670,
    { NULL, D_actor_150400_80139A88, NULL, NULL, D_actor_150400_80139B84, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_150400_8013A6C0[12] = {
#include "assets/actor_150400_animation_08FE0_bank1.inc"
};

AnimationPackedRotation D_actor_150400_8013A750[187] = {
#include "assets/actor_150400_animation_08FE0_bank4.inc"
};

AnimationRecord D_actor_150400_8013AA3C[231] = {
#include "assets/actor_150400_animation_08FE0_records.inc"
};

u16 D_actor_150400_8013ADD8[20] = {
#include "assets/actor_150400_animation_08FE0_indices.inc"
};

AnimationSet D_actor_150400_8013AE00 = {
    D_actor_150400_8013AA3C,
    D_actor_150400_8013ADD8,
    { NULL, D_actor_150400_8013A6C0, NULL, NULL, D_actor_150400_8013A750, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_150400_8013AE28[20] = {
#include "assets/actor_150400_animation_09BD0_bank1.inc"
};

AnimationPackedRotation D_actor_150400_8013AF18[321] = {
#include "assets/actor_150400_animation_09BD0_bank4.inc"
};

AnimationRecord D_actor_150400_8013B41C[363] = {
#include "assets/actor_150400_animation_09BD0_records.inc"
};

u16 D_actor_150400_8013B9C8[20] = {
#include "assets/actor_150400_animation_09BD0_indices.inc"
};

AnimationSet D_actor_150400_8013B9F0 = {
    D_actor_150400_8013B41C,
    D_actor_150400_8013B9C8,
    { NULL, D_actor_150400_8013AE28, NULL, NULL, D_actor_150400_8013AF18, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_150400_8013BA18[16] = {
#include "assets/actor_150400_animation_0A538_bank1.inc"
};

AnimationPackedRotation D_actor_150400_8013BAD8[237] = {
#include "assets/actor_150400_animation_0A538_bank4.inc"
};

AnimationRecord D_actor_150400_8013BE8C[297] = {
#include "assets/actor_150400_animation_0A538_records.inc"
};

u16 D_actor_150400_8013C330[20] = {
#include "assets/actor_150400_animation_0A538_indices.inc"
};

AnimationSet D_actor_150400_8013C358 = {
    D_actor_150400_8013BE8C,
    D_actor_150400_8013C330,
    { NULL, D_actor_150400_8013BA18, NULL, NULL, D_actor_150400_8013BAD8, NULL, NULL, NULL },
};

TmdBone D_actor_150400_8013C380[1] = {
#include "assets/actor_150400_model_0AA80_skeleton.inc"
};

u32 D_actor_150400_8013C3A4[1] = {
#include "assets/actor_150400_model_0AA80_partVerts.inc"
};

SVECTOR D_actor_150400_8013C3A8[29] = {
#include "assets/actor_150400_model_0AA80_verts.inc"
};

SVECTOR D_actor_150400_8013C490[24] = {
#include "assets/actor_150400_model_0AA80_normals.inc"
};

u32 D_actor_150400_8013C550[212] = {
#include "assets/actor_150400_model_0AA80_stream.inc"
};

TmdSource D_actor_150400_8013C8A0 = {
    0,
    1436,
    0,
    1,
    D_actor_150400_8013C3A4,
    D_actor_150400_8013C3A8,
    D_actor_150400_8013C490,
    D_actor_150400_8013C380,
    D_actor_150400_8013C550,
};

Actor150400MsgEntry D_actor_150400_8013C8C4[6] = {
    { 2003, { .call1 = func_actor_150400_801326A4 } },
    { 2005, { .call3 = func_actor_150400_80132710 } },
    { 2004, { .call2 = func_actor_150400_80132774 } },
    { 2011, { .call0 = func_actor_150400_801327EC } },
    { 2013, { .call2 = func_actor_150400_801327F4 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_150400_8013C8F4[2] = {
    { TASK_BODY_TMD, 96, func_actor_150400_801323E0, { .model = &D_actor_150400_80139A64 } },
    { (TASK_BODY_TMD | 0x100), 96, func_actor_150400_801328BC, { .model = &D_actor_150400_8013C8A0 } },
};

u8 D_actor_150400_8013C90C[24] = {
    0,
    0,
    0,
    0,
    152,
    166,
    19,
    128,
    0,
    174,
    19,
    128,
    240,
    185,
    19,
    128,
    88,
    195,
    19,
    128,
    0,
    0,
    0,
    0,
};

Task* D_actor_150400_8013C924 = NULL;

Task* D_actor_150400_8013C928;

static void func_actor_150400_80131FB8(void);
static void func_actor_150400_80132014(GpEnemy* enemy, Task* task);

/// Per-frame callback of the model task `D_actor_150400_80132CF0` describes,
/// spawned twice by `func_actor_150400_80131FB8` with `spawnArg1` 1 and 2.
/// State 0 places the model's coordinate (the two copies differ only in z) and
/// moves on to 1; `func_actor_150400_80131F9C` puts both copies into state 2,
/// which slides them along x by 4 a frame up to 0x406. The model is drawn only
/// while the save's view byte is 5; otherwise its flags are set to 0x84, which
/// hides it.
void func_actor_150400_80131E24(Task* task)
{
    TmdObject* obj   = task->extra.tmd;
    GfxCoord*  coord = obj->coords;

    if (task->state == 0) {
        coord->coord.t[0] = 0x2DA;
        coord->coord.t[1] = -0x564;
        if (task->spawnArg1.value == 1) {
            coord->coord.t[2] = -0x116C;
        } else {
            coord->coord.t[2] = -0x1018;
        }
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        task->state++;
    }
    if (task->state == 2) {
        coord->coord.t[0] += 4;
        if (coord->coord.t[0] > 0x406) {
            coord->coord.t[0] = 0x406;
        }
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (Mc_SaveData[0].state.at4.loc.view != 5) {
        obj->flags = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    } else {
        obj->flags    = 0;
        obj->otOffset = 0;
    }
}

void func_actor_150400_80131ECC(void)
{
    if (Mc_SaveData[0].state.demoScene != 9) {
        SetDispMask(1);
        GameFlag_SetNibble(0xE5, 1);
        Gp_EnqueueConfigCd(1);
        Gp_ApplyAreaRecs(D_shelter_b1_control_room_80183BE0);
        Mc_SaveData[0].state.at4.loc.stage = 4;
        Mc_SaveData[0].state.at4.loc.area  = 0x21;
        Mc_SaveData[0].state.at4.loc.warp  = 4;
        Mc_SaveData[0].state.at4.loc.room  = 1;
        gDisplayState.spriteVariant        = 1;
        Task_Spawn(0, 0x11, 0, 0);
        Gp_RestoreStreamRng();
    }
}

void func_actor_150400_80131F6C(void)
{
    Task_SpawnFromTable(D_shelter_b1_control_room_80181BBC, 0, 0, 0);
}

void func_actor_150400_80131F9C(s32 arg0)
{
    D_actor_150400_8013C924->state = arg0;
    D_actor_150400_8013C928->state = arg0;
}

static void func_actor_150400_80131FB8(void)
{
    D_actor_150400_8013C924 = Task_SpawnFromTable(&D_actor_150400_80132CF0, 0, 1, 0);
    D_actor_150400_8013C928 = Task_SpawnFromTable(&D_actor_150400_80132CF0, 0, 2, 0);
}

/// State-0 handler of the actor's task: allocates the work block, starts the
/// sub-model task and parents it under this one, textures the sub-model from
/// the placement record of the current area, then starts the animation in
/// state 2 and runs the step body `func_actor_150400_80132228` once.
static void func_actor_150400_80132014(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor150400Work* work;
    GfxCoord*        coord;
    TmdObject*       obj;
    GpEnemy*         spawned;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    task->work = (work = (Actor150400Work*)memCalloc(sizeof(Actor150400Work), false));
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_150400_801324B8;
    coord->parent                = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->flags                   = 0;
    obj->otOffset                = 1;
    work->enemy                  = enemy;
    spawned                      = Gp_SpawnEnemyFromTable(D_actor_150400_8013C8F4, 1, 0, enemy);
    actorTintModel(spawned->task->extra.tmd, enemy);
    Task_Reparent(task, spawned->task);
    work->pairTask = spawned->task;
    obj->lightMtx  = &work->light;
    obj->colorMtx  = &work->color;
    vec.vx         = coord->workm.t[0];
    vec.vy         = coord->workm.t[1] - 0x320;
    vec.vz         = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->rig.anim, D_actor_150400_8013C90C, obj,
                  &work->rig.poses, work->rig.slots);
    work->st.animId = 1;
    work->st.state  = 2;
    task->msgTable  = D_actor_150400_8013C8C4;
    func_actor_150400_80132228(task);
    task->state++;
}

/// Step body of the actor's animation state machine. States 1 and 2 reseed the
/// animation slots (with and without `animArg`) and advance to state 3; state 3
/// walks the root coordinate 0x11 units per frame while clip 4 has `travel`
/// left, dropping back to clip 1 with argument 0xA when it runs out, then ticks
/// the slots.
static void func_actor_150400_80132228(Task* task)
{
    Actor150400Work* work;
    s16              animId;

    work = (Actor150400Work*)task->work;
    if (work->st.state == 1) {
        func_actor_150400_80132640(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 2) {
        func_actor_150400_801325C8(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 3) {
        // The loop-end note ends cse's first block here, so the pause check
        // loads its own 1 instead of reusing the state test's.
        do {
        } while (0);
        animId = work->st.animId;
        if (animId == 4 && work->st.travel != 0) {
            actorMoveForward(task->extra.tmd->coords, 0x11);
            work->st.travel = (u16)work->st.travel - 1;
            if (work->st.travel == 0) {
                work->animArg   = 0xA;
                work->st.animId = 1;
            }
        }
        func_actor_150400_8013257C(task);
        return;
    }
}

/// Per-frame callback of the actor's task: runs the state's handler, the spawn
/// handler `func_actor_150400_80132014` in state 0 and the per-frame update
/// `func_actor_150400_80132434` after it, passing the task's `GpEnemy`.
void func_actor_150400_801323E0(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_150400_80132014,
        func_actor_150400_80132434,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// State-1 handler of the actor's task, run every frame: refreshes the model
/// root's coordinate, feeds its world translation (raised by 800 on y) to
/// `func_800D7A9C`, then runs the animation step body and draws the ground
/// shadow.
static void func_actor_150400_80132434(GpEnemy* enemy, Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR     pos;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 800;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_150400_80132228(task);
    func_actor_150400_801324E0(task);
}

/// Exit callback of the actor's task: hands its `GpEnemy` back to
/// `Gp_DestroyEnemy`.
static void func_actor_150400_801324B8(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

/// Draws the actor's ground shadow under its root part, unless the model is
/// hidden (`flags` bit 0x80) or has no buffer. The position is the root part's
/// world translation, staged on the scratchpad stack, and the shade follows the
/// room's current `Gp_State1C` level.
static void func_actor_150400_801324E0(Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR3*   vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (!(obj->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && obj->buffer != NULL) {
        vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShadowShade);
        SCRATCH_POP_BYTES(0x18);
    }
}

/// Ticks animation slots 1..0x12.
static void func_actor_150400_8013257C(Task* task)
{
    Actor150400Work* work;
    s32              i;

    work = (Actor150400Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->rig.anim, i);
        i++;
    } while (i < 0x13);
}

/// Resets animation slots 1..0x12 to clip `animId` at rate 1, without a reseed
/// argument, and records the clip as the applied one.
static void func_actor_150400_801325C8(Task* task)
{
    Actor150400Work* work;
    s32              i;

    work = (Actor150400Work*)task->work;
    for (i = 1; i < 0x13; i++) {
        work->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&work->rig.anim, i, work->st.animId);
    }
    work->st.appliedAnimId = work->st.animId;
}

/// Reseeds animation slots 1..0x12 with clip `animId` and argument `animArg`,
/// and records the clip as the applied one.
static void func_actor_150400_80132640(Task* task)
{
    Actor150400Work* work;
    s32              i;

    work = (Actor150400Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->rig.anim, i, work->st.animId, 0, work->animArg);
        i++;
    } while (i < 0x13);
    work->st.appliedAnimId = work->st.animId;
}

/// Starts the actor's scripted animation selected by the request.
///
/// Rejects ids 6 and above before changing playback state.
/// The blend path carries the requested duration in whole frames.
s32 func_actor_150400_801326A4(Task* task, s32 arg1, AnimationPlayRequest* args)
{
    Actor150400Work* work;

    work = (Actor150400Work*)task->work;
    if (args->animationId < 6) {
        work->st.animId = args->animationId;
        if (args->blend != ANIMATION_BLEND_RESET) {
            work->st.state = 1;
            work->animArg  = args->blendFrames;
        } else {
            work->st.state = 2;
        }
        work->st.field_6 = 0;
        func_actor_150400_80132228(task);
        return 0;
    }
    return -1;
}

/// Script opcode: set the visibility of the actor's model and of its sub-model
/// (the model of the task in `pairTask`) together. `flags` bit 0 shows both
/// (`TmdObject::flags` = 0) and its absence hides them (0x80); bit 1
/// additionally sets bit 0x4 on both.
s32 func_actor_150400_80132710(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = task->extra.tmd;
    other = ((Actor150400Work*)task->work)->pairTask->extra.tmd;

    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        other->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }

    if (flags & 2) {
        self->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        other->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

/// Script opcode: place the actor. Yaws its root coordinate to
/// `placement->rot.vy`, caching that yaw in `yaw`, then drops the placement
/// translation into the matrix and marks it for recomputation.
s32 func_actor_150400_80132774(Task* task, s32 arg1, ActorTransform* placement)
{
    GfxCoord*        coord;
    Actor150400Work* work;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor150400Work*)task->work;
    yaw          = placement->rot.vy;
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return 0;
}

s32 func_actor_150400_801327EC(void)
{
    return 0;
}

/// Script opcode: walk to `target`. Aims the actor's root coordinate at it by
/// the yaw of the horizontal offset from the coordinate's own translation,
/// caches that yaw in `yaw` and rebuilds the local matrix from it, then sets
/// `travel` to the distance divided by 17, the step body's per-frame stride.
s32 func_actor_150400_801327F4(Task* task, s32 arg1, ActorTransform* target)
{
    GfxCoord*        coord;
    Actor150400Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord        = task->extra.tmd->coords;
    work         = (Actor150400Work*)task->work;
    dx           = target->pos.vx - coord->coord.t[0];
    dz           = target->pos.vz - coord->coord.t[2];
    yaw          = ratan2(dx, dz);
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 17;
    return 0;
}

/// Per-frame callback of the actor's sub-model task, which the spawn handler
/// parents under the actor's own task. On the first frame it draws the
/// sub-model under the actor's `light` / `color` matrices and parents its root
/// coordinate to part 7 of the actor's model, then advances to state 1; from
/// then on it only marks the coordinate for recomputation each frame.
void func_actor_150400_801328BC(Task* task)
{
    char             pad[0x10];
    Task*            parent = task->parent;
    TmdObject*       obj    = task->extra.tmd;
    GfxCoord*        coord  = obj->coords;
    GfxCoord*        sub    = &parent->extra.tmd->coords[7];
    Actor150400Work* work   = (Actor150400Work*)parent->work;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            obj->lightMtx       = &work->light;
            obj->colorMtx       = &work->color;
            coord->parent       = sub;
            task->state++;
            break;
        case 1:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}
