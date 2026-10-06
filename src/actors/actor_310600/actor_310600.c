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
/// setup entry it is spawned from is `func_actor_310600_80161E64`.
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
static void func_actor_310600_80161E64(Task* task);
static void func_actor_310600_80161FA0(Task* task);
static void func_actor_310600_8016231C(Task* arg0);
s32         func_actor_310600_8016246C(Task* task, s32 arg1, AnimationPlayRequest* cmd, s32 arg3);
s32         func_actor_310600_801625F0(Task* task, s32 arg1, s32 arg2, s32 arg3);
static void func_actor_310600_801629C4(Task* task);
static void func_actor_310600_80162A24(Task* arg0);
static void func_actor_310600_80162A58(Task* arg0);
static void func_actor_310600_80162A74(Task* task);
static void func_actor_310600_80162A7C(Task* task);
static void func_actor_310600_80162AD8(Task* task);
static void func_actor_310600_80162B98(Task* task);

/// State handlers of the child part task, which `func_actor_310600_8016274C`
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
    func_actor_310600_801629C4,
    taskKill,
} };

/// The actor's own state handlers, which `func_actor_310600_801629CC` runs by
/// `Task::state`: spawn/setup, per-frame tick and teardown.
static const TaskFuncTable3 D_actor_310600_80161E3C = { {
    func_actor_310600_80161E64,
    func_actor_310600_80161FA0,
    func_actor_310600_80162A24,
} };

/// The actor's movement steps, which `func_actor_310600_80162A7C` runs by
/// `_Actor310600RupertBroderickWork::walkStep`: turn to face the target point,
/// start moving, stop on arrival.
static const TaskFuncTable3 D_actor_310600_80161E48 = { {
    func_actor_310600_80162AD8,
    func_actor_310600_80162B98,
    func_actor_310600_8016231C,
} };

/// The constant local-space offset `func_actor_310600_80162B98` rotates,
/// `{ 0, 0, 0x200000, 0 }` -- straight ahead along the part's own +Z.
static const VECTOR D_actor_310600_80161E54 = { 0, 0, 0x200000, 0 };

static TmdSource _gActor310600RupertBroderickBody1;
static TmdSource _gActor310600RupertBroderickMongoose;
void             func_actor_310600_8016274C(Task*);
void             func_actor_310600_801629CC(Task*);

s32 func_actor_310600_8016246C(Task*, s32, AnimationPlayRequest*, s32);
s32 func_actor_310600_801625F0(Task*, s32, s32, s32);
s32 func_actor_310600_80162C94(Task*, s32, VECTOR*, s32);

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
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_310600_801629CC, { .model = &_gActor310600RupertBroderickBody1 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_310600_8016274C, { .model = &_gActor310600RupertBroderickMongoose } },
};

