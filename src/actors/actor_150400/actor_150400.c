#include "actors/actor_150400.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/companion_load.h"
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
static void _actor150400DestroyPairWalker(Task* task);
static void _actorRenderDrawWalkerGroundShadow(Task* task);

static TmdSource _gActor150400No9GolemDryfieldBody;
static TmdSource _gActor150400GolemBeamSword;
void             func_actor_150400_801323E0(Task*);

static s32 _actor150400IgnorePairWalkCommand(Task* unusedTask, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArgument);
static s32 _actor150400PairWalkSetWalkTarget(Task* task, s32 messageId, const ActorTransform* target, s32 unusedArgument);

void func_actor_150400_80131ECC(void);
void func_actor_150400_80131F6C(void);

void func_actor_150400_80131ECC(void);
void func_actor_150400_80131F6C(void);

/// Sliding-model states posted by the scene event script.
enum {
    ACTOR_150400_SLIDING_MODEL_INITIALIZE = 0,
    ACTOR_150400_SLIDING_MODEL_MOVE       = 2,
};

static void _actor150400SlidingModelTask(Task* task);
void        func_actor_150400_80131ECC(void);
void        func_actor_150400_80131F6C(void);
static void _actor150400SetSlidingModelState(s32 state);

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

TaskDesc D_actor_150400_80132CF0 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 32 } }, _actor150400SlidingModelTask, { .model = &_gActor150400Model00C44 } };

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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor150400SetSlidingModelState }, { .value = ACTOR_150400_SLIDING_MODEL_INITIALIZE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor150400SetSlidingModelState }, { .value = ACTOR_150400_SLIDING_MODEL_MOVE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor150400IgnorePairWalkCommand },
    { ACTOR_MESSAGE_WALK_TO, _actor150400PairWalkSetWalkTarget },
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

static void _actor150400PairWalkSpawn(Enemy* enemy, Task* task);

/// Places and slides one of the freezer scene's paired models along X.
///
/// Requires a live TMD root. State zero places the first copy for spawn argument
/// one and the second for every other value, then enters hold state one.
/// State two slides four parent-coordinate units per update and clamps X at 1030.
/// Only saved view five draws either copy; every update replaces all draw flags.
/// Each transform change marks composition dirty. The scene sets both task states
/// together; this callback retains no work allocation.
static void _actor150400SlidingModelTask(Task* task)
{
    enum {
        ACTOR_150400_SLIDER_INITIALIZE = 0,
        ACTOR_150400_SLIDER_HOLD       = 1,
        ACTOR_150400_SLIDER_MOVE       = 2,
        ACTOR_150400_SLIDER_VIEW       = 5,
        ACTOR_150400_SLIDER_FIRST_COPY = 1,
        ACTOR_150400_SLIDER_START_X    = 730,
        ACTOR_150400_SLIDER_END_X      = 1030,
        ACTOR_150400_SLIDER_Y          = -1380,
        ACTOR_150400_SLIDER_FIRST_Z    = -4460,
        ACTOR_150400_SLIDER_SECOND_Z   = -4120,
        ACTOR_150400_SLIDER_STEP_UNITS = 4,
    };
    TmdObject* model     = task->extra.tmd;
    GfxCoord*  rootCoord = model->coords;

    if (task->state == ACTOR_150400_SLIDER_INITIALIZE) {
        rootCoord->coord.t[0] = ACTOR_150400_SLIDER_START_X;
        rootCoord->coord.t[1] = ACTOR_150400_SLIDER_Y;
        if (task->spawnArg1.value == ACTOR_150400_SLIDER_FIRST_COPY) {
            rootCoord->coord.t[2] = ACTOR_150400_SLIDER_FIRST_Z;
        } else {
            rootCoord->coord.t[2] = ACTOR_150400_SLIDER_SECOND_Z;
        }
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        task->state            += ACTOR_150400_SLIDER_HOLD - ACTOR_150400_SLIDER_INITIALIZE;
    }
    if (task->state == ACTOR_150400_SLIDER_MOVE) {
        rootCoord->coord.t[0] += ACTOR_150400_SLIDER_STEP_UNITS;
        if (rootCoord->coord.t[0] > ACTOR_150400_SLIDER_END_X) {
            rootCoord->coord.t[0] = ACTOR_150400_SLIDER_END_X;
        }
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view != ACTOR_150400_SLIDER_VIEW) {
        model->flags = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    } else {
        model->flags    = 0;
        model->otOffset = 0;
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
        taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
        streamFinishScene();
    }
}

void func_actor_150400_80131F6C(void)
{
    taskSpawnFromTable(D_shelter_b1_control_room_80181BBC, 0, 0, 0);
}

/// Applies one state word to both scene sliding-model tasks.
///
/// Both retained tasks must be live. States 0, 1 and 2 initialize placement,
/// hold and slide respectively. The event script posts 0 before the scene and
/// 2 at its movement cue; this allocates no task or work.
static void _actor150400SetSlidingModelState(s32 state)
{
    D_actor_150400_8013C924->state = state;
    D_actor_150400_8013C928->state = state;
}

void actor150400SpawnSlidingModels(void)
{
    enum {
        ACTOR_150400_SLIDING_FIRST_COPY  = 1,
        ACTOR_150400_SLIDING_SECOND_COPY = 2,
    };

    D_actor_150400_8013C924 = taskSpawnFromTable(&D_actor_150400_80132CF0, 0, ACTOR_150400_SLIDING_FIRST_COPY, 0);
    D_actor_150400_8013C928 = taskSpawnFromTable(&D_actor_150400_80132CF0, 0, ACTOR_150400_SLIDING_SECOND_COPY, 0);
}

