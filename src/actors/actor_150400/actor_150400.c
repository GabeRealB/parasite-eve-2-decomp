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
#include "../../shared/walker.h"
#include "../../shared/pair_walk.h"

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

static void func_actor_150400_80132434(Enemy* enemy, Task* task);
static void func_actor_150400_801324B8(Task* task);

extern TmdSource D_actor_150400_80139A64;
extern TmdSource D_actor_150400_8013C8A0;
void             func_actor_150400_801323E0(Task*);

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

TaskDesc D_actor_150400_80132CF0 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_MODEL_BUFFER), 32 } }, func_actor_150400_80131E24, { .model = &D_actor_150400_80132CCC } };

AnimationPlayRequest D_actor_150400_80132CFC = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_150400_80132D10 = { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_150400_80132D24 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_150400_80132D38 = { { 6705, -500, -3316, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_150400_80132D50 = { { 1535, -500, -3316, 0 }, { 0, 1024, 0, 0 } };

EvsSceneKey D_actor_150400_80132D68 = { 5, 4, 11 };

EvsCommand D_actor_150400_80132D70[33] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_150400_80132D68 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_150400_80132D24 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_150400_80131F9C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_actor_150400_80132D38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_actor_150400_80132D10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 4 }, { .value = 0 }, { .value = 2013 }, { .storage = &D_actor_150400_80132D50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_150400_80131F9C }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_150400_80131F6C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_150400_80131ECC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_150400_80133088[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_150400_80131F6C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_150400_80131ECC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { 2003, { .call1 = pairWalkPlay } },
    { 2005, { .call3 = pairWalkSetVisibility } },
    { 2004, { .call2 = pairWalkPlace } },
    { 2011, { .call0 = func_actor_150400_801327EC } },
    { 2013, { .call2 = func_actor_150400_801327F4 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

TaskDesc D_actor_150400_8013C8F4[2] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_150400_801323E0, { .model = &D_actor_150400_80139A64 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_MODEL_BUFFER), 96 } }, pairWalkSubModelTask, { .model = &D_actor_150400_8013C8A0 } },
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
static void func_actor_150400_80132014(Enemy* enemy, Task* task);

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
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view != 5) {
        obj->flags = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    } else {
        obj->flags    = 0;
        obj->otOffset = 0;
    }
}

void func_actor_150400_80131ECC(void)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 9) {
        SetDispMask(1);
        GameFlag_SetNibble(0xE5, 1);
        Gp_EnqueueConfigCd(1);
        Gp_ApplyAreaRecs(D_shelter_b1_control_room_80183BE0);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = 4;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = 0x21;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 4;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
        gDisplayState.spriteVariant                                 = 1;
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
/// state 2 and runs the step body `pairWalkUpdate` once.
static void func_actor_150400_80132014(Enemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor150400Work* work;
    GfxCoord*        coord;
    TmdObject*       obj;
    Enemy*           spawned;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    task->work = (work = memCalloc(sizeof(Actor150400Work), false));
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback               = func_actor_150400_801324B8;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->flags                       = 0;
    obj->otOffset                    = 1;
    work->enemy                      = enemy;
    spawned                          = Gp_SpawnEnemyFromTable(D_actor_150400_8013C8F4, 1, 0, enemy);
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
    pairWalkUpdate(task);
    task->state++;
}

#include "../../shared/pair_walk_update.inc.c"

/// Per-frame callback of the actor's task: runs the state's handler, the spawn
/// handler `func_actor_150400_80132014` in state 0 and the per-frame update
/// `func_actor_150400_80132434` after it, passing the task's `Enemy`.
void func_actor_150400_801323E0(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_150400_80132014,
        func_actor_150400_80132434,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

#define walkerFrame      func_actor_150400_80132434
#define walkerUpdate     pairWalkUpdate
#define walkerDrawShadow walkerDrawShadowShaded
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

/// Exit callback of the actor's task: hands its `Enemy` back to
/// `Gp_DestroyEnemy`.
static void func_actor_150400_801324B8(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2.pointer, task);
}

#include "../../shared/walker_shadow_shaded.inc.c"

#include "../../shared/pair_walk_tick_anim.inc.c"

#include "../../shared/pair_walk_reset_anim.inc.c"

#include "../../shared/pair_walk_reseed_anim.inc.c"

#include "../../shared/pair_walk_play.inc.c"

#include "../../shared/pair_walk_visibility.inc.c"

#include "../../shared/pair_walk_place.inc.c"

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
    gfxRotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 17;
    return 0;
}

#include "../../shared/pair_walk_sub_model.inc.c"
