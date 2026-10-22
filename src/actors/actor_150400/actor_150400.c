#include "actors/actor_150400.h"
#include "actors/task_tables.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/companion_load.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
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
#include "main/tmd_types.h"

#include "rooms/shelter_b1_control_room.h"
#include "../../shared/walk/pair_walk.h"

static s32  _pairWalkPlay(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static void _pairWalkSubModelTask(Task* task);

static Task* _gActor150400FirstSlidingModelTask;
static Task* _gActor150400SecondSlidingModelTask;

// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

/// Scratchpad stack pointer the per-frame helpers carve temporary frames off.

static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _actor150400DestroyPairWalker(Task* task);
static void _actorRenderDrawWalkerGroundShadow(Task* task);

static TmdSource _gGolemNo9BodyDryfield;
static TmdSource _gGolemBeamSword;
static void      _actor150400PairWalkerTask(Task* task);

static s32 _actor150400IgnorePairWalkCommand(Task* unusedTask, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArgument);
static s32 _actor150400PairWalkSetWalkTarget(Task* task, s32 messageId, const ActorTransform* target, s32 unusedArgument);

/// Sliding-model states posted by the scene event script.
enum {
    ACTOR_150400_SLIDING_MODEL_INITIALIZE = 0,
    ACTOR_150400_SLIDING_MODEL_MOVE       = 2,
};

static void _actor150400SlidingModelTask(Task* task);
static void _actor150400ReloadMainCorridor(void);
static void _actor150400StartControlRoomMovie(void);
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

enum { ACTOR_150400_SLIDING_MODEL_PRIORITY = 32 };

/// Recipe used twice for the freezer scene's sliding TMD models.
///
/// Spawn argument 1 selects copy 1 or 2; scene cues initialize and then slide
/// both live tasks. Spawning skips automatic primitive-buffer allocation.
static TaskDesc _gActor150400SlidingModelTaskDesc = {
    .header   = { .fields = { .flags = (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), .priority = ACTOR_150400_SLIDING_MODEL_PRIORITY } },
    .callback = _actor150400SlidingModelTask,
    .data     = { .model = &_gActor150400Model00C44 },
};

/// Retained bank-0 clip-1 reset request with no recovered consumer.
///
/// Collision is disabled and reset ignores the zero blend duration. No receiver
/// is established, so this is not assigned to the player or scene walker.
static AnimationPlayRequest _gActor150400UnusedAnimation1Request = {
    .source               = { .index = 0 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

enum {
    ACTOR_150400_FREEZER_WALK_BLEND_FRAMES = 10,
    ACTOR_150400_FREEZER_PLAYER_CLIP       = 1,
};

/// Starts No. 9's walk clip with a ten-frame blend in the freezer scene.
///
/// The walker consumes clip and blend values during dispatch; its bank-selector
/// and world-collision words are ignored. Keep the clip bank loaded during travel.
static AnimationPlayRequest _gActor150400FreezerNo9Animation4Request = {
    .source               = { .index = 0 },
    .animationId          = PAIR_WALK_ANIM_WALK,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = ACTOR_150400_FREEZER_WALK_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets equipped-weapon clip 1 and drops player grid participation for the freezer scene.
///
/// The event interpreter copies this request and resolves its bank selector
/// against the equipped weapon before synchronous player dispatch.
static AnimationPlayRequest _gActor150400FreezerPlayerAnimation1Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_150400_FREEZER_PLAYER_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// No. 9's initial freezer-scene position and negative-quarter-turn yaw.
///
/// XYZ are whole units in the model root's parent frame. The placement message
/// consumes the record synchronously; the fourth vector components have no role.
static ActorTransform _gActor150400FreezerNo9Placement = {
    .pos = { 6705, -500, -3316, 0 },
    .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// No. 9's freezer-scene walk destination in the root's parent-coordinate frame.
///
/// Only X/Z are consumed: the receiver stores a derived yaw and whole 17-unit
/// travel ticks. Y and rotation are retained data but do not affect this request.
static ActorTransform _gActor150400FreezerNo9WalkTarget = {
    .pos = { 1535, -500, -3316, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 },
};

/// Scene/audio stream key for the freezer confrontation.
///
/// Selects group 5, stream 4, qualifier 11; the absent second qualifier is zero.
/// Event playback borrows this key while the actor overlay remains loaded.
static EvsSceneKey _gActor150400FreezerSceneKey = { .group = 5, .streamId = 4, .subId = 11 };

enum {
    ACTOR_150400_CAP_FREEZER_SCENE = 2,
    ACTOR_150400_WEAPONS_PLAYER    = 0,
    ACTOR_150400_FREEZER_SKIP_VIEW = 3,
};

EvsCommand gActor150400FreezerSceneScript[33] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor150400FreezerSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_150400_CAP_FREEZER_SCENE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor150400FreezerPlayerAnimation1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = ACTOR_150400_WEAPONS_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor150400SetSlidingModelState }, { .value = ACTOR_150400_SLIDING_MODEL_INITIALIZE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor150400FreezerNo9Placement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor150400FreezerNo9Animation4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .transform = &_gActor150400FreezerNo9WalkTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor150400SetSlidingModelState }, { .value = ACTOR_150400_SLIDING_MODEL_MOVE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor150400StartControlRoomMovie }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor150400ReloadMainCorridor }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor150400FreezerSceneSkipScript[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor150400StartControlRoomMovie }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor150400ReloadMainCorridor }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_150400_FREEZER_SKIP_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_150400_WEAPONS_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "assets/enemies/golem/no_9/body_dryfield.inc"

#include "assets/rooms/shelter/b1_control_room/no_9_clips.inc"

#include "assets/enemies/golem/beam_sword.inc"

/// No. 9's animation, visibility, placement, ignored-command and walk messages.
///
/// Five handlers are followed by the end sentinel. The live pair walker borrows
/// this table; animation IDs must select loaded entries 1..4 in its clip bank.
static TaskMessageEntry _gActor150400No9Messages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _pairWalkPlay },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _pairWalkSetVisibility },
    { ACTOR_MESSAGE_PLACE, _pairWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor150400IgnorePairWalkCommand },
    { ACTOR_MESSAGE_WALK_TO, _actor150400PairWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

enum { ACTOR_150400_GOLEM_TASK_PRIORITY = 96 };

enum { ACTOR_150400_BEAM_SWORD_DESCRIPTOR = 1 };

TaskDesc gActor150400No9GolemTaskDescriptors[2] = {
    // 0: body.
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_150400_GOLEM_TASK_PRIORITY } },
        .callback = _actor150400PairWalkerTask,
        .data     = { .model = &_gGolemNo9BodyDryfield },
    },
    // 1: beam sword.
    {
        .header   = { .fields = { .flags = (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), .priority = ACTOR_150400_GOLEM_TASK_PRIORITY } },
        .callback = _pairWalkSubModelTask,
        .data     = { .model = &_gGolemBeamSword },
    },
};

