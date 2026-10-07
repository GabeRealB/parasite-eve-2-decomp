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
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/shelter_b1_control_room.h"
#include "../../shared/pair_walk.h"

static s32  _pairWalkPlay(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static void _pairWalkSubModelTask(Task* task);

extern TaskDesc D_actor_150400_80132CF0;
extern Task*    D_actor_150400_8013C924;
extern Task*    D_actor_150400_8013C928;

extern TaskDesc D_actor_150400_8013C8F4[];
extern u8       D_actor_150400_8013C90C[];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_150400_8013C8C4[];

/// Scratchpad stack pointer the per-frame helpers carve temporary frames off.

static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void func_actor_150400_801324B8(Task* task);
static void _actorRenderDrawWalkerGroundShadow(Task* task);

static TmdSource _gActor150400No9GolemDryfieldBody;
static TmdSource _gActor150400GolemBeamSword;
void             func_actor_150400_801323E0(Task*);

s32 func_actor_150400_801327EC(Task*, s32, s32, s32);
s32 func_actor_150400_801327F4(Task* task, s32 msgId, ActorTransform* target, s32 arg3);

void func_actor_150400_80131ECC(void);
void func_actor_150400_80131F6C(void);

void func_actor_150400_80131ECC(void);
void func_actor_150400_80131F6C(void);

void func_actor_150400_80131E24(Task*);
void func_actor_150400_80131ECC(void);
void func_actor_150400_80131F6C(void);
void func_actor_150400_80131F9C(s32);

static TmdBone _gActor150400Model00C44Skeleton[1] = {
#include "assets/actor_150400_model_00C44_skeleton.inc"
};

static u32 _gActor150400Model00C44PartVerts[1] = {
#include "assets/actor_150400_model_00C44_partVerts.inc"
};

static SVECTOR _gActor150400Model00C44Verts[34] = {
#include "assets/actor_150400_model_00C44_verts.inc"
};

static u32 _gActor150400Model00C44Stream[154] = {
#include "assets/actor_150400_model_00C44_stream.inc"
};

static TmdSource _gActor150400Model00C44 = {
    0,
    1144,
    0,
    1,
    _gActor150400Model00C44PartVerts,
    _gActor150400Model00C44Verts,
    &_gActor150400Model00C44Verts[34],
    _gActor150400Model00C44Skeleton,
    _gActor150400Model00C44Stream,
};

TaskDesc D_actor_150400_80132CF0 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 32 } }, func_actor_150400_80131E24, { .model = &_gActor150400Model00C44 } };

