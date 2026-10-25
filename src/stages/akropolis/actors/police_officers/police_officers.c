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

#include "stages/akropolis/rooms/plaza.h" // The plaza's clips, which the banks bind

// The officers are the plaza's: its movies time them and its sound bank holds their steps.
#include "../../rooms/plaza/config.h"

#include "assets/rooms/akropolis/plaza/police_rookie_schema.h"
#include "assets/rooms/akropolis/plaza/police_sergeant_schema.h"

/* The Akropolis plaza's two police officers: the rookie at the tower's entrance and the sergeant at the barricade.
 *
 * Each officer is two tasks. His controller is the actor the plaza's placements spawn: it shows a posed model while the
 * plaza's movies have him in shot, and releases it otherwise. His model task is that model - his culled body, posed
 * and never animated, or his body, which plays the clips the scenes ask for. The scenes swap one for the other by
 * message, with a display hold between, and play his clips and the player's (Aya's clips in the officers' scenes) by
 * message too; each clip settles into a follow-up when it ends. His clips, and the player's, are the plaza's - its
 * package places them - except the rookie's later ones, which are this overlay's.
 */

/// The ground shadow under an officer: a square, in the units of his part-1 frame.
enum {
    SHADOW_SIDE = 0x300, // Side of the shadow square
    SHADOW_DROP = 0x380, // Distance from the frame's origin down to the shadow
};

/// What an officer's model is doing (`_OfficerWork::playState`): holding its spawn pose, playing the clips the
/// scenes request, or frozen. A body draws its shadow in every state; a culled body, which never animates, while posed
/// (the rookie's) or until frozen (the sergeant's).
enum {
    PLAY_STATE_POSED   = 0, // Holding the spawn pose: relit each frame, animation not stepped
    PLAY_STATE_PLAYING = 1, // A play request arrived: the body model steps its animation and follow-ups
    PLAY_STATE_FROZEN  = 2, // Neither stepped nor relit
};

/// Which officer a model swap is for (its `spawnArg1`); the same values as `POLICE_OFFICERS_CULLED_BODY_*`.
enum {
    MODEL_SELECT_SERGEANT = 0,
    MODEL_SELECT_ROOKIE   = 1,
};

/// The model entries of an officer's task descriptors, after the controller at 0.
enum {
    MODEL_TASK_BODY        = 1,
    MODEL_TASK_CULLED_BODY = 2,
};

/// Controller lifecycle and one-time model initialization states. A stopped controller keeps no model until a scene
/// installs one.
enum {
    CONTROLLER_INIT    = 0,
    CONTROLLER_HIDDEN  = 1,
    CONTROLLER_SHOWN   = 2,
    CONTROLLER_STOPPED = 3,
    MODEL_INIT         = 0,
    MODEL_READY        = 1,
};

/// A model swap's states: it holds the display for three frames, then installs the new model.
enum {
    SWAP_STATE_HOLD    = 0,
    SWAP_STATE_WAIT_1  = 1,
    SWAP_STATE_WAIT_2  = 2,
    SWAP_STATE_INSTALL = 3,
};

/// How many frames an officer's and the player's clips blend over.
enum {
    OFFICER_BLEND_FRAMES = 8,
    PLAYER_BLEND_FRAMES  = 10,
};

