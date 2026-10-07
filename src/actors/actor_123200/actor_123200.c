#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"
#include "../../shared/coord_math.h"
#include "../../shared/actor_messages.h"
#include "../../shared/anim_driver.h"

/// Values of `_Actor123200Work::state`: the index of the handler the per-frame
/// tick runs.
///
/// Animation numbers are indices into the package's animation-set table. The
/// two walking states do the same thing; a change from one to the other is
/// what makes the walk start over.
enum {
    ACTOR_123200_STATE_HIDDEN             = 0, // not drawn, not lockable, no ground shadow
    ACTOR_123200_STATE_WALK               = 1, // restarts animation 2, then steps 5 units along the facing every tick; entered by actor command 2 and by the model-draw message
    ACTOR_123200_STATE_FIRST_STAGING_WALK = 2  // the same walk; entered only by actor command 1
};

/// Values of `_Actor123200Work::lightScale`, in 4096ths.
enum {
    ACTOR_123200_LIGHT_SCALE_QUARTER = 0x400,
    ACTOR_123200_LIGHT_SCALE_FULL    = 0x1000 // the tick leaves the light matrix as built
};

/// Animation-set indices used by this actor's driver and sound selector.
///
/// Sets 3 and 5 retain sound tests, but this package never requests them.
enum {
    ACTOR_123200_ANIM_SPAWN = 1,
    ACTOR_123200_ANIM_WALK  = 2,
    ACTOR_123200_ANIM_SET_3 = 3,
    ACTOR_123200_ANIM_SET_5 = 5,
};

/// Forward displacement per walking tick, in parent-coordinate units.
enum { ACTOR_123200_WALK_STEP_DISTANCE = 5 };

/// Work block of the package's enemy task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. The
/// actor is a bone suckler staged for the Dryfield motel room 1 event: it has
/// no collision bodies and no hit points, and only walks straight ahead while
/// the event's actor commands pick its state and how brightly it is lit.
///
/// Everything up to and including `driver` is laid out as `AnimDriverWork`,
/// through which the shared animation driver reaches the block: its
/// `carrierState` bytes are the five words and the gap ahead of `rig` here. A
/// cue index is the low ten bits of the record a slot's current pose names
/// (`ANIMATION_POSE_CUE_INDEX_MASK`).
typedef struct {
    s16           state;        // `ACTOR_123200_STATE_*`
    s16           prevState;    // `state` the tick last ran; -1 from spawn, so the first tick enters `state` afresh
    s16           stateEntered; // 1 on the first tick of a state, when the handler sets itself up (0 otherwise)
    s16           stateFrame;   // ticks a walking state has run since it was entered; never read
    s16           field_8;      // cleared at spawn and never accessed again; role unproven
    byte          pad_A[2];     // never accessed
    ActorAnimRig6 rig;          // playback of the model's parts; slot 1's status and cue index time the sound cues
    struct {
        s16 state;              // `ANIM_DRIVER_STATE_*` (0 idle, 1 or 2 restart requested, 3 playing)
        s16 playingSet;         // animation the slots were last restarted on
        s16 requestedSet;       // animation the next restart plays; also what the sound cues are keyed on
        s16 rate;               // playback rate in sixteenths of a frame per tick; 16 plus the placement index (odd placements) or minus half of it (even)
        s16 rateBias;           // added to `rate`; always 0 here
        s16 tickCount;          // advancing ticks since the last restart
        s16 jumpCount;          // of those, ticks on which slot 1 followed a control jump: loops of a looping animation
    } driver;                   // the animation driver's state, member for member `AnimDriverWork`'s
    s16     field_17E;          // cleared at spawn and never accessed again; role unproven
    byte    pad_180[0x14];      // never accessed
    u8      lastCommandStage;   // stage tag of the last actor command received, whatever its namespace; never read
    u8      lastCommandArea;    // area tag of that command; never read
    u8      lastCommand;        // low byte of that command's selector; never read
    byte    pad_197[1];         // never accessed
    u16     field_198;          // 5 at spawn, offset by the placement index the way `driver.rate` is; never accessed again; role unproven
    u16     field_19A;          // 20 at spawn, offset the same way; never accessed again; role unproven
    byte    pad_19C[0xC];       // never accessed
    SVECTOR spawnPos;           // root position at spawn; never read
    SVECTOR walkTarget;         // world point stored on entering a walking state; never read, since the walk follows the facing instead of steering
    byte    pad_1B8[4];         // never accessed
    MATRIX  lightMtx;           // storage for the model's `TmdObject::lightMtx`
    MATRIX  colorMtx;           // storage for the model's `TmdObject::colorMtx`
    byte    pad_1FC[0x20];      // never accessed
    s16     lightScale;         // `ACTOR_123200_LIGHT_SCALE_*`: scales `lightMtx` after every tick's relighting, dimming the model; 0 from spawn until an actor command sets it
    byte    pad_21E[2];         // never accessed
    u16     lastSoundCueIndex;  // slot 1's cue index when the walk cue sound last fired, so a held cue sounds once; 0 on any other cue
    byte    pad_222[0xA];       // never accessed
} _Actor123200Work;
STATIC_ASSERT_SIZEOF(_Actor123200Work, 0x22C);
STATIC_ASSERT(OFFSET_OF(_Actor123200Work, rig) == OFFSET_OF(AnimDriverWork, rig), Actor123200Work_rig);
STATIC_ASSERT(OFFSET_OF(_Actor123200Work, driver) == OFFSET_OF(AnimDriverWork, state), Actor123200Work_driver);

