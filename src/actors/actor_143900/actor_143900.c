#include "actors/actor_143900.h"
#include "actors/task_tables.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
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

#include "rooms/shelter_r49.h"
// The scripted walk's update and walk-to handler run on the first walker's
// block; the second walker's copies of the two rebind the type to its own.
#define SCRIPTED_WALK_WORK_T _Actor143900Work
#include "../../shared/walk/scripted_walk.h"

/// Draw-request bits shared by this package's two walker message handlers.
enum {
    ACTOR_143900_WALKER_DRAW_SHOW             = 1,
    ACTOR_143900_WALKER_DRAW_SKIP_AUTO_BUFFER = 2,
};

/// Results of either walker's play-animation request, without validating the loaded-clip domain.
enum {
    ACTOR_143900_PLAY_ANIMATION_APPLIED  = 0,
    ACTOR_143900_PLAY_ANIMATION_REJECTED = -1,
};

/// Both walkers start on clip 1 and sample three lights 800 coordinate units above the root.
enum {
    ACTOR_143900_WALKER_INITIAL_CLIP   = 1,
    ACTOR_143900_WALKER_LIGHT_Y_OFFSET = 800,
    ACTOR_143900_WALKER_LIGHT_COUNT    = 3,
};

static s16 _gScriptedWalkMode;

/// Work block of the package's first walker, allocated zeroed by the walker's
/// spawn state and kept both at `Task::work` and in `_gScriptedWalkWork`.
///
/// It is the block of a scripted walker that carries nothing: the two light
/// matrices, the rig, the animation request and the turn countdown, which is
/// where `ScriptedWalkAttachmentsWork`, the block of the package's second
/// walker, goes on to its two attachment tasks.
typedef struct {
    MATRIX          light;      // Light-direction matrix lent to the model object
    MATRIX          color;      // Light-colour matrix lent to the model object
    ActorAnimRig20  rig;        // Playback storage of the twenty-part model; slots 1 to 19 are driven
    ActorEnemyState st;         // Animation request, heading last given the root and frames of walk left
    s16             turnFrames; // Frames the update still turns the model for while the turn clip plays; command 0 starts 20
} _Actor143900Work;
STATIC_ASSERT_SIZEOF(_Actor143900Work, 0x4F0);

static _Actor143900Work* _gScriptedWalkWork;

/// The first variant's message table; its spawn routine publishes it as
/// `Task::msgTable`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

static ScriptedWalkAttachmentsWork* _gScriptedWalkSecondWork;

static Task* _gActor143900SecondScriptedWalkerTask;

/// Approach mode of the second walker, stored as `SCRIPTED_WALK_MODE_*`.
///
/// The walk-to message narrows its argument to this signed halfword. Its
/// independent value selects a 60-unit forward, 15-unit backward or 25-unit
/// forward step until the next approach message.
static s16 _gScriptedWalkSecondMode;

static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _actor143900ExitScriptedWalker(Task* task);
static void _scriptedWalkUpdateSecond(Task* task);
static void _actorRenderWalkerFrameSecond(Enemy* unusedEnemy, Task* task);
static void _actor143900ExitSecondScriptedWalker(Task* task);
static void _actorRenderDrawWalkerGroundShadow(Task* task);
static void _actorRenderDrawSecondWalkerGroundShadow(Task* task);
static void _scriptedWalkTickSecondAnim(void);
static void _scriptedWalkResetSecondAnim(void);
static void _scriptedWalkBlendSecondAnim(void);

static TmdSource _gStaffOfficerBody;
static TmdSource _gActor143900Model173A8;
static TmdSource _gActor143900Model17688;
static s32       _actor143900PlaySecondScriptedWalkerAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32       _actor143900SetSecondScriptedWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument);
static s32       _scriptedWalkPlaceSecond(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument);
static s32       _actor143900SelectSecondScriptedWalkerAttachment(Task* unusedTask, s32 messageId, const ActorCommand* request, s32 unusedArgument);
static s32       _scriptedWalkToSecond(Task* task, s32 messageId, const VECTOR* target, s32 mode);
static void      _actor143900SecondScriptedWalkerTask(Task* task);
static void      _actor143900ScriptedWalkerAttachmentTask(Task* task);
static void      _actor143900SpawnSecondScriptedWalker(Enemy* enemy, Task* task);

static s32  _actor143900PlayScriptedWalkerAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32  _actor143900SetScriptedWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument);
static s32  _actor143900ApplyScriptedWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* request, s32 unusedArgument);
static void _actor143900ScriptedWalkerTask(Task* task);
static void _actor143900SpawnScriptedWalker(Enemy* enemy, Task* task);

static void _actor143900StartObservatoryTransition(void);

/// Entry-scene player request: equipped-bank clip 1, reset, world collision disabled.
///
/// The weapon-animation opcode copies the request and replaces its stored bank index.
static AnimationPlayRequest _gActor143900EntryPlayerAnimation = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Entry-scene walker request for clip 1, reset without world collision.
static AnimationPlayRequest _gActor143900EntryWalkerAnimation1 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Entry-scene walker request for clip 2, reset without world collision.
static AnimationPlayRequest _gActor143900EntryWalkerAnimation2 = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Entry-scene walker request for clip 3, reset without world collision.
static AnimationPlayRequest _gActor143900EntryWalkerAnimation3 = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Entry-scene walker request for clip 4, reset without world collision.
static AnimationPlayRequest _gActor143900EntryWalkerAnimation4 = { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand gActor143900ObservatoryEntryScript[32] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54310002 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor143900EntryPlayerAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor143900EntryWalkerAnimation1 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54310001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54310003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor143900EntryWalkerAnimation2 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor143900EntryWalkerAnimation3 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor143900EntryWalkerAnimation4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor143900StartObservatoryTransition }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor143900ObservatoryEntrySkipScript[12] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor143900StartObservatoryTransition }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

