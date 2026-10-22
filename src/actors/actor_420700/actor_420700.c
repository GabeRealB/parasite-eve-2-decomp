#include "actors/task_tables.h"

#include "actors/actor_420700.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/walk/scripted_walk.h"

/// The animation request and the head turn the actor keeps right after its
/// rig.
///
/// The scripted walk library's slot reset reads `animId` and records
/// `appliedAnimId` through the block's `st` member, so those two keep the
/// library's names. The first three members are the ones `ActorEnemyState`
/// opens with, used the same way; from `turnMode` on the layout is the
/// package's own, so the block is not that type.
typedef struct {
    s16 state;         // Step of the animation (0 none, else `ACTOR_ENEMY_ANIM_BLEND`, `_RESET` or `_TICK`)
    s16 appliedAnimId; // Clip the slots were last seeded with; recorded, never read
    s16 animId;        // Clip the last play request selected, an index into the actor's animation sets
    s16 turnMode;      // What the head turn does each frame (`ACTOR_420700_TURN_*`), as the last actor command set it
    s16 turnWeight;    // Share of the remaining angle the head turns each frame, 0 to `ONE` for none to all of it
    s16 field_A;       // Cleared by each play request; never read, role unproven
} _Actor420700State;
STATIC_ASSERT_SIZEOF(_Actor420700State, 0xC);

/// What the actor's head turn does each frame, kept in
/// `_Actor420700State::turnMode`: the command `ACTOR_COMMAND_MESSAGE_APPLY`
/// carries.
enum {
    ACTOR_420700_TURN_AUTO    = 0, // Toward the player, the weight rising while the player faces away from the actor and falling otherwise
    ACTOR_420700_TURN_PLAYER  = 1, // Toward the player, the weight rising from 0
    ACTOR_420700_TURN_RELEASE = 2, // Toward the player, the weight falling from `ONE`
    ACTOR_420700_TURN_POINT   = 3, // Toward a fixed point of the room, the weight rising from 0
};

/// Work block of the overlay's actor, allocated zeroed at its full size by the
/// spawn step and kept both at `Task::work` and in `_gScriptedWalkWork`, which
/// the actor's task handler republishes every tick.
///
/// The model object borrows `light` and `color` for as long as the block
/// lives. The matrices, the rig and the request at the start of `st` sit where
/// the scripted walkers' blocks keep theirs, which is what lets the package
/// carry that library's slot tick and slot reset under the library's name for
/// the block. It has no walk, and the rest of `st` is its own.
typedef struct {
    MATRIX            light;         // Light-direction matrix lent to the model object
    MATRIX            color;         // Light-colour matrix lent to the model object
    ActorAnimRig20    rig;           // Playback storage of the model's parts; slots 1 to 19 are driven
    _Actor420700State st;            // Animation request and head-turn state
    byte              pad_4C0[0xE0]; // Rest of the allocation; the package neither reads nor writes it, contents unproven
} _Actor420700Work;
STATIC_ASSERT_SIZEOF(_Actor420700Work, 0x5A0);

/// Borrowed work block used by this actor's scripted-walk animation fragments.
///
/// The spawn and dispatcher publish the allocation also held by `Task::work`.
/// The included fragments require it to remain live; task teardown releases
/// it without clearing this pointer. The carrier's remaining state drives its
/// head turn; it does not implement the scripted walk's movement.
static _Actor420700Work* _gScriptedWalkWork;

static Task* _gActor420700BodyTask;

static Task* _gActor420700HeadHatTask;

static Task* _gActor420700ShotgunTask;

static void _actor420700Exit(Task* task);
static void _actor420700UpdateAnimation(Task* task);
static void _actor420700BlendAnimation(void);

enum {
    ACTOR_420700_ATTACHMENT_INIT     = 0,
    ACTOR_420700_ATTACHMENT_LIGHTING = 1,
    ACTOR_420700_LIGHTING_Y_OFFSET   = -800,
};

// Message-table callbacks use the argument views required by this TU.