/// Work of one officer's task, his controller's or his model's - the same layout, each using its own members. The
/// controller allocates it uncleared and uses only `modelTask`, `spawnAnimationId` and `bodyAnimationId`; the model
/// clears it and uses the rest. No access to the `pad_` runs is known.
typedef struct {
    ActorAnimRig19         rig;               // Model: playback storage of the nineteen-part officer model
    MATRIX                 light;             // Model: light matrix lent to the model object
    MATRIX                 color;             // Model: color matrix lent to the model object
    byte                   pad_47C[0x68];
    Task*                  modelTask;         // Controller: the officer's live model task, `NULL` once hidden or torn down
    Task*                  playerTask;        // Model: the player's task, asked to play the animations paired with the officer's
    const AnimationRecord* lastCueRecord;     // Model: record slot 1 was on at the last step, so its sound cues fire once; borrowed from the bound set
    u16                    playState;         // Model: `PLAY_STATE_*`
    byte                   pad_4F2[0x4];
    u16                    playerAnimationId; // Model: animation last requested of the player; indexes the player's follow-up table
    u16                    followUpTable;     // Model: follow-up table the officer chains through (`POLICE_OFFICERS_ANIMATION_PLAYER` until the first request, then `POLICE_OFFICERS_FOLLOW_UP_*`)
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

/// The rookie's bank, for his body and culled body: its first five clips are the plaza's, the others this overlay's,
/// so both stay loaded while a model plays from it.
#include "assets/rooms/akropolis/plaza/police_rookie_bank.inc"

/// The sergeant's bank, for his body and culled body: all its clips are the plaza's.
#include "assets/rooms/akropolis/plaza/police_sergeant_bank.inc"

#include "assets/rooms/akropolis/plaza/police_scene_player_bank.inc"

/// A follow-up entry that ends the chain.
enum { NO_FOLLOW_UP_ANIMATION = -1 };

/// The player's follow-ups in the officers' scenes, by `POLICE_SCENE_PLAYER_ANIMATION_*`: none, so a player clip
/// ends where it ends. Slot 12 of the bank has no entry; no scene plays it.
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

/// The rookie's follow-ups, by `POLICE_ROOKIE_ANIMATION_*`: the clip each clip settles into when it ends - his
/// idle, his idle aside, or his talk idle in the officers' talk.
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

/// The sergeant's follow-ups, by `POLICE_SERGEANT_ANIMATION_*`: the clip each clip settles into when it ends - his
/// idle at the barricade, his talk idle, or a turn of the head back.
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

/// The follow-up tables, by a request's source index: the player's, the rookie's, the sergeant's.
static const s16* _gPoliceOfficersFollowUps[3] = {
    _gPoliceOfficersPlayerFollowUps,
    _gPoliceOfficersRookieFollowUps,
    _gPoliceOfficersSergeantFollowUps,
};

/// The rookie's steps, in turn, one a clip's second cue, the last repeating. Unreachable: his model's placement id,
/// which selects them, is cleared at spawn (`_policeOfficersInitModel`), so he steps like the sergeant.
static s32 _gPoliceOfficersRookieStepSounds[3] = {
    AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_ROOKIE_STEP_1),
    AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_ROOKIE_STEP_2),
    AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_ROOKIE_STEP_3),
};

/// The officers' messages, answered by the controller and installed on the model too: play a clip, place, swap
/// in the body or the culled body (or freeze), and stop.
static TaskMessageEntry _gPoliceOfficersMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _policeOfficersPlayAnimation },
    { ACTOR_MESSAGE_PLACE, _policeOfficersPlace },
    { POLICE_OFFICERS_MESSAGE_SET_CULLED_BODY, _policeOfficersSetCulledBody },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _policeOfficersSetBody },
    { POLICE_OFFICERS_MESSAGE_STOP_CONTROLLER, _policeOfficersStopController },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// The display task that swaps an officer's model for his body.
static TaskDesc _gPoliceOfficersBodySwapTaskDescriptor = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_CONTROL } },
    .callback = _policeOfficersSwapBodyTask,
    .data     = { .value = 0 },
};

/// The display task that swaps an officer's model for his culled body.
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