#include "assets/characters/president/body.inc"

#include "assets/characters/president/office_scenes_clips_1.inc"

/// Latched duration of the first walker's next child-part blend, in whole normal-rate frames.
///
/// Play requests narrow `AnimationPlayRequest.blendFrames` to this signed
/// halfword; walk completion replaces it with `SCRIPTED_WALK_IDLE_BLEND_FRAMES`.
/// Plain resets leave it intact. Zero requests no transition time; 0..2047
/// keeps the playback timer nonnegative. The range is not checked.
static s16 _gScriptedWalkBlendFrames = SCRIPTED_WALK_DEFAULT_BLEND_FRAMES;

/// Terminated message table for the first twenty-part scripted walker.
///
/// Dispatches animation, drawing, placement, command and walk-to messages.
/// The task borrows the table until teardown; callbacks use its initialized
/// work and the carrier's published singleton walker state.
static TaskMessageEntry _gActor143900ScriptedWalkerMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor143900PlayScriptedWalkerAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor143900SetScriptedWalkerModelDraw },
    { ACTOR_MESSAGE_PLACE, SCRIPTED_WALK_PLACE },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor143900ApplyScriptedWalkerCommand },
    { ACTOR_MESSAGE_WALK_TO, SCRIPTED_WALK_TO },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor143900ScriptedWalkerTaskDesc = { { { TASK_BODY_TMD, 192 } }, _actor143900ScriptedWalkerTask, { .model = &_gPresidentBody } };

/// Borrowed animation sets for the first scripted walker.
///
/// Clips 1..19 are loaded; slot 0 is NULL. The rig borrows this writable table
/// and its descriptors for the walker lifetime; requests must select a loaded clip.
#include "assets/characters/president/bank.inc"

#include "assets/characters/staff_officer/body.inc"

#include "assets/characters/president/office_scenes_clips_2.inc"

static TmdBone _gActor143900Model173A8Skeleton[1] = {
#include "assets/actor_143900_model_173A8_skeleton.inc"
};

static u32 _gActor143900Model173A8PartVerts[1] = {
#include "assets/actor_143900_model_173A8_partVerts.inc"
};

static SVECTOR _gActor143900Model173A8Verts[18] = {
#include "assets/actor_143900_model_173A8_verts.inc"
};

static SVECTOR _gActor143900Model173A8Normals[9] = {
#include "assets/actor_143900_model_173A8_normals.inc"
};

static u32 _gActor143900Model173A8Stream[121] = {
#include "assets/actor_143900_model_173A8_stream.inc"
};

static TmdSource _gActor143900Model173A8 = {
    0,
    944,
    0,
    1,
    _gActor143900Model173A8PartVerts,
    _gActor143900Model173A8Verts,
    _gActor143900Model173A8Normals,
    _gActor143900Model173A8Skeleton,
    _gActor143900Model173A8Stream,
};

static TmdBone _gActor143900Model17688Skeleton[1] = {
#include "assets/actor_143900_model_17688_skeleton.inc"
};

static u32 _gActor143900Model17688PartVerts[1] = {
#include "assets/actor_143900_model_17688_partVerts.inc"
};

static SVECTOR _gActor143900Model17688Verts[14] = {
#include "assets/actor_143900_model_17688_verts.inc"
};

static SVECTOR _gActor143900Model17688Normals[8] = {
#include "assets/actor_143900_model_17688_normals.inc"
};

static u32 _gActor143900Model17688Stream[89] = {
#include "assets/actor_143900_model_17688_stream.inc"
};

static TmdSource _gActor143900Model17688 = {
    0,
    680,
    0,
    1,
    _gActor143900Model17688PartVerts,
    _gActor143900Model17688Verts,
    _gActor143900Model17688Normals,
    _gActor143900Model17688Skeleton,
    _gActor143900Model17688Stream,
};

/// Latched duration of the second walker's next child-part blend, in whole normal-rate frames.
///
/// Independent of the first walker's latch. Play requests narrow
/// `AnimationPlayRequest.blendFrames` to this signed halfword; walk completion
/// replaces it with `SCRIPTED_WALK_IDLE_BLEND_FRAMES`. Plain resets leave it
/// intact. Zero requests no transition time; 0..2047 keeps the playback timer
/// nonnegative. The range is not checked.
static s16 _gScriptedWalkSecondBlendFrames = SCRIPTED_WALK_DEFAULT_BLEND_FRAMES;

/// Second walker message dispatch: animation, drawing, placement, attachments and approach.
///
/// The terminated table borrows the second walker's published singleton work.
/// Animation requests must select a loaded clip, 1..10.
static TaskMessageEntry _gActor143900SecondScriptedWalkerMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor143900PlaySecondScriptedWalkerAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor143900SetSecondScriptedWalkerModelDraw },
    { ACTOR_MESSAGE_PLACE, _scriptedWalkPlaceSecond },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor143900SelectSecondScriptedWalkerAttachment },
    { ACTOR_MESSAGE_WALK_TO, _scriptedWalkToSecond },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor143900SecondScriptedWalkerTaskDescriptors[3] = {
    { { { TASK_BODY_TMD, 192 } }, _actor143900SecondScriptedWalkerTask, { .model = &_gStaffOfficerBody } },
    { { { TASK_BODY_TMD, 192 } }, _actor143900ScriptedWalkerAttachmentTask, { .model = &_gActor143900Model173A8 } },
    { { { TASK_BODY_TMD, 192 } }, _actor143900ScriptedWalkerAttachmentTask, { .model = &_gActor143900Model17688 } },
};

