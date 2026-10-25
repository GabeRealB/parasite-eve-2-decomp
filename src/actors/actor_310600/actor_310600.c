#include "actors/actor_310600.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "stages/akropolis/rooms/cafeteria.h"
#include "../../shared/actor/actor_motion_walk_helpers.h"
#include "../../shared/actor/model_placement.h"
#include "../../shared/actor/actor_messages.h"

/// Draw policies of Rupert's body and its paired collision sphere.
enum {
    ACTOR_310600_DRAW_HIDE                  = 0,
    ACTOR_310600_DRAW_SHOW                  = 1,
    ACTOR_310600_DRAW_HIDE_AND_RELEASE      = 2,
    ACTOR_310600_BUFFER_RELEASE_NONE        = -1,
    ACTOR_310600_BUFFER_RELEASE_DELAY_TICKS = 2,
};

/// Body-bank selection and first walk step; retained walk clips are absent here.
enum {
    ACTOR_310600_BODY_ANIMATION_BANK   = 0,
    ACTOR_310600_WALK_STEP_FACE_TARGET = 0,
    ACTOR_310600_RETAINED_WALK_CLIP    = 12,
    ACTOR_310600_RETAINED_ARRIVAL_CLIP = 13,
};

/// Work block of Rupert Broderick's body, the package's scripted actor.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. It opens with the twenty-part rig and the bytes that
/// say what the rig is playing; the model object borrows `light` and `color`
/// for as long as the block lives.
///
/// A clip can carry a list of cue frames. `cueFrame` counts the clip's ticks
/// against that list, and clips 1 and 2 count the cues they have reached in
/// `shotCueCount`, which decides whether a cue is still one of the shots.
///
/// The actor has a collision sphere of its own, which the spawn state links
/// and the exit callback unlinks, so `body` and the table it borrows sit
/// between the matrices and the walk.
///
/// The walk is a scripted walker's: a walk request records the destination
/// and sets `walkMotion`, and the tick then runs the step `walkStep` selects
/// - turn to face `walkTarget`, set `walkVelocity` straight ahead, stop on
/// arrival. From `walkTarget` on the members are laid out as `ActorWalkState`
/// lays them out, but the block is not that type: it keeps the two selectors
/// ahead of the matrices and ends before the place that type gives them, and
/// nothing here accesses the eight bytes where that type keeps the
/// destination's rotation.
typedef struct {
    ActorAnimRig20        rig;              // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    s8                    ticking;          // Set once a clip has been applied, never cleared: the slots are ticked each frame
    s8                    animId;           // Clip the slots were last seeded with, within `bank` (`ACTOR_MOTION_ANIMATION_INDEX_NONE` before the first request); also selects the clip's cue list
    s8                    bank;             // Index, in the package's animation bank table, of the bank the rig is bound to (`ACTOR_MOTION_ANIMATION_INDEX_NONE` before the first request)
    s8                    freeCountdown;    // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    s16                   cueFrame;         // Ticks since the clip was applied, counted only while the clip has a cue list; a cue fires on the tick this equals its frame
    s16                   shotCueCount;     // Cues clip 1 or 2 has reached since it was applied: the first five each spawn a muzzle flash, a later one starts the room's follow-up task instead
    s16                   walkMotion;       // Handler the tick runs (0 `ACTOR_WALK_MOTION_IDLE`, 1 `ACTOR_WALK_MOTION_WALKING`)
    s16                   walkStep;         // Step of the walk in progress (0 turn to face the target, 1 set the velocity, 2 move until arrival)
    MATRIX                light;            // Light-direction matrix lent to the model object
    MATRIX                color;            // Light-colour matrix lent to the model object
    WorldCollisionBody    body;             // Sphere on part 1, linked for the task's life; its pair pass is enabled while the model is shown
    WorldCollisionContact contacts[1];      // One-entry table `body` borrows. The entry is marked LAST; an occupied contact is cleared each frame the model is drawn and never read
    VECTOR                walkTarget;       // Destination of the walk, a position the root coordinate's translation is to reach; the fourth word is never accessed
    VECTOR                walkVelocity;     // Displacement added each frame, in signed 16.16 units; zero while standing. The fourth word is never accessed
    Fixed16               walkCarry[3];     // X, Y and Z displacement not yet applied; only the fractions survive a frame
    byte                  pad_524[0x4];     // never accessed
    SVECTOR               walkLastDistance; // Absolute X and Z distance to `walkTarget` at the last arrival check, `ACTOR_MOTION_INITIAL_ARRIVAL_GAP` before the first; Y is only seeded
    byte                  pad_530[0x8];     // never accessed
} _Actor310600RupertBroderickWork;
STATIC_ASSERT_SIZEOF(_Actor310600RupertBroderickWork, 0x538);

