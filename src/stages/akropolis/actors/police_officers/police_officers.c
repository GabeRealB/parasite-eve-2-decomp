#include "stages/akropolis/actors/police_officers.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "stages/akropolis/rooms/plaza.h"

#include "assets/rooms/akropolis/plaza/police_rookie_schema.h"
#include "assets/rooms/akropolis/plaza/police_sergeant_schema.h"

/// Ground shadow of an officer model, in the units of its part-1 frame.
enum {
    SHADOW_SIDE = 0x300, // Side of the shadow square
    SHADOW_DROP = 0x380, // Distance from the frame's origin down to the shadow
};

/// Values of `_OfficerWork::playState`.
///
/// The body model draws its floor quad in every state. The culled-body model
/// never steps its animation after spawn; the officer's draws its floor quad
/// only while posed, the sergeant's until frozen.
enum {
    PLAY_STATE_POSED   = 0, // Holding the spawn pose: relit each frame, animation not stepped
    PLAY_STATE_PLAYING = 1, // A play request arrived: the body model steps its animation and follow-ups
    PLAY_STATE_FROZEN  = 2, // Neither stepped nor relit
};

/// Officer selected by a model-swap request.
enum {
    MODEL_SELECT_SERGEANT = 0,
    MODEL_SELECT_ROOKIE   = 1,
};

/// Model entries in each officer's task descriptor table.
enum {
    MODEL_TASK_BODY        = 1,
    MODEL_TASK_CULLED_BODY = 2,
};

/// Controller lifecycle and one-time model initialization states.
enum {
    CONTROLLER_INIT   = 0,
    CONTROLLER_HIDDEN = 1,
    CONTROLLER_SHOWN  = 2,
    MODEL_INIT        = 0,
    MODEL_READY       = 1,
};

/// Plaza movie parts whose timeline controls automatic officer visibility.
enum {
    PLAZA_STREAM_PART_2 = 2,
    PLAZA_STREAM_PART_3 = 3,
    PLAZA_STREAM_PART_4 = 4,
    PLAZA_STREAM_PART_5 = 5,
};

/// Presentation hold preceding installation of a replacement model.
enum {
    SWAP_STATE_HOLD    = 0,
    SWAP_STATE_WAIT_1  = 1,
    SWAP_STATE_WAIT_2  = 2,
    SWAP_STATE_INSTALL = 3,
};

/// Fixed animation transition durations in normal-rate frames.
enum {
    OFFICER_BLEND_FRAMES = 8,
    PLAYER_BLEND_FRAMES  = 10,
};

/// Work block of one acropolis-plaza police officer, kept at `Task::work` by
/// both of the officer's tasks.
///
/// The controller task is the placed actor the room's messages reach. It shows
/// and hides the officer's model as the plaza movie advances and uses only
/// `modelTask`, `spawnAnimationId` and `bodyAnimationId`. It allocates the
/// block without clearing it, so those members are indeterminate until written.
/// The model task clears its own block at spawn and uses the rig, the matrices
/// and the playback members; its `modelTask` stays `NULL`.
///
/// No access to the `pad_` runs has been observed; whether they are unused
/// members or padding is unproven.
typedef struct {
    ActorAnimRig19         rig;               // Model: playback storage of the nineteen-part officer model
    MATRIX                 light;             // Model: light matrix lent to the model object
    MATRIX                 color;             // Model: colour matrix lent to the model object
    byte                   pad_47C[0x68];
    Task*                  modelTask;         // Controller: the officer's live model task, `NULL` once hidden or torn down
    Task*                  playerTask;        // Model: the player's task, asked to play the animations paired with the officer's
    const AnimationRecord* lastCueRecord;     // Model: record slot 1 was on at the last step, so its sound cues fire once; borrowed from the bound set
    u16                    playState;         // Model: `PLAY_STATE_*`
    byte                   pad_4F2[0x4];
    u16                    playerAnimationId; // Model: animation last requested of the player; indexes the player's follow-up table
    u16                    followUpTable;     // Model: follow-up table the officer chains through (0 the player's until the first request, 1 the officer, 2 the sergeant)
    u16                    animationId;       // Model: animation last requested of the officer or chained to; 0 at spawn whatever the spawn animation
    byte                   pad_4FC[0x8];
    u16                    spawnAnimationId;  // Controller: animation the sergeant's culled-body model spawns on. Model: the one a culled-body model spawned on
    u16                    bodyAnimationId;   // Controller: animation the body model starts on when a draw-mode message swaps it in
    u16                    placementId;       // Model: its `POLICE_OFFICERS_PLACEMENT_*`, stored just before the spawn clear and so always read back as 0
    u16                    stepSoundIndex;    // Model: next entry of the officer's step-sound sequence, stopping at the last
} _OfficerWork;
STATIC_ASSERT_SIZEOF(_OfficerWork, 0x50C);

static s32 _policeOfficersSetBody(Task* task, s32 messageId, s32 officerSelector, const AnimationPlayRequest* animation);
static s32 _policeOfficersSetCulledBody(Task* task, s32 messageId, s32 mode, s32 unusedSecondArg);
static s32 _policeOfficersPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* animation, s32 unusedSecondArg);
static s32 _policeOfficersPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedSecondArg);
static s32 _policeOfficersStopController(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);

static TmdSource _gPoliceRookieBody;
static TmdSource _gPoliceRookieCulledBody;
static TmdSource _gPoliceSergeantBody;
static TmdSource _gPoliceSergeantCulledBody;
static void      _policeOfficersSwapBodyTask(Task* task);
static void      _policeOfficersSwapCulledBodyTask(Task* task);
static void      _policeOfficersRookieControllerTask(Task* task);
static void      _policeOfficersSergeantControllerTask(Task* task);
static void      _policeOfficersRookieBodyTask(Task* task);
static void      _policeOfficersSergeantBodyTask(Task* task);
static void      _policeOfficersRookieCulledBodyTask(Task* task);
static void      _policeOfficersSergeantCulledBodyTask(Task* task);

#include "assets/characters/police_rookie/body.inc"

#include "assets/characters/police_rookie/culled_body.inc"

#include "assets/characters/police_sergeant/body.inc"

#include "assets/characters/police_sergeant/culled_body.inc"

#include "assets/rooms/akropolis/plaza/police_officers_clips.inc"