/// No. 9's six-entry body-animation bank, with loaded clips 1..4.
///
/// Entries 0 and 5 are unplayable NULL holes. Idle uses 1 and walking uses 4;
/// message requests may also select 2 or 3. Playback borrows the sets and tracks
/// while this overlay is loaded. The table is six native PS1 pointer words.
#include "assets/rooms/shelter/b1_control_room/no_9_bank.inc"

/// Retained first sliding-model task of the freezer scene, placed at Z = -4460.
///
/// Spawn replaces the handle; later scene cues require a successful, still-live
/// task. Teardown does not clear it, and another spawn replaces the old handle.
static Task* _gActor150400FirstSlidingModelTask = NULL;

/// Retained second sliding-model task of the freezer scene, placed at Z = -4120.
///
/// Shares hold/slide cues with the first copy. Spawn must succeed and the task
/// must remain live for those cues; the handle is left uncleared on teardown.
static Task* _gActor150400SecondSlidingModelTask;

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
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view != ACTOR_150400_SLIDER_VIEW) {
        model->flags = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    } else {
        model->flags    = 0;
        model->otOffset = 0;
    }
}

/// Completes the freezer scene and queues arrival in the B2 main corridor.
///
/// Demo scene 9 suppresses every side effect. Otherwise enables display, records
/// the freezer unlock, requests character images and applies the control room's
/// saved-area update before selecting warp 4 in room 1 with sprite variant 1.
/// Queues a captured-frame reload, then finishes scene streaming to restore its
/// saved RNG state. Requires the loaded room update list, reload bank and a
/// previously selected scene. Neither resource loading nor task spawning is waited on.
static void _actor150400ReloadMainCorridor(void)
{
    enum {
        ACTOR_150400_DEMO_SCENE                   = 9,
        ACTOR_150400_FREEZER_UNLOCKED             = 1,
        ACTOR_150400_MAIN_CORRIDOR_ARRIVAL_WARP   = 4,
        ACTOR_150400_MAIN_CORRIDOR_ROOM           = 1,
        ACTOR_150400_MAIN_CORRIDOR_SPRITE_VARIANT = 1,
    };

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != ACTOR_150400_DEMO_SCENE) {
        SetDispMask(true);
        gameFlagSetNibble(GAME_FLAG_GOLEM_FREEZER_UNLOCKED, ACTOR_150400_FREEZER_UNLOCKED);
        loadingEnqueueCharacterResources(true);
        areaApplySavedUpdates(gShelterB1ControlRoomFreezerSceneAreaUpdates);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_MINE_SHELTER;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_SHELTER_B2_MAIN_CORRIDOR;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = ACTOR_150400_MAIN_CORRIDOR_ARRIVAL_WARP;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = ACTOR_150400_MAIN_CORRIDOR_ROOM;
        gDisplayState.spriteVariant                                 = ACTOR_150400_MAIN_CORRIDOR_SPRITE_VARIANT;
        taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
        streamFinishScene();
    }
}