static void _modelPlacementAttachPartTask(Task* childTask);
static void _modelPlacementMirrorParentDrawFlags(Task* childTask);
static void _actor310600InitBody(Task* task);
static void _actor310600UpdateBody(Task* task);
static void _actor310600CheckWalkArrival(Task* task);
static s32  _actor310600PlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32  _actor310600SetModelDraw(Task* task, s32 messageId, s32 mode, s32 unusedArg);
static void _actor310600IdleAttachedPart(Task* task);
static void _actor310600ExitBody(Task* task);
static void _actor310600BindLighting(Task* task);
static void _actor310600IdleMotion(Task* task);
static void _actor310600TickWalk(Task* task);
static void _actor310600FaceWalkTarget(Task* task);
static void _actor310600StartWalkVelocity(Task* task);

/// Mongoose task states: 0 attach and inherit draw policy, 1 mirror it, 2 kill.
///
/// The attached child borrows its parent's selected coordinate and lighting;
/// the parent's teardown tree keeps them alive through the child's use.
static const TaskFuncTable3 _gActor310600MongooseTaskStates = { {
    _modelPlacementAttachChild,
    _modelPlacementMirrorParentDrawFlags,
    taskKill,
} };

/// Retained part-attachment states: 0 attach, 1 idle, 2 kill.
///
/// Attachment retains the child's draw policy. No dispatcher selects this table.
static const TaskFuncTable3 _gActor310600RetainedPartTaskStates = { {
    _modelPlacementAttachPartTask,
    _actor310600IdleAttachedPart,
    taskKill,
} };

/// Rupert body task states: 0 initialize, 1 update, 2 unlink and tear down.
///
/// States 1 and 2 require the work and collision storage established by state 0.
static const TaskFuncTable3 _gActor310600BodyTaskStates = { {
    _actor310600InitBody,
    _actor310600UpdateBody,
    _actor310600ExitBody,
} };

/// Retained walk steps: 0 face the target, 1 seed velocity, 2 check arrival.
///
/// `walkStep` must be in 0..2. The loaded cafeteria scripts do not start a walk;
/// the retained walk request selects clips absent from this package's bank.
static const TaskFuncTable3 _gActor310600WalkStepHandlers = { {
    _actor310600FaceWalkTarget,
    _actor310600StartWalkVelocity,
    _actor310600CheckWalkArrival,
} };

/// One parent-coordinate unit in the walk's signed 16.16 velocity.
enum { ACTOR_310600_WALK_VELOCITY_ONE = 0x10000 };

/// Local +Z walk velocity: 32 parent-coordinate units per tick in signed 16.16.
///
/// Rotate through the root's rotation before integrating into its translation.
static const VECTOR _gActor310600WalkLocalVelocity = { 0, 0, 32 * ACTOR_310600_WALK_VELOCITY_ONE, 0 };

static TmdSource _gRupertBody;
static TmdSource _gRupertMongoose;
static void      _actor310600MongooseTask(Task* task);
static void      _actor310600BodyTask(Task* task);

static s32 _actor310600WalkTo(Task* task, s32 messageId, const ActorTransform* target, s32 unusedArg);

#include "assets/rooms/akropolis/cafeteria/rupert_clips_1.inc"

#include "assets/characters/rupert/body.inc"

#include "assets/characters/rupert/mongoose.inc"

#include "assets/rooms/akropolis/cafeteria/rupert_clips_2.inc"

/// Rupert's bank-0 clips: 1 shooting, 2 uncued, 3 reload, 4 uncued; 0 is NULL.
///
/// Playback borrows this table and the loaded set data. Only clips 1..4 are
/// usable; the retained walk's clips 12/13 are absent and must not be selected.
#include "assets/rooms/akropolis/cafeteria/rupert_bank.inc"

/// Rupert's sole body animation bank, selected by bank index 0.
///
/// Context binding borrows the selected clip table for the body's playback life.
static AnimationSet** _gActor310600AnimationBanks[1] = {
    _gActor310600BodyAnimationSets,
};