/// Borrowed animation sets for the second scripted walker.
///
/// Loaded twenty-track clips occupy 1..10; entries 0 and 11 are NULL and cannot
/// be played. Keep the table and descriptors loaded for the walker's lifetime.
#include "assets/characters/staff_officer/bank.inc"

/// Borrowed work block of the first scripted walker.
///
/// The spawn and dispatcher publish the allocation also held by `Task::work`.
/// Animation and message handlers require it to remain live; task teardown
/// releases it without clearing this pointer.
static _Actor143900Work* _gScriptedWalkWork = NULL;

/// Borrowed task of the first scripted walker.
///
/// Spawn publishes it for animation and draw requests. The singleton must be live
/// for those requests; teardown ends its lifetime without clearing the pointer.
static Task* _gActor143900ScriptedWalkerTask = NULL;

/// Signed approach mode of the first walker's last walk-to message (`SCRIPTED_WALK_MODE_*`).
static s16 _gScriptedWalkMode = 0;

/// Initialized halfword with no recovered consumer; its original role is unproven.
u16 D_actor_143900_801496C2 = 0x49E7;

/// Borrowed work block of the second scripted walker and its two model attachments.
///
/// The spawn and dispatcher publish the allocation also held by `Task::work`.
/// The fragment binding selects this pointer while operating on the second
/// walker. Task teardown ends its lifetime without clearing the pointer.
static ScriptedWalkAttachmentsWork* _gScriptedWalkSecondWork = NULL;

/// Borrowed task of the second scripted walker and its attachments.
///
/// Spawn publishes it for child attachment, animation and draw requests. The
/// singleton must stay live; teardown does not clear this pointer.
static Task* _gActor143900SecondScriptedWalkerTask;

/// Starts the room movie and transition to Neo Ark's observatory.
///
/// Both event-script endings call this while Shelter R49 is loaded. Demo scene
/// 9 suppresses the request. Otherwise stores scene event 20, which selects
/// music column 0 in the destination stage, and spawns the room transition
/// task. The room overlay and its movie resources must remain live until that
/// task reloads the destination. Spawn failure still leaves the event stored.
static void _actor143900StartObservatoryTransition(void)
{
    enum {
        ACTOR_143900_DEMO_SCENE              = 9,
        ACTOR_143900_OBSERVATORY_SCENE_EVENT = 20,
    };

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != ACTOR_143900_DEMO_SCENE) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACTOR_143900_OBSERVATORY_SCENE_EVENT;
        taskSpawnFromTable(gShelterR49ObservatoryTransitionTasks, SHELTER_R49_OBSERVATORY_TRANSITION_TASK_INDEX, 0, 0);
    }
}