static s32 _gActor420700AutoHeadTurnStep;
/// Word initialized to 150 at body spawn; no reader or further role is established.
extern s32 D_actor_420700_8013EFF4;

static TmdSource _gGaryBody;
static TmdSource _gGaryHeadHat;
static TmdSource _gGaryShotgun2;
static void      _actor420700GaryDouglasTask(Task* task);
static void      _actor420700HeadHatTask(Task* task);
static void      _actor420700ShotgunTask(Task* task);

static s32 _actor420700HandleAnimationMessage(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32 _actor420700HandleModelDrawMessage(Task* task, s32 messageId, s32 drawFlags, s32 unusedArg);
static s32 _actor420700HandleHeadTurnCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);

/// Advances automatic head-turn weight, narrowing to its stored halfword before clamping.
///
/// Requires the published live work block. The signed step is in 1/4096 units;
/// automatic mode supplies +64 or -128 and keeps the result between 0 and `ONE`.
static inline void _actor420700AdvanceAutoHeadTurn(void)
{
    _gScriptedWalkWork->st.turnWeight += _gActor420700AutoHeadTurnStep;
    if (_gScriptedWalkWork->st.turnWeight > ONE) {
        _gScriptedWalkWork->st.turnWeight = ONE;
    }
    if (_gScriptedWalkWork->st.turnWeight < 0) {
        _gScriptedWalkWork->st.turnWeight = 0;
    }
}

#include "assets/rooms/dryfield/trailer_coach/gary_clips_1.inc"

#include "assets/characters/gary/body.inc"

#include "assets/characters/gary/head_hat.inc"

#include "assets/characters/gary/shotgun_2.inc"

#include "assets/rooms/dryfield/trailer_coach/gary_clips_2.inc"

/// Gary Douglas's animation, drawing and head-turn messages, ending at the sentinel.
///
/// Animation and command payloads are borrowed during synchronous dispatch.
static TaskMessageEntry _gActor420700Messages[4] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor420700HandleAnimationMessage },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor420700HandleModelDrawMessage },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor420700HandleHeadTurnCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor420700GaryDouglasTaskDescriptors[3] = {
    { { { TASK_BODY_TMD, 192 } }, _actor420700GaryDouglasTask, { .model = &_gGaryBody } },
    { { { TASK_BODY_TMD, 192 } }, _actor420700HeadHatTask, { .model = &_gGaryHeadHat } },
    { { { TASK_BODY_TMD, 192 } }, _actor420700ShotgunTask, { .model = &_gGaryShotgun2 } },
};

/// Gary Douglas's clip directory, borrowed by the body rig while this overlay is loaded.
///
/// Index 0 is unavailable; indices 1..20 select the loaded descriptors. Request
/// selectors add 0, 10 or 17 to their local ID; the resulting index must be 1..20.
#include "assets/rooms/dryfield/trailer_coach/gary_bank.inc"

/// Borrowed body task of this singleton scripted NPC.
///
/// Spawn publishes it for animation/draw requests and the head/hat and shotgun
/// attachment roots. Teardown ends its lifetime without clearing the pointer.
static Task* _gActor420700BodyTask;

/// Live head-and-hat attachment task of the singleton Gary Douglas actor.
///
/// Spawn publishes it before texture binding; draw messages require it live.
/// The body exit kills the task and leaves the pointer uncleared. Attachment
/// allocation must succeed before the spawning body continues.
static Task* _gActor420700HeadHatTask;

/// Retained shotgun-model task of the singleton Gary Douglas actor.
///
/// Spawn publishes it for draw messages, which require a live model. Its root
/// borrows body part 8; the handle is not cleared on teardown. The body's exit
/// does not stop this task, so the surrounding scene's release order is unproven.
static Task* _gActor420700ShotgunTask;

/// Signed Q12 automatic head-turn weight increment per body update.
///
/// Starts at zero; automatic aiming sets +64 when behind the player's facing
/// and -128 otherwise or during events. Adding it narrows the weight to s16
/// before clamping that weight to 0..4096.
static s32 _gActor420700AutoHeadTurnStep;