/// Extra initial pose ticks by clip id, in addition to the first playback tick.
///
/// Clip 1 advances three ticks on selection; the other entries advance once.
/// This eight-byte table covers loaded clips 1..4, not retained walk clips 12/13.
static s8 _gActor310600AnimationPreRollTicks[8] = {
    0,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
};

/// End marker for a clip's list of whole-tick effect cue times.
enum { ACTOR_310600_ANIMATION_CUE_END = 0 };

/// Shooting cues: five Mongoose shots, then the Stranger's scene handoff.
///
/// Times are ticks since clip selection, after its initial pose ticks. The
/// final zero halfword terminates the list rather than firing another cue.
static s16 _gActor310600ShootingCueFrames[7] = {
    74,
    95,
    117,
    137,
    159,
    223,
    ACTOR_310600_ANIMATION_CUE_END,
};

/// Reload casing-drop cue at tick 20 since clip selection, followed by the end marker.
static s16 _gActor310600ReloadCueFrames[2] = {
    20,
    ACTOR_310600_ANIMATION_CUE_END,
};

/// Clip-indexed cue lists: 1 shooting, 3 reload; NULL entries have no cue clock.
///
/// Lists end at `ACTOR_310600_ANIMATION_CUE_END`. Clip 2's retained shooting
/// branch has no list here. Loaded clips 1..4 fit this eleven-pointer table;
/// the retained walk's absent clips 12/13 must not index it.
static s16* _gActor310600AnimationCueFrames[11] = {
    NULL,
    _gActor310600ShootingCueFrames,
    NULL,
    _gActor310600ReloadCueFrames,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

/// Unreferenced vector storage; its role and coordinate units are unproven.
static SVECTOR D_actor_310600_8017968C = { -30, 450, 140, 0 };

/// Reload casing-drop position relative to the Mongoose's body part, in model units.
///
/// Effect placement reads XYZ and retains a borrowed pointer to this offset;
/// keep the actor package loaded through the spawned effect's lifetime.
static SVECTOR _gActor310600ReloadCasingsOffset = { -78, 80, 83, 0 };

/// Boss Stranger command ids in the cafeteria namespace, sent by Rupert's cues.
enum {
    ACTOR_310600_STRANGER_COMMAND_SHOT_RECOIL     = 6,
    ACTOR_310600_STRANGER_COMMAND_FOLLOWUP_RECOIL = 7,
};

/// Selects the Stranger's shot recoil and random hit effect on each Mongoose shot.
///
/// Borrowed through synchronous actor-command dispatch in the cafeteria context.
static ActorCommand _gActor310600StrangerShotCommand __attribute__((aligned(4))) = {
    { .loc = { GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_CAFETERIA } },
    ACTOR_310600_STRANGER_COMMAND_SHOT_RECOIL,
};

/// Selects the Stranger's follow-up recoil after Rupert's fifth shot.
///
/// Borrowed through synchronous actor-command dispatch before the room's settling task.
static ActorCommand _gActor310600StrangerFollowupCommand __attribute__((aligned(4))) = {
    { .loc = { GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_CAFETERIA } },
    ACTOR_310600_STRANGER_COMMAND_FOLLOWUP_RECOIL,
};

TaskDesc gActor310600RupertBroderickTasks[2] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _actor310600BodyTask, { .model = &_gRupertBody } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _actor310600MongooseTask, { .model = &_gRupertMongoose } },
};

/// Rupert body messages: animation request, Euler placement, draw mode and retained walk.
///
/// Animation borrows `AnimationPlayRequest`; place and walk borrow `ActorTransform`;
/// draw takes an integer mode. The second payload is ignored. Walk selects absent
/// clip 12 and is unsupported by the loaded bank. The final record ends lookup.
/// Keep this table and its callbacks loaded while the body can receive messages.
static TaskMessageEntry _gActor310600BodyMessageHandlers[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor310600PlayAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor310600SetModelDraw },
    { ACTOR_MESSAGE_WALK_TO, _actor310600WalkTo },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Initializes Rupert's hidden body, attached Mongoose and collision sphere.