/// The officer's animation bank for both ordinary and culled plaza body models.
///
/// Clips 1..14 are populated; 0 and 15 are NULL. Clips 1..5 borrow plaza
/// resources, and 6..14 belong to this actor package. Both overlays must remain
/// loaded while a body rig retains this table, its poses and its clip records.
#include "assets/rooms/akropolis/plaza/police_rookie_bank.inc"

/// The sergeant's animation bank for both ordinary and culled plaza body models.
///
/// Populated clips are 2, 4..10 and 14..24; 0, 1, 3, 11..13 and 25 are NULL.
/// All clips borrow plaza resources. The plaza and this actor package must
/// remain loaded while a body rig retains the table and its clip records.
#include "assets/rooms/akropolis/plaza/police_sergeant_bank.inc"

#include "assets/rooms/akropolis/plaza/police_scene_player_bank.inc"

/// Sentinel ending a police-officer scene animation chain.
enum { NO_FOLLOW_UP_ANIMATION = -1 };

/// Player clip successors for the police-officer scene, indexed by animation ID.
///
/// All twelve entries end the chain. The paired player bank has thirteen slots;
/// only IDs 0..11 are supported here, and consumers do not check that bound.
static s16 _gPoliceOfficersPlayerFollowUps[12] = {
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
};

/// The officer's clip successors, indexed by animation ID 0..15.
///
/// A nonnegative entry selects the next clip when slot 1 reaches its boundary;
/// NO_FOLLOW_UP_ANIMATION ends the chain. Storage is borrowed by
/// the follow-up table directory for the overlay's lifetime.
static s16 _gPoliceOfficersRookieFollowUps[16] = {
    0,
    NO_FOLLOW_UP_ANIMATION,
    POLICE_ROOKIE_ANIMATION_SHIFT_STANCE,
    POLICE_ROOKIE_ANIMATION_IDLE,
    POLICE_ROOKIE_ANIMATION_IDLE,
    POLICE_ROOKIE_ANIMATION_IDLE,
    POLICE_ROOKIE_ANIMATION_ASIDE_IDLE,
    POLICE_ROOKIE_ANIMATION_ASIDE_IDLE,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    POLICE_ROOKIE_ANIMATION_TALK_IDLE,
    POLICE_ROOKIE_ANIMATION_TALK_IDLE,
    POLICE_ROOKIE_ANIMATION_TALK_IDLE,
    NO_FOLLOW_UP_ANIMATION,
    0,
};

/// The sergeant's clip successors, indexed by animation ID 0..25.
///
/// A nonnegative entry selects the next clip when slot 1 reaches its boundary;
/// NO_FOLLOW_UP_ANIMATION ends the chain. Storage is borrowed by
/// the follow-up table directory for the overlay's lifetime.
static s16 _gPoliceOfficersSergeantFollowUps[26] = {
    0,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    POLICE_SERGEANT_ANIMATION_TALK_IDLE,
    POLICE_SERGEANT_ANIMATION_TALK_IDLE,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    POLICE_SERGEANT_ANIMATION_TALK_IDLE,
    POLICE_SERGEANT_ANIMATION_IDLE,
    POLICE_SERGEANT_ANIMATION_IDLE,
    POLICE_SERGEANT_ANIMATION_IDLE,
    POLICE_SERGEANT_ANIMATION_IDLE,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    NO_FOLLOW_UP_ANIMATION,
    POLICE_SERGEANT_ANIMATION_TURNED_IDLE,
    NO_FOLLOW_UP_ANIMATION,
    POLICE_SERGEANT_ANIMATION_TURN_HEAD_BACK,
    NO_FOLLOW_UP_ANIMATION,
    0,
};

/// Clip-successor tables for the paired player and two police-officer models.
///
/// Selectors 0/1/2 choose player/officer/sergeant, with animation ID domains
/// 0..11, 0..15 and 0..25 respectively. A signed -1 entry ends the chain.
/// Slot-1 boundaries advance officer clips; stopped playback advances the player.
/// The directory borrows its child tables for the overlay's lifetime.
static const s16* _gPoliceOfficersFollowUps[3] = {
    _gPoliceOfficersPlayerFollowUps,
    _gPoliceOfficersRookieFollowUps,
    _gPoliceOfficersSergeantFollowUps,
};

/// The officer's step-sound sequence for newly entered slot-1 cue-2 records.
///
/// The index starts at zero and saturates at entry 2, repeating the final sound.
/// This branch requires the officer's placement ID; current model initializers
/// store that ID before clearing the work, so they leave the branch unreachable.
static s32 _gPoliceOfficersRookieStepSounds[3] = {
    SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PLAZA, 8),
    SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PLAZA, 9),
    SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PLAZA, 10),
};

/// Police-officer animation, placement and model-control message bindings.
///
/// Terminated by TASK_MESSAGE_TABLE_END. Control callbacks require a controller
/// with a live model where appropriate; the same table is also installed on
/// model tasks. Senders discard the callbacks' unset result word.
static TaskMessageEntry _gPoliceOfficersMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _policeOfficersPlayAnimation },
    { ACTOR_MESSAGE_PLACE, _policeOfficersPlace },
    { POLICE_OFFICERS_MESSAGE_SET_CULLED_BODY, _policeOfficersSetCulledBody },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _policeOfficersSetBody },
    { POLICE_OFFICERS_MESSAGE_STOP_CONTROLLER, _policeOfficersStopController },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless display-task recipe for a delayed police-officer body-model swap.
///
/// Entry 0 runs at priority 192. Spawn argument 1 selects the sergeant with 0 or
/// the officer with 1; argument 2 borrows the live controller. After three display
/// ticks the replacement uses its saved body animation and area placement,
/// then resumes the game loop. The old model is released before scheduling.
static TaskDesc _gPoliceOfficersBodySwapTaskDescriptor = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_CONTROL } },
    .callback = _policeOfficersSwapBodyTask,
    .data     = { .value = 0 },
};

/// Bodyless display-task recipe for a delayed police-officer culled-body swap.
///
/// Entry 0 runs at priority 192 and holds display flipping for three ticks.
/// Spawn argument 1 selects the sergeant with 0 (his talk idle) or the officer with 1
/// (clip 7); argument 2 borrows the live controller. Requires its area placement
/// and loaded model/clip data. Replacement is posed before the game loop resumes.
static TaskDesc _gPoliceOfficersCulledBodySwapTaskDescriptor = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_CONTROL } },
    .callback = _policeOfficersSwapCulledBodyTask,
    .data     = { .value = 0 },
};