s32 D_actor_420700_8013EFF4;

static void _actor420700Spawn(Enemy* enemy, Task* task);
static void _actor420700Update(Enemy* enemy, Task* task);

/// Initializes Gary Douglas's singleton body, head-and-hat and shotgun tasks.
///
/// Requires a live enemy/body model and current area placement. Publishes the
/// zeroed work allocation; allocation failure destroys the enemy task. On
/// success installs teardown, view-parents the body and disables targeting,
/// then spawns both attachment tasks. The first child must exist for texture
/// binding; no failure guard is present. Binds borrowed matrices and the loaded
/// 21-entry animation bank, samples three lights at cached root view-space XYZ
/// with Y reduced by 800, starts clip 5, installs messages and advances state.
static void _actor420700Spawn(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_420700_SPAWN_HEAD_HAT_TASK = 1,
        ACTOR_420700_SPAWN_SHOTGUN_TASK  = 2,
        ACTOR_420700_SPAWN_IDLE_CLIP     = 5,
        ACTOR_420700_SPAWN_OT_OFFSET     = 1,
        ACTOR_420700_SPAWN_LIGHT_COUNT   = 3,
    };

    VECTOR            lightingPosition;
    GfxCoord*         rootCoord;
    TmdObject*        bodyModel;
    _Actor420700Work* work;

    bodyModel          = task->extra.tmd;
    rootCoord          = bodyModel->coords;
    work               = memCalloc(sizeof(_Actor420700Work), false);
    _gScriptedWalkWork = work;
    task->work         = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _actor420700Exit;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    bodyModel->otOffset              = ACTOR_420700_SPAWN_OT_OFFSET;
    bodyModel->flags                 = 0;
    // The head-and-hat must spawn successfully before its texture binding.
    _gActor420700BodyTask    = task;
    _gActor420700HeadHatTask = taskSpawnFromTable(gActor420700GaryDouglasTaskDescriptors, ACTOR_420700_SPAWN_HEAD_HAT_TASK, 0, NULL);
    _gActor420700ShotgunTask = taskSpawnFromTable(gActor420700GaryDouglasTaskDescriptors, ACTOR_420700_SPAWN_SHOTGUN_TASK, 0, NULL);
    _actorRenderApplyTaskPlacementTextureOffsets(_gActor420700HeadHatTask, enemy);
    bodyModel->lightMtx           = &_gScriptedWalkWork->light;
    bodyModel->colorMtx           = &_gScriptedWalkWork->color;
    _gActor420700AutoHeadTurnStep = 0;
    lightingPosition.vx           = rootCoord->workm.t[0];
    lightingPosition.vy           = rootCoord->workm.t[1] + ACTOR_420700_LIGHTING_Y_OFFSET;
    D_actor_420700_8013EFF4       = 150; // No reader is recovered; the stored value's role is unproven.
    lightingPosition.vz           = rootCoord->workm.t[2];
    worldCoordSetModelLighting(bodyModel, &lightingPosition, 0, ACTOR_420700_SPAWN_LIGHT_COUNT);
    animationInitContext(&_gScriptedWalkWork->rig.anim, _gActor420700AnimationSets, bodyModel,
                         _gScriptedWalkWork->rig.poses, _gScriptedWalkWork->rig.slots);
    _gScriptedWalkWork->st.animId = ACTOR_420700_SPAWN_IDLE_CLIP;
    _gScriptedWalkWork->st.state  = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable                = _gActor420700Messages;
    _actor420700UpdateAnimation(task);
    task->state++;
}