///
/// Requires a live twenty-part TMD task with its owned Enemy in
/// `spawnArg2.pointer`. Allocates zeroed primary-heap work for the task's
/// lifetime; allocation failure starts enemy teardown. The model borrows the
/// work's lighting matrices, and the sphere borrows its single contact entry.
/// Installs the message table and exit callback, then advances state 0 to 1.
static void _actor310600InitBody(Task* task)
{
    enum {
        ACTOR_310600_MONGOOSE_TASK_INDEX  = 1,
        ACTOR_310600_MONGOOSE_PARENT_PART = 8,
        ACTOR_310600_BODY_SPHERE_PART     = 1,
    };

    _Actor310600RupertBroderickWork* work;
    WorldCollisionBody*              body;

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work              = work;
    work->animId            = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->bank              = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->freeCountdown     = ACTOR_310600_BUFFER_RELEASE_NONE;
    work->walkMotion        = ACTOR_WALK_MOTION_IDLE;
    work->walkStep          = ACTOR_310600_WALK_STEP_FACE_TARGET;
    work->walkCarry[0].word = 0;
    work->walkCarry[1].word = 0;
    work->walkCarry[2].word = 0;
    // The one-part gun follows body coordinate 8 and joins its teardown tree.
    taskSpawnFromTable(gActor310600RupertBroderickTasks, ACTOR_310600_MONGOOSE_TASK_INDEX, ACTOR_310600_MONGOOSE_PARENT_PART, task);
    _actor310600BindLighting(task);
    // Link the body sphere before the initial hidden draw policy disables pairs.
    body                   = &work->body;
    body->coord            = &task->extra.tmd->coords[ACTOR_310600_BODY_SPHERE_PART];
    body->context.contacts = work->contacts;
    body->key              = WORLD_COLLISION_CONTACT_ENEMY_BODY;
    body->radius           = 0x100;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(body->context.contacts, ARRAY_SIZE(work->contacts), 0);
    task->msgTable = _gActor310600BodyMessageHandlers;
    _actor310600SetModelDraw(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_310600_DRAW_HIDE, 0);
    task->exitCallback = _actor310600ExitBody;
    task->state++;
}

/// Applies signed 16.16 root-parent velocity, retaining unsigned XYZ fractions.
///
/// Requires initialized walk work and a live writable root. Applies signed high
/// halfwords in X/Y/Z order and retains each zero-extended low halfword for the
/// next tick. Velocity is already in the root parent's coordinate frame; this
/// step performs no rotation or collision correction. Dirties composition at rest.
static inline void _actor310600IntegrateWalkVelocity(_Actor310600RupertBroderickWork* work, GfxCoord* rootCoord)
{
    work->walkCarry[0].word += work->walkVelocity.vx;
    work->walkCarry[1].word += work->walkVelocity.vy;
    work->walkCarry[2].word += work->walkVelocity.vz;
    rootCoord->coord.t[0]   += work->walkCarry[0].halves.integer;
    rootCoord->coord.t[1]   += work->walkCarry[1].halves.integer;
    rootCoord->coord.t[2]   += work->walkCarry[2].halves.integer;
    rootCoord->composeStamp  = GRAPHICS_COORD_DIRTY;
    work->walkCarry[0].word  = work->walkCarry[0].halves.fraction;
    work->walkCarry[1].word  = work->walkCarry[1].halves.fraction;
    work->walkCarry[2].word  = work->walkCarry[2].halves.fraction;
}