TaskMessageEntry D_actor_310600_801796BC[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_310600_8016246C },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_310600_801625F0 },
    { ACTOR_MESSAGE_WALK_TO, func_actor_310600_80162C94 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void func_actor_310600_80161E64(Task* task)
{
    _Actor310600RupertBroderickWork* work;
    WorldCollisionBody*              obj;

    work = memCalloc(sizeof(_Actor310600RupertBroderickWork), 0);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work              = work;
    work->animId            = ACTOR_MODEL_STATE_NONE;
    work->bank              = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown     = -1;
    work->walkMotion        = ACTOR_WALK_MOTION_IDLE;
    work->walkStep          = 0;
    work->walkCarry[0].word = 0;
    work->walkCarry[1].word = 0;
    work->walkCarry[2].word = 0;
    taskSpawnFromTable(D_actor_310600_801796A4, 1, 8, task);
    func_actor_310600_80162A58(task);
    obj                   = &work->body;
    obj->coord            = &task->extra.tmd->coords[1];
    obj->context.contacts = work->contacts;
    obj->key              = 0x30000;
    obj->radius           = 0x100;
    obj->pos.vx           = 0;
    obj->pos.vy           = 0;
    obj->pos.vz           = 0;
    obj->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, obj);
    obj->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(obj->context.contacts, ARRAY_SIZE(work->contacts), 0);
    task->msgTable = D_actor_310600_801796BC;
    func_actor_310600_801625F0(task, ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
    task->exitCallback = func_actor_310600_80162A24;
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
    TaskFunc                         funcs[2] = { func_actor_310600_80162A74, func_actor_310600_80162A7C };
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
                                    Gp_SpawnEff(EFFECT_ACTOR_MUZZLE_FLASH, coord, 9, NULL);
                                    TASK_MESSAGE_DISPATCH_POINTER(Gp_LookupSlot4(0), ACTOR_COMMAND_MESSAGE_APPLY, &D_actor_310600_8017969C, 0);
                                } else {
                                    TASK_MESSAGE_DISPATCH_POINTER(Gp_LookupSlot4(0), ACTOR_COMMAND_MESSAGE_APPLY, &D_actor_310600_801796A0, 0);
                                    taskSpawnFromTable(D_acropolis_cafeteria_80182AD8, 2, 0, 0);
                                }
                                break;
                            case 3:
                                Gp_SpawnEff(EFFECT_RELOAD_CASINGS_DROP, coord, 6, &D_actor_310600_80179694);
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

/// Arrival handler of the actor's second state table (`walkStep`), reached once
/// `func_actor_310600_80162B98` has laid down the per-frame `walkVelocity`: takes
/// each horizontal axis' gap between the target point `walkTarget.vx` / `walkTarget.vz`
/// and the root part's world translation -- the low 16 bits of the signed
/// difference, as in `func_actor_310600_80162AD8` -- and compares it against the
/// axis' stop threshold in `walkLastDistance`, which starts at
/// `ACTOR_WALK_DISTANCE_NONE`. Both gaps past their
/// threshold means the actor has stopped closing in: the arrival preset of
/// message 0x7D3 is queued (animation bank 0, id 0xD, path 1, param 0xA), but
/// only while `animId` still holds 0xC, and then `walkVelocity`, `walkMotion`
/// and `walkStep` are cleared and the handler returns without re-arming. Otherwise each
/// threshold is pulled down to the gap just measured, so the next tick that
/// fails to shrink it is the one that fires.
static void func_actor_310600_8016231C(Task* arg0)
{
    _Actor310600RupertBroderickWork* work;
    GfxCoord*                        coord;
    SVECTOR                          d;
    s32                              dx;
    s32                              dz;
    AnimationPlayRequest             cmd;

    work  = (_Actor310600RupertBroderickWork*)arg0->work;
    coord = (arg0->extra.tmd)->coords;
    if (work->walkTarget.vx - coord->coord.t[0] >= 0) {
        dx = (u16)work->walkTarget.vx - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->walkTarget.vx;
    }
    d.vx = dx;
    if (work->walkTarget.vz - coord->coord.t[2] >= 0) {
        dz = (u16)work->walkTarget.vz - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->walkTarget.vz;
    }
    d.vz = dz;
    if (d.vx >= work->walkLastDistance.vx && d.vz >= work->walkLastDistance.vz) {
        if (work->animId == 0xC) {
            cmd.source.index         = 0;
            cmd.animationId          = 0xD;
            cmd.blend                = ANIMATION_BLEND_INTERPOLATE;
            cmd.blendFrames          = 0xA;
            cmd.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            func_actor_310600_8016246C(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &cmd, 0);
        }
        work->walkVelocity.vx = 0;
        work->walkVelocity.vy = 0;
        work->walkVelocity.vz = 0;
        work->walkMotion      = ACTOR_WALK_MOTION_IDLE;
        work->walkStep        = 0;
        return;
    }
    work->walkLastDistance.vx = d.vx < 0 ? -d.vx : d.vx;
    work->walkLastDistance.vz = d.vz < 0 ? -d.vz : d.vz;
}

/// Animation preset handler of message 0x7D3: re-seeds the slot array off bank
/// table
/// `D_actor_310600_80179640` when the preset's bank index changes -- clearing
/// the latched id to -1 so the state below is re-applied -- then restarts or
/// resets every slot and ticks them, repeating the tick pass `1 +
/// D_actor_310600_80179644[state]` times.
///
/// The two byte stores must stay in this order. The second one is a QImode
/// store to a varying address, so cse treats it as aliasing everything and
/// drops the equivalence the first one recorded; that is what keeps
/// `work->bank` a reload instead of the register `cmd->source.index` arrived in.
s32 func_actor_310600_8016246C(Task* task, s32 arg1, AnimationPlayRequest* cmd, s32 arg3)
{
    _Actor310600RupertBroderickWork* work;
    TmdObject*                       ext;
    s32                              i;
    s32                              j;

    work = (_Actor310600RupertBroderickWork*)task->work;
    ext  = task->extra.tmd;
    if (cmd->source.index != work->bank) {
        work->bank   = cmd->source.index;
        work->animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, D_actor_310600_80179640[work->bank], ext, work->rig.poses,
                             work->rig.slots);
    }
    if (cmd->animationId != work->animId) {
        work->animId = cmd->animationId;
        if (cmd->blend != ANIMATION_BLEND_RESET) {
            for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
                animationSeekSlotWithBlend(&work->rig.anim, i, work->animId, 0, cmd->blendFrames);
            }
        } else {
            for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
                animationResetSlot(&work->rig.anim, i, work->animId);
            }
        }
        for (j = 0; j <= D_actor_310600_80179644[work->animId]; j++) {
            for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
                animationTickSlot(&work->rig.anim, i);
            }
        }
        work->ticking      = 1;
        work->cueFrame     = 0;
        work->shotCueCount = 0;
    }
    return 0;
}