/// Updates Gary Douglas's lighting, animation and head turn once per actor tick.
///
/// Requires the published live work block, twenty-part body and live player model.
/// Head-turn weights use 1/4096 units and angles use 4096 units per turn.
/// `ACTOR_420700_TURN_AUTO` raises the weight while the player faces away,
/// releases it otherwise or during events, and holds slots 1..19 when the
/// pre-step weight is nonzero outside events. Explicit player/point modes raise
/// the weight; release and unrecognized modes lower it. Only point mode aims
/// at the fixed room focus; all other modes aim at the player.
static void _actor420700Update(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_420700_LIGHTING_PART  = 2,
        ACTOR_420700_HEAD_MAX_YAW   = ACTOR_TRANSFORM_ANGLE_TURN / 8,
        ACTOR_420700_HEAD_MAX_PITCH = ACTOR_TRANSFORM_ANGLE_TURN / 16,
        ACTOR_420700_HEAD_TURN_RISE = ONE / 64,
        ACTOR_420700_HEAD_TURN_STEP = ONE / 32,
        ACTOR_420700_ROOM_FOCUS_X   = 4467,
        ACTOR_420700_ROOM_FOCUS_Y   = 0,
        ACTOR_420700_ROOM_FOCUS_Z   = -1843,
    };
    VECTOR     lightingPosition;
    GfxCoord   roomFocus;    // Only the translation carries a world point for the head-aim API
    byte       unused[0x50]; // Untouched frame storage; its original role is unproven
    GfxCoord*  bodyCoords;
    GfxCoord*  playerCoords;
    GfxCoord*  lightingPart;
    GameActor* playerActor;
    s32        dx;
    s32        dz;
    s32        playerCosYaw;
    s32        slotIndex;
    u8         animationRate;

    bodyCoords   = task->extra.tmd->coords;
    lightingPart = &bodyCoords[ACTOR_420700_LIGHTING_PART];
    playerCoords = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    actorRenderComposeCoord(lightingPart);
    lightingPosition.vx = lightingPart->workm.t[0];
    lightingPosition.vy = lightingPart->workm.t[1];
    lightingPosition.vz = lightingPart->workm.t[2];
    worldCoordUpdateActorColor(enemy, &lightingPosition, 0, 0);
    _actor420700UpdateAnimation(task);
    animationRate = ANIMATION_RATE_ONE;
    // The head adjustment follows the clip tick, and its weight persists between ticks.
    if (_gScriptedWalkWork->st.turnMode != ACTOR_420700_TURN_AUTO) {
        if (_gScriptedWalkWork->st.turnMode == ACTOR_420700_TURN_PLAYER ||
            _gScriptedWalkWork->st.turnMode == ACTOR_420700_TURN_POINT) {
            _gScriptedWalkWork->st.turnWeight += ACTOR_420700_HEAD_TURN_STEP;
            if (_gScriptedWalkWork->st.turnWeight > ONE) {
                _gScriptedWalkWork->st.turnWeight = ONE;
            }
        } else {
            _gScriptedWalkWork->st.turnWeight -= ACTOR_420700_HEAD_TURN_STEP;
            if (_gScriptedWalkWork->st.turnWeight < 0) {
                _gScriptedWalkWork->st.turnWeight = 0;
            }
        }
        if (_gScriptedWalkWork->st.turnMode == ACTOR_420700_TURN_POINT) {
            roomFocus.coord.t[0] = ACTOR_420700_ROOM_FOCUS_X;
            roomFocus.coord.t[1] = ACTOR_420700_ROOM_FOCUS_Y;
            roomFocus.coord.t[2] = ACTOR_420700_ROOM_FOCUS_Z;
            animationAimHeadAtPoint(task, &roomFocus, ACTOR_420700_HEAD_MAX_YAW, ACTOR_420700_HEAD_MAX_PITCH, _gScriptedWalkWork->st.turnWeight);
        } else {
            animationAimHeadAtTask(task, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ACTOR_420700_HEAD_MAX_YAW, ACTOR_420700_HEAD_MAX_PITCH, _gScriptedWalkWork->st.turnWeight);
        }
    } else {
        if (gGameSession->eventState == 0) {
            dx           = bodyCoords->coord.t[0] - playerCoords->coord.t[0];
            dz           = bodyCoords->coord.t[2] - playerCoords->coord.t[2];
            playerActor  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
            playerCosYaw = rcos(playerActor->rotation.vy);
            // A negative forward projection places Gary behind the player's facing.
            if (dx * rsin(playerActor->rotation.vy) + dz * playerCosYaw < 0) {
                _gActor420700AutoHeadTurnStep = ACTOR_420700_HEAD_TURN_RISE;
            } else {
                _gActor420700AutoHeadTurnStep = -ACTOR_420700_HEAD_TURN_STEP;
            }
            if (_gScriptedWalkWork->st.turnWeight != 0) {
                animationRate = 0;
            }
        } else {
            _gActor420700AutoHeadTurnStep = -ACTOR_420700_HEAD_TURN_STEP;
        }
        _actor420700AdvanceAutoHeadTurn();
        animationAimHeadAtTask(task, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ACTOR_420700_HEAD_MAX_YAW, ACTOR_420700_HEAD_MAX_PITCH, _gScriptedWalkWork->st.turnWeight);
    }
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(_gScriptedWalkWork->rig.slots); slotIndex++) {
        _gScriptedWalkWork->rig.slots[slotIndex].rate = animationRate;
    }
}

