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

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "rooms/acropolis_cafeteria.h"
#include "../../shared/model_placement.h"
#include "../../shared/actor_messages.h"

/// Draw policies of Rupert's body and its paired collision sphere.
enum {
    ACTOR_310600_DRAW_HIDE                  = 0,
    ACTOR_310600_DRAW_SHOW                  = 1,
    ACTOR_310600_DRAW_HIDE_AND_RELEASE      = 2,
    ACTOR_310600_DRAW_SHOW_SKIP_AUTO_BUFFER = 3,
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
    s8                    animId;           // Clip the slots were last seeded with, within `bank` (`ACTOR_MODEL_STATE_NONE` before the first request); also selects the clip's cue list
    s8                    bank;             // Index, in the package's animation bank table, of the bank the rig is bound to (`ACTOR_MODEL_STATE_NONE` before the first request)
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
    SVECTOR               walkLastDistance; // Absolute X and Z distance to `walkTarget` at the last arrival check, `ACTOR_WALK_DISTANCE_NONE` before the first; Y is only seeded
    byte                  pad_530[0x8];     // never accessed
} _Actor310600RupertBroderickWork;
STATIC_ASSERT_SIZEOF(_Actor310600RupertBroderickWork, 0x538);

/// Spawn table entry 1 is this actor's `Task::state` dispatcher; the type-1
/// setup entry it is spawned from is `_actor310600InitBody`.
extern TaskDesc D_actor_310600_801796A4[];

/// The overlay's `TaskMessageEntry` table, parked in `Task::msgTable`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_310600_801796BC[];

/// Per-animation cue lists: the entry `_Actor310600RupertBroderickWork::animId`
/// selects is a zero-terminated list of the frames at which that animation
/// fires its effect.
extern s16*    D_actor_310600_80179660[];
extern SVECTOR D_actor_310600_80179694;
extern s32     D_actor_310600_8017969C;
extern s32     D_actor_310600_801796A0;

extern AnimationSet*  D_actor_310600_8017962C[5];
extern AnimationSet** D_actor_310600_80179640[1]; // animation bank table `_Actor310600RupertBroderickWork::bank` indexes
extern s8             D_actor_310600_80179644[];  // extra ticks owed to the animation id in `_Actor310600RupertBroderickWork::animId`

/// Spawn table of the follow-up task queued once the cue has fired five times.

static void _modelPlacementAttachPartTask(Task* childTask);
static void _modelPlacementMirrorParentDrawFlags(Task* childTask);
static void _actor310600InitBody(Task* task);
static void func_actor_310600_80161FA0(Task* task);
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

/// State handlers of the child part task, which `_actor310600MongooseTask`
/// runs by `Task::state`: setup, tick and exit.
static const TaskFuncTable3 D_actor_310600_80161E24 = { {
    _modelPlacementAttachChild,
    _modelPlacementMirrorParentDrawFlags,
    taskKill,
} };

/// A second child handler triple - attach to the parent's part, an empty tick,
/// exit. No dispatcher in this package reads it.
static const TaskFuncTable3 D_actor_310600_80161E30 = { {
    _modelPlacementAttachPartTask,
    _actor310600IdleAttachedPart,
    taskKill,
} };

/// The actor's own state handlers, which `_actor310600BodyTask` runs by
/// `Task::state`: spawn/setup, per-frame tick and teardown.
static const TaskFuncTable3 D_actor_310600_80161E3C = { {
    _actor310600InitBody,
    func_actor_310600_80161FA0,
    _actor310600ExitBody,
} };

/// The actor's movement steps, which `_actor310600TickWalk` runs by
/// `_Actor310600RupertBroderickWork::walkStep`: turn to face the target point,
/// start moving, stop on arrival.
static const TaskFuncTable3 D_actor_310600_80161E48 = { {
    _actor310600FaceWalkTarget,
    _actor310600StartWalkVelocity,
    _actor310600CheckWalkArrival,
} };

/// The constant local-space offset `_actor310600StartWalkVelocity` rotates,
/// `{ 0, 0, 0x200000, 0 }` -- straight ahead along the part's own +Z.
static const VECTOR D_actor_310600_80161E54 = { 0, 0, 0x200000, 0 };