AnimationPlayRequest D_actor_150400_80132CFC = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_150400_80132D10 = { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_150400_80132D24 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_150400_80132D38 = { { 6705, -500, -3316, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_150400_80132D50 = { { 1535, -500, -3316, 0 }, { 0, 1024, 0, 0 } };

EvsSceneKey D_actor_150400_80132D68 = { 5, 4, 11 };

EvsCommand D_actor_150400_80132D70[33] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_150400_80132D68 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_150400_80132D24 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_150400_80131F9C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_150400_80132D38 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_150400_80132D10 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_actor_150400_80132D50 } }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gActor150400No9GolemDryfieldBodySkeleton[19] = {
#include "assets/no9_golem_dryfield_body_skeleton.inc"
};

static u32 _gActor150400No9GolemDryfieldBodyPartVerts[19] = {
#include "assets/no9_golem_dryfield_body_partVerts.inc"
};

static SVECTOR _gActor150400No9GolemDryfieldBodyVerts[432] = {
#include "assets/no9_golem_dryfield_body_verts.inc"
};

static SVECTOR _gActor150400No9GolemDryfieldBodyNormals[444] = {
#include "assets/no9_golem_dryfield_body_normals.inc"
};

static u32 _gActor150400No9GolemDryfieldBodyStream[4749] = {
#include "assets/no9_golem_dryfield_body_stream.inc"
};

static TmdSource _gActor150400No9GolemDryfieldBody = {
    0,
    26564,
    6624,
    19,
    _gActor150400No9GolemDryfieldBodyPartVerts,
    _gActor150400No9GolemDryfieldBodyVerts,
    _gActor150400No9GolemDryfieldBodyNormals,
    _gActor150400No9GolemDryfieldBodySkeleton,
    _gActor150400No9GolemDryfieldBodyStream,
};

static AnimationPackedPose _gActor150400Animation08878Bank1[21] = {
#include "assets/actor_150400_animation_08878_bank1.inc"
};

static AnimationPackedRotation _gActor150400Animation08878Bank4[317] = {
#include "assets/actor_150400_animation_08878_bank4.inc"
};

static AnimationRecord _gActor150400Animation08878Records[382] = {
#include "assets/actor_150400_animation_08878_records.inc"
};

static u16 _gActor150400Animation08878Indices[20] = {
#include "assets/actor_150400_animation_08878_indices.inc"
};

static AnimationSet _gActor150400Animation08878 = {
    _gActor150400Animation08878Records,
    _gActor150400Animation08878Indices,
    { NULL, _gActor150400Animation08878Bank1, NULL, NULL, _gActor150400Animation08878Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor150400Animation08FE0Bank1[12] = {
#include "assets/actor_150400_animation_08FE0_bank1.inc"
};

static AnimationPackedRotation _gActor150400Animation08FE0Bank4[187] = {
#include "assets/actor_150400_animation_08FE0_bank4.inc"
};

static AnimationRecord _gActor150400Animation08FE0Records[231] = {
#include "assets/actor_150400_animation_08FE0_records.inc"
};

static u16 _gActor150400Animation08FE0Indices[20] = {
#include "assets/actor_150400_animation_08FE0_indices.inc"
};

static AnimationSet _gActor150400Animation08FE0 = {
    _gActor150400Animation08FE0Records,
    _gActor150400Animation08FE0Indices,
    { NULL, _gActor150400Animation08FE0Bank1, NULL, NULL, _gActor150400Animation08FE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor150400Animation09BD0Bank1[20] = {
#include "assets/actor_150400_animation_09BD0_bank1.inc"
};

static AnimationPackedRotation _gActor150400Animation09BD0Bank4[321] = {
#include "assets/actor_150400_animation_09BD0_bank4.inc"
};

static AnimationRecord _gActor150400Animation09BD0Records[363] = {
#include "assets/actor_150400_animation_09BD0_records.inc"
};

static u16 _gActor150400Animation09BD0Indices[20] = {
#include "assets/actor_150400_animation_09BD0_indices.inc"
};

static AnimationSet _gActor150400Animation09BD0 = {
    _gActor150400Animation09BD0Records,
    _gActor150400Animation09BD0Indices,
    { NULL, _gActor150400Animation09BD0Bank1, NULL, NULL, _gActor150400Animation09BD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor150400Animation0A538Bank1[16] = {
#include "assets/actor_150400_animation_0A538_bank1.inc"
};

static AnimationPackedRotation _gActor150400Animation0A538Bank4[237] = {
#include "assets/actor_150400_animation_0A538_bank4.inc"
};

static AnimationRecord _gActor150400Animation0A538Records[297] = {
#include "assets/actor_150400_animation_0A538_records.inc"
};

static u16 _gActor150400Animation0A538Indices[20] = {
#include "assets/actor_150400_animation_0A538_indices.inc"
};

static AnimationSet _gActor150400Animation0A538 = {
    _gActor150400Animation0A538Records,
    _gActor150400Animation0A538Indices,
    { NULL, _gActor150400Animation0A538Bank1, NULL, NULL, _gActor150400Animation0A538Bank4, NULL, NULL, NULL },
};

static TmdBone _gActor150400GolemBeamSwordSkeleton[1] = {
#include "assets/golem_beam_sword_skeleton.inc"
};

static u32 _gActor150400GolemBeamSwordPartVerts[1] = {
#include "assets/golem_beam_sword_partVerts.inc"
};

static SVECTOR _gActor150400GolemBeamSwordVerts[29] = {
#include "assets/golem_beam_sword_verts.inc"
};

static SVECTOR _gActor150400GolemBeamSwordNormals[24] = {
#include "assets/golem_beam_sword_normals.inc"
};

static u32 _gActor150400GolemBeamSwordStream[212] = {
#include "assets/golem_beam_sword_stream.inc"
};

static TmdSource _gActor150400GolemBeamSword = {
    0,
    1436,
    0,
    1,
    _gActor150400GolemBeamSwordPartVerts,
    _gActor150400GolemBeamSwordVerts,
    _gActor150400GolemBeamSwordNormals,
    _gActor150400GolemBeamSwordSkeleton,
    _gActor150400GolemBeamSwordStream,
};

TaskMessageEntry D_actor_150400_8013C8C4[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _pairWalkPlay },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _pairWalkSetVisibility },
    { ACTOR_MESSAGE_PLACE, _pairWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_150400_801327EC },
    { ACTOR_MESSAGE_WALK_TO, func_actor_150400_801327F4 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_150400_8013C8F4[2] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_150400_801323E0, { .model = &_gActor150400No9GolemDryfieldBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _pairWalkSubModelTask, { .model = &_gActor150400GolemBeamSword } },
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

void        func_actor_150400_80131FB8(void);
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
        gameFlagSetNibble(GAME_FLAG_GOLEM_FREEZER_UNLOCKED, 1);
        loadingEnqueueCharacterResources(1);
        areaApplySavedUpdates(D_shelter_b1_control_room_80183BE0);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_MINE_SHELTER;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_SHELTER_B2_MAIN_CORRIDOR;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 4;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
        gDisplayState.spriteVariant                                 = 1;
        taskSpawn(0, 0x11, 0, 0);
        streamFinishScene();
    }
}

void func_actor_150400_80131F6C(void)
{
    taskSpawnFromTable(D_shelter_b1_control_room_80181BBC, 0, 0, 0);
}

void func_actor_150400_80131F9C(s32 arg0)
{
    D_actor_150400_8013C924->state = arg0;
    D_actor_150400_8013C928->state = arg0;
}

void func_actor_150400_80131FB8(void)
{
    D_actor_150400_8013C924 = taskSpawnFromTable(&D_actor_150400_80132CF0, 0, 1, 0);
    D_actor_150400_8013C928 = taskSpawnFromTable(&D_actor_150400_80132CF0, 0, 2, 0);
}

/// State-0 handler of the actor's task: allocates the work block, starts the
/// sub-model task and parents it under this one, textures the sub-model from
/// the placement record of the current area, then starts the animation in
/// state 2 and runs the step body `_pairWalkUpdate` once.
static void func_actor_150400_80132014(Enemy* enemy, Task* task)
{
    VECTOR        vec;
    PairWalkWork* work;
    GfxCoord*     coord;
    TmdObject*    obj;
    Enemy*        spawned;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    task->work = (work = memCalloc(sizeof(PairWalkWork), false));
    if (work == NULL) {
        enemyDestroy(enemy, task);
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
    spawned                          = enemySpawnFromTable(D_actor_150400_8013C8F4, 1, 0, enemy);
    _actorRenderApplyPlacementTextureOffsets(spawned->task->extra.tmd, enemy);
    taskReparent(task, spawned->task);
    work->pairTask = spawned->task;
    obj->lightMtx  = &work->light;
    obj->colorMtx  = &work->color;
    vec.vx         = coord->workm.t[0];
    vec.vy         = coord->workm.t[1] - 0x320;
    vec.vz         = coord->workm.t[2];
    worldCoordSetModelLighting(obj, &vec, 0, 3);
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_150400_8013C90C, obj,
                         work->rig.poses, work->rig.slots);
    work->st.animId = 1;
    work->st.state  = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable  = D_actor_150400_8013C8C4;
    _pairWalkUpdate(task);
    task->state++;
}

#include "../../shared/pair_walk_update.inc.c"

/// Per-frame callback of the actor's task: runs the state's handler, the spawn
/// handler `func_actor_150400_80132014` in state 0 and the per-frame update
/// `_actorRenderWalkerFrame` after it, passing the task's `Enemy`.
void func_actor_150400_801323E0(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_150400_80132014,
        _actorRenderWalkerFrame,
    };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(Enemy*, Task*) function declared in the prologue.
/// This identifier alias evaluates no arguments; undefine after the fragment.
#define ACTOR_RENDER_WALKER_FRAME _actorRenderWalkerFrame
/// Selects this frame instance's motion and animation update.
///
/// Bind to a declared static void(Task*) function for the same task and work.
/// The frame calls it once after lighting and before drawing the shadow.
/// This object-like identifier alias captures no locals or constructed tokens;
/// undefine it after each inclusion of walker_frame.inc.c.
#define ACTOR_RENDER_UPDATE_WALKER _pairWalkUpdate
/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Exit callback of the actor's task: hands its `Enemy` back to
/// `enemyDestroy`.
static void func_actor_150400_801324B8(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// Names this carrier's private room-shaded shadow function for one inclusion.
///
/// Bind to the prologue's `static void name(Task* task)` declaration; the
/// replacement is one identifier and evaluates no arguments or object state.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW

#include "../../shared/pair_walk_tick_anim.inc.c"

#include "../../shared/pair_walk_reset_anim.inc.c"

#include "../../shared/pair_walk_reseed_anim.inc.c"

#include "../../shared/pair_walk_play.inc.c"

#include "../../shared/pair_walk_visibility.inc.c"

#include "../../shared/pair_walk_place.inc.c"

s32 func_actor_150400_801327EC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Script opcode: walk to `target`. Aims the actor's root coordinate at it by
/// the yaw of the horizontal offset from the coordinate's own translation,
/// caches that yaw in `yaw` and rebuilds the local matrix from it, then sets
/// `travel` to the distance divided by 17, the step body's per-frame stride.
s32 func_actor_150400_801327F4(Task* task, s32 arg1, ActorTransform* target, s32 arg3)
{
    GfxCoord*     coord;
    PairWalkWork* work;
    s32           dx;
    s32           dz;
    u16           yaw;

    coord        = task->extra.tmd->coords;
    work         = task->work;
    dx           = target->pos.vx - coord->coord.t[0];
    dz           = target->pos.vz - coord->coord.t[2];
    yaw          = ratan2(dx, dz);
    work->st.yaw = yaw;
    gfxRotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 17;
    return 0;
}

#include "../../shared/pair_walk_sub_model.inc.c"