/// Updates Rupert Broderick's walk, animation cues, shadow and model resources.
///
/// Requires initialized twenty-part body work, walkMotion 0/1 and loaded
/// clip/cue tables for the selected signed-byte animId. Motion uses signed
/// 16.16 root-parent units and retains unsigned fractions. Driven slots 1..19
/// advance even when hidden. Clip 1's first five cues fire the Mongoose and send
/// actor commands; its sixth starts the room follow-up. The retained clip-2
/// shooting branch has no cue list here. Clip 3 drops reload casings; its spawn
/// argument is unused by that effect. Cue frames and shot counts wrap as s16.
/// Visible ticks draw a ground shadow and clear contacts. A ready view refreshes
/// part 1 lighting. A nonnegative countdown frees buffers on the tick reading
/// zero and then settles at -1; work and collision storage remain live.
static void _actor310600UpdateBody(Task* task)
{
    enum {
        ACTOR_310600_SHOOTING_CLIP           = 1,
        ACTOR_310600_ALTERNATE_SHOOTING_CLIP = 2,
        ACTOR_310600_RELOAD_CLIP             = 3,
        ACTOR_310600_SHOT_CUE_COUNT          = 5,
        ACTOR_310600_MONGOOSE_PART           = 8,
        ACTOR_310600_MONGOOSE_FLASH_PROFILE  = 9,
        ACTOR_310600_ROOM_FOLLOWUP_TASK      = 2,
        ACTOR_310600_RETAINED_RELOAD_ARG     = 6,
        ACTOR_310600_SHADOW_HALF_SIZE        = 768,
        ACTOR_310600_LIGHT_COUNT             = 3,
    };
    TmdObject*                       model             = task->extra.tmd;
    _Actor310600RupertBroderickWork* work              = task->work;
    TaskFunc                         motionHandlers[2] = { _actor310600IdleMotion, _actor310600TickWalk };
    VECTOR3                          groundPoint;
    GfxCoord*                        activeCoord;
    s16*                             cueFrames;
    s16*                             cueCursor;
    s32                              slotIndex;

    motionHandlers[work->walkMotion](task);
    activeCoord = task->extra.tmd->coords;
    _actor310600IntegrateWalkVelocity(work, activeCoord);
    if (work->ticking != 0) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
    // Cue time advances only for clips with a non-NULL, zero-terminated frame list.
    if (work->animId > 0) {
        cueFrames = _gActor310600AnimationCueFrames[work->animId];
        if (cueFrames != NULL) {
            if (*cueFrames != ACTOR_310600_ANIMATION_CUE_END) {
                cueCursor = cueFrames;
                do {
                    if (*cueCursor == work->cueFrame) {
                        activeCoord = &task->extra.tmd->coords[ACTOR_310600_MONGOOSE_PART];
                        switch (work->animId) {
                            case ACTOR_310600_SHOOTING_CLIP:
                            case ACTOR_310600_ALTERNATE_SHOOTING_CLIP:
                                if (work->shotCueCount++ < ACTOR_310600_SHOT_CUE_COUNT) {
                                    effectSpawn(EFFECT_ACTOR_MUZZLE_FLASH, activeCoord, ACTOR_310600_MONGOOSE_FLASH_PROFILE, NULL);
                                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_COMMAND_MESSAGE_APPLY, &_gActor310600StrangerShotCommand, 0);
                                } else {
                                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_COMMAND_MESSAGE_APPLY, &_gActor310600StrangerFollowupCommand, 0);
                                    taskSpawnFromTable(gAkropolisCafeteriaSceneTasks, ACTOR_310600_ROOM_FOLLOWUP_TASK, 0, NULL);
                                }
                                break;
                            case ACTOR_310600_RELOAD_CLIP:
                                effectSpawn(EFFECT_RELOAD_CASINGS_DROP, activeCoord, ACTOR_310600_RETAINED_RELOAD_ARG, &_gActor310600ReloadCasingsOffset);
                                break;
                        }
                        break;
                    }
                    cueCursor++;
                } while (*cueCursor != ACTOR_310600_ANIMATION_CUE_END);
            }
            work->cueFrame++;
        }
    }
    // Visibility gates the ground shadow and contact clearing, not motion or playback.
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords[1].workm), &groundPoint) != 0) {
            effectDrawGroundShadow(&groundPoint, ACTOR_310600_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
        worldCollisionClearContacts(work->contacts);
    }
    // A ready view refreshes part 1 and its room lighting before delayed buffer release.
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        worldCoordSetModelLighting(model, task->extra.tmd->coords[1].workm.t, 0, ACTOR_310600_LIGHT_COUNT);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

/// Stops the retained walk once neither horizontal axis gets closer.
///
/// Requires live body work and coordinate 0. Distances use root-parent units,
/// with the low 16 bits interpreted as signed before the stop comparison;
/// the previous thresholds store their absolute values. Stops velocity and
/// resets both walk selectors without snapping position or clearing fractions.
/// Clip 12 additionally requests clip 13 with a ten-frame blend. These clips
/// are absent here; cafeteria scripts never start this retained walk path.
static void _actor310600CheckWalkArrival(Task* task)
{
    _Actor310600RupertBroderickWork* work;
    GfxCoord*                        rootCoord;
    SVECTOR                          distance;
    s32                              deltaX;
    s32                              deltaZ;
    AnimationPlayRequest             arrivalRequest;

    enum { ACTOR_310600_ARRIVAL_BLEND_FRAMES = 10 };

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    // Compare full words for direction, then retain only a signed halfword gap.
    deltaX      = _actorMotionWalkAxisGap(&work->walkTarget.vx, &rootCoord->coord.t[0]);
    distance.vx = deltaX;
    deltaZ      = _actorMotionWalkAxisGap(&work->walkTarget.vz, &rootCoord->coord.t[2]);
    distance.vz = deltaZ;
    if (distance.vx >= work->walkLastDistance.vx && distance.vz >= work->walkLastDistance.vz) {
        if (work->animId == ACTOR_310600_RETAINED_WALK_CLIP) {
            arrivalRequest.source.index         = ACTOR_310600_BODY_ANIMATION_BANK;
            arrivalRequest.animationId          = ACTOR_310600_RETAINED_ARRIVAL_CLIP;
            arrivalRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
            arrivalRequest.blendFrames          = ACTOR_310600_ARRIVAL_BLEND_FRAMES;
            arrivalRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            _actor310600PlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &arrivalRequest, 0);
        }
        work->walkVelocity.vx = 0;
        work->walkVelocity.vy = 0;
        work->walkVelocity.vz = 0;
        work->walkMotion      = ACTOR_WALK_MOTION_IDLE;
        work->walkStep        = ACTOR_310600_WALK_STEP_FACE_TARGET;
        return;
    }
    work->walkLastDistance.vx = distance.vx < 0 ? -distance.vx : distance.vx;
    work->walkLastDistance.vz = distance.vz < 0 ? -distance.vz : distance.vz;
}