static TmdSource _gActor310600RupertBroderickBody1;
static TmdSource _gActor310600RupertBroderickMongoose;
static void      _actor310600MongooseTask(Task* task);
static void      _actor310600BodyTask(Task* task);

static s32 _actor310600WalkTo(Task* task, s32 messageId, const ActorTransform* target, s32 unusedArg);

static AnimationPackedPose _gActor310600Animation04ADCBank1[102] = {
#include "assets/actor_310600_animation_04ADC_bank1.inc"
};

static AnimationPackedRotation _gActor310600Animation04ADCBank4[1555] = {
#include "assets/actor_310600_animation_04ADC_bank4.inc"
};

static AnimationRecord _gActor310600Animation04ADCRecords[1970] = {
#include "assets/actor_310600_animation_04ADC_records.inc"
};

static u16 _gActor310600Animation04ADCIndices[20] = {
#include "assets/actor_310600_animation_04ADC_indices.inc"
};

AnimationSet gActor310600Animation04ADC = {
    _gActor310600Animation04ADCRecords,
    _gActor310600Animation04ADCIndices,
    { NULL, _gActor310600Animation04ADCBank1, NULL, NULL, _gActor310600Animation04ADCBank4, NULL, NULL, NULL },
};

static TmdBone _gActor310600RupertBroderickBody1Skeleton[20] = {
#include "assets/rupert_broderick_body_1_skeleton.inc"
};

static u32 _gActor310600RupertBroderickBody1PartVerts[20] = {
#include "assets/rupert_broderick_body_1_partVerts.inc"
};

static SVECTOR _gActor310600RupertBroderickBody1Verts[386] = {
#include "assets/rupert_broderick_body_1_verts.inc"
};

static SVECTOR _gActor310600RupertBroderickBody1Normals[385] = {
#include "assets/rupert_broderick_body_1_normals.inc"
};

static u32 _gActor310600RupertBroderickBody1Stream[4327] = {
#include "assets/rupert_broderick_body_1_stream.inc"
};

static TmdSource _gActor310600RupertBroderickBody1 = {
    0,
    23980,
    6012,
    20,
    _gActor310600RupertBroderickBody1PartVerts,
    _gActor310600RupertBroderickBody1Verts,
    _gActor310600RupertBroderickBody1Normals,
    _gActor310600RupertBroderickBody1Skeleton,
    _gActor310600RupertBroderickBody1Stream,
};

static TmdBone _gActor310600RupertBroderickMongooseSkeleton[1] = {
#include "assets/rupert_broderick_mongoose_skeleton.inc"
};

static u32 _gActor310600RupertBroderickMongoosePartVerts[1] = {
#include "assets/rupert_broderick_mongoose_partVerts.inc"
};

static SVECTOR _gActor310600RupertBroderickMongooseVerts[28] = {
#include "assets/rupert_broderick_mongoose_verts.inc"
};

static SVECTOR _gActor310600RupertBroderickMongooseNormals[28] = {
#include "assets/rupert_broderick_mongoose_normals.inc"
};

static u32 _gActor310600RupertBroderickMongooseStream[211] = {
#include "assets/rupert_broderick_mongoose_stream.inc"
};

static TmdSource _gActor310600RupertBroderickMongoose = {
    0,
    1464,
    0,
    1,
    _gActor310600RupertBroderickMongoosePartVerts,
    _gActor310600RupertBroderickMongooseVerts,
    _gActor310600RupertBroderickMongooseNormals,
    _gActor310600RupertBroderickMongooseSkeleton,
    _gActor310600RupertBroderickMongooseStream,
};

static AnimationPackedPose _gActor310600Animation0E6B0Bank1[73] = {
#include "assets/actor_310600_animation_0E6B0_bank1.inc"
};

static AnimationPackedRotation _gActor310600Animation0E6B0Bank4[1085] = {
#include "assets/actor_310600_animation_0E6B0_bank4.inc"
};

static AnimationRecord _gActor310600Animation0E6B0Records[2229] = {
#include "assets/actor_310600_animation_0E6B0_records.inc"
};