TaskDesc gPoliceOfficersRookieTaskDescriptors[3] = {
    // 0: controller.
    {
        .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _policeOfficersRookieControllerTask,
        .data     = { .value = 0 },
    },
    // 1: animated body.
    {
        .header   = { .fields = { .flags = (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), .priority = TASK_PRIORITY_CONTROL } },
        .callback = _policeOfficersRookieBodyTask,
        .data     = { .model = &_gPoliceRookieBody },
    },
    // 2: culled body.
    {
        .header   = { .fields = { .flags = (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), .priority = TASK_PRIORITY_CONTROL } },
        .callback = _policeOfficersRookieCulledBodyTask,
        .data     = { .model = &_gPoliceRookieCulledBody },
    },
};

TaskDesc gPoliceOfficersSergeantTaskDescriptors[3] = {
    // 0: controller.
    {
        .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _policeOfficersSergeantControllerTask,
        .data     = { .value = 0 },
    },
    // 1: animated body.
    {
        .header   = { .fields = { .flags = (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), .priority = TASK_PRIORITY_CONTROL } },
        .callback = _policeOfficersSergeantBodyTask,
        .data     = { .model = &_gPoliceSergeantBody },
    },
    // 2: culled body.
    {
        .header   = { .fields = { .flags = (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), .priority = TASK_PRIORITY_CONTROL } },
        .callback = _policeOfficersSergeantCulledBodyTask,
        .data     = { .model = &_gPoliceSergeantCulledBody },
    },
};

/// Emits newly entered slot-1 sound cues, advances the eighteen non-root slots,
/// and returns slot 1's `ANIMATION_SLOT_REACHED_BOUNDARY` bit.
///
/// The initialized model rig borrows its animation sets. Cue identity is the
/// record pointer, so a held record sounds once. Both model initializers clear
/// `placementId` after storing it; their models therefore use the fixed sound
/// pair. The retained officer's sequence branch requires a placement ID those
/// initializers do not preserve.
static s32 _policeOfficersTickAnimation(Task* task)
{
    _OfficerWork*          work;
    const AnimationRecord* cueRecord;
    GfxCoord*              rootCoord;
    s32                    slotIndex;
    u16                    stepSoundIndex;

    work      = (_OfficerWork*)task->work;
    rootCoord = task->extra.tmd->coords;
    cueRecord = animationGetCurrentRecord(&work->rig.anim, &work->rig.slots[1]);
    // Process cues before advancing so the current record is heard once.
    if (cueRecord != work->lastCueRecord) {
        if (cueRecord != NULL) {
            if (work->placementId == POLICE_OFFICERS_PLACEMENT_ROOKIE) {
                if (cueRecord->flags & ANIMATION_RECORD_CUE_2) {
                    sndEvtRequestScriptStart(_gPoliceOfficersRookieStepSounds[work->stepSoundIndex], worldCoordGetOriginAudioPan(rootCoord), 0);
                    stepSoundIndex = work->stepSoundIndex;
                    if (stepSoundIndex < ARRAY_SIZE(_gPoliceOfficersRookieStepSounds) - 1U) {
                        work->stepSoundIndex = (u16)(stepSoundIndex + 1);
                    }
                }
            } else {
                if (cueRecord->flags & ANIMATION_RECORD_CUE_2) {
                    sndEvtRequestScriptStart(SOUND_AKROPOLIS_PLAZA_POLICE_STEP_1, worldCoordGetOriginAudioPan(rootCoord), 0);
                }
                if (cueRecord->flags & ANIMATION_RECORD_CUE_1) {
                    sndEvtRequestScriptStart(SOUND_AKROPOLIS_PLAZA_POLICE_STEP_2, worldCoordGetOriginAudioPan(rootCoord), 0);
                }
            }
        }
        work->lastCueRecord = cueRecord;
    }
    slotIndex = 1;
    do {
        animationTickSlot(&work->rig.anim, slotIndex & 0xFFFF);
        slotIndex += 1;
    } while ((u32)(slotIndex & 0xFFFF) < ARRAY_SIZE(work->rig.slots));
    return work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY;
}