/// Selects and starts an animation on Rupert's body, preserving an unchanged clip.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION` for initialized body work and its
/// twenty-part model. Borrows the request through dispatch and retains only
/// its bank and clip selectors. Bank 0 clips 1..4 are the loaded choices;
/// indices are unchecked and stored as signed bytes. Slots 1..19 reset when
/// blend is zero, otherwise blend from their current poses for `blendFrames`
/// normal-rate frames (0..2047). Applies one initial tick plus the clip's
/// pre-roll, then clears cue counters and enables per-frame playback.
/// Ignores message ID, second payload and requested world collision. Returns 0.
static s32 _actor310600PlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    _Actor310600RupertBroderickWork* work;
    TmdObject*                       model;
    s32                              slotIndex;
    s32                              preRollTick;

    work  = task->work;
    model = task->extra.tmd;
    // Changing banks forces the selected clip to be seeded again.
    if (request->source.index != work->bank) {
        work->bank   = request->source.index;
        work->animId = ACTOR_MOTION_ANIMATION_INDEX_NONE;
        animationInitContext(&work->rig.anim, _gActor310600AnimationBanks[work->bank], model, work->rig.poses,
                             work->rig.slots);
    }
    if (request->animationId != work->animId) {
        work->animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animId, 0, request->blendFrames);
            }
        } else {
            for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationResetSlot(&work->rig.anim, slotIndex, work->animId);
            }
        }
        // Include the first pose tick plus the clip-specific pre-roll.
        for (preRollTick = 0; preRollTick <= _gActor310600AnimationPreRollTicks[work->animId]; preRollTick++) {
            for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationTickSlot(&work->rig.anim, slotIndex);
            }
        }
        work->ticking      = 1;
        work->cueFrame     = 0;
        work->shotCueCount = 0;
    }
    return 0;
}