/// Binds the walker’s work matrices and samples its cached root lighting.
///
/// Standalone statements only. All four arguments must be side-effect-free
/// lvalues; model/work/root are read repeatedly and sample is a writable VECTOR.
/// Captures the spawn’s 800-unit Y offset and three-row light count. The model
/// borrows the matrices until teardown; root composition is not refreshed.
#define ACTOR_150400_INITIALIZE_MODEL_LIGHTING(model, work, rootCoord, sample)        \
    (model)->lightMtx = &(work)->light;                                               \
    (model)->colorMtx = &(work)->color;                                               \
    (sample).vx       = (rootCoord)->workm.t[0];                                      \
    (sample).vy       = (rootCoord)->workm.t[1] - ACTOR_150400_LIGHT_SAMPLE_Y_OFFSET; \
    (sample).vz       = (rootCoord)->workm.t[2];                                      \
    worldCoordSetModelLighting((model), &(sample), 0, ACTOR_150400_LIGHT_COUNT)

/// Initializes the freezer scene pair walker and creates its attachment task.
///
/// Allocates zeroed task-owned work, destroying the enemy on failure. Requires
/// a live model/Enemy and descriptor slot 1 to produce a live attachment. Parents
/// the untargetable root to the view, textures the attachment from the owner’s
/// area placement and adopts it into task teardown. The model borrows work-owned
/// lighting matrices and samples cached XYZ with Y minus 800, before composition.
/// Binds the loaded clip table, requests clip 1, installs messages/exit, updates
/// once and advances spawn state 0 to running state 1. Resources must outlive work.
static void _actor150400PairWalkSpawn(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_150400_SPAWN_CLIP            = 1,
        ACTOR_150400_LIGHT_SAMPLE_Y_OFFSET = 800,
        ACTOR_150400_LIGHT_COUNT           = 3,
    };

    VECTOR        lightPosition;
    PairWalkWork* work;
    GfxCoord*     rootCoord;
    TmdObject*    model;
    Enemy*        attachmentEnemy;

    model      = task->extra.tmd;
    rootCoord  = model->coords;
    task->work = (work = memCalloc(sizeof(PairWalkWork), false));
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _actor150400DestroyPairWalker;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->flags                     = 0;
    model->otOffset                  = 1;
    work->enemy                      = enemy;
    attachmentEnemy                  = enemySpawnFromTable(D_actor_150400_8013C8F4, 1, 0, enemy);
    _actorRenderApplyPlacementTextureOffsets(attachmentEnemy->task->extra.tmd, enemy);
    taskReparent(task, attachmentEnemy->task);
    work->pairTask = attachmentEnemy->task;
    ACTOR_150400_INITIALIZE_MODEL_LIGHTING(model, work, rootCoord, lightPosition);
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_150400_8013C90C, model,
                         work->rig.poses, work->rig.slots);
    work->st.animId = ACTOR_150400_SPAWN_CLIP;
    work->st.state  = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable  = D_actor_150400_8013C8C4;
    _pairWalkUpdate(task);
    task->state++;
}
#undef ACTOR_150400_INITIALIZE_MODEL_LIGHTING

#include "../../shared/pair_walk_update.inc.c"

/// Per-frame callback of the actor's task: runs the state's handler, the spawn
/// handler `_actor150400PairWalkSpawn` in state 0 and the per-frame update
/// `_actorRenderWalkerFrame` after it, passing the task's `Enemy`.
void func_actor_150400_801323E0(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        _actor150400PairWalkSpawn,
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

/// Releases the pair walker's enemy through its task exit callback.
///
/// spawnArg2 must retain the Enemy that owns this task. enemyDestroy coordinates
/// enemy/task teardown; the callback makes no access after that call.
static void _actor150400DestroyPairWalker(Task* task)
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

/// Accepts this pair walker's actor-command message without changing anything.
///
/// All arguments, including the borrowed command payload, are ignored. Returns 0.
static s32 _actor150400IgnorePairWalkCommand(Task* unusedTask, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArgument)
{
    return 0;
}

/// Aims the pair walker at a destination and records its remaining 17-unit travel ticks.
///
/// Requires live PairWalkWork and a TMD root. Borrows only target X/Z in the root
/// parent's frame; Y and rotation are ignored. Stores yaw in 4096 units per turn
/// and floor(horizontal distance / 17) in signed-halfword travel without clamping.
/// The signed square sum must be representable and travel must fit 0..32767.
/// Replaces rotation without invalidating composition or starting animation;
/// a separate walk-clip request starts motion. Other message arguments are ignored.
/// The destination is retained only as yaw and travel; returns 0.
static s32 _actor150400PairWalkSetWalkTarget(Task* task, s32 messageId, const ActorTransform* target, s32 unusedArgument)
{
    enum { ACTOR_150400_PAIR_WALK_STEP_UNITS = 17 };
    GfxCoord*     rootCoord;
    PairWalkWork* work;
    s32           deltaX;
    s32           deltaZ;
    u16           yaw;

    rootCoord    = task->extra.tmd->coords;
    work         = task->work;
    deltaX       = target->pos.vx - rootCoord->coord.t[0];
    deltaZ       = target->pos.vz - rootCoord->coord.t[2];
    yaw          = ratan2(deltaX, deltaZ);
    work->st.yaw = yaw;
    gfxRotMatrixY(&rootCoord->coord, (s16)yaw, GRAPHICS_ROTATION_REPLACE);
    work->st.travel = SquareRoot0(deltaX * deltaX + deltaZ * deltaZ) / ACTOR_150400_PAIR_WALK_STEP_UNITS;
    return 0;
}

#include "../../shared/pair_walk_sub_model.inc.c"