/// Reports whether the borrowed player task is absent or has finished its clip.
///
/// Returns 1 when the work has no player task or the player answers
/// `ANIMATION_MESSAGE_IS_PLAYING` with zero, otherwise 0.
static inline u16 _policeOfficersIsPlayerAnimationStopped(Task* task)
{
    Task* playerTask;

    playerTask = ((_OfficerWork*)task->work)->playerTask;
    if (playerTask == NULL || taskMessageDispatch(playerTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        return 1;
    }
    return 0;
}

/// Advances the officer and chains the officer's and player's follow-up clips.
///
/// The initialized rig and borrowed player task must remain live. Follow-up
/// selector 0 uses the twelve-entry player table, 1 the sixteen-entry officer's
/// table, and 2 the twenty-six-entry sergeant's table; cached animation IDs must
/// fit their selected tables. A negative table entry ends that chain.
/// Officer transitions blend for eight frames; player transitions use ten
/// frames and enable world collision. A missing player still advances its
/// cached chain. The player table currently contains only end markers.
static void _policeOfficersUpdateAnimation(Task* task)
{
    AnimationPlayRequest playerRequest;
    _OfficerWork*        work;
    _OfficerWork*        officerWork;
    _OfficerWork*        playerWork;
    u16                  followUpAnimationId;
    s32                  slotIndex;

    work = (_OfficerWork*)task->work;
    if (_policeOfficersTickAnimation(task) & 0xFFFF) {
        followUpAnimationId = _gPoliceOfficersFollowUps[work->followUpTable][work->animationId];
        if (_gPoliceOfficersFollowUps[work->followUpTable][work->animationId] >= 0) {
            officerWork = (_OfficerWork*)task->work;
            slotIndex   = 1;
            do {
                animationSeekSlotWithBlend(&officerWork->rig.anim, slotIndex & 0xFFFF, followUpAnimationId & 0xFFFF, 0, OFFICER_BLEND_FRAMES);
                slotIndex += 1;
            } while ((u32)(slotIndex & 0xFFFF) < ARRAY_SIZE(officerWork->rig.slots));
            work->animationId = followUpAnimationId;
        }
    }
    // Chain the paired player clip only after the player reports it has stopped.
    if (_policeOfficersIsPlayerAnimationStopped(task)) {
        followUpAnimationId = _gPoliceOfficersFollowUps[POLICE_OFFICERS_ANIMATION_PLAYER][work->playerAnimationId];
        if (_gPoliceOfficersFollowUps[POLICE_OFFICERS_ANIMATION_PLAYER][work->playerAnimationId] >= 0) {
            playerWork = (_OfficerWork*)task->work;
            if (playerWork->playerTask != NULL) {
                playerRequest.source.sets          = gPoliceOfficersPlayerAnimSets;
                playerRequest.animationId          = followUpAnimationId;
                playerRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
                playerRequest.blendFrames          = PLAYER_BLEND_FRAMES;
                playerRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(playerWork->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &playerRequest, 0);
            }
            work->playerAnimationId = followUpAnimationId;
        }
    }
}

/// Places the replacement officer model, holds its pose and releases the display handoff.
///
/// The live controller must own an initialized model and the area must contain
/// `placementId`. XYZ use the root's parent frame; yaw uses 4096 units per turn.
/// The replacement is already on the default task list. Holds its pose, kills
/// the temporary display task, then resumes the game loop; retains no pointer.
static inline void _policeOfficersFinishModelSwap(Task* displayTask, const _OfficerWork* controllerWork, u8 placementId)
{
    _OfficerWork*        modelWork;
    Task*                modelTask;
    TmdObject*           model;
    GfxCoord*            rootCoord;
    const AreaPlacement* placement;

    modelTask = controllerWork->modelTask;
    placement = areaGetVariant(&gGameSession->location)->placements;
    while (placement->entryId != AREA_PLACEMENT_END && placement->entryId != placementId) {
        placement++;
    }
    model                 = modelTask->extra.tmd;
    rootCoord             = model->coords;
    rootCoord->coord.t[0] = placement->x;
    rootCoord->coord.t[1] = placement->y;
    rootCoord->coord.t[2] = placement->z;
    gfxRotMatrixY(&rootCoord->coord, placement->yaw, 0);
    modelWork            = controllerWork->modelTask->work;
    modelWork->playState = PLAY_STATE_POSED;
    taskKill(displayTask);
    displayResumeGameLoop();
}

/// Replaces the controller's officer model with its body model after a three-tick display hold.
///
/// `spawnArg2` borrows the live controller task. `spawnArg1` selects the sergeant
/// with 0 or the officer with 1; other selectors are unsupported. The area's
/// placement table must contain that officer's record. The replacement starts
/// posed on the default task list, then this temporary display task releases
/// presentation back to the game loop. Its clip is the controller's `bodyAnimationId`.
static void _policeOfficersSwapBodyTask(Task* task)
{
    _OfficerWork* controllerWork;
    u8            placementId;

    controllerWork = (_OfficerWork*)((Task*)task->spawnArg2.pointer)->work;
    switch (task->state) {
        case SWAP_STATE_HOLD:
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
            /* fallthrough */
        case SWAP_STATE_WAIT_1:
        case SWAP_STATE_WAIT_2:
            task->state++;
            return;
        case SWAP_STATE_INSTALL:
            if (task->spawnArg1.value == MODEL_SELECT_SERGEANT) {
                do {
                    placementId = POLICE_OFFICERS_PLACEMENT_SERGEANT;
                } while (0);
                controllerWork->modelTask = taskSpawnFromTableOnDefaultList(gPoliceOfficersSergeantTaskDescriptors, MODEL_TASK_BODY, (s32)controllerWork->bodyAnimationId, 0);
            } else if (task->spawnArg1.value == MODEL_SELECT_ROOKIE) {
                do {
                    do {
                        placementId = POLICE_OFFICERS_PLACEMENT_ROOKIE;
                    } while (0);
                } while (0);
                controllerWork->modelTask = taskSpawnFromTableOnDefaultList(gPoliceOfficersRookieTaskDescriptors, MODEL_TASK_BODY, (s32)controllerWork->bodyAnimationId, 0);
            }
            _policeOfficersFinishModelSwap(task, controllerWork, placementId);
            break;
    }
}

/// Replaces the controller's officer model with its culled-body model after a three-tick display hold.
///
/// `spawnArg2` borrows the live controller task. `spawnArg1` selects the sergeant
/// with 0 or the officer with 1; other selectors are unsupported. The area's
/// placement table must contain that officer's record. The replacement starts
/// posed on the default task list, then this temporary display task releases
/// presentation back to the game loop. The sergeant starts on his talk idle, the officer on his idle aside.
static void _policeOfficersSwapCulledBodyTask(Task* task)
{
    // Spawn clips for the two culled-body models.
    enum {
        SERGEANT_CULLED_BODY_ANIMATION = 5,
        ROOKIE_CULLED_BODY_ANIMATION   = 7,
    };

    _OfficerWork* controllerWork;
    u8            placementId;

    controllerWork = (_OfficerWork*)((Task*)task->spawnArg2.pointer)->work;
    switch (task->state) {
        case SWAP_STATE_HOLD:
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
            /* fallthrough */
        case SWAP_STATE_WAIT_1:
        case SWAP_STATE_WAIT_2:
            task->state++;
            return;
        case SWAP_STATE_INSTALL:
            if (task->spawnArg1.value == MODEL_SELECT_SERGEANT) {
                do {
                    placementId = POLICE_OFFICERS_PLACEMENT_SERGEANT;
                } while (0);
                controllerWork->modelTask = taskSpawnFromTableOnDefaultList(gPoliceOfficersSergeantTaskDescriptors, MODEL_TASK_CULLED_BODY, SERGEANT_CULLED_BODY_ANIMATION, 0);
            } else if (task->spawnArg1.value == MODEL_SELECT_ROOKIE) {
                do {
                    do {
                        placementId = POLICE_OFFICERS_PLACEMENT_ROOKIE;
                    } while (0);
                } while (0);
                controllerWork->modelTask = taskSpawnFromTableOnDefaultList(gPoliceOfficersRookieTaskDescriptors, MODEL_TASK_CULLED_BODY, ROOKIE_CULLED_BODY_ANIMATION, 0);
            }
            _policeOfficersFinishModelSwap(task, controllerWork, placementId);
            break;
    }
}

/// Restarts every non-root officer slot at unit rate on the selected clip.
///
/// Requires an initialized nineteen-part rig and a loaded `animationId` with
/// tracks 1..18 in its bound table. Resets slot timing, track endpoints and
/// playback flags without ticking a pose; slot 0 and coordinates are untouched.
/// Clip data must remain live during playback. Borrows work without allocating.
static inline void _policeOfficersResetSlots(_OfficerWork* slotWork, u16 animationId)
{
    u16 slotIndex;

    slotIndex = 1;
    do {
        slotWork->rig.slots[slotIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&slotWork->rig.anim, slotIndex, animationId);
        slotIndex += 1;
    } while (slotIndex < ARRAY_SIZE(slotWork->rig.slots));
}

/// Initializes an officer's body-model rig, primitive buffer and placement textures.
///
/// `placementId` is `POLICE_OFFICERS_PLACEMENT_ROOKIE` or
/// `POLICE_OFFICERS_PLACEMENT_SERGEANT` and must have an area placement record.
/// The task already owns the nineteen-part model and its coordinates. This
/// allocates task-owned work, borrows the live player task and the selected
/// animation sets, parents the root to the view, and starts slots 1..18 on
/// the low halfword of `spawnArg1`. Allocation failure kills the task.
/// Clearing the work erases the preceding placement-ID store. It then runs one
/// animation/follow-up update before installing its message table.
static void _policeOfficersInitBody(Task* task, s32 placementId)
{
    _OfficerWork*  work;
    _OfficerWork*  slotWork;
    TmdObject*     model;
    GfxCoord*      rootCoord;
    AreaPlacement* placement;
    u16            officerPlacementId;
    u16            spawnAnimationId;
    u8             texturePlacementId;

    rootCoord          = task->extra.tmd->coords;
    model              = task->extra.tmd;
    work               = memMalloc(sizeof(*work), false);
    officerPlacementId = placementId;
    task->work         = work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    work->placementId = placementId;
    // Retain the store-before-clear order: placementId becomes zero.
    memFillBytes(task->work, 0U, sizeof(*work));
    work->playerTask  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    rootCoord->parent = &gGfxViewCoord;
    tmdAllocPrimitiveBuffer(model);
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
    model->flags    = 0;
    if (officerPlacementId == POLICE_OFFICERS_PLACEMENT_ROOKIE) {
        animationInitContext(&work->rig.anim, _gPoliceOfficersRookieAnimSets, model, work->rig.poses,
                             &work->rig.slots[0]);
    } else {
        animationInitContext(&work->rig.anim, _gPoliceOfficersSergeantAnimSets, model, work->rig.poses,
                             &work->rig.slots[0]);
    }
    spawnAnimationId = task->spawnArg1.value;
    slotWork         = (_OfficerWork*)task->work;
    _policeOfficersResetSlots(slotWork, spawnAnimationId);
    _policeOfficersUpdateAnimation(task);
    task->msgTable     = _gPoliceOfficersMessages;
    texturePlacementId = officerPlacementId;
    placement          = areaGetVariant(&gGameSession->location)->placements;
    while (placement->entryId != AREA_PLACEMENT_END && placement->entryId != texturePlacementId) {
        placement++;
    }
    tmdSetTextureOffsets(model, placement->texturePageOffset, placement->clutRowOffset);
}

/// Initializes an officer's culled-body rig, primitive buffer and placement textures.
///
/// `placementId` is `POLICE_OFFICERS_PLACEMENT_ROOKIE` or
/// `POLICE_OFFICERS_PLACEMENT_SERGEANT` and must have an area placement record.
/// The task already owns the nineteen-part model and its coordinates. This
/// allocates task-owned work, borrows the live player task and the selected
/// animation sets, parents the root to the view, and starts slots 1..18 on
/// the low halfword of `spawnArg1`. Allocation failure kills the task.
/// Clearing the work erases the preceding placement-ID store. The spawn clip
/// is retained for the controller to restore when this model is hidden. It runs
/// one animation/follow-up update; its frame handlers hold the resulting pose.
static void _policeOfficersInitCulledBody(Task* task, s32 placementId)
{
    _OfficerWork*  work;
    _OfficerWork*  slotWork;
    TmdObject*     model;
    GfxCoord*      rootCoord;
    AreaPlacement* placement;
    u16            officerPlacementId;
    u16            spawnAnimationId;
    u8             texturePlacementId;

    rootCoord          = task->extra.tmd->coords;
    model              = task->extra.tmd;
    work               = memMalloc(sizeof(*work), false);
    officerPlacementId = placementId;
    task->work         = work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    work->placementId = placementId;
    // Retain the store-before-clear order: placementId becomes zero.
    memFillBytes(task->work, 0U, sizeof(*work));
    work->playerTask  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    rootCoord->parent = &gGfxViewCoord;
    tmdAllocPrimitiveBuffer(model);
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
    model->flags    = 0;
    if (officerPlacementId == POLICE_OFFICERS_PLACEMENT_ROOKIE) {
        animationInitContext(&work->rig.anim, _gPoliceOfficersRookieAnimSets, model, work->rig.poses,
                             &work->rig.slots[0]);
    } else {
        animationInitContext(&work->rig.anim, _gPoliceOfficersSergeantAnimSets, model, work->rig.poses,
                             &work->rig.slots[0]);
    }
    work->spawnAnimationId = task->spawnArg1.value;
    spawnAnimationId       = work->spawnAnimationId;
    slotWork               = (_OfficerWork*)task->work;
    _policeOfficersResetSlots(slotWork, spawnAnimationId);
    _policeOfficersUpdateAnimation(task);
    task->msgTable     = _gPoliceOfficersMessages;
    texturePlacementId = officerPlacementId;
    placement          = areaGetVariant(&gGameSession->location)->placements;
    while (placement->entryId != AREA_PLACEMENT_END && placement->entryId != texturePlacementId) {
        placement++;
    }
    tmdSetTextureOffsets(model, placement->texturePageOffset, placement->clutRowOffset);
}

/// Shows or releases the officer's posed model as the plaza movie advances.
///
/// Allocates uncleared controller work and installs the scene-message table.
/// Stream sub-ID 3 parks the controller without releasing its current model.
/// Sub-IDs 0/1 show it from movie frame 230; sub-ID 2 hides it at frames 7..107.
/// Other sub-IDs show it. Requires the officer's area placement and successful
/// model-task creation. Hidden-to-shown transitions restart culled-body clip 1;
/// placement uses parent-frame game units and yaw at 4096 units per turn.
static void _policeOfficersRookieControllerTask(Task* task)
{
    enum {
        ROOKIE_SHOW_FRAME       = 230,
        ROOKIE_HIDE_FRAME       = 7,
        ROOKIE_HIDE_FRAME_COUNT = 101,
    };
    /// Stores whether this movie part and frame should show the officer.
    ///
    /// Captures gCdCmdQueue and the local timeline constants. result must be a
    /// side-effect-free u16 lvalue; it may be assigned repeatedly. Invoke in a
    /// braced block. The unsigned
    /// frame subtraction also excludes frames before the window starts.
#define ROOKIE_MOVIE_VISIBLE(result)                                                            \
    {                                                                                           \
        (result) = 1;                                                                           \
        if (gCdCmdQueue.plazaStreamSubId < (u32)PLAZA_STREAM_PART_2) {                          \
            (result) = gCdCmdQueue.sceneFrame >= (u32)ROOKIE_SHOW_FRAME;                        \
        }                                                                                       \
        if (gCdCmdQueue.plazaStreamSubId == PLAZA_STREAM_PART_2 &&                              \
            (u32)(gCdCmdQueue.sceneFrame - ROOKIE_HIDE_FRAME) < (u32)ROOKIE_HIDE_FRAME_COUNT) { \
            (result) = 0;                                                                       \
        }                                                                                       \
    }
    _OfficerWork*  work;
    _OfficerWork*  hiddenWork;
    AreaPlacement* placement;
    GfxCoord*      rootCoord;
    Task*          modelTask;
    u16            streamSubId;
    u16            modelVisible;

    streamSubId = gCdCmdQueue.plazaStreamSubId;
    if (streamSubId == PLAZA_STREAM_PART_3) {
        task->state = streamSubId;
    }
    switch (task->state) {
        case CONTROLLER_INIT:
            task->work = memMalloc(sizeof(*work), false);
            if (task->work == NULL) {
                enemyDestroy(task->spawnArg2.pointer, task);
                return;
            }
            task->msgTable = _gPoliceOfficersMessages;
            task->state++;
            break;
        case CONTROLLER_HIDDEN:
            // Keep the model lifetime synchronized with its movie visibility window:
            ROOKIE_MOVIE_VISIBLE(modelVisible);
            if (modelVisible) {
                work      = task->work;
                placement = areaGetVariant(&gGameSession->location)->placements;
                while (placement->entryId != AREA_PLACEMENT_END && placement->entryId != POLICE_OFFICERS_PLACEMENT_ROOKIE) {
                    placement++;
                }
                modelTask             = taskSpawnFromTable(gPoliceOfficersRookieTaskDescriptors, MODEL_TASK_CULLED_BODY, POLICE_ROOKIE_ANIMATION_STAND, 0);
                work->modelTask       = modelTask;
                rootCoord             = modelTask->extra.tmd->coords;
                rootCoord->coord.t[0] = placement->x;
                rootCoord->coord.t[1] = placement->y;
                rootCoord->coord.t[2] = placement->z;
                gfxRotMatrixY(&rootCoord->coord, placement->yaw, 0);
                task->state++;
            }
            break;
        case CONTROLLER_SHOWN:
            ROOKIE_MOVIE_VISIBLE(modelVisible);
            if (!modelVisible) {
                hiddenWork = task->work;
                task->state--;
                taskKill(hiddenWork->modelTask);
                hiddenWork->modelTask = NULL;
            }
            break;
    }
#undef ROOKIE_MOVIE_VISIBLE
}

/// Shows or releases the sergeant's posed model as the plaza movie advances.
///
/// Allocates uncleared controller work, installs scene messages and seeds clip
/// 24. Stream sub-ID 3 parks the controller without releasing its current model.
/// Sub-IDs 0/1 show it at movie frames 75..195; 2, 4 and 5 hide it; others show it.
/// Requires the sergeant's area placement and successful model-task creation.
/// Saves the model's spawn clip before hiding and reuses it on the next spawn.
/// Placement uses parent-frame game units and yaw at 4096 units per turn.
static void _policeOfficersSergeantControllerTask(Task* task)
{
    enum {
        SERGEANT_SHOW_FRAME       = 75,
        SERGEANT_SHOW_FRAME_COUNT = 121,
    };
    /// Stores whether this movie part and frame should show the officer.
    ///
    /// Captures gCdCmdQueue and the local timeline constants. result must be a
    /// side-effect-free u16 lvalue; it may be assigned repeatedly. Invoke in a
    /// braced block. The unsigned
    /// frame subtraction also excludes frames before the window starts.
#define SERGEANT_MOVIE_VISIBLE(result)                                                                       \
    {                                                                                                        \
        (result) = 1;                                                                                        \
        if (gCdCmdQueue.plazaStreamSubId < (u32)PLAZA_STREAM_PART_2) {                                       \
            (result) = (u32)(gCdCmdQueue.sceneFrame - SERGEANT_SHOW_FRAME) < (u32)SERGEANT_SHOW_FRAME_COUNT; \
        }                                                                                                    \
        if (gCdCmdQueue.plazaStreamSubId == PLAZA_STREAM_PART_2) {                                           \
            (result) = 0;                                                                                    \
        }                                                                                                    \
        if (gCdCmdQueue.plazaStreamSubId == PLAZA_STREAM_PART_4) {                                           \
            (result) = 0;                                                                                    \
        }                                                                                                    \
        if (gCdCmdQueue.plazaStreamSubId == PLAZA_STREAM_PART_5) {                                           \
            (result) = 0;                                                                                    \
        }                                                                                                    \
    }
    _OfficerWork*  work;
    AreaPlacement* placement;
    GfxCoord*      rootCoord;
    Task*          modelTask;
    u16            streamSubId;
    u16            modelVisible;

    work        = task->work;
    streamSubId = gCdCmdQueue.plazaStreamSubId;
    if (streamSubId == PLAZA_STREAM_PART_3) {
        task->state = streamSubId;
    }
    switch (task->state) {
        case CONTROLLER_INIT:
            task->work = (work = memMalloc(sizeof(*work), false));
            if (work == NULL) {
                enemyDestroy(task->spawnArg2.pointer, task);
                return;
            }
            task->msgTable         = _gPoliceOfficersMessages;
            work->spawnAnimationId = POLICE_SERGEANT_ANIMATION_IDLE;
            task->state++;
            break;
        case CONTROLLER_HIDDEN:
            SERGEANT_MOVIE_VISIBLE(modelVisible);
            if (modelVisible) {
                placement = areaGetVariant(&gGameSession->location)->placements;
                while (placement->entryId != AREA_PLACEMENT_END && placement->entryId != POLICE_OFFICERS_PLACEMENT_SERGEANT) {
                    placement++;
                }
                modelTask             = taskSpawnFromTable(gPoliceOfficersSergeantTaskDescriptors, MODEL_TASK_CULLED_BODY, (s32)work->spawnAnimationId, 0);
                work->modelTask       = modelTask;
                rootCoord             = modelTask->extra.tmd->coords;
                rootCoord->coord.t[0] = placement->x;
                rootCoord->coord.t[1] = placement->y;
                rootCoord->coord.t[2] = placement->z;
                gfxRotMatrixY(&rootCoord->coord, placement->yaw, 0);
                task->state++;
            }
            break;
        case CONTROLLER_SHOWN:
            SERGEANT_MOVIE_VISIBLE(modelVisible);
            if (!modelVisible) {
                work->spawnAnimationId = ((const _OfficerWork*)work->modelTask->work)->spawnAnimationId;
                task->state--;
                taskKill(work->modelTask);
                work->modelTask = NULL;
            }
            break;
    }
#undef SERGEANT_MOVIE_VISIBLE
}

/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` by scheduling an officer body-model swap.
///
/// The receiver is the live officer controller; selector 0 chooses the sergeant,
/// 1 chooses the officer. This receiver requires a non-null, synchronously
/// borrowed `AnimationPlayRequest` as the second payload and copies only its
/// animation ID's low halfword. It releases any old model before the delayed
/// swap. The callback leaves the result word unset; senders must discard it.
static s32 _policeOfficersSetBody(Task* task, s32 messageId, s32 officerSelector, const AnimationPlayRequest* animation)
{
    _OfficerWork* controllerWork;

    controllerWork = (_OfficerWork*)task->work;
    if (controllerWork->modelTask != NULL) {
        taskKill(controllerWork->modelTask);
    }
    controllerWork->bodyAnimationId = animation->animationId;
    displaySpawnTaskFromTable(&_gPoliceOfficersBodySwapTaskDescriptor, 0, officerSelector, task);
}

/// Handles the culled-body message by freezing the model or scheduling its replacement.
///
/// The controller must have live model work before every mode, including a
/// replacement request. Mode 3 freezes that model without replacing it; mode
/// 0 replaces it with the sergeant's culled body, mode 1 with the officer's.
/// The second payload is unused. The callback leaves the result word unset;
/// senders must discard it.
static s32 _policeOfficersSetCulledBody(Task* task, s32 messageId, s32 mode, s32 unusedSecondArg)
{
    _OfficerWork* controllerWork;
    _OfficerWork* modelWork;

    controllerWork = (_OfficerWork*)task->work;
    modelWork      = (_OfficerWork*)controllerWork->modelTask->work;
    if (mode == POLICE_OFFICERS_CULLED_BODY_FREEZE) {
        modelWork->playState = PLAY_STATE_FROZEN;
    } else {
        if (controllerWork->modelTask != NULL) {
            taskKill(controllerWork->modelTask);
        }
        displaySpawnTaskFromTable(&_gPoliceOfficersCulledBodySwapTaskDescriptor, 0, mode, task);
    }
}

/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION` for the officer or its paired player.
///
/// The controller must have an initialized live model. `animation` is borrowed
/// for this dispatch: the source index is an
/// `POLICE_OFFICERS_ANIMATION_*` target, matching the model's bound sets. IDs must fit the twelve-entry
/// player or sixteen-/twenty-six-entry officer follow-up table. ID and blend
/// values narrow to unsigned halfwords. Officer reset requests restore unit
/// rates; nonzero blends use eight frames. Player requests use ten frames and
/// enable world collision, ignoring the corresponding incoming fields.
/// Both routes enable model playback. The second payload is unused; senders
/// must discard the unset callback result.
static s32 _policeOfficersPlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* animation, s32 unusedSecondArg)
{
    AnimationPlayRequest playerRequest;
    _OfficerWork*        controllerWork;
    _OfficerWork*        modelWork;
    _OfficerWork*        playerWork;
    _OfficerWork*        slotWork;
    Task*                modelTask;
    u16                  playerAnimationId;
    u16                  playerBlend;
    u16                  officerBlend;
    u16                  officerAnimationId;
    s32                  slotIndex;

    controllerWork       = (_OfficerWork*)task->work;
    modelTask            = controllerWork->modelTask;
    modelWork            = (_OfficerWork*)modelTask->work;
    modelWork->playState = PLAY_STATE_PLAYING;
    // A player request installs the paired sets; officer requests drive this rig.
    if (animation->source.index == POLICE_OFFICERS_ANIMATION_PLAYER) {
        modelWork->playerAnimationId = animation->animationId;
        playerWork                   = (_OfficerWork*)modelTask->work;
        playerAnimationId            = animation->animationId;
        playerBlend                  = animation->blend;
        if (playerWork->playerTask != NULL) {
            playerRequest.source.sets          = gPoliceOfficersPlayerAnimSets;
            playerRequest.animationId          = playerAnimationId;
            playerRequest.blend                = playerBlend;
            playerRequest.blendFrames          = PLAYER_BLEND_FRAMES;
            playerRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(playerWork->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &playerRequest, 0);
        }
    } else {
        officerAnimationId = animation->animationId;
        officerBlend       = animation->blend;
        slotWork           = (_OfficerWork*)modelTask->work;
        slotIndex          = 1;
        if (officerBlend == ANIMATION_BLEND_RESET) {
            _policeOfficersResetSlots(slotWork, officerAnimationId);
        } else {
            do {
                animationSeekSlotWithBlend(&slotWork->rig.anim, slotIndex & 0xFFFF, officerAnimationId, 0, OFFICER_BLEND_FRAMES);
                slotIndex += 1;
            } while ((u32)(slotIndex & 0xFFFF) < ARRAY_SIZE(slotWork->rig.slots));
        }
        modelWork->followUpTable = animation->source.index;
        modelWork->animationId   = animation->animationId;
    }
}