/// Initializes the package's first scripted walker and starts its initial clip.
///
/// State 0 requires a live owned enemy and twenty-part TMD model. Allocates
/// zeroed primary-heap work, publishes it for the singleton handlers, and lends
/// its matrices and playback buffers to the model/rig until task teardown.
/// Allocation failure destroys the enemy and task without advancing state.
/// The root is parented to the view and made untargetable. Initial lighting
/// reads its cached translation with Y reduced by 800 coordinate units;
/// this does not compose the root or convert the sample to world space.
/// Requires loaded clips and the lighting/animation helpers' scratch/GTE state.
static void _actor143900SpawnScriptedWalker(Enemy* enemy, Task* task)
{
    enum { ACTOR_143900_WALKER_OT_OFFSET = 1 };
    VECTOR            lightSample;
    _Actor143900Work* walkerWork;
    TmdObject*        walkerModel;
    GfxCoord*         rootCoord;

    walkerModel = task->extra.tmd;
    rootCoord   = walkerModel->coords;
    // Publish the owned allocation before lending any of its storage.
    walkerWork         = memCalloc(sizeof(*walkerWork), false);
    _gScriptedWalkWork = walkerWork;
    task->work         = walkerWork;
    if (walkerWork == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _actor143900ExitScriptedWalker;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    walkerModel->otOffset            = ACTOR_143900_WALKER_OT_OFFSET;
    walkerModel->flags               = 0;
    walkerModel->lightMtx            = &_gScriptedWalkWork->light;
    walkerModel->colorMtx            = &_gScriptedWalkWork->color;
    lightSample.vx                   = rootCoord->workm.t[0];
    lightSample.vy                   = rootCoord->workm.t[1] - ACTOR_143900_WALKER_LIGHT_Y_OFFSET;
    _gActor143900ScriptedWalkerTask  = task;
    lightSample.vz                   = rootCoord->workm.t[2];
    worldCoordSetModelLighting(walkerModel, &lightSample, 0, ACTOR_143900_WALKER_LIGHT_COUNT);
    // Seed the child tracks now; the next task state performs ordinary updates.
    animationInitContext(&_gScriptedWalkWork->rig.anim, _gActor143900ScriptedWalkerAnimationSets, walkerModel,
                         _gScriptedWalkWork->rig.poses, _gScriptedWalkWork->rig.slots);
    _gScriptedWalkWork->st.animId  = ACTOR_143900_WALKER_INITIAL_CLIP;
    _gScriptedWalkWork->st.state   = ACTOR_ENEMY_ANIM_RESET;
    _gScriptedWalkWork->st.travel  = 0;
    _gScriptedWalkWork->turnFrames = 0;
    task->msgTable                 = _gActor143900ScriptedWalkerMessages;
    SCRIPTED_WALK_UPDATE(task);
    task->state += 1;
}

#include "../../shared/walk/scripted_walk_update.inc.c"

/// Dispatches initialization or one frame of the first scripted walker.
///
/// `task->state` must be 0 (spawn) or 1 (frame); indexing is unchecked.
/// The task owns its enemy in `spawnArg2.pointer`, TMD model and initialized
/// work. Republishes that work for the singleton animation/message handlers
/// before dispatch. State 0 creates the work and may destroy the task on failure.
static void _actor143900ScriptedWalkerTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        _actor143900SpawnScriptedWalker,
        _actorRenderWalkerFrame,
    };

    _gScriptedWalkWork = task->work;
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
#define ACTOR_RENDER_UPDATE_WALKER SCRIPTED_WALK_UPDATE
/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/walk/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases the first scripted walker's enemy and begins task/model teardown.
///
/// Requires a live task owning its enemy in `spawnArg2.pointer` and its work.
/// The published task/work pointers are left stale and must not be used again.
/// Model release follows the resident task system's immediate/deferred policy.
static void _actor143900ExitScriptedWalker(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// Names this carrier's private room-shaded shadow function for one inclusion.
///
/// Bind to the prologue's `static void name(Task* task)` declaration; the
/// replacement is one identifier and evaluates no arguments or object state.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/actor/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW

#include "../../shared/walk/scripted_walk_tick_anim.inc.c"

#include "../../shared/walk/scripted_walk_reset_anim.inc.c"

#include "../../shared/walk/scripted_walk_blend_anim.inc.c"

/// Reseeds the published first scripted walker from a borrowed animation request.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION`; receiver, message ID, second payload
/// and other request words are ignored. Requires a live walker and initialized
/// rig with loaded child-part tracks. Playable table keys are 1..19. The signed
/// check rejects IDs >= 20 with -1 but accepts zero and negative IDs; an accepted
/// ID narrows to `s16` and must select a loaded clip after narrowing.
/// Nonzero blend latches the low signed halfword of `blendFrames` in whole
/// normal-rate frames (0..2047 keeps playback nonnegative); zero blend restarts
/// and leaves that latch intact. Applies the reseed immediately, returns 0 on
/// acceptance and retains no request pointer.
static s32 _actor143900PlayScriptedWalkerAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum { ACTOR_143900_WALKER_CLIP_COUNT = (s32)ARRAY_SIZE(_gActor143900ScriptedWalkerAnimationSets) };

    if (request->animationId < ACTOR_143900_WALKER_CLIP_COUNT) {
        _gScriptedWalkWork->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            _gScriptedWalkBlendFrames    = request->blendFrames;
        } else {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        _gScriptedWalkWork->st.field_6 = 0;
        // Apply the restart now; ordinary movement waits for a later update.
        SCRIPTED_WALK_UPDATE(_gActor143900ScriptedWalkerTask);
        return ACTOR_143900_PLAY_ANIMATION_APPLIED;
    }
    return ACTOR_143900_PLAY_ANIMATION_REJECTED;
}