static u16 _gActor310600Animation0E6B0Indices[20] = {
#include "assets/actor_310600_animation_0E6B0_indices.inc"
};

static AnimationSet _gActor310600Animation0E6B0 = {
    _gActor310600Animation0E6B0Records,
    _gActor310600Animation0E6B0Indices,
    { NULL, _gActor310600Animation0E6B0Bank1, NULL, NULL, _gActor310600Animation0E6B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor310600Animation0EB84Bank1[8] = {
#include "assets/actor_310600_animation_0EB84_bank1.inc"
};

static AnimationPackedRotation _gActor310600Animation0EB84Bank4[115] = {
#include "assets/actor_310600_animation_0EB84_bank4.inc"
};

static AnimationRecord _gActor310600Animation0EB84Records[150] = {
#include "assets/actor_310600_animation_0EB84_records.inc"
};

static u16 _gActor310600Animation0EB84Indices[20] = {
#include "assets/actor_310600_animation_0EB84_indices.inc"
};

static AnimationSet _gActor310600Animation0EB84 = {
    _gActor310600Animation0EB84Records,
    _gActor310600Animation0EB84Indices,
    { NULL, _gActor310600Animation0EB84Bank1, NULL, NULL, _gActor310600Animation0EB84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor310600Animation15668Bank1[127] = {
#include "assets/actor_310600_animation_15668_bank1.inc"
};

static AnimationPackedRotation _gActor310600Animation15668Bank4[2329] = {
#include "assets/actor_310600_animation_15668_bank4.inc"
};

static AnimationRecord _gActor310600Animation15668Records[4111] = {
#include "assets/actor_310600_animation_15668_records.inc"
};

static u16 _gActor310600Animation15668Indices[20] = {
#include "assets/actor_310600_animation_15668_indices.inc"
};

static AnimationSet _gActor310600Animation15668 = {
    _gActor310600Animation15668Records,
    _gActor310600Animation15668Indices,
    { NULL, _gActor310600Animation15668Bank1, NULL, NULL, _gActor310600Animation15668Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor310600Animation177E4Bank1[68] = {
#include "assets/actor_310600_animation_177E4_bank1.inc"
};

static AnimationPackedRotation _gActor310600Animation177E4Bank4[781] = {
#include "assets/actor_310600_animation_177E4_bank4.inc"
};

static AnimationRecord _gActor310600Animation177E4Records[1138] = {
#include "assets/actor_310600_animation_177E4_records.inc"
};

static u16 _gActor310600Animation177E4Indices[20] = {
#include "assets/actor_310600_animation_177E4_indices.inc"
};

static AnimationSet _gActor310600Animation177E4 = {
    _gActor310600Animation177E4Records,
    _gActor310600Animation177E4Indices,
    { NULL, _gActor310600Animation177E4Bank1, NULL, NULL, _gActor310600Animation177E4Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_310600_8017962C[5] = {
    NULL,
    &_gActor310600Animation0E6B0,
    &_gActor310600Animation0EB84,
    &_gActor310600Animation15668,
    &_gActor310600Animation177E4,
};

AnimationSet** D_actor_310600_80179640[1] = {
    D_actor_310600_8017962C,
};

// Cafeteria's scripts select bank 0 animations 1..4. The retained movement
// handler requests 12/13, but has no caller in those scripts or companion actors.
s8 D_actor_310600_80179644[8] = {
    0,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
};

s16 D_actor_310600_8017964C[7] = {
    74,
    95,
    117,
    137,
    159,
    223,
    0,
};

s16 D_actor_310600_8017965C[2] = {
    20,
    0,
};

s16* D_actor_310600_80179660[11] = {
    NULL,
    D_actor_310600_8017964C,
    NULL,
    D_actor_310600_8017965C,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

SVECTOR D_actor_310600_8017968C = { -30, 450, 140, 0 };

SVECTOR D_actor_310600_80179694 = { -78, 80, 83, 0 };

s32 D_actor_310600_8017969C = 0x60401;

s32 D_actor_310600_801796A0 = 0x70401;

TaskDesc D_actor_310600_801796A4[2] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor310600BodyTask, { .model = &_gActor310600RupertBroderickBody1 } },
    { { { TASK_BODY_TMD, 192 } }, _actor310600MongooseTask, { .model = &_gActor310600RupertBroderickMongoose } },
};

TaskMessageEntry D_actor_310600_801796BC[5] = {
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
    work->animId            = ACTOR_MODEL_STATE_NONE;
    work->bank              = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown     = ACTOR_310600_BUFFER_RELEASE_NONE;
    work->walkMotion        = ACTOR_WALK_MOTION_IDLE;
    work->walkStep          = ACTOR_310600_WALK_STEP_FACE_TARGET;
    work->walkCarry[0].word = 0;
    work->walkCarry[1].word = 0;
    work->walkCarry[2].word = 0;
    // The one-part gun follows body coordinate 8 and joins its teardown tree.
    taskSpawnFromTable(D_actor_310600_801796A4, ACTOR_310600_MONGOOSE_TASK_INDEX, ACTOR_310600_MONGOOSE_PARENT_PART, task);
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
    task->msgTable = D_actor_310600_801796BC;
    _actor310600SetModelDraw(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_310600_DRAW_HIDE, 0);
    task->exitCallback = _actor310600ExitBody;
    task->state++;
}

/// The actor's per-frame handler. Runs the entry of its second state table that
/// `walkMotion` selects, then advances the root part by `walkVelocity`: each
/// axis of `walkCarry` carries a 16.16 offset whose whole part is added to the world
/// translation and whose fraction is kept, and clearing `composeStamp` makes
/// `actorRenderComposeCoordChain` rebuild the composed matrix from it.
///
/// Once the slots have been started (`ticking`) every animation slot is
/// ticked, and the frame counter `cueFrame` is walked against the cue list
/// `D_actor_310600_80179660[animId]` -- a zero-terminated list of frames at
/// which the animation currently playing fires an effect. The effect is chosen
/// by the animation id: ids 1 and 2 spawn 0x6006A and ask slot 4 for the
/// follow-up message, but only for the first five of them (`shotCueCount`), after which the
/// other payload is sent and `D_acropolis_cafeteria_80182AD8` is spawned instead; id 3 spawns
/// 0x6006D. The remaining ids have no cue.
///
/// While the model is visible its ground shadow is drawn at the root part's
/// world position and the occupancy table is cleared, and while the session
/// flag at `field_4D` is set the second part is re-derived and re-lit.
/// `freeCountdown` is the teardown countdown: it frees the model buffers on the
/// tick it reaches zero and then stops at -1.
static void func_actor_310600_80161FA0(Task* task)
{
    TmdObject*                       ext      = task->extra.tmd;
    _Actor310600RupertBroderickWork* work     = (_Actor310600RupertBroderickWork*)task->work;
    TaskFunc                         funcs[2] = { _actor310600IdleMotion, _actor310600TickWalk };
    VECTOR3                          pos;
    GfxCoord*                        coord;
    s16*                             cues;
    s16*                             cue;
    s32                              i;

    funcs[work->walkMotion](task);
    coord                    = task->extra.tmd->coords;
    work->walkCarry[0].word += work->walkVelocity.vx;
    work->walkCarry[1].word += work->walkVelocity.vy;
    work->walkCarry[2].word += work->walkVelocity.vz;
    coord->coord.t[0]       += work->walkCarry[0].halves.integer;
    coord->coord.t[1]       += work->walkCarry[1].halves.integer;
    coord->coord.t[2]       += work->walkCarry[2].halves.integer;
    coord->composeStamp      = GRAPHICS_COORD_DIRTY;
    work->walkCarry[0].word  = work->walkCarry[0].halves.fraction;
    work->walkCarry[1].word  = work->walkCarry[1].halves.fraction;
    work->walkCarry[2].word  = work->walkCarry[2].halves.fraction;
    if (work->ticking != 0) {
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
    if (work->animId > 0) {
        cues = D_actor_310600_80179660[work->animId];
        if (cues != NULL) {
            if (*cues != 0) {
                cue = cues;
                do {
                    if (*cue == work->cueFrame) {
                        coord = &task->extra.tmd->coords[8];
                        switch (work->animId) {
                            case 1:
                            case 2:
                                if (work->shotCueCount++ < 5) {
                                    effectSpawn(EFFECT_ACTOR_MUZZLE_FLASH, coord, 9, NULL);
                                    TASK_MESSAGE_DISPATCH_POINTER(Gp_LookupSlot4(0), ACTOR_COMMAND_MESSAGE_APPLY, &D_actor_310600_8017969C, 0);
                                } else {
                                    TASK_MESSAGE_DISPATCH_POINTER(Gp_LookupSlot4(0), ACTOR_COMMAND_MESSAGE_APPLY, &D_actor_310600_801796A0, 0);
                                    taskSpawnFromTable(D_acropolis_cafeteria_80182AD8, 2, 0, 0);
                                }
                                break;
                            case 3:
                                effectSpawn(EFFECT_RELOAD_CASINGS_DROP, coord, 6, &D_actor_310600_80179694);
                                break;
                        }
                        break;
                    }
                    cue++;
                } while (*cue != 0);
            }
            work->cueFrame++;
        }
    }
    if (!(ext->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
            effectDrawGroundShadow(&pos, 0x300, gRoomEffectState->groundShadowShade);
        }
        worldCollisionClearContacts(work->contacts);
    }
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        worldCoordSetModelLighting(ext, task->extra.tmd->coords[1].workm.t, 0, 3);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(ext);
        }
        work->freeCountdown--;
    }
}

/// Measures an axis gap using the direction of the full-word difference.
///
/// Borrows two word-aligned coordinate components through the call. The signed
/// difference must fit a word; its direction chooses which unsigned low
/// halfwords to subtract. Returns that subtraction before signed-halfword
/// truncation, preserving wrap at 65536 rather than taking a full-word absolute
/// value. The caller narrows the result for the arrival comparison.
static inline s32 _actor310600WalkAxisGap(const long* targetAxis, const long* rootAxis)
{
    s32 gap;

    if (*targetAxis - *rootAxis >= 0) {
        gap = (u16)*targetAxis - (u16)*rootAxis;
    } else {
        gap = (u16)*rootAxis - (u16)*targetAxis;
    }
    return gap;
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
    deltaX      = _actor310600WalkAxisGap(&work->walkTarget.vx, &rootCoord->coord.t[0]);
    distance.vx = deltaX;
    deltaZ      = _actor310600WalkAxisGap(&work->walkTarget.vz, &rootCoord->coord.t[2]);
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
        work->animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, D_actor_310600_80179640[work->bank], model, work->rig.poses,
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
        for (preRollTick = 0; preRollTick <= D_actor_310600_80179644[work->animId]; preRollTick++) {
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
        case ACTOR_310600_DRAW_SHOW_SKIP_AUTO_BUFFER:
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

    states = D_actor_310600_80161E24;
    states.funcs[task->state](task);
}

#include "../../shared/model_placement_attach.inc.c"

#include "../../shared/model_placement_mirror_parent.inc.c"

#include "../../shared/model_placement_attach_part.inc.c"

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

    states = D_actor_310600_80161E3C;
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
    steps = D_actor_310600_80161E48;
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
/// `ACTOR_WALK_DISTANCE_NONE` and advances to step 2. No position is changed
/// here. Cafeteria scripts never select this retained walk path.
static void _actor310600StartWalkVelocity(Task* task)
{
    _Actor310600RupertBroderickWork* work;
    GfxCoord*                        rootCoord;
    VECTOR                           localVelocity;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    localVelocity = D_actor_310600_80161E54;
    ApplyMatrixLV(&rootCoord->coord, &localVelocity, &work->walkVelocity);
    work->walkLastDistance.vx = ACTOR_WALK_DISTANCE_NONE;
    work->walkLastDistance.vy = ACTOR_WALK_DISTANCE_NONE;
    work->walkLastDistance.vz = ACTOR_WALK_DISTANCE_NONE;
    work->walkStep++;
}

#include "../../shared/actor_messages_place_euler.inc.c"

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