/// Advances an officer's clip one frame: plays the steps on a record's cues as it enters it, ticks the 18 non-root
/// slots, and returns whether slot 1 reached the clip's end.
static u16 _policeOfficersTickAnimation(Task* task)
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
                    sndEvtRequestScriptStart(AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_POLICE_STEP_1), worldCoordGetOriginAudioPan(rootCoord), 0);
                }
                if (cueRecord->flags & ANIMATION_RECORD_CUE_1) {
                    sndEvtRequestScriptStart(AKROPOLIS_PLAZA_SOUND(AKROPOLIS_PLAZA_SOUND_POLICE_STEP_2), worldCoordGetOriginAudioPan(rootCoord), 0);
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

/// Places a model task's root at a placement record: its position, and its yaw (4096 units a turn).
static inline void _policeOfficersPlaceAt(Task* modelTask, const AreaPlacement* placement)
{
    GfxCoord* rootCoord;

    rootCoord             = modelTask->extra.tmd->coords;
    rootCoord->coord.t[0] = placement->x;
    rootCoord->coord.t[1] = placement->y;
    rootCoord->coord.t[2] = placement->z;
    gfxRotMatrixY(&rootCoord->coord, placement->yaw, 0);
}

/// The current area's placement record of an officer, or its list's end if absent.
static inline AreaPlacement* _policeOfficersFindPlacement(u8 placementId)
{
    AreaPlacement* placement;

    placement = areaGetVariant(&gGameSession->location)->placements;
    while (placement->entryId != AREA_PLACEMENT_END && placement->entryId != placementId) {
        placement++;
    }
    return placement;
}

/// Whether the player has no task or has finished its clip.
static inline u16 _policeOfficersIsPlayerAnimationStopped(Task* task)
{
    Task* playerTask;

    playerTask = ((_OfficerWork*)task->work)->playerTask;
    if (playerTask == NULL || taskMessageDispatch(playerTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        return 1;
    }
    return 0;
}

/// Advances an officer's clip and moves him and the player on to their follow-ups as their clips end; the player's
/// runs in the player's own bank and enables world collision.
static void _policeOfficersUpdateAnimation(Task* task)
{
    AnimationPlayRequest playerRequest;
    _OfficerWork*        work;
    _OfficerWork*        officerWork;
    _OfficerWork*        playerWork;
    u16                  followUpAnimationId;
    s32                  slotIndex;

    work = (_OfficerWork*)task->work;
    if (_policeOfficersTickAnimation(task)) {
        followUpAnimationId = _gPoliceOfficersFollowUps[work->followUpTable][work->animationId];
        if (_gPoliceOfficersFollowUps[work->followUpTable][work->animationId] >= 0) {
            // Blend every non-root slot into the follow-up. This loop and the play handler's stay written out: as a
            // helper they allocate differently.
            officerWork = (_OfficerWork*)task->work;
            slotIndex   = 1;
            do {
                animationSeekSlotWithBlend(&officerWork->rig.anim, slotIndex & 0xFFFF, followUpAnimationId, 0, OFFICER_BLEND_FRAMES);
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

/// Finishes a swap: places the new model at the officer's placement, posed, ends the display task and gives the
/// display back to the game loop.
static inline void _policeOfficersFinishModelSwap(Task* displayTask, const _OfficerWork* controllerWork, u8 placementId)
{
    _OfficerWork*        modelWork;
    Task*                modelTask;
    const AreaPlacement* placement;

    modelTask = controllerWork->modelTask;
    placement = _policeOfficersFindPlacement(placementId);
    _policeOfficersPlaceAt(modelTask, placement);
    modelWork            = controllerWork->modelTask->work;
    modelWork->playState = PLAY_STATE_POSED;
    taskKill(displayTask);
    displayResumeGameLoop();
}

/// Swaps the controller's officer model after a three-tick display hold: spawns his body on the controller's
/// `bodyAnimationId`, or his culled body (`culled`) on his posed clip - the sergeant's talk idle, the rookie's idle
/// aside - places it at his area placement, posed, and hands the display back to the game loop. `spawnArg1` selects
/// the sergeant (`MODEL_SELECT_SERGEANT`) or the rookie; `spawnArg2` is the live controller task.
static inline void _policeOfficersSwapModel(Task* task, s32 culled)
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
            // The loops around the placement ids are the original's: they weight the allocation it has.
            if (task->spawnArg1.value == MODEL_SELECT_SERGEANT) {
                do {
                    placementId = POLICE_OFFICERS_PLACEMENT_SERGEANT;
                } while (0);
                controllerWork->modelTask = taskSpawnFromTableOnDefaultList(gPoliceOfficersSergeantTaskDescriptors, culled ? MODEL_TASK_CULLED_BODY : MODEL_TASK_BODY,
                                                                            culled ? POLICE_SERGEANT_ANIMATION_TALK_IDLE : (s32)controllerWork->bodyAnimationId, 0);
            } else if (task->spawnArg1.value == MODEL_SELECT_ROOKIE) {
                do {
                    do {
                        placementId = POLICE_OFFICERS_PLACEMENT_ROOKIE;
                    } while (0);
                } while (0);
                controllerWork->modelTask = taskSpawnFromTableOnDefaultList(gPoliceOfficersRookieTaskDescriptors, culled ? MODEL_TASK_CULLED_BODY : MODEL_TASK_BODY,
                                                                            culled ? POLICE_ROOKIE_ANIMATION_ASIDE_IDLE : (s32)controllerWork->bodyAnimationId, 0);
            }
            _policeOfficersFinishModelSwap(task, controllerWork, placementId);
            break;
    }
}

/// Swaps in an officer's animated body.
static void _policeOfficersSwapBodyTask(Task* task)
{
    _policeOfficersSwapModel(task, false);
}

/// Swaps in an officer's culled body.
static void _policeOfficersSwapCulledBodyTask(Task* task)
{
    _policeOfficersSwapModel(task, true);
}

/// Restarts the 18 non-root slots on a clip at its start, at the normal rate.
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

/// Initializes an officer's model: allocates and clears its work (the placement id stored just before the clear is
/// lost, see `_policeOfficersTickAnimation`), takes the player's task, parents the model to the view, binds the
/// officer's bank and starts slots 1..18 on the spawn clip (`spawnArg1`), runs one animation update, installs the
/// officers' messages and applies its placement's texture offsets. A culled body keeps its spawn clip, which the
/// controller restores when it shows him again. Allocation failure kills the task.
static inline void _policeOfficersInitModel(Task* task, s32 placementId, s32 keepSpawnClip)
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
    if (keepSpawnClip) {
        work->spawnAnimationId = task->spawnArg1.value;
        spawnAnimationId       = work->spawnAnimationId;
    } else {
        spawnAnimationId = task->spawnArg1.value;
    }
    slotWork = (_OfficerWork*)task->work;
    _policeOfficersResetSlots(slotWork, spawnAnimationId);
    _policeOfficersUpdateAnimation(task);
    task->msgTable     = _gPoliceOfficersMessages;
    texturePlacementId = officerPlacementId;
    placement          = _policeOfficersFindPlacement(texturePlacementId);
    tmdSetTextureOffsets(model, placement->texturePageOffset, placement->clutRowOffset);
}

/// Initializes an officer's animated body.
static void _policeOfficersInitBody(Task* task, s32 placementId)
{
    _policeOfficersInitModel(task, placementId, false);
}

/// Initializes an officer's culled body, keeping its spawn clip.
static void _policeOfficersInitCulledBody(Task* task, s32 placementId)
{
    _policeOfficersInitModel(task, placementId, true);
}

/// The rookie's controller: the placed actor, which shows his posed culled body while the plaza's movies show him -
/// in the traversal from frame 230 on, in every other movie but the helicopter wreck's frames 7..107 - and releases
/// it otherwise. The officers' talk stops it; the scenes then drive his model by messages.
static void _policeOfficersRookieControllerTask(Task* task)
{
    enum {
        ROOKIE_SHOW_FRAME       = 230,
        ROOKIE_HIDE_FRAME       = 7,
        ROOKIE_HIDE_FRAME_COUNT = 101,
    };
    /// Stores whether the current movie and frame show the rookie.
    ///
    /// Captures gCdCmdQueue and the local timeline constants. result must be a
    /// side-effect-free u16 lvalue; it may be assigned repeatedly. Invoke in a
    /// braced block. The unsigned frame subtraction also excludes frames before the window starts.
#define ROOKIE_MOVIE_VISIBLE(result)                                                            \
    {                                                                                           \
        (result) = 1;                                                                           \
        if (gCdCmdQueue.plazaStreamSubId < (u32)AKROPOLIS_PLAZA_MOVIE_HELICOPTER_WRECK) {       \
            (result) = gCdCmdQueue.sceneFrame >= (u32)ROOKIE_SHOW_FRAME;                        \
        }                                                                                       \
        if (gCdCmdQueue.plazaStreamSubId == AKROPOLIS_PLAZA_MOVIE_HELICOPTER_WRECK &&           \
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
    // The officers' talk stops the controller: its movie's id is the stopped state.
    if (streamSubId == AKROPOLIS_PLAZA_MOVIE_OFFICERS_TALK) {
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
                work            = task->work;
                placement       = _policeOfficersFindPlacement(POLICE_OFFICERS_PLACEMENT_ROOKIE);
                modelTask       = taskSpawnFromTable(gPoliceOfficersRookieTaskDescriptors, MODEL_TASK_CULLED_BODY, POLICE_ROOKIE_ANIMATION_STAND, 0);
                work->modelTask = modelTask;
                _policeOfficersPlaceAt(modelTask, placement);
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

/// The sergeant's controller: shows his posed culled body in the traversal at frames 75..195 and releases it
/// in the helicopter wreck's, the badge scene's and the entering tower's movies, keeping the clip it was on for the next
/// time. The officers' talk stops it; the scenes then drive his model by messages.
static void _policeOfficersSergeantControllerTask(Task* task)
{
    enum {
        SERGEANT_SHOW_FRAME       = 75,
        SERGEANT_SHOW_FRAME_COUNT = 121,
    };
    /// Stores whether the current movie and frame show the sergeant.
    ///
    /// Captures gCdCmdQueue and the local timeline constants. result must be a
    /// side-effect-free u16 lvalue; it may be assigned repeatedly. Invoke in a
    /// braced block. The unsigned frame subtraction also excludes frames before the window starts.
#define SERGEANT_MOVIE_VISIBLE(result)                                                                       \
    {                                                                                                        \
        (result) = 1;                                                                                        \
        if (gCdCmdQueue.plazaStreamSubId < (u32)AKROPOLIS_PLAZA_MOVIE_HELICOPTER_WRECK) {                    \
            (result) = (u32)(gCdCmdQueue.sceneFrame - SERGEANT_SHOW_FRAME) < (u32)SERGEANT_SHOW_FRAME_COUNT; \
        }                                                                                                    \
        if (gCdCmdQueue.plazaStreamSubId == AKROPOLIS_PLAZA_MOVIE_HELICOPTER_WRECK) {                        \
            (result) = 0;                                                                                    \
        }                                                                                                    \
        if (gCdCmdQueue.plazaStreamSubId == AKROPOLIS_PLAZA_MOVIE_ENTRANCE_OFFICER_TALK) {                   \
            (result) = 0;                                                                                    \
        }                                                                                                    \
        if (gCdCmdQueue.plazaStreamSubId == AKROPOLIS_PLAZA_MOVIE_ENTERING_TOWER) {                          \
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
    // The officers' talk stops the controller: its movie's id is the stopped state.
    if (streamSubId == AKROPOLIS_PLAZA_MOVIE_OFFICERS_TALK) {
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
                placement       = _policeOfficersFindPlacement(POLICE_OFFICERS_PLACEMENT_SERGEANT);
                modelTask       = taskSpawnFromTable(gPoliceOfficersSergeantTaskDescriptors, MODEL_TASK_CULLED_BODY, (s32)work->spawnAnimationId, 0);
                work->modelTask = modelTask;
                _policeOfficersPlaceAt(modelTask, placement);
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

/// `ACTOR_MESSAGE_SET_MODEL_DRAW`: a scene swaps in the officer's (`MODEL_SELECT_*`) body, on the request's clip.
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

/// `POLICE_OFFICERS_MESSAGE_SET_CULLED_BODY`: swaps in the officer's culled body, posed, or freezes his model.
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

/// `ACTOR_MESSAGE_PLAY_ANIMATION`: plays a clip on the player (`POLICE_OFFICERS_ANIMATION_PLAYER`, from the player's
/// bank) or on the officer (from his own bank, blended in or restarted), whose follow-ups then come from the table the
/// request names.
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

/// `ACTOR_MESSAGE_PLACE`: places the officer's model, at a position and a yaw.
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

/// `POLICE_OFFICERS_MESSAGE_STOP_CONTROLLER`: releases the officer's model and stops his controller, which then waits
/// for a scene to swap a model in.
static s32 _policeOfficersStopController(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    _OfficerWork* controllerWork;

    controllerWork = (_OfficerWork*)task->work;
    if (controllerWork->modelTask != NULL) {
        taskKill(controllerWork->modelTask);
        controllerWork->modelTask = NULL;
    }
    task->state = CONTROLLER_STOPPED;
}

/// Lights an officer's model at the origin of his part-1 frame.
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

/// Draws an officer's ground shadow, `SHADOW_DROP` below his part-1 frame.
static inline void _policeOfficersDrawShadow(Task* modelTask)
{
    SVECTOR offset;

    offset.vx = 0;
    offset.vy = SHADOW_DROP;
    offset.vz = 0;
    actorRenderDrawGroundShadow(&modelTask->extra.tmd->coords[1], SHADOW_SIDE, &offset);
}

/// An officer's animated body: initializes on its first frame, then each frame draws the shadow and, unless frozen,
/// advances its clip while playing and relights.
static inline void _policeOfficersUpdateBody(Task* task, s32 placementId)
{
    _OfficerWork* work;

    // Retain the incoming work pointer across the spawn tick's initialization.
    work = task->work;
    switch (task->state) {
        case MODEL_INIT:
            _policeOfficersInitBody(task, placementId);
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

/// The rookie's animated body.
static void _policeOfficersRookieBodyTask(Task* task)
{
    _policeOfficersUpdateBody(task, POLICE_OFFICERS_PLACEMENT_ROOKIE);
}

/// The sergeant's animated body.
static void _policeOfficersSergeantBodyTask(Task* task)
{
    _policeOfficersUpdateBody(task, POLICE_OFFICERS_PLACEMENT_SERGEANT);
}

/// Spawns an officer's culled body in its pose: binds the clip, composes part 1 and samples lighting once. The pose
/// never moves or relights afterwards.
static inline void _policeOfficersPoseCulledBody(Task* task, s32 placementId)
{
    _policeOfficersInitCulledBody(task, placementId);
    actorRenderComposeCoord(&task->extra.tmd->coords[1]);
    _policeOfficersLightModel(task);
}

/// The rookie's culled body: posed on its first frame, then its shadow while it holds the pose.
static void _policeOfficersRookieCulledBodyTask(Task* task)
{
    _OfficerWork* work;

    work = task->work;
    switch (task->state) {
        case MODEL_INIT:
            _policeOfficersPoseCulledBody(task, POLICE_OFFICERS_PLACEMENT_ROOKIE);
            task->state++;
            break;
        case MODEL_READY:
            if (work->playState == PLAY_STATE_POSED) {
                _policeOfficersDrawShadow(task);
            }
            break;
    }
}

/// The sergeant's culled body: posed on its first frame, then its shadow until it is frozen.
static void _policeOfficersSergeantCulledBodyTask(Task* task)
{
    _OfficerWork* work;

    work = task->work;
    switch (task->state) {
        case MODEL_INIT:
            _policeOfficersPoseCulledBody(task, POLICE_OFFICERS_PLACEMENT_SERGEANT);
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