/// Dispatches Gary Douglas's spawn or per-frame update and publishes his work.
///
/// Task state must be 0 (spawn) or 1 (update), with a live TMD body and enemy
/// spawn argument. Publishes the current task work before dispatch; the spawn
/// handler replaces it after allocation. The published block is borrowed until
/// actor teardown. The package's body descriptor is this entry's only consumer.
static void _actor420700GaryDouglasTask(Task* task)
{
    EnemyTaskFunc stateHandlers[2] = {
        _actor420700Spawn,
        _actor420700Update,
    };

    _gScriptedWalkWork = task->work;
    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Releases the head-and-hat attachment, enemy record and body task at actor exit.
///
/// The head-and-hat task must still be live. The shotgun task is not explicitly
/// killed here; both task globals and the borrowed work pointer are left intact.
static void _actor420700Exit(Task* task)
{
    taskKill(_gActor420700HeadHatTask);
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// Attaches Gary's head-and-hat model to body part 4 and refreshes its lighting.
///
/// The body task and its coordinates must outlive this task. Initialization
/// enables drawing and enters the lighting state; later ticks sample the body
/// root's composed translation with a -800 world-unit Y offset. Other states
/// do nothing. Lighting uses the model's borrowed default matrices.
static void _actor420700HeadHatTask(Task* task)
{
    enum { ACTOR_420700_HEAD_ATTACHMENT_PART = 4 };
    TmdObject* headModel  = task->extra.tmd;
    GfxCoord*  headRoot   = headModel->coords;
    GfxCoord*  bodyCoords = _gActor420700BodyTask->extra.tmd->coords;
    GfxCoord*  headPart   = bodyCoords + ACTOR_420700_HEAD_ATTACHMENT_PART;
    VECTOR     lightingPosition;

    switch (task->state) {
        case ACTOR_420700_ATTACHMENT_INIT:
            headRoot->composeStamp = GRAPHICS_COORD_DIRTY;
            headModel->flags       = 0;
            headRoot->parent       = headPart;
            task->state++;
            break;
        case ACTOR_420700_ATTACHMENT_LIGHTING:
            lightingPosition.vx = bodyCoords->workm.t[0];
            lightingPosition.vy = bodyCoords->workm.t[1] + ACTOR_420700_LIGHTING_Y_OFFSET;
            lightingPosition.vz = bodyCoords->workm.t[2];
            worldCoordSetModelLighting(headModel, &lightingPosition, 0, ARRAY_SIZE(headModel->colorMtx->m[0]));
            break;
    }
}

/// Applies a pending animation reset/blend or ticks the actor's playing slots.
///
/// Uses the published work block; `task` is unused. Reset and blend requests
/// enter `ACTOR_ENEMY_ANIM_TICK` without a further playback tick. Other states
/// leave playback unchanged. The body, rig and selected clip must remain live.
static void _actor420700UpdateAnimation(Task* task)
{
    if (_gScriptedWalkWork->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        _actor420700BlendAnimation();
        _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (_gScriptedWalkWork->st.state == ACTOR_ENEMY_ANIM_RESET) {
        _scriptedWalkResetAnim();
        _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (_gScriptedWalkWork->st.state == ACTOR_ENEMY_ANIM_TICK) {
        _scriptedWalkTickAnim();
    }
}

#include "../../shared/walk/scripted_walk_tick_anim.inc.c"

#include "../../shared/walk/scripted_walk_reset_anim.inc.c"

/// Blends slots 1..19 from their current poses to the requested clip over eight frames.
///
/// The published rig must be bound to a live body and a non-null loaded set
/// at `st.animId`, with tracks for all driven slots. Captures each slot's ticked
/// pose and seeks its existing track to record offset zero, retaining its rate.
/// Leaves slot 0 and the animation state unchanged, and records `appliedAnimId`.
static void _actor420700BlendAnimation(void)
{
    enum { ACTOR_420700_ANIMATION_BLEND_FRAMES = 8 };
    s32 slotIndex;

    slotIndex = 1;
    do {
        animationSeekSlotWithBlend(&_gScriptedWalkWork->rig.anim, slotIndex, _gScriptedWalkWork->st.animId, 0, ACTOR_420700_ANIMATION_BLEND_FRAMES);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(_gScriptedWalkWork->rig.slots));
    _gScriptedWalkWork->st.appliedAnimId = _gScriptedWalkWork->st.animId;
}

/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION` by immediately resetting or blending the body clip.
///
/// Borrows `request` through dispatch; the published work and body must be live.
/// Selector 1 adds 10, selector 2 adds 17, and every other selector adds zero.
/// Valid ids for non-null local sets are 0..10 for selector 1, 0..3 for selector
/// 2, and 1..20 otherwise. Only the signed raw id < 21 is checked; negative ids
/// and offset sums are not validated. Returns -1 for raw ids >= 21, else 0.
/// Nonzero `blend` uses eight frames regardless of `blendFrames`; collision
/// participation, `task`, `messageId` and the second payload are unused.
static s32 _actor420700HandleAnimationMessage(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    enum {
        ACTOR_420700_ANIMATION_REQUEST_LIMIT = 21,
        ACTOR_420700_ANIMATION_BANK_1        = 1,
        ACTOR_420700_ANIMATION_BANK_2        = 2,
        ACTOR_420700_ANIMATION_BANK_1_BASE   = 10,
        ACTOR_420700_ANIMATION_BANK_2_BASE   = 17,
    };
    s32               setOffset;
    _Actor420700Work* work;

    if (request->animationId < ACTOR_420700_ANIMATION_REQUEST_LIMIT) {
        switch (request->source.index) {
            case ACTOR_420700_ANIMATION_BANK_1:
                setOffset = ACTOR_420700_ANIMATION_BANK_1_BASE;
                break;
            case ACTOR_420700_ANIMATION_BANK_2:
                setOffset = ACTOR_420700_ANIMATION_BANK_2_BASE;
                break;
            default:
                setOffset = 0;
                break;
        }
        work            = _gScriptedWalkWork;
        work->st.animId = request->animationId + setOffset;
        if (request->blend != ANIMATION_BLEND_RESET) {
            work->st.state = ACTOR_ENEMY_ANIM_BLEND;
        } else {
            work->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        _gScriptedWalkWork->st.field_A = 0;
        _actor420700UpdateAnimation(_gActor420700BodyTask);
        return 0;
    }
    return -1;
}

/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` for the body, head-and-hat and shotgun models.
///
/// All three tasks/models must be live. Bit 0 of `drawFlags` enables active
/// drawing; bit 1 disables automatic buffer allocation. Replaces all prior
/// flags, ignores other input bits and allocates/releases no buffers. Always
/// returns 0; the receiver, message id and second payload are unused.
static s32 _actor420700HandleModelDrawMessage(Task* task, s32 messageId, s32 drawFlags, s32 unusedArg)
{
    enum {
        ACTOR_420700_DRAW_VISIBLE        = 1 << 0,
        ACTOR_420700_DRAW_NO_AUTO_BUFFER = 1 << 1,
    };
    TmdObject* bodyModel    = _gActor420700BodyTask->extra.tmd;
    TmdObject* headHatModel = _gActor420700HeadHatTask->extra.tmd;
    TmdObject* shotgunModel = _gActor420700ShotgunTask->extra.tmd;

    if (drawFlags & ACTOR_420700_DRAW_VISIBLE) {
        bodyModel->flags    = 0;
        headHatModel->flags = 0;
        shotgunModel->flags = 0;
    } else {
        bodyModel->flags    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        headHatModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        shotgunModel->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (drawFlags & ACTOR_420700_DRAW_NO_AUTO_BUFFER) {
        bodyModel->flags    |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        headHatModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        shotgunModel->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

/// Applies a trailer-coach head-turn command delivered by `ACTOR_COMMAND_MESSAGE_APPLY`.
///
/// Borrows `command` through dispatch. Only stage 2/area 27 commands are accepted;
/// a different namespace returns -1 without touching work. Player/point modes
/// start at weight 0, release starts at `ONE`, and auto keeps the current weight.
/// Stores other commands too, leaving their weight unchanged; the update treats
/// them as release. Returns 0 on acceptance. Requires the published live work;
/// the receiver, message id and second payload are unused.
static s32 _actor420700HandleHeadTurnCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    enum { ACTOR_420700_HEAD_TURN_COMMAND_CONTEXT = (GAME_AREA_DRYFIELD_TRAILER_COACH << 8) | GAME_STAGE_DRYFIELD };

    if (command->context.key != ACTOR_420700_HEAD_TURN_COMMAND_CONTEXT) {
        return -1;
    }
    _gScriptedWalkWork->st.turnMode = command->command;
    switch (command->command) {
        case ACTOR_420700_TURN_AUTO:
            break;
        case ACTOR_420700_TURN_PLAYER:
        case ACTOR_420700_TURN_POINT:
            _gScriptedWalkWork->st.turnWeight = 0;
            break;
        case ACTOR_420700_TURN_RELEASE:
            _gScriptedWalkWork->st.turnWeight = ONE;
            break;
    }
    return 0;
}

/// Attaches Gary's shotgun to body part 8 and refreshes its lighting.
///
/// The body task and its coordinates must outlive this task. Initialization
/// enables drawing, gives the model an ordering-table bias of -2 and enters
/// the lighting state. Later ticks sample the body root's composed translation
/// with a -800 world-unit Y offset. Other states do nothing. Lighting uses
/// the model's borrowed default matrices.
static void _actor420700ShotgunTask(Task* task)
{
    enum {
        ACTOR_420700_SHOTGUN_ATTACHMENT_PART = 8,
        ACTOR_420700_SHOTGUN_OT_BIAS         = -2,
    };
    TmdObject* shotgunModel   = task->extra.tmd;
    GfxCoord*  shotgunRoot    = shotgunModel->coords;
    GfxCoord*  bodyCoords     = _gActor420700BodyTask->extra.tmd->coords;
    GfxCoord*  attachmentPart = bodyCoords + ACTOR_420700_SHOTGUN_ATTACHMENT_PART;
    VECTOR     lightingPosition;

    switch (task->state) {
        case ACTOR_420700_ATTACHMENT_INIT:
            shotgunRoot->composeStamp = GRAPHICS_COORD_DIRTY;
            shotgunModel->flags       = 0;
            shotgunModel->otOffset    = ACTOR_420700_SHOTGUN_OT_BIAS;
            shotgunRoot->parent       = attachmentPart;
            task->state++;
            break;
        case ACTOR_420700_ATTACHMENT_LIGHTING:
            lightingPosition.vx = bodyCoords->workm.t[0];
            lightingPosition.vy = bodyCoords->workm.t[1] + ACTOR_420700_LIGHTING_Y_OFFSET;
            lightingPosition.vz = bodyCoords->workm.t[2];
            worldCoordSetModelLighting(shotgunModel, &lightingPosition, 0, ARRAY_SIZE(shotgunModel->colorMtx->m[0]));
            break;
    }
}