/// Queues the control room's movie-start task for either event-script ending.
///
/// Requires the loaded room descriptors and movie resources to outlive playback.
/// The bodyless trigger transfers display ownership on its next tick; this call
/// does not wait or check spawn failure. The scripts wait three ticks before
/// requesting their destination reload.
static void _actor150400StartControlRoomMovie(void)
{
    enum { ACTOR_150400_CONTROL_ROOM_MOVIE_TASK_INDEX = 0 };

    taskSpawnFromTable(gShelterB1ControlRoomMovieTaskDescs, ACTOR_150400_CONTROL_ROOM_MOVIE_TASK_INDEX, 0, 0);
}

/// Applies one state word to both scene sliding-model tasks.
///
/// Both retained tasks must be live. States 0, 1 and 2 initialize placement,
/// hold and slide respectively. The event script posts 0 before the scene and
/// 2 at its movement cue; this allocates no task or work.
static void _actor150400SetSlidingModelState(s32 state)
{
    _gActor150400FirstSlidingModelTask->state  = state;
    _gActor150400SecondSlidingModelTask->state = state;
}

void actor150400SpawnSlidingModels(void)
{
    enum {
        ACTOR_150400_SLIDING_FIRST_COPY  = 1,
        ACTOR_150400_SLIDING_SECOND_COPY = 2,
    };

    _gActor150400FirstSlidingModelTask  = taskSpawnFromTable(&_gActor150400SlidingModelTaskDesc, 0, ACTOR_150400_SLIDING_FIRST_COPY, 0);
    _gActor150400SecondSlidingModelTask = taskSpawnFromTable(&_gActor150400SlidingModelTaskDesc, 0, ACTOR_150400_SLIDING_SECOND_COPY, 0);
}

/// Binds work-owned light/color matrices and samples cached root lighting.
///
/// Requires live model/work/root pointers with stable, side-effect-free
/// expressions; model, work and rootCoord occur repeatedly. sample is a
/// writable VECTOR lvalue, also repeated. The model borrows both matrices
/// until teardown. Reads workm without composition, with Y decreased by the
/// captured 800-unit sample offset, and initializes all three light rows.
/// The query consumes sample XYZ synchronously. Invoke within braces.
#define ACTOR_150400_INITIALIZE_MODEL_LIGHTING(model, work, rootCoord, sample)            \
    {                                                                                     \
        (model)->lightMtx = &(work)->light;                                               \
        (model)->colorMtx = &(work)->color;                                               \
        (sample).vx       = (rootCoord)->workm.t[0];                                      \
        (sample).vy       = (rootCoord)->workm.t[1] - ACTOR_150400_LIGHT_SAMPLE_Y_OFFSET; \
        (sample).vz       = (rootCoord)->workm.t[2];                                      \
        worldCoordSetModelLighting((model), &(sample), 0, ACTOR_150400_LIGHT_COUNT);      \
    }

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
    attachmentEnemy                  = enemySpawnFromTable(gActor150400No9GolemTaskDescriptors, ACTOR_150400_BEAM_SWORD_DESCRIPTOR, 0, enemy);
    _actorRenderApplyPlacementTextureOffsets(attachmentEnemy->task->extra.tmd, enemy);
    taskReparent(task, attachmentEnemy->task);
    work->pairTask = attachmentEnemy->task;
    ACTOR_150400_INITIALIZE_MODEL_LIGHTING(model, work, rootCoord, lightPosition);
    animationInitContext(&work->rig.anim, _gActor150400No9AnimationSets, model,
                         work->rig.poses, work->rig.slots);
    work->st.animId = ACTOR_150400_SPAWN_CLIP;
    work->st.state  = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable  = _gActor150400No9Messages;
    _pairWalkUpdate(task);
    task->state++;
}
#undef ACTOR_150400_INITIALIZE_MODEL_LIGHTING

#include "../../shared/walk/pair_walk_update.inc.c"

/// Dispatches the freezer scene's No. 9 pair walker initialization or frame update.
///
/// Descriptor 0 requires a live Enemy in `spawnArg2.pointer`, a nineteen-part
/// TMD body and state 0..1. State 0 allocates the pair-walk work and attachment;
/// state 1 composes, lights, moves and animates the model and draws its shadow.
/// Initialization may destroy the enemy and task on allocation failure.
/// The handler array has no bounds check; keep this overlay and clips loaded.
static void _actor150400PairWalkerTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        _actor150400PairWalkSpawn,
        _actorRenderWalkerFrame,
    };

    stateHandlers[task->state](task->spawnArg2.pointer, task);
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
#include "../../shared/walk/walker_frame.inc.c"
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

/// Selects this pair walker's private ground shadow definition with room shading.
///
/// Bind to one function identifier declared `static void (Task*)` in the prologue.
/// Use the same identifier for `ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW` when
/// including `walker_frame.inc.c` for this walker. This object-like alias takes
/// no arguments, captures no locals and uses no stringification or token pasting.
/// Define it only around `actor_render_walker_shadow.inc.c`, then undefine it;
/// further shadow instances need distinct function identifiers and prototypes.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/actor/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW

#include "../../shared/walk/pair_walk.inc.c"

#include "../../shared/walk/pair_walk_play.inc.c"

#include "../../shared/walk/pair_walk_visibility.inc.c"

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

#include "../../shared/walk/pair_walk_sub_model.inc.c"