/// Replaces the published first scripted walker's model draw flags.
///
/// Requires a live published task/model. `ACTOR_MESSAGE_SET_MODEL_DRAW` bit 0
/// enables active drawing; bit 1 suppresses automatic primitive-buffer allocation.
/// Clears all other model flags and ignores other request bits. Allocates and
/// releases no buffers. Receiver, message ID and second payload are ignored;
/// returns 0.
static s32 _actor143900SetScriptedWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument)
{
    TmdObject* walkerModel;

    walkerModel = _gActor143900ScriptedWalkerTask->extra.tmd;
    if (drawFlags & ACTOR_143900_WALKER_DRAW_SHOW) {
        walkerModel->flags = 0;
    } else {
        walkerModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (drawFlags & ACTOR_143900_WALKER_DRAW_SKIP_AUTO_BUFFER) {
        walkerModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/walk/scripted_walk_place.inc.c"

/// Schedules twenty turning updates for the published first scripted walker.
///
/// Requires live work and a command borrowed through `ACTOR_COMMAND_MESSAGE_APPLY`.
/// Command 0 arms the countdown consumed while turn clip 3 plays; it does not
/// select that clip. Each turn adds 51/4096 turns to the root yaw independently
/// of actor freezing. Other commands do nothing. Context tags, receiver, message
/// ID and second payload are ignored; returns 0 and retains no request pointer.
static s32 _actor143900ApplyScriptedWalkerCommand(Task* unusedTask, s32 messageId, const ActorCommand* request, s32 unusedArgument)
{
    enum {
        ACTOR_143900_WALKER_COMMAND_TURN = 0,
        ACTOR_143900_WALKER_TURN_UPDATES = 20,
    };

    if (request->command == ACTOR_143900_WALKER_COMMAND_TURN) {
        _gScriptedWalkWork->turnFrames = ACTOR_143900_WALKER_TURN_UPDATES;
    }
    return 0;
}

#include "../../shared/walk/scripted_walk_to.inc.c"

/// Initializes the second scripted walker, its initial clip and two model attachments.
///
/// State 0 requires a live owned enemy and twenty-part TMD model. Allocates
/// zeroed primary-heap work and publishes it for the singleton handlers, lending
/// its matrices and rig storage until teardown. Allocation failure destroys the
/// enemy and task. The root becomes view-parented and untargetable; lighting uses
/// its cached translation with Y reduced by 800 coordinate units, without
/// composition or conversion to world space. Requires loaded clips and the
/// lighting/animation helpers' scratch/GTE state.
/// Attachment entries 1/2 borrow model parts 1/12 for their lifetime. Failed
/// attachment spawns leave NULL handles; later draw/command/exit handlers require
/// both spawns to have succeeded. The attachment tasks are owned separately from
/// the walker's task tree and killed explicitly by its exit callback.
static void _actor143900SpawnSecondScriptedWalker(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_143900_SECOND_WALKER_OT_OFFSET = 16,
        ACTOR_143900_ATTACHMENT1_ENTRY       = 1,
        ACTOR_143900_ATTACHMENT2_ENTRY       = 2,
        ACTOR_143900_ATTACHMENT1_PART        = 1,
        ACTOR_143900_ATTACHMENT2_PART        = 12,
    };
    VECTOR                       lightSample;
    ScriptedWalkAttachmentsWork* walkerWork;
    GfxCoord*                    rootCoord;
    TmdObject*                   walkerModel;
    Task*                        attachmentTask;

    walkerModel = task->extra.tmd;
    rootCoord   = walkerModel->coords;
    // Publish the owned allocation before lending its matrices and rig storage.
    walkerWork               = memCalloc(sizeof(*walkerWork), false);
    _gScriptedWalkSecondWork = walkerWork;
    task->work               = walkerWork;
    if (walkerWork == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback                    = _actor143900ExitSecondScriptedWalker;
    rootCoord->parent                     = &gGfxViewCoord;
    enemy->field_4                        = &rootCoord->coord;
    enemy->node.state.parts.flags         = WORLD_TARGET_NOT_LOCKABLE;
    enemy->field_48                       = 0;
    enemy->node.state.parts.targeted      = 0;
    walkerModel->otOffset                 = ACTOR_143900_SECOND_WALKER_OT_OFFSET;
    walkerModel->lightMtx                 = &_gScriptedWalkSecondWork->light;
    walkerModel->colorMtx                 = &_gScriptedWalkSecondWork->color;
    walkerModel->flags                    = 0;
    lightSample.vx                        = rootCoord->workm.t[0];
    lightSample.vy                        = rootCoord->workm.t[1] - ACTOR_143900_WALKER_LIGHT_Y_OFFSET;
    lightSample.vz                        = rootCoord->workm.t[2];
    _gActor143900SecondScriptedWalkerTask = task;
    worldCoordSetModelLighting(walkerModel, &lightSample, 0, ACTOR_143900_WALKER_LIGHT_COUNT);
    animationInitContext(&_gScriptedWalkSecondWork->rig.anim, _gActor143900SecondScriptedWalkerAnimationSets, walkerModel,
                         _gScriptedWalkSecondWork->rig.poses, _gScriptedWalkSecondWork->rig.slots);
    _gScriptedWalkSecondWork->st.animId = ACTOR_143900_WALKER_INITIAL_CLIP;
    _gScriptedWalkSecondWork->st.state  = ACTOR_ENEMY_ANIM_RESET;
    // The attachments borrow coordinates, but do not join the walker's task tree.
    attachmentTask = taskSpawnFromTable(gActor143900SecondScriptedWalkerTaskDescriptors, ACTOR_143900_ATTACHMENT1_ENTRY, ACTOR_143900_ATTACHMENT1_PART, 0);
    if (attachmentTask != NULL) {
        _gScriptedWalkSecondWork->attachment1 = attachmentTask;
    }
    attachmentTask = taskSpawnFromTable(gActor143900SecondScriptedWalkerTaskDescriptors, ACTOR_143900_ATTACHMENT2_ENTRY, ACTOR_143900_ATTACHMENT2_PART, 0);
    if (attachmentTask != NULL) {
        _gScriptedWalkSecondWork->attachment2 = attachmentTask;
    }
    _gScriptedWalkSecondWork->st.travel  = 0;
    _gScriptedWalkSecondWork->turnFrames = 0;
    task->msgTable                       = _gActor143900SecondScriptedWalkerMessages;
    _scriptedWalkUpdateSecond(task);
    task->state++;
}

#undef SCRIPTED_WALK_UPDATE
/// Selects the second walker's private `void(Task*)` update for this inclusion.
///
/// The work, mode, duration and animation-helper bindings below select that
/// same walker; its prologue prototype supplies static linkage.
#define SCRIPTED_WALK_UPDATE _scriptedWalkUpdateSecond
#undef SCRIPTED_WALK_TICK_ANIM
/// Routes the second walker's update to its private `void(void)` animation tick.
///
/// The tick fragment is included later with this same function and work binding.
#define SCRIPTED_WALK_TICK_ANIM _scriptedWalkTickSecondAnim
#undef SCRIPTED_WALK_RESET_ANIM
/// Routes the second walker's update to its private `void(void)` track restart.
///
/// The reset fragment uses this function binding and the same live work block.
#define SCRIPTED_WALK_RESET_ANIM _scriptedWalkResetSecondAnim
#undef SCRIPTED_WALK_BLEND_ANIM
/// Routes the second walker's update to its private `void(void)` child-track blend.
///
/// The blend fragment uses this function, work block and blend-frame binding.
#define SCRIPTED_WALK_BLEND_ANIM _scriptedWalkBlendSecondAnim
#undef SCRIPTED_WALK_WORK
/// Selects the second walker's allocation for this fragment instance.
#define SCRIPTED_WALK_WORK _gScriptedWalkSecondWork
#undef SCRIPTED_WALK_BLEND_FRAMES
/// Selects the second walker's independent writable `s16` duration latch.
///
/// Whole normal-rate frames, with the same binding at the blend definition.
#define SCRIPTED_WALK_BLEND_FRAMES (_gScriptedWalkSecondBlendFrames)
#undef SCRIPTED_WALK_MODE
/// Selects the second walker's independent signed-halfword approach mode.
#define SCRIPTED_WALK_MODE (_gScriptedWalkSecondMode)
#undef SCRIPTED_WALK_WORK_T
#define SCRIPTED_WALK_WORK_T ScriptedWalkAttachmentsWork
#include "../../shared/walk/scripted_walk_update.inc.c"
#undef SCRIPTED_WALK_UPDATE
#define SCRIPTED_WALK_UPDATE _scriptedWalkUpdate
#undef SCRIPTED_WALK_TICK_ANIM
#undef SCRIPTED_WALK_RESET_ANIM
#define SCRIPTED_WALK_RESET_ANIM _scriptedWalkResetAnim
#undef SCRIPTED_WALK_BLEND_ANIM
#define SCRIPTED_WALK_BLEND_ANIM _scriptedWalkBlendAnim
#undef SCRIPTED_WALK_WORK
#define SCRIPTED_WALK_WORK _gScriptedWalkWork
#undef SCRIPTED_WALK_BLEND_FRAMES
#define SCRIPTED_WALK_BLEND_FRAMES (_gScriptedWalkBlendFrames)
#undef SCRIPTED_WALK_MODE
#define SCRIPTED_WALK_MODE (_gScriptedWalkMode)
#undef SCRIPTED_WALK_WORK_T
#define SCRIPTED_WALK_WORK_T _Actor143900Work

/// Dispatches initialization or one frame of the second scripted walker.
///
/// `task->state` must be 0 (spawn) or 1 (frame); indexing is unchecked.
/// Owns its enemy in `spawnArg2.pointer`, TMD model and initialized attachment
/// work. Republishes that work before the selected singleton update. State 0
/// allocates work and may destroy the task on failure.
static void _actor143900SecondScriptedWalkerTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        _actor143900SpawnSecondScriptedWalker,
        _actorRenderWalkerFrameSecond,
    };
    // Retain the original 0x60-byte frame; these 64 bytes are never accessed.
    u8 reservedFrame[0x40]; // Original purpose unproven.

    _gScriptedWalkSecondWork = task->work;
    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(Enemy*, Task*) function declared in the prologue.
/// This identifier alias evaluates no arguments; undefine after the fragment.
#define ACTOR_RENDER_WALKER_FRAME _actorRenderWalkerFrameSecond
/// Selects this frame instance's motion and animation update.
///
/// Bind to a declared static void(Task*) function for the same task and work.
/// The frame calls it once after lighting and before drawing the shadow.
/// This object-like identifier alias captures no locals or constructed tokens;
/// undefine it after each inclusion of walker_frame.inc.c.
#define ACTOR_RENDER_UPDATE_WALKER             _scriptedWalkUpdateSecond
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawSecondWalkerGroundShadow
#include "../../shared/walk/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Begins teardown of the second scripted walker and its two attachment tasks.
///
/// Requires a live task with its owned enemy in `spawnArg2.pointer` and two
/// live, non-NULL attachment handles in its work. Published work/task pointers
/// remain stale. `enemyDestroy` frees that work before the attachment handles
/// are read; the retained order relies on those released bytes remaining readable.
/// The models follow the task system's immediate/deferred release policy.
static void _actor143900ExitSecondScriptedWalker(Task* task)
{
    ScriptedWalkAttachmentsWork* walkerWork = task->work;

    enemyDestroy(task->spawnArg2.pointer, task);
    // Preserve the post-release handle reads and teardown order.
    taskKill(walkerWork->attachment1);
    taskKill(walkerWork->attachment2);
}

/// Selects this overlay's private second-walker ground-shadow drawer.
///
/// Bind to the prologue's `static void name(Task* task)` declaration; the
/// following fragment defines it. This identifier alias lasts one inclusion.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawSecondWalkerGroundShadow
#include "../../shared/actor/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW

/// Attaches a carried model to a walker part with a +15-entry ordering-table offset.
///
/// Requires a live model with at least one coordinate; `attachmentRoot` is its
/// preloaded `attachmentModel->coords`. Its local transform becomes relative to
/// the walker part, and its cached composition is marked stale. `walkerPart` is
/// borrowed and must remain live while the attachment is composed; its parent
/// chain must not reach `attachmentRoot`.
/// Clearing all model flags permits active drawing and automatic allocation of
/// a missing primitive buffer. Depth indices including the +15-entry offset must
/// fit the active ordering table. Model, buffer, lighting and task ownership stay
/// with their existing owners; the caller handles attachment teardown separately.
static inline void _actor143900AttachModelToWalkerPart(TmdObject* attachmentModel, GfxCoord* attachmentRoot, GfxCoord* walkerPart)
{
    enum { ACTOR_143900_ATTACHMENT_OT_OFFSET = 15 };

    attachmentRoot->composeStamp = GRAPHICS_COORD_DIRTY;
    attachmentModel->flags       = 0;
    attachmentModel->otOffset    = ACTOR_143900_ATTACHMENT_OT_OFFSET;
    attachmentRoot->parent       = walkerPart;
}

/// Parents a model to the second scripted walker and refreshes its room lighting.
///
/// Requires live models on this task and the published walker. `spawnArg1.value`
/// is a coordinate index in that twenty-part model; the two spawns use 1 and 12.
/// State 0 attaches the root and enters state 1, which samples three lights at
/// the walker root's cached translation with Y reduced by 800 coordinate units.
/// The sample keeps that cache's composition frame; this function does not
/// compose coordinates or convert the sample to world space. Other states do
/// nothing. The task borrows its parent coordinate until model teardown and
/// requires the lighting helper's initialized matrices, scratch and GTE state.
static void _actor143900ScriptedWalkerAttachmentTask(Task* task)
{
    enum {
        ACTOR_143900_ATTACHMENT_INITIALIZE = 0,
        ACTOR_143900_ATTACHMENT_LIGHT      = 1,
    };
    TmdObject* attachmentModel = task->extra.tmd;
    GfxCoord*  attachmentRoot  = attachmentModel->coords;
    GfxCoord*  walkerCoords    = _gActor143900SecondScriptedWalkerTask->extra.tmd->coords;
    GfxCoord*  parentPart      = walkerCoords + task->spawnArg1.value;
    VECTOR     lightSample;

    switch (task->state) {
        case ACTOR_143900_ATTACHMENT_INITIALIZE:
            _actor143900AttachModelToWalkerPart(attachmentModel, attachmentRoot, parentPart);
            task->state++;
            break;
        case ACTOR_143900_ATTACHMENT_LIGHT:
            lightSample.vx = walkerCoords->workm.t[0];
            lightSample.vy = walkerCoords->workm.t[1] - ACTOR_143900_WALKER_LIGHT_Y_OFFSET;
            lightSample.vz = walkerCoords->workm.t[2];
            worldCoordSetModelLighting(attachmentModel, &lightSample, 0, ACTOR_143900_WALKER_LIGHT_COUNT);
            break;
    }
}

/// Defines the private `void(void)` animation tick for the second walker's body rig.
#define SCRIPTED_WALK_TICK_ANIM _scriptedWalkTickSecondAnim
#undef SCRIPTED_WALK_WORK
#define SCRIPTED_WALK_WORK _gScriptedWalkSecondWork
#include "../../shared/walk/scripted_walk_tick_anim.inc.c"
#undef SCRIPTED_WALK_TICK_ANIM
#undef SCRIPTED_WALK_WORK
#define SCRIPTED_WALK_WORK _gScriptedWalkWork

#undef SCRIPTED_WALK_RESET_ANIM
/// Defines the private track restart selected by the second walker's update.
#define SCRIPTED_WALK_RESET_ANIM _scriptedWalkResetSecondAnim
#undef SCRIPTED_WALK_WORK
#define SCRIPTED_WALK_WORK _gScriptedWalkSecondWork
#include "../../shared/walk/scripted_walk_reset_anim.inc.c"
#undef SCRIPTED_WALK_RESET_ANIM
#define SCRIPTED_WALK_RESET_ANIM _scriptedWalkResetAnim
#undef SCRIPTED_WALK_WORK
#define SCRIPTED_WALK_WORK _gScriptedWalkWork

#undef SCRIPTED_WALK_BLEND_ANIM
/// Defines the private child-track blend selected by the second walker's update.
#define SCRIPTED_WALK_BLEND_ANIM _scriptedWalkBlendSecondAnim
#undef SCRIPTED_WALK_WORK
#define SCRIPTED_WALK_WORK _gScriptedWalkSecondWork
#undef SCRIPTED_WALK_BLEND_FRAMES
/// Supplies the same whole-frame `s16` latch selected by the second update.
#define SCRIPTED_WALK_BLEND_FRAMES (_gScriptedWalkSecondBlendFrames)
#include "../../shared/walk/scripted_walk_blend_anim.inc.c"
#undef SCRIPTED_WALK_BLEND_ANIM
#define SCRIPTED_WALK_BLEND_ANIM _scriptedWalkBlendAnim
#undef SCRIPTED_WALK_WORK
#define SCRIPTED_WALK_WORK _gScriptedWalkWork
#undef SCRIPTED_WALK_BLEND_FRAMES
#define SCRIPTED_WALK_BLEND_FRAMES (_gScriptedWalkBlendFrames)

/// Reseeds the published second scripted walker from a borrowed animation request.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION`; receiver, message ID, second payload
/// and other request words are ignored. Requires a live walker and initialized
/// rig with loaded child-part tracks. Playable keys are 1..10. The signed check
/// rejects IDs >= 12 with -1 but accepts negative IDs and NULL entries 0/11;
/// an accepted ID narrows to `s16` and must select a loaded clip after narrowing.
/// Nonzero blend latches the low signed halfword of `blendFrames` in whole
/// normal-rate frames (0..2047 keeps playback nonnegative); zero blend restarts
/// and leaves that latch intact. Reseeding runs immediately. Returns 0 on
/// acceptance and retains no request pointer.
static s32 _actor143900PlaySecondScriptedWalkerAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum { ACTOR_143900_WALKER_CLIP_COUNT = (s32)ARRAY_SIZE(_gActor143900SecondScriptedWalkerAnimationSets) };

    if (request->animationId < ACTOR_143900_WALKER_CLIP_COUNT) {
        _gScriptedWalkSecondWork->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            _gScriptedWalkSecondWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            _gScriptedWalkSecondBlendFrames    = request->blendFrames;
        } else {
            _gScriptedWalkSecondWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        _gScriptedWalkSecondWork->st.field_6 = 0;
        // Apply the restart now; ordinary movement waits for a later update.
        _scriptedWalkUpdateSecond(_gActor143900SecondScriptedWalkerTask);
        return ACTOR_143900_PLAY_ANIMATION_APPLIED;
    }
    return ACTOR_143900_PLAY_ANIMATION_REJECTED;
}

/// Replaces the second scripted walker and both attachment models' draw flags.
///
/// Requires live published task/work and two non-NULL attachment models.
/// `ACTOR_MESSAGE_SET_MODEL_DRAW` bit 0 enables active drawing; bit 1 suppresses
/// automatic primitive-buffer allocation. Clears all other model flags and
/// ignores other request bits, allocating and releasing no buffers. Receiver,
/// message ID and second payload are ignored; returns 0.
static s32 _actor143900SetSecondScriptedWalkerModelDraw(Task* unusedTask, s32 messageId, s32 drawFlags, s32 unusedArgument)
{
    TmdObject* walkerModel      = _gActor143900SecondScriptedWalkerTask->extra.tmd;
    TmdObject* attachment1Model = _gScriptedWalkSecondWork->attachment1->extra.tmd;
    TmdObject* attachment2Model = _gScriptedWalkSecondWork->attachment2->extra.tmd;

    if (drawFlags & ACTOR_143900_WALKER_DRAW_SHOW) {
        walkerModel->flags      = 0;
        attachment1Model->flags = 0;
        attachment2Model->flags = 0;
    } else {
        walkerModel->flags      = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        attachment1Model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        attachment2Model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (drawFlags & ACTOR_143900_WALKER_DRAW_SKIP_AUTO_BUFFER) {
        walkerModel->flags      |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        attachment1Model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        attachment2Model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#undef SCRIPTED_WALK_PLACE
/// Defines the private placement callback for the second walker.
///
/// Its static prologue prototype establishes linkage; the work binding below
/// selects the same receiver's allocation for the cached signed yaw.
#define SCRIPTED_WALK_PLACE _scriptedWalkPlaceSecond
#undef SCRIPTED_WALK_WORK
#define SCRIPTED_WALK_WORK _gScriptedWalkSecondWork
#include "../../shared/walk/scripted_walk_place.inc.c"
#undef SCRIPTED_WALK_PLACE
#define SCRIPTED_WALK_PLACE _scriptedWalkPlace
#undef SCRIPTED_WALK_WORK
#define SCRIPTED_WALK_WORK _gScriptedWalkWork

/// Selects which of the second scripted walker's two attachment models is shown.
///
/// Requires live published work and both non-NULL attachment models. The borrowed
/// `ACTOR_COMMAND_MESSAGE_APPLY` command selects attachment 2 with 0 or attachment
/// 1 with 1, clearing that model's flags and replacing the other's with active-draw
/// suppression. Other commands leave both models unchanged. Context tags,
/// receiver, message ID and second payload are ignored; returns 0 and retains
/// no request pointer. This changes flags only and manages no primitive buffers.
static s32 _actor143900SelectSecondScriptedWalkerAttachment(Task* unusedTask, s32 messageId, const ActorCommand* request, s32 unusedArgument)
{
    enum {
        ACTOR_143900_WALKER_COMMAND_SHOW_ATTACHMENT2 = 0,
        ACTOR_143900_WALKER_COMMAND_SHOW_ATTACHMENT1 = 1,
    };
    TmdObject* attachment1Model;
    TmdObject* attachment2Model;

    attachment1Model = _gScriptedWalkSecondWork->attachment1->extra.tmd;
    attachment2Model = _gScriptedWalkSecondWork->attachment2->extra.tmd;
    switch (request->command) {
        case ACTOR_143900_WALKER_COMMAND_SHOW_ATTACHMENT2:
            attachment2Model->flags = 0;
            attachment1Model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case ACTOR_143900_WALKER_COMMAND_SHOW_ATTACHMENT1:
            attachment1Model->flags = 0;
            attachment2Model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
    return 0;
}

#undef SCRIPTED_WALK_TO
/// Selects the private walk-to callback for the second walker.
///
/// The prologue declares its signature and static linkage. The work type and
/// mode bindings below select the same receiver for this fragment inclusion.
#define SCRIPTED_WALK_TO _scriptedWalkToSecond
#undef SCRIPTED_WALK_MODE
#define SCRIPTED_WALK_MODE (_gScriptedWalkSecondMode)
#undef SCRIPTED_WALK_WORK_T
#define SCRIPTED_WALK_WORK_T ScriptedWalkAttachmentsWork
#include "../../shared/walk/scripted_walk_to.inc.c"
#undef SCRIPTED_WALK_TO
#define SCRIPTED_WALK_TO _scriptedWalkTo
#undef SCRIPTED_WALK_MODE
#define SCRIPTED_WALK_MODE (_gScriptedWalkMode)
#undef SCRIPTED_WALK_WORK_T
#define SCRIPTED_WALK_WORK_T _Actor143900Work