/// Mode handler for the actor's display object, called by the setup path
/// (`func_actor_310600_80161E64` with message 0x7D5 and mode 0) with the mode in
/// `arg2`. Modes 0 and 2 hide the model: bit 0x80 of `TmdObject.flags` goes on,
/// the 0x8000 flag comes off the actor's own object, and 0x4 is cleared. Modes 1
/// and 3 show it: 0x80 comes off, 0x8000 goes on, the buffers are reinstated
/// through `tmdAllocPrimitiveBuffer`, and 0x4 is set. Mode 2 additionally latches
/// `freeCountdown` to 2. Returns 1 for a mode outside 0..3.
///
/// `work` and `w` are the same block on purpose. cse turns the second load of
/// `task->work` into a copy of the first and keeps the copy's register for the
/// mode 0..2 walks, because the only later use of the first load's register is
/// the `obj` assignment in the entry block -- so mode 3's walk reads the first
/// load's register and the other three read the copy's, the split the target
/// has. Writing `&work->body` inside case 3 instead leaves cse canonicalizing the
/// walks the other way, and the overlay comes out three instructions short.
s32 func_actor_310600_801625F0(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    _Actor310600RupertBroderickWork* work;
    _Actor310600RupertBroderickWork* w;
    TmdObject*                       ext;
    WorldCollisionBody*              p;
    WorldCollisionBody*              obj;
    s32                              i;
    s32                              ret;

    work = (_Actor310600RupertBroderickWork*)task->work;
    ext  = task->extra.tmd;
    w    = (_Actor310600RupertBroderickWork*)task->work;
    obj  = &work->body;
    ret  = 0;
    switch (arg2) {
        case 0:
            ext->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            p           = &w->body;
            for (i = 0; i <= 0; i++) {
                p->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                p++;
            }
            ext->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            ext->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            p           = &w->body;
            for (i = 0; i <= 0; i++) {
                p->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                p++;
            }
            tmdAllocPrimitiveBuffer(ext);
            ext->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            ext->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            p           = &w->body;
            for (i = 0; i <= 0; i++) {
                p->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                p++;
            }
            w->freeCountdown = 2;
            ext->flags      |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            ext->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            p           = obj;
            for (i = 0; i <= 0; i++) {
                p->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                p++;
            }
            ext->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

void func_actor_310600_8016274C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_310600_80161E24;
    sp.funcs[task->state](task);
}

#include "../../shared/model_placement_attach.inc.c"

#include "../../shared/model_placement_mirror_parent.inc.c"

#include "../../shared/model_placement_attach_part.inc.c"

/// Tick state of the second child handler triple `D_actor_310600_80161E30`:
/// does nothing.
static void func_actor_310600_801629C4(Task* task)
{
}

void func_actor_310600_801629CC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_310600_80161E3C;
    sp.funcs[task->state](task);
}

static void func_actor_310600_80162A24(Task* arg0)
{
    worldCollisionUnlinkBody(&((_Actor310600RupertBroderickWork*)arg0->work)->body);
    enemyTaskExit(arg0);
}