/// Enemy parameters the spawn handler installs at `Enemy::param`.
extern EnemyParams D_actor_123200_80134208;

/// Resource whose animation-set table is bound by `animationInitContext`.
extern u8 D_actor_123200_80137154[];

/// Message table the spawn handler publishes as `Task::msgTable`.
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_123200_80137214[4];

/// Integer part of the last movement step `func_actor_123200_801329F0`
/// applied.
static SVECTOR ActorContact_ScratchPosition;

/// Overlay-wide spawn record the spawn handler fills for the instance's own
/// coordinate, with the 0x100 / 1 argument pair. Each overlay that spawns this
/// way keeps one, and they differ only in the coordinate and the argument.
extern EffectSpawnArg D_actor_123200_80137248;

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

static void _actor123200Hide(Enemy* enemy, Task* task);

static TmdSource _gActor123200BoneSucklerBody;
static void      _actor123200Task(Task* task);

static s32 _actor123200SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static s32 _actor123200ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);

#include "../../shared/actor_contacts.h"

DamageAttack D_actor_123200_80134204[1] = {
    { 24, 7 },
};

EnemyParams D_actor_123200_80134208 = { D_actor_123200_80134204, 1, 6, 20, 3, 100, 0, 100, 0 };

static TmdBone _gActor123200BoneSucklerBodySkeleton[6] = {
#include "assets/bone_suckler_body_skeleton.inc"
};

static u32 _gActor123200BoneSucklerBodyPartVerts[6] = {
#include "assets/bone_suckler_body_partVerts.inc"
};

static SVECTOR _gActor123200BoneSucklerBodyVerts[102] = {
#include "assets/bone_suckler_body_verts.inc"
};

static SVECTOR _gActor123200BoneSucklerBodyNormals[139] = {
#include "assets/bone_suckler_body_normals.inc"
};

static u32 _gActor123200BoneSucklerBodyStream[1048] = {
#include "assets/bone_suckler_body_stream.inc"
};

static TmdSource _gActor123200BoneSucklerBody = {
    0,
    5984,
    1264,
    6,
    _gActor123200BoneSucklerBodyPartVerts,
    _gActor123200BoneSucklerBodyVerts,
    _gActor123200BoneSucklerBodyNormals,
    _gActor123200BoneSucklerBodySkeleton,
    _gActor123200BoneSucklerBodyStream,
};

static AnimationPackedPose _gActor123200Animation03E30Bank1[4] = {
#include "assets/actor_123200_animation_03E30_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation03E30Bank4[21] = {
#include "assets/actor_123200_animation_03E30_bank4.inc"
};

static AnimationRecord _gActor123200Animation03E30Records[43] = {
#include "assets/actor_123200_animation_03E30_records.inc"
};

static u16 _gActor123200Animation03E30Indices[6] = {
#include "assets/actor_123200_animation_03E30_indices.inc"
};