/// Handles `ACTOR_MESSAGE_PLACE` by placing the controller's live officer model.
///
/// Borrows `placement` through dispatch. Translation is in the root's parent
/// coordinate units; only Y rotation is used, at 4096 angle units per turn.
/// Rebuilds the root's yaw and marks composition dirty. The second payload is
/// unused; senders must discard the unset callback result.
static s32 _policeOfficersPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedSecondArg)
{
    _OfficerWork* controllerWork;
    GfxCoord*     rootCoord;

    controllerWork        = (_OfficerWork*)task->work;
    rootCoord             = controllerWork->modelTask->extra.tmd->coords;
    rootCoord->coord.t[0] = placement->pos.vx;
    rootCoord->coord.t[1] = placement->pos.vy;
    rootCoord->coord.t[2] = placement->pos.vz;
    gfxRotMatrixY(&rootCoord->coord, placement->rot.vy, 0);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Handles the stop message by releasing the model and stopping the officer controller.
///
/// The receiver's controller work must be initialized. Both payloads are
/// unused. The controller task remains alive in state 3 so a later scene
/// message can install another model; senders must discard the unset result.
static s32 _policeOfficersStopController(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    enum { CONTROLLER_STATE_STOPPED = 3 };

    _OfficerWork* controllerWork;

    controllerWork = (_OfficerWork*)task->work;
    if (controllerWork->modelTask != NULL) {
        taskKill(controllerWork->modelTask);
        controllerWork->modelTask = NULL;
    }
    task->state = CONTROLLER_STATE_STOPPED;
}

/// Rebuilds an officer model's full lighting at the origin of its part-1 frame.
///
/// Requires a live nineteen-part model with writable light and colour matrices.
/// Samples its cached signed world translation without composing it; the part
/// and view must already be current. The lighting query borrows nested scratch
/// storage and changes GTE state; no pointer to this local sample is retained.
static inline void _policeOfficersLightModel(Task* task)
{
    VECTOR     position;
    TmdObject* model;

    model       = task->extra.tmd;
    position.vx = model->coords[1].workm.t[0];
    position.vy = task->extra.tmd->coords[1].workm.t[1];
    position.vz = task->extra.tmd->coords[1].workm.t[2];
    worldCoordSetModelLighting(model, &position, 0, ARRAY_SIZE(model->colorMtx->m[0]));
}

/// Draws an officer model's ground shadow: a square in the XZ plane of its
/// part-1 frame, centred at positive local Y `SHADOW_DROP`.
///
/// Requires the task's live nineteen-part model and coordinate hierarchy.
/// The side and centre offset use that part's integer coordinate units; the
/// ground-shadow drawer composes the frame and borrows a packet for this frame.
static inline void _policeOfficersDrawShadow(Task* modelTask)
{
    SVECTOR offset;

    offset.vx = 0;
    offset.vy = SHADOW_DROP;
    offset.vz = 0;
    actorRenderDrawGroundShadow(&modelTask->extra.tmd->coords[1], SHADOW_SIDE, &offset);
}

/// Initializes and updates the officer's animated body, lighting and ground shadow.
///
/// Requires a nineteen-part model; ready ticks require initialized work.
/// Initialization binds the officer's rig and falls through to ready processing.
/// Every ready tick draws the shadow first. Playing advances animation and
/// follow-ups; posed holds the rig. Frozen or unhandled playback states return
/// after the shadow, before animation and lighting.
static void _policeOfficersRookieBodyTask(Task* task)
{
    _OfficerWork* work;

    // Retain the incoming work pointer across the spawn tick's initialization.
    work = task->work;
    switch (task->state) {
        case MODEL_INIT:
            _policeOfficersInitBody(task, POLICE_OFFICERS_PLACEMENT_ROOKIE);
            task->state++;
            /* fallthrough */
        case MODEL_READY:
            _policeOfficersDrawShadow(task);
            switch (work->playState) {
                case PLAY_STATE_POSED:
                    break;
                case PLAY_STATE_PLAYING:
                    _policeOfficersUpdateAnimation(task);
                    break;
                case PLAY_STATE_FROZEN:
                default:
                    return;
            }
            _policeOfficersLightModel(task);
            break;
    }
}

/// Initializes and updates the sergeant's animated body, lighting and ground shadow.
///
/// Requires a nineteen-part model; ready ticks require initialized work.
/// Initialization binds the sergeant's rig and falls through to ready processing.
/// Every ready tick draws the shadow first. Playing advances animation and
/// follow-ups; posed holds the rig. Frozen or unhandled playback states return
/// after the shadow, before animation and lighting.
static void _policeOfficersSergeantBodyTask(Task* task)
{
    _OfficerWork* work;

    // Retain the incoming work pointer across the spawn tick's initialization.
    work = task->work;
    switch (task->state) {
        case MODEL_INIT:
            _policeOfficersInitBody(task, POLICE_OFFICERS_PLACEMENT_SERGEANT);
            task->state++;
            /* fallthrough */
        case MODEL_READY:
            _policeOfficersDrawShadow(task);
            switch (work->playState) {
                case PLAY_STATE_POSED:
                    break;
                case PLAY_STATE_PLAYING:
                    _policeOfficersUpdateAnimation(task);
                    break;
                case PLAY_STATE_FROZEN:
                default:
                    return;
            }
            _policeOfficersLightModel(task);
            break;
    }
}

/// Initializes the officer's culled body and draws its shadow while posed.
///
/// Requires a nineteen-part model; ready ticks require initialized work.
/// The spawn tick binds
/// the selected pose, composes part 1 and samples lighting once. Ready ticks
/// never advance animation or lighting; only the posed playback state draws
/// the shadow. Playing, frozen and unhandled states draw no shadow.
static void _policeOfficersRookieCulledBodyTask(Task* task)
{
    _OfficerWork* work;

    work = task->work;
    switch (task->state) {
        case MODEL_INIT:
            _policeOfficersInitCulledBody(task, POLICE_OFFICERS_PLACEMENT_ROOKIE);
            actorRenderComposeCoord(&task->extra.tmd->coords[1]);
            _policeOfficersLightModel(task);
            task->state++;
            break;
        case MODEL_READY:
            if (work->playState == PLAY_STATE_POSED) {
                _policeOfficersDrawShadow(task);
            }
            break;
    }
}

/// Initializes the sergeant's culled body and draws its shadow until frozen.
///
/// Requires a nineteen-part model; ready ticks require initialized work.
/// The spawn tick binds
/// the selected pose, composes part 1 and samples lighting once. Ready ticks
/// never advance animation or lighting; posed and playing states draw the
/// shadow. Frozen and unhandled states draw no shadow.
static void _policeOfficersSergeantCulledBodyTask(Task* task)
{
    _OfficerWork* work;

    work = task->work;
    switch (task->state) {
        case MODEL_INIT:
            _policeOfficersInitCulledBody(task, POLICE_OFFICERS_PLACEMENT_SERGEANT);
            actorRenderComposeCoord(&task->extra.tmd->coords[1]);
            _policeOfficersLightModel(task);
            task->state++;
            break;
        case MODEL_READY:
            switch (work->playState) {
                case PLAY_STATE_POSED:
                case PLAY_STATE_PLAYING:
                    _policeOfficersDrawShadow(task);
                    break;
            }
            break;
    }
}