static void func_actor_310600_80162A58(Task* arg0)
{
    TmdObject*                       ext;
    _Actor310600RupertBroderickWork* work;

    work          = (_Actor310600RupertBroderickWork*)arg0->work;
    ext           = arg0->extra.tmd;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

/// Entry 0 of the two-entry stack table `func_actor_310600_80161FA0` dispatches
/// through by `walkMotion`: the idle handler, which does nothing. Entry 1 is
/// `func_actor_310600_80162A7C`; both receive the current task.
static void func_actor_310600_80162A74(Task* task)
{
}

/// Runs the entry of the actor's second state table that `walkStep` selects -
/// the counter `func_actor_310600_80162AD8` and `func_actor_310600_80162B98`
/// bump as they finish, so the table steps through the handlers in turn. Copies
/// the table onto the stack first, the same dispatch `func_actor_310600_801629CC`
/// performs over `state`.
static void func_actor_310600_80162A7C(Task* task)
{
    _Actor310600RupertBroderickWork* work;
    TaskFuncTable3                   fns;

    work = (_Actor310600RupertBroderickWork*)task->work;
    fns  = D_actor_310600_80161E48;
    fns.funcs[work->walkStep](task);
}

/// Turns the actor's root part to face the work block's stored point: normalises
/// the offset from the part's own translation, takes its yaw with `ratan2`, and
/// rebuilds the local matrix from that yaw alone. Clearing `composeStamp` makes
/// `actorRenderComposeCoordChain` recompute the composed matrix from it, and bumping
/// `walkStep` moves the actor on to the next handler of its state table.
static void func_actor_310600_80162AD8(Task* task)
{
    _Actor310600RupertBroderickWork* work;
    GfxCoord*                        coord;
    VECTOR                           delta;
    SVECTOR                          dir;
    SVECTOR                          rot;

    work  = (_Actor310600RupertBroderickWork*)task->work;
    coord = task->extra.tmd->coords;

    delta.vx = work->walkTarget.vx - coord->coord.t[0];
    delta.vy = work->walkTarget.vy - coord->coord.t[1];
    delta.vz = work->walkTarget.vz - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;

    coord->param.rot.vx = rot.vx;
    coord->param.rot.vy = rot.vy;
    coord->param.rot.vz = rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->walkStep++;
}

/// State handler reached by the `walkStep` advance `func_actor_310600_80162AD8`
/// ends with: rotates the constant local-space offset
/// `D_actor_310600_80161E54` through the root part's matrix into `work->walkVelocity`,
/// opens the per-axis stop threshold `walkLastDistance` to
/// `ACTOR_WALK_DISTANCE_NONE`, which disables it for the update
/// loop, and advances `walkStep` again so the dispatcher runs the next handler.
static void func_actor_310600_80162B98(Task* task)
{
    _Actor310600RupertBroderickWork* work;
    GfxCoord*                        coord;
    VECTOR                           vec;

    coord = task->extra.tmd->coords;
    work  = (_Actor310600RupertBroderickWork*)task->work;

    vec = D_actor_310600_80161E54;
    ApplyMatrixLV(&coord->coord, &vec, &work->walkVelocity);
    work->walkLastDistance.vx = ACTOR_WALK_DISTANCE_NONE;
    work->walkLastDistance.vy = ACTOR_WALK_DISTANCE_NONE;
    work->walkLastDistance.vz = ACTOR_WALK_DISTANCE_NONE;
    work->walkStep++;
}

#include "../../shared/actor_messages_place_euler.inc.c"

/// Sends the actor walking to the point `arg2`: stores it as the target the
/// movement steps of `D_actor_310600_80161E48` turn toward and close in on,
/// switches the tick onto those steps (`walkMotion`), and starts animation 0xC
/// of bank 0 through `func_actor_310600_8016246C`. `arg1` is unused.
s32 func_actor_310600_80162C94(Task* arg0, s32 arg1, VECTOR* arg2, s32 arg3)
{
    _Actor310600RupertBroderickWork* work;
    AnimationPlayRequest             cmd;

    work = (_Actor310600RupertBroderickWork*)arg0->work;

    work->walkMotion    = ACTOR_WALK_MOTION_WALKING;
    work->walkTarget.vx = arg2->vx;
    work->walkTarget.vy = arg2->vy;
    work->walkTarget.vz = arg2->vz;

    cmd.source.index         = 0;
    cmd.animationId          = 0xC;
    cmd.blend                = ANIMATION_BLEND_RESET;
    cmd.blendFrames          = 0;
    cmd.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;

    func_actor_310600_8016246C(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &cmd, 0);
}