static AnimationSet _gActor123200Animation03E30 = {
    _gActor123200Animation03E30Records,
    _gActor123200Animation03E30Indices,
    { NULL, _gActor123200Animation03E30Bank1, NULL, NULL, _gActor123200Animation03E30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor123200Animation040F4Bank1[19] = {
#include "assets/actor_123200_animation_040F4_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation040F4Bank4[36] = {
#include "assets/actor_123200_animation_040F4_bank4.inc"
};

static AnimationRecord _gActor123200Animation040F4Records[71] = {
#include "assets/actor_123200_animation_040F4_records.inc"
};

static u16 _gActor123200Animation040F4Indices[6] = {
#include "assets/actor_123200_animation_040F4_indices.inc"
};

static AnimationSet _gActor123200Animation040F4 = {
    _gActor123200Animation040F4Records,
    _gActor123200Animation040F4Indices,
    { NULL, _gActor123200Animation040F4Bank1, NULL, NULL, _gActor123200Animation040F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor123200Animation04398Bank1[20] = {
#include "assets/actor_123200_animation_04398_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation04398Bank4[31] = {
#include "assets/actor_123200_animation_04398_bank4.inc"
};

static AnimationRecord _gActor123200Animation04398Records[65] = {
#include "assets/actor_123200_animation_04398_records.inc"
};

static u16 _gActor123200Animation04398Indices[6] = {
#include "assets/actor_123200_animation_04398_indices.inc"
};

static AnimationSet _gActor123200Animation04398 = {
    _gActor123200Animation04398Records,
    _gActor123200Animation04398Indices,
    { NULL, _gActor123200Animation04398Bank1, NULL, NULL, _gActor123200Animation04398Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor123200Animation04578Bank1[6] = {
#include "assets/actor_123200_animation_04578_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation04578Bank4[38] = {
#include "assets/actor_123200_animation_04578_bank4.inc"
};

static AnimationRecord _gActor123200Animation04578Records[51] = {
#include "assets/actor_123200_animation_04578_records.inc"
};

static u16 _gActor123200Animation04578Indices[6] = {
#include "assets/actor_123200_animation_04578_indices.inc"
};

static AnimationSet _gActor123200Animation04578 = {
    _gActor123200Animation04578Records,
    _gActor123200Animation04578Indices,
    { NULL, _gActor123200Animation04578Bank1, NULL, NULL, _gActor123200Animation04578Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor123200Animation046B0Bank1[4] = {
#include "assets/actor_123200_animation_046B0_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation046B0Bank4[13] = {
#include "assets/actor_123200_animation_046B0_bank4.inc"
};

static AnimationRecord _gActor123200Animation046B0Records[40] = {
#include "assets/actor_123200_animation_046B0_records.inc"
};

static u16 _gActor123200Animation046B0Indices[6] = {
#include "assets/actor_123200_animation_046B0_indices.inc"
};

static AnimationSet _gActor123200Animation046B0 = {
    _gActor123200Animation046B0Records,
    _gActor123200Animation046B0Indices,
    { NULL, _gActor123200Animation046B0Bank1, NULL, NULL, _gActor123200Animation046B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor123200Animation04850Bank1[7] = {
#include "assets/actor_123200_animation_04850_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation04850Bank4[28] = {
#include "assets/actor_123200_animation_04850_bank4.inc"
};

static AnimationRecord _gActor123200Animation04850Records[42] = {
#include "assets/actor_123200_animation_04850_records.inc"
};

static u16 _gActor123200Animation04850Indices[6] = {
#include "assets/actor_123200_animation_04850_indices.inc"
};

static AnimationSet _gActor123200Animation04850 = {
    _gActor123200Animation04850Records,
    _gActor123200Animation04850Indices,
    { NULL, _gActor123200Animation04850Bank1, NULL, NULL, _gActor123200Animation04850Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor123200Animation04A64Bank1[6] = {
#include "assets/actor_123200_animation_04A64_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation04A64Bank4[42] = {
#include "assets/actor_123200_animation_04A64_bank4.inc"
};

static AnimationRecord _gActor123200Animation04A64Records[60] = {
#include "assets/actor_123200_animation_04A64_records.inc"
};

static u16 _gActor123200Animation04A64Indices[6] = {
#include "assets/actor_123200_animation_04A64_indices.inc"
};

static AnimationSet _gActor123200Animation04A64 = {
    _gActor123200Animation04A64Records,
    _gActor123200Animation04A64Indices,
    { NULL, _gActor123200Animation04A64Bank1, NULL, NULL, _gActor123200Animation04A64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor123200Animation04E74Bank1[17] = {
#include "assets/actor_123200_animation_04E74_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation04E74Bank4[85] = {
#include "assets/actor_123200_animation_04E74_bank4.inc"
};

static AnimationRecord _gActor123200Animation04E74Records[111] = {
#include "assets/actor_123200_animation_04E74_records.inc"
};

static u16 _gActor123200Animation04E74Indices[6] = {
#include "assets/actor_123200_animation_04E74_indices.inc"
};

static AnimationSet _gActor123200Animation04E74 = {
    _gActor123200Animation04E74Records,
    _gActor123200Animation04E74Indices,
    { NULL, _gActor123200Animation04E74Bank1, NULL, NULL, _gActor123200Animation04E74Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor123200Animation05134Bank1[12] = {
#include "assets/actor_123200_animation_05134_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation05134Bank4[52] = {
#include "assets/actor_123200_animation_05134_bank4.inc"
};

static AnimationRecord _gActor123200Animation05134Records[75] = {
#include "assets/actor_123200_animation_05134_records.inc"
};

static u16 _gActor123200Animation05134Indices[6] = {
#include "assets/actor_123200_animation_05134_indices.inc"
};

static AnimationSet _gActor123200Animation05134 = {
    _gActor123200Animation05134Records,
    _gActor123200Animation05134Indices,
    { NULL, _gActor123200Animation05134Bank1, NULL, NULL, _gActor123200Animation05134Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor123200Animation0530CBank1[5] = {
#include "assets/actor_123200_animation_0530C_bank1.inc"
};

static AnimationPackedRotation _gActor123200Animation0530CBank4[19] = {
#include "assets/actor_123200_animation_0530C_bank4.inc"
};

static AnimationRecord _gActor123200Animation0530CRecords[71] = {
#include "assets/actor_123200_animation_0530C_records.inc"
};

static u16 _gActor123200Animation0530CIndices[6] = {
#include "assets/actor_123200_animation_0530C_indices.inc"
};

static AnimationSet _gActor123200Animation0530C = {
    _gActor123200Animation0530CRecords,
    _gActor123200Animation0530CIndices,
    { NULL, _gActor123200Animation0530CBank1, NULL, NULL, _gActor123200Animation0530CBank4, NULL, NULL, NULL },
};

u8 D_actor_123200_80137154[192] = {
    0,
    0,
    0,
    0,
    80,
    92,
    19,
    128,
    20,
    95,
    19,
    128,
    184,
    97,
    19,
    128,
    152,
    99,
    19,
    128,
    208,
    100,
    19,
    128,
    112,
    102,
    19,
    128,
    132,
    104,
    19,
    128,
    148,
    108,
    19,
    128,
    84,
    111,
    19,
    128,
    44,
    113,
    19,
    128,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    6,
    0,
    0,
    0,
    0,
    0,
    9,
    0,
    0,
    0,
    0,
    0,
    6,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    6,
    6,
    6,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

TaskMessageEntry D_actor_123200_80137214[4] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor123200SetModelDraw },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor123200ApplyCommand },
    { ACTOR_MESSAGE_PLACE, actorMsgPlace },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_123200_80137234 = { { { TASK_BODY_TMD, 96 } }, _actor123200Task, { .model = &_gActor123200BoneSucklerBody } };

static SVECTOR ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
///
/// Components are signed 16.16 corrections shifted right by 16 and narrowed
/// to halfwords. Fractional X/Z add a further unit in the correction's sign;
/// X/Z record the root correction, while Y is only recorded. No grid hit
/// leaves the old value intact. The borrowed vector lives for the overlay's
/// lifetime; `pad` is unused.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

EffectSpawnArg D_actor_123200_80137248;

#include "../../shared/actor_contacts.h"

#include "../../shared/actor_contacts.inc.c"

#include "../../shared/anim_driver_tick.inc.c"

/// Selects the sound for the driven part's current animation cue, or zero.
///
/// A live initialized work block supplies slot 1. Sets 2 and 3 latch their two
/// cue indices so a held cue sounds once; another cue clears the latch. Set 5
/// reports its sound on every call with a control-jump flag. Other requests
/// leave the latch unchanged. The returned character-bank id has no instance;
/// the caller adds the placement index before playing it.
static s32 _actor123200SelectCueSound(_Actor123200Work* work)
{
    enum {
        ACTOR_123200_WALK_CUE_FIRST   = 0x11,
        ACTOR_123200_WALK_CUE_SECOND  = 0x15,
        ACTOR_123200_SET_3_CUE_FIRST  = 0xD,
        ACTOR_123200_SET_3_CUE_SECOND = 0x12,
        ACTOR_123200_SOUND_CUE_NONE   = 0,
        ACTOR_123200_SOUND_POSE_CUE   = SOUND_CHARACTER(0xC, 1),
        ACTOR_123200_SOUND_JUMP_CUE   = SOUND_CHARACTER(0xC, 5),
    };
    u16 cueIndex;
    s32 cueValue;

    // Both cue pairs share the same latch update and halfword promotion.
    switch (work->driver.requestedSet) {
        case ACTOR_123200_ANIM_WALK:
            cueIndex = work->rig.slots[ANIM_DRIVER_FIRST_SLOT].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            cueValue = cueIndex;
            if (cueValue != ACTOR_123200_WALK_CUE_SECOND) {
                goto testOtherWalkCue;
            }
        checkNewCue:
            if (work->lastSoundCueIndex == cueValue) {
                goto holdCue;
            }
            work->lastSoundCueIndex = cueIndex;
            return ACTOR_123200_SOUND_POSE_CUE;
        testOtherWalkCue:
            if (cueValue == ACTOR_123200_WALK_CUE_FIRST) {
                goto checkNewCue;
            }
        clearCueLatch:
            work->lastSoundCueIndex = ACTOR_123200_SOUND_CUE_NONE;
            break;
        case ACTOR_123200_ANIM_SET_3:
            cueIndex = work->rig.slots[ANIM_DRIVER_FIRST_SLOT].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            cueValue = cueIndex;
            if (cueValue != ACTOR_123200_SET_3_CUE_FIRST && cueValue != ACTOR_123200_SET_3_CUE_SECOND) {
                goto clearCueLatch;
            }
            goto checkNewCue;
        holdCue:
            work->lastSoundCueIndex = cueIndex;
            break;
        case ACTOR_123200_ANIM_SET_5:
            if (work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
                return ACTOR_123200_SOUND_JUMP_CUE;
            }
            break;
    }
    return 0;
}

/// Initializes the event actor's model, animation and owned work block.
///
/// The task must own the loaded six-coordinate model and borrow its live
/// enemy through spawn argument 2. Allocation failure destroys the pair;
/// success advances the task to its update state, initially hidden. The work
/// block owns the model's lighting storage until enemy teardown. Placement
/// index bits bias playback speed without changing the fixed walking distance.
static void _actor123200Spawn(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_123200_NO_PREVIOUS_STATE        = -1,
        ACTOR_123200_SPAWN_DIRECTION_DISTANCE = 1000,
    };
    SVECTOR           spawnDisplacement;
    _Actor123200Work* work;
    TmdObject*        model;
    GfxCoord*         rootCoord;
    u32               placementIndex;
    u32               placementParity;

    model      = task->extra.tmd;
    rootCoord  = model->coords;
    work       = memCalloc(sizeof(_Actor123200Work), false);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // Bind the animation rig and publish a non-targetable display node.
    task->msgTable    = D_actor_123200_80137214;
    rootCoord->parent = &gGfxViewCoord;
    model->flags      = 0;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_123200_80137154, model, work->rig.poses, work->rig.slots);

    enemy->field_4    = &rootCoord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &task->extra.tmd->coords[2];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->param                  = &D_actor_123200_80134208;
    enemy->reactionFlags          = 0;
    enemy->hpMax                  = 0;
    enemy->hp                     = 0;
    enemy->recs                   = 0;

    work->driver.requestedSet = ACTOR_123200_ANIM_SPAWN;
    work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
    work->driver.rate         = ANIMATION_RATE_ONE;
    work->driver.rateBias     = 0;
    _animDriverTick(task);
    work->field_17E         = 0;
    work->field_8           = 0;
    model->lightMtx         = &work->lightMtx;
    model->colorMtx         = &work->colorMtx;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_198         = 5;
    work->field_19A         = 0x14;

    // Keep the halfword narrowing and the signed placement arithmetic distinct.
    placementIndex  = (u16)(enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT);
    placementParity = placementIndex & 1;
    if (placementParity == 1) {
        work->driver.rate += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_19A   += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_198   += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    } else {
        work->driver.rate -= placementIndex >> 1;
        work->field_19A   -= (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
        work->field_198   -= (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
    }

    work->spawnPos.vx = task->extra.tmd->coords->coord.t[0];
    work->spawnPos.vy = task->extra.tmd->coords->coord.t[1];
    work->spawnPos.vz = task->extra.tmd->coords->coord.t[2];

    // Preserve the horizontal displacement calculation even though it is unused.
    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, &spawnDisplacement);
    spawnDisplacement.vy = 0;
    _actorMovementBuildDisplacement(&spawnDisplacement, ACTOR_123200_SPAWN_DIRECTION_DISTANCE);

    work->state                        = ACTOR_123200_STATE_HIDDEN;
    work->prevState                    = ACTOR_123200_NO_PREVIOUS_STATE;
    D_actor_123200_80137248.coord      = task->extra.tmd->coords;
    D_actor_123200_80137248.spawnArgLo = 0x100;
    D_actor_123200_80137248.spawnArgHi = 1;
    task->state++;
}

/// Advances a writable coordinate five parent-coordinate units along local Z.
///
/// Includes Y and removes the axis's scale; SDK normalization and GTE rounding
/// may change the exact distance. Requires an initialized scratch stack with
/// one aligned SVECTOR free (eight bytes), released before return. Overwrites
/// GTE state and marks composition dirty. The caller handles actor freezing.
static __inline__ void _actor123200StepForward(GfxCoord* coord)
{
    SVECTOR* scratchHead;
    SVECTOR* displacement;

    scratchHead                   = SCRATCH_STACK_CURSOR(SVECTOR);
    displacement                  = scratchHead - 1;
    SCRATCH_STACK_CURSOR(SVECTOR) = displacement;

    gfxReadMatrixZAxis(&coord->coord, displacement);
    _actorMovementBuildDisplacement(displacement, ACTOR_123200_WALK_STEP_DISTANCE);

    coord->coord.t[0]  += displacement->vx;
    coord->coord.t[1]  += displacement->vy;
    coord->coord.t[2]  += displacement->vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Runs the second staging walk or the walk selected by a draw message.
///
/// Requires live enemy, model and initialized work storage. Entry restores
/// drawing buffers and restarts the walk without moving. Later ticks advance
/// animation and move five parent-coordinate units unless actors are frozen.
/// The scratch stack needs 12 bytes for the enclosing frame plus the step's
/// eight-byte vector and any animation workspace; reservations are released.
static void _actor123200Walk(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_123200_WALK_RESERVED_BYTES = 12,
        ACTOR_123200_FROZEN              = 1,
    };
    _Actor123200Work* work;
    TmdObject*        model;
    GfxCoord*         rootCoord;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->walkTarget.vx       = 0x115D;
        work->walkTarget.vy       = 1;
        work->walkTarget.vz       = 0x12D5;
        work->driver.requestedSet = ACTOR_123200_ANIM_WALK;
        work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
        _animDriverTick(task);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->stateFrame                      = 0;
        return;
    }
    work->stateFrame++;
    // Retain the enclosing scratch frame; its contents are never accessed here.
    SCRATCH_STACK_RESERVE_BYTES(ACTOR_123200_WALK_RESERVED_BYTES);
    rootCoord = task->extra.tmd->coords;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != ACTOR_123200_FROZEN) {
        _actor123200StepForward(rootCoord);
    }
    _animDriverTick(task);
    SCRATCH_STACK_RELEASE_BYTES(ACTOR_123200_WALK_RESERVED_BYTES);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Runs the first staging walk, whose lighting is chosen by actor command 1.
///
/// Requires live enemy, model and initialized work storage. Entry restores
/// drawing buffers and restarts the walk without moving. Later ticks advance
/// animation and move five parent-coordinate units unless actors are frozen.
/// The scratch stack needs an aligned eight-byte step vector plus animation
/// workspace. Switching to the other walk state deliberately restarts the clip.
static void _actor123200FirstStagingWalk(Enemy* enemy, Task* task)
{
    _Actor123200Work* work;
    TmdObject*        model;
    GfxCoord*         rootCoord;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->walkTarget.vx       = 0x115D;
        work->walkTarget.vy       = 1;
        work->walkTarget.vz       = 0x12D5;
        work->driver.requestedSet = ACTOR_123200_ANIM_WALK;
        work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
        _animDriverTick(task);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->stateFrame                      = 0;
        return;
    }
    work->stateFrame++;
    rootCoord = task->extra.tmd->coords;
    _actorMovementStepForward(rootCoord, ACTOR_123200_WALK_STEP_DISTANCE);
    _animDriverTick(task);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// The three handlers `_actor123200Update` picks between by the work
/// block's `state` (`ACTOR_123200_STATE_*`), copied onto its stack before the
/// call: 0 the hidden state, 1 and 2 the two walking handlers.
static const EnemyTaskFuncTable3 D_actor_123200_80131E24 = {
    {
        _actor123200Hide,
        _actor123200Walk,
        _actor123200FirstStagingWalk,
    },
};

/// Relights, draws and advances the staged actor for one running scene tick.
///
/// Requires live enemy, model, room lighting and initialized work storage.
/// The work state must be one of the three ACTOR_123200_STATE values. Paused
/// scenes still relight and draw the shadow; hidden scenes suppress the model.
/// Both return before state entry, animation or sound handling. Running scenes
/// dispatch state entry once and spatialize cue sounds by placement instance.
static void _actor123200Update(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_123200_SHADOW_HALF_SIZE     = 0x180, // Coordinate units
        ACTOR_123200_SOUND_INSTANCE_SHIFT = 8,
    };
    VECTOR              lightingVector;
    EnemyTaskFuncTable3 stateHandlers;
    _Actor123200Work*   work;
    s32                 soundId;
    s32                 panOffset;
    s32                 cueSoundId;

    // Compose the root before querying lighting and the projected sound position.
    work                                  = task->work;
    stateHandlers                         = D_actor_123200_80131E24;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
    lightingVector.vx = task->extra.tmd->coords->workm.t[0];
    lightingVector.vy = task->extra.tmd->coords->workm.t[1];
    lightingVector.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(enemy, &lightingVector, 0, 0);
    if (work->lightScale != ACTOR_123200_LIGHT_SCALE_FULL) {
        // Reuse the sample storage for the uniform Q12 light-matrix scale.
        lightingVector.vx = lightingVector.vy = lightingVector.vz = work->lightScale;
        ScaleMatrix(&work->lightMtx, &lightingVector);
    }
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->state != ACTOR_123200_STATE_HIDDEN) {
                task->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&task->extra.tmd->coords->workm), ACTOR_123200_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (work->state != ACTOR_123200_STATE_HIDDEN) {
                task->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&task->extra.tmd->coords->workm), ACTOR_123200_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
            }
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
    // Distinct walk states make consecutive staging commands restart the same clip.
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = work->state;
    stateHandlers.funcs[work->state](enemy, task);
    cueSoundId = _actor123200SelectCueSound(work);
    if (cueSoundId != 0) {
        soundId   = cueSoundId | ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_123200_SOUND_INSTANCE_SHIFT);
        panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// The enemy's three task states -- spawn, per-frame tick and teardown -- which
/// `_actor123200Task` runs by `Task::state`.
static const EnemyTaskFuncTable3 D_actor_123200_80131E30 = {
    {
        _actor123200Spawn,
        _actor123200Update,
        enemyDestroy,
    },
};

/// Applies this staged actor's model-draw message and selects its walk or hidden state.
///
/// Requires the live model and initialized work. Modes 0/1 hide/show and allocate
/// buffers, selecting WALK; the next walk entry permits drawing in either case.
/// Mode 2 retains existing flags and disables automatic buffer recovery, selecting
/// HIDDEN. Modes 3/4 replace flags with that recovery exclusion and select HIDDEN.
/// Other modes change nothing. The id and second argument are ignored. Returns 0.
static s32 _actor123200SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    enum {
        ACTOR_123200_DRAW_RETAIN_FLAGS_SKIP_AUTO_BUFFER  = 2,
        ACTOR_123200_DRAW_RESET_FLAGS_SKIP_AUTO_BUFFER_3 = 3,
        ACTOR_123200_DRAW_RESET_FLAGS_SKIP_AUTO_BUFFER_4 = 4,
    };
    TmdObject*        model;
    _Actor123200Work* work;

    model = task->extra.tmd;
    work  = task->work;
    switch (drawMode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            work->state = ACTOR_123200_STATE_WALK;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags = 0;
            tmdAllocPrimitiveBuffer(model);
            work->state = ACTOR_123200_STATE_WALK;
            break;
        case ACTOR_123200_DRAW_RETAIN_FLAGS_SKIP_AUTO_BUFFER:
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state   = ACTOR_123200_STATE_HIDDEN;
            break;
        case ACTOR_123200_DRAW_RESET_FLAGS_SKIP_AUTO_BUFFER_3:
        case ACTOR_123200_DRAW_RESET_FLAGS_SKIP_AUTO_BUFFER_4:
            model->flags  = 0;
            work->state   = ACTOR_123200_STATE_HIDDEN;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// Applies the Dryfield motel room 1 event's staging command to this actor.
///
/// Requires initialized work, a live borrowed enemy and a readable command
/// through synchronous dispatch. Every request stores its context and low
/// command byte. In this room's namespace, 1 starts the first walk at quarter
/// lighting except placement 1, 2 starts the second walk at full lighting, and
/// 3 hides the actor. Other selectors and namespaces take no action. The id and
/// second argument are ignored; the payload is not retained. Returns 0.
static s32 _actor123200ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    enum {
        ACTOR_123200_COMMAND_NONE           = 0,
        ACTOR_123200_COMMAND_FIRST_STAGING  = 1,
        ACTOR_123200_COMMAND_SECOND_STAGING = 2,
        ACTOR_123200_COMMAND_START_COMBAT   = 3,
        ACTOR_123200_FULL_LIGHT_PLACEMENT   = 1,
    };
    _Actor123200Work* work;
    Enemy*            enemy;

    work                   = task->work;
    enemy                  = task->spawnArg2.pointer;
    work->lastCommandStage = command->context.loc.stage;
    work->lastCommandArea  = command->context.loc.area;
    work->lastCommand      = command->command;
    // The packed little-endian key has the stage byte below the area byte.
    if (command->context.key == (GAME_STAGE_DRYFIELD | (GAME_AREA_DRYFIELD_MOTEL_ROOM_1 << 8))) {
        switch (command->command) {
            case ACTOR_123200_COMMAND_FIRST_STAGING:
                if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == ACTOR_123200_FULL_LIGHT_PLACEMENT) {
                    work->lightScale = ACTOR_123200_LIGHT_SCALE_FULL;
                } else {
                    work->lightScale = ACTOR_123200_LIGHT_SCALE_QUARTER;
                }
                work->state = ACTOR_123200_STATE_FIRST_STAGING_WALK;
                break;
            case ACTOR_123200_COMMAND_SECOND_STAGING:
                work->lightScale = ACTOR_123200_LIGHT_SCALE_FULL;
                work->state      = ACTOR_123200_STATE_WALK;
                break;
            case ACTOR_123200_COMMAND_START_COMBAT:
                work->state = ACTOR_123200_STATE_HIDDEN;
                break;
            case ACTOR_123200_COMMAND_NONE:
                break;
        }
    }
    return 0;
}

#include "../../shared/actor_messages_place.inc.c"

#include "../../shared/coord_math_yaw_scale.inc.c"

/// Hides the staged model and excludes lock-on when its hidden state is entered.
///
/// Requires a live enemy, model and initialized work; later hidden ticks do nothing.
static void _actor123200Hide(Enemy* enemy, Task* task)
{
    TmdObject*        model;
    _Actor123200Work* work;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

/// Dispatches the staged bone suckler's spawn, update or teardown task state.
///
/// The task must own a live model and borrow its enemy in spawn argument 2.
/// State must be 0 (spawn), 1 (update), or 2 (teardown); there is no bounds check.
/// Spawn allocates the work block, update requires it, and teardown releases
/// the task and enemy, so neither may be used after that callback returns.
static void _actor123200Task(Task* task)
{
    EnemyTaskFuncTable3 taskHandlers;

    taskHandlers = D_actor_123200_80131E30;
    taskHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}