/// Sets body drawing, sphere-pair participation and primitive-buffer policy.
///
/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` with initialized body work.
/// Modes 0/2 hide the model and disable sphere pairs; 1/3 show and enable them.
/// Modes 0/1 permit automatic buffers, with 1 requesting a missing buffer now.
/// Mode 2 suppresses automatic buffers and seeds a two-tick release countdown;
/// buffers are freed on the tick that reads zero. Mode 3 suppresses automatic
/// buffers without allocating. Existing release
/// countdowns are retained by the other modes. Ignores message ID and second
/// payload. Returns 0 for modes 0..3, or 1 without changes for another mode.
static s32 _actor310600SetModelDraw(Task* task, s32 messageId, s32 mode, s32 unusedArg)
{
    _Actor310600RupertBroderickWork* work;
    _Actor310600RupertBroderickWork* drawWork;
    TmdObject*                       model;
    WorldCollisionBody*              bodyCursor;
    WorldCollisionBody*              body;
    s32                              bodyIndex;
    s32                              result;

    enum { ACTOR_310600_COLLISION_BODY_COUNT = 1 };

    work     = task->work;
    model    = task->extra.tmd;
    drawWork = work;
    body     = &work->body;
    result   = 0;
    switch (mode) {
        case ACTOR_310600_DRAW_HIDE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyCursor    = &drawWork->body;
            for (bodyIndex = 0; bodyIndex < ACTOR_310600_COLLISION_BODY_COUNT; bodyIndex++) {
                bodyCursor->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                bodyCursor++;
            }
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_310600_DRAW_SHOW:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyCursor    = &drawWork->body;
            for (bodyIndex = 0; bodyIndex < ACTOR_310600_COLLISION_BODY_COUNT; bodyIndex++) {
                bodyCursor->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                bodyCursor++;
            }
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_310600_DRAW_HIDE_AND_RELEASE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyCursor    = &drawWork->body;
            for (bodyIndex = 0; bodyIndex < ACTOR_310600_COLLISION_BODY_COUNT; bodyIndex++) {
                bodyCursor->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                bodyCursor++;
            }
            drawWork->freeCountdown = ACTOR_310600_BUFFER_RELEASE_DELAY_TICKS;
            model->flags           |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW_SKIP_AUTO_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyCursor    = body;
            for (bodyIndex = 0; bodyIndex < ACTOR_310600_COLLISION_BODY_COUNT; bodyIndex++) {
                bodyCursor->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                bodyCursor++;
            }
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Dispatches the attached Mongoose's setup, draw-policy tick or teardown.
///
/// Requires a live TMD task and state 0..2. Setup borrows the body task from
/// `spawnArg2.pointer` and attaches to coordinate index `spawnArg1.value`;
/// the parent, coordinate and lighting must outlive the child. No result or
/// bounds check; the selected state callback may destroy the task.
static void _actor310600MongooseTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gActor310600MongooseTaskStates;
    states.funcs[task->state](task);
}

#include "../../shared/actor/model_placement_attach.inc.c"

#include "../../shared/actor/model_placement_mirror_parent.inc.c"

#include "../../shared/actor/model_placement_attach_part.inc.c"

/// Leaves an attached part unchanged during its idle state.
///
/// Retained in the unused part-attachment state table; the task is not read.
static void _actor310600IdleAttachedPart(Task* task)
{
}

/// Dispatches Rupert's body initialization, per-frame update or teardown.
///
/// Requires a live twenty-part TMD task, its owned Enemy in `spawnArg2.pointer`
/// and state 0..2. State 1/2 require successfully initialized work. No bounds
/// check; the selected callback may destroy the task.
static void _actor310600BodyTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gActor310600BodyTaskStates;
    states.funcs[task->state](task);
}

/// Unlinks Rupert's sphere before releasing the enemy and body task.
///
/// Requires successfully initialized work whose body remains linked. Task
/// teardown owns child, work and model release; borrowed lighting and contact
/// storage cease to be valid when that work is released.
static void _actor310600ExitBody(Task* task)
{
    _Actor310600RupertBroderickWork* work = task->work;

    worldCollisionUnlinkBody(&work->body);
    enemyTaskExit(task);
}

/// Lends the body's work-owned lighting matrices to its model.
///
/// Requires live body work and a TMD model; work must outlive all model and
/// attached-child uses of these pointers. Does not initialize the matrices.
static void _actor310600BindLighting(Task* task)
{
    TmdObject*                       model;
    _Actor310600RupertBroderickWork* work;

    work            = task->work;
    model           = task->extra.tmd;
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
}

/// Leaves the body stationary when the motion selector is idle.
///
/// The task is not read; normal animation and rendering continue in the tick.
static void _actor310600IdleMotion(Task* task)
{
}

/// Dispatches the retained walk's facing, velocity or arrival step.
///
/// Requires initialized body work with `walkStep` in 0..2 and live model
/// coordinate 0. Does not itself integrate movement or check the selector.
/// Cafeteria scripts never select this motion path.
static void _actor310600TickWalk(Task* task)
{
    _Actor310600RupertBroderickWork* work;
    TaskFuncTable3                   steps;

    work  = task->work;
    steps = _gActor310600WalkStepHandlers;
    steps.funcs[work->walkStep](task);
}

/// Replaces root Euler rotation and invalidates its composed transform.
///
/// The writable root and borrowed angle vector must be live through the call.
/// Reads XYZ only, in 4096 units per turn. Leaves the root Euler vector's
/// fourth halfword, translation and parent unchanged. Uses Rx * Ry * Rz.
static inline void _actor310600SetRootFacing(GfxCoord* rootCoord, const SVECTOR* angles)
{
    rootCoord->param.rot.vx = angles->vx;
    rootCoord->param.rot.vy = angles->vy;
    rootCoord->param.rot.vz = angles->vz;
    RotMatrix(&rootCoord->param.rot, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Faces the body's root toward the retained walk target and advances the step.
///
/// Requires initialized body work and coordinate 0. The target and root
/// translation use the root parent's frame. Replaces root Euler rotation
/// with yaw in 4096 units per turn, zeroing pitch and roll, and invalidates
/// composition. Cafeteria scripts never select this retained walk path.
static void _actor310600FaceWalkTarget(Task* task)
{
    _Actor310600RupertBroderickWork* work;
    GfxCoord*                        rootCoord;
    VECTOR                           targetOffset;
    SVECTOR                          direction;
    SVECTOR                          facingAngles;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;

    targetOffset.vx = work->walkTarget.vx - rootCoord->coord.t[0];
    targetOffset.vy = work->walkTarget.vy - rootCoord->coord.t[1];
    targetOffset.vz = work->walkTarget.vz - rootCoord->coord.t[2];
    VectorNormalS(&targetOffset, &direction);

    facingAngles.vx = 0;
    facingAngles.vy = ratan2(direction.vx, direction.vz);
    facingAngles.vz = 0;

    _actor310600SetRootFacing(rootCoord, &facingAngles);
    work->walkStep++;
}

/// Starts the retained walk's forward velocity and arms its arrival check.
///
/// Requires initialized body work and the root rotation set by the facing
/// step. Rotates a local +Z speed of 32 world units per tick into signed
/// 16.16 `walkVelocity`, seeds distance thresholds with
/// `ACTOR_MOTION_INITIAL_ARRIVAL_GAP` and advances to step 2. No position is changed
/// here. Cafeteria scripts never select this retained walk path.
static void _actor310600StartWalkVelocity(Task* task)
{
    _Actor310600RupertBroderickWork* work;
    GfxCoord*                        rootCoord;
    VECTOR                           localVelocity;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    localVelocity = _gActor310600WalkLocalVelocity;
    ApplyMatrixLV(&rootCoord->coord, &localVelocity, &work->walkVelocity);
    work->walkLastDistance.vx = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walkLastDistance.vy = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walkLastDistance.vz = ACTOR_MOTION_INITIAL_ARRIVAL_GAP;
    work->walkStep++;
}

#include "../../shared/actor/actor_messages_place_euler.inc.c"

/// Records a destination and starts the retained scripted walk.
///
/// Handles `ACTOR_MESSAGE_WALK_TO` for initialized body work and coordinate 0.
/// Borrows a word-aligned transform through dispatch, copying only its XYZ
/// position in the root parent's frame; rotation and the vector's fourth word
/// are ignored. Sets walking motion without resetting a walk already in
/// progress, then requests bank 0 clip 12. That clip is absent here, and no
/// cafeteria script or companion actor sends this message. The message ID and
/// second payload are ignored. Returns the animation handler's result (0).
static s32 _actor310600WalkTo(Task* task, s32 messageId, const ActorTransform* target, s32 unusedArg)
{
    _Actor310600RupertBroderickWork* work;
    AnimationPlayRequest             walkRequest;

    work = task->work;

    work->walkMotion    = ACTOR_WALK_MOTION_WALKING;
    work->walkTarget.vx = target->pos.vx;
    work->walkTarget.vy = target->pos.vy;
    work->walkTarget.vz = target->pos.vz;

    walkRequest.source.index         = ACTOR_310600_BODY_ANIMATION_BANK;
    walkRequest.animationId          = ACTOR_310600_RETAINED_WALK_CLIP;
    walkRequest.blend                = ANIMATION_BLEND_RESET;
    walkRequest.blendFrames          = 0;
    walkRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;

    return _actor310600PlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &walkRequest, 0);
}
