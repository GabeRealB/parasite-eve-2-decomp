#include "actors/actor_223600.h"

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
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
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
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"
#include "../../shared/actor/actor_messages.h"
#include "../../shared/actor/anim_driver.h"

/// Values of `_Actor223600Work::state`: the index of the handler the per-frame
/// tick runs.
///
/// Animation numbers are indices into the package's animation-set table, and a
/// placement is the enemy's placement index, the part of `Enemy::placeKey`
/// above `ENEMY_PLACE_INDEX_SHIFT`. No state ends of its own accord: the
/// model-draw message selects `WALK` or `HIDDEN`, and the
/// `ACTOR_223600_COMMAND_*` actor commands select `DROP_IN` or `HIDDEN`.
enum {
    ACTOR_223600_STATE_HIDDEN  = 0, // not drawn, not lockable
    ACTOR_223600_STATE_WALK    = 1, // from a start point fixed per placement (0 and 1; others stay where they are), walks on animation 2 toward `walkTarget`, turning at most 0x10 a tick
    ACTOR_223600_STATE_DROP_IN = 2  // from a point above the floor fixed per placement, falls to the floor on animation 14 and plays the landing on animation 15
};

/// `ActorCommand::context.key` of the commands this enemy acts on.
///
/// Its room, Dryfield's general store, broadcasts its commands to every actor
/// under its own stage and area. The enemy records a command of any namespace
/// and acts on this one alone.
enum {
    ACTOR_223600_COMMAND_CONTEXT = GAME_STAGE_DRYFIELD | (GAME_AREA_DRYFIELD_GENERAL_STORE << 8)
};

/// `ActorCommand::command` selectors of that namespace, as this enemy answers them.
enum {
    ACTOR_223600_COMMAND_IGNORED             = 0, // a command of the room's that this enemy accepts and does nothing with
    ACTOR_223600_COMMAND_DROP_IN             = 1, // selects `ACTOR_223600_STATE_DROP_IN`
    ACTOR_223600_COMMAND_HIDE                = 2, // follows `DROP_IN` 0x5A frames later; selects `ACTOR_223600_STATE_HIDDEN`
    ACTOR_223600_COMMAND_HIDE_UNTIL_CUTSCENE = 9  // sent when the room starts with its cutscene still to play; selects `ACTOR_223600_STATE_HIDDEN`
};

/// Work block of the package's enemy task.
///
/// The spawn handler allocates it zeroed and keeps it at `Task::work`. It
/// holds the state machine, the animation rig and its driver's state, the
/// point the walk heads for, and storage for the model's matrices.
///
/// Everything up to and including `driver` is laid out as `AnimDriverWork`,
/// through which the shared animation driver reaches the block: its
/// `carrierState` bytes are the five words and the gap ahead of `rig` here. A
/// cue index is the low ten bits of the record a slot's current pose names
/// (`ANIMATION_POSE_CUE_INDEX_MASK`). World Y grows downward, so a fall adds
/// to it.
typedef struct {
    s16           state;        // `ACTOR_223600_STATE_*`
    s16           prevState;    // `state` the tick last ran; -1 from spawn, so the first tick enters `state` afresh
    s16           stateEntered; // 1 on the first tick of a state, when the handler sets itself up (0 otherwise)
    s16           stateFrame;   // tick counter of the running state: `WALK` counts up from 0 and never reads it; `DROP_IN` starts it at a value fixed per placement, times the drop by it and restarts it at 0 for the landing
    s16           field_8;      // cleared at spawn and never accessed again; role unproven
    byte          pad_A[2];     // never accessed
    ActorAnimRig6 rig;          // playback of the model's parts; slot 1's status and cue index time the sound cues
    struct {
        s16 state;              // `ANIM_DRIVER_STATE_*` (0 idle, 1 or 2 restart requested, 3 playing)
        s16 playingSet;         // animation the slots were last restarted on
        s16 requestedSet;       // animation the next restart plays; also what the sound cues and the phases of `DROP_IN` are keyed on
        s16 rate;               // playback rate in sixteenths of a frame per tick; from spawn 16 plus the placement index (odd placements) or minus half of it (even), until `DROP_IN` sets its own
        s16 rateBias;           // added to `rate`; always 0 here
        s16 tickCount;          // advancing ticks since the last restart
        s16 jumpCount;          // of those, ticks on which slot 1 followed a control jump: loops of a looping animation
    } driver;                   // the animation driver's state, member for member `AnimDriverWork`'s
    s16     field_17E;          // cleared at spawn and never accessed again; role unproven
    u8      lastCommandStage;   // stage tag of the last actor command received, whatever its namespace; never read
    u8      lastCommandArea;    // area tag of that command; never read
    u8      lastCommand;        // low byte of that command's selector; never read
    byte    pad_183[1];         // never accessed
    u16     field_184;          // 5 at spawn, offset by the placement index the way `driver.rate` is; never accessed again; role unproven
    u16     field_186;          // 20 at spawn, offset the same way; never accessed again; role unproven
    byte    pad_188[0xC];       // never accessed
    SVECTOR spawnPos;           // root position at spawn; never read
    SVECTOR walkTarget;         // point `WALK` heads for, set on entering it; only X and Z are used
    byte    pad_1A4[4];         // never accessed
    MATRIX  lightMtx;           // storage for the model's `TmdObject::lightMtx`
    MATRIX  colorMtx;           // storage for the model's `TmdObject::colorMtx`
    byte    pad_1E8[0x20];      // never accessed
    u16     lastSoundCueIndex;  // slot 1's cue index when the cue sound of animation 2 or 3 last fired, so a held cue sounds once; 0 on any other cue
    byte    pad_20A[2];         // never accessed
    s8      relightPending;     // 1 when the last tick left the root coordinate awaiting composition (it moved, or the view changed); the next tick then rebuilds `colorMtx` from the room lights (0 otherwise)
    byte    pad_20D[5];         // never accessed
    s16     fallSpeed;          // base downward step per tick of the fall in `DROP_IN`, fixed per placement on entry; each tick adjusts it by a term that grows with `stateFrame`
} _Actor223600Work;
STATIC_ASSERT_SIZEOF(_Actor223600Work, 0x214);
STATIC_ASSERT(OFFSET_OF(_Actor223600Work, rig) == OFFSET_OF(AnimDriverWork, rig), Actor223600Work_rig);
STATIC_ASSERT(OFFSET_OF(_Actor223600Work, driver) == OFFSET_OF(AnimDriverWork, state), Actor223600Work_driver);

/// The scratch-stack block of the drop-in state: one frame's step along one of
/// the model's own axes.
///
/// The state reserves one block a frame and reuses it for every step of that
/// frame. An axis of the root coordinate is read into `step`, normalised to
/// 4096 = 1.0 and scaled to the step's length in world units, and the result
/// is added to, or taken from, the coordinate's translation.
typedef struct {
    SVECTOR step;           // Axis of the root coordinate, then that axis scaled into the displacement applied to the translation
    byte    unknown_8[0x4]; // Reserved with the block and never accessed; role unproven
} _Actor223600AxisStepScratch;
STATIC_ASSERT_SIZEOF(_Actor223600AxisStepScratch, 0xC);

/// Integer part of the last movement step `func_actor_223600_8014AA04`
/// applied.
static SVECTOR ActorContact_ScratchPosition;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

static s32  _actor223600PollSoundCue(_Actor223600Work* work);
static void _actor223600Spawn(Enemy* enemy, Task* task);
static void _actor223600Walk(Enemy* enemy, Task* task);
static void _actor223600DropIn(Enemy* enemy, Task* task);
static void _actor223600Update(Enemy* enemy, Task* task);
static void _actor223600Hide(Enemy* enemy, Task* task);
static void _actor223600Task(Task* task);
static s32  _actor223600SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static s32  _actor223600ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);

/// Loaded animation-set indices used by this package's state handlers.
enum {
    ACTOR_223600_ANIM_REQUEST_INITIAL = 1,
    ACTOR_223600_ANIM_REQUEST_WALK    = 2,
    ACTOR_223600_ANIM_REQUEST_FALL    = 14,
    ACTOR_223600_ANIM_REQUEST_LAND    = 15,
};

/// Adds a signed displacement along a normalized root axis during the drop-in.
///
/// Captures the live task, scratchTop, axisStep and gteStep: both vector pointers
/// must alias scratchTop[-1].step in the reserved scratch block. readAxis is a
/// matrix-axis reader and stepDistance is in parent-coordinate units; each is
/// evaluated once. Normalization uses 4096 as one before GTE scaling. Includes
/// Y and bypasses the freeze gate; the caller invalidates the composed root.
/// Statement-only GNU expression; its value is not part of the interface.
#define ACTOR_223600_STEP_LOCAL_AXIS(readAxis, stepDistance)           \
    ({                                                                 \
        (readAxis)(&task->extra.tmd->coords->coord, axisStep);         \
        VectorNormalSS(axisStep, axisStep);                            \
        gte_lddp(stepDistance);                                        \
        gte_ldsv(gteStep);                                             \
        gte_gpf12();                                                   \
        gte_stsv(gteStep);                                             \
        task->extra.tmd->coords->coord.t[0] += scratchTop[-1].step.vx; \
        task->extra.tmd->coords->coord.t[1] += axisStep->vy;           \
        task->extra.tmd->coords->coord.t[2] += axisStep->vz;           \
    })

static TmdSource _gSucklerBloodBody;

#include "../../shared/actor/actor_contacts.h"

/// Blood Suckler attack row 0: base damage 24 with the spark-and-puff hit reaction.
///
/// The kind parameters borrow this single row for packed contact keys; it applies no status.
static DamageAttack _gActor223600Attacks[1] = {
    { 24, GAME_ACTOR_REACTION_SPARK_AND_PUFFS },
};

/// Shared combat parameters of this staged Blood Suckler kind.
///
/// Every spawned enemy borrows this record and its one attack row. Starts at
/// one hit point; release credits 6 experience, 20 battle points and 3 magic
/// points. Reaction chances are 100 percent, with zero buildup steps and damage
/// pulses; those zero counts do not expire their respective reactions.
static EnemyParams _gActor223600EnemyParams = {
    .attacks              = _gActor223600Attacks,
    .hpMax                = 1,
    .exp                  = 6,
    .bp                   = 20,
    .mp                   = 3,
    .critChance           = 100,
    .buildupSteps         = 0,
    .damageOverTimeChance = 100,
    .damageOverTimeTicks  = 0,
};

enum {
    ACTOR_223600_UNUSED_VIBRATION_HOLD  = 0,
    ACTOR_223600_UNUSED_VIBRATION_FULL  = 1,
    ACTOR_223600_UNUSED_VIBRATION_TAPER = 2,
};

/// Retained two-lane vibration sequence with no recovered spawn consumer.
///
/// The binary lane holds for nine eligible frames, then stops. The variable
/// lane holds full intensity for twelve frames and tapers over six, then stops.
/// Command and segment indices stay in 0..2. A spawn would borrow both arrays
/// until vibration-task teardown; this staged actor never starts the sequence.
static PadScriptCmd _gActor223600UnusedVibrationCommands[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, ACTOR_223600_UNUSED_VIBRATION_HOLD), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, ACTOR_223600_UNUSED_VIBRATION_FULL) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, ACTOR_223600_UNUSED_VIBRATION_TAPER) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

/// Retained nine-frame binary hold, twelve-frame full level and six-frame taper.
///
/// Intensities are bytes before attenuation. The variable taper starts at 100
/// and approaches 50; the interpreter posts no extra final endpoint. The last
/// byte of each record is preserved with no established interpreter meaning.
static PadScriptVibrationSegment _gActor223600UnusedVibrationSegments[3] = {
    [ACTOR_223600_UNUSED_VIBRATION_HOLD]  = { .startIntensity = 0, .endIntensity = 0, .durationFrames = 9, .field_3 = 0 },
    [ACTOR_223600_UNUSED_VIBRATION_FULL]  = { .startIntensity = 255, .endIntensity = 255, .durationFrames = 12, .field_3 = 1 },
    [ACTOR_223600_UNUSED_VIBRATION_TAPER] = { .startIntensity = 100, .endIntensity = 50, .durationFrames = 6, .field_3 = 1 },
};

#include "assets/enemies/suckler/blood/body.inc"

#include "assets/rooms/dryfield/general_store/blood_suckler_clips.inc"

/// Animation-set table for the staged Blood Suckler, indexed by requested set.
///
/// Loaded indices are 1..8 and 10..15: 1 initial, 2 walk, 14 fall and 15 land.
/// Indices 0, 9 and 16..25 are null slots, not terminators. The rig borrows
/// this table and its loaded sets until teardown; playback does not check bounds.
#include "assets/rooms/dryfield/general_store/bank.inc"

/// Retained image bytes with no recovered access or established record layout.
///
/// The 256-byte span contains sparse values 6 and 9 among zero bytes. Its
/// original object boundaries and purpose are unproven; preserve it in place.
u8 D_actor_223600_80150A28[256] = {
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

/// Message map for the staged Blood Suckler's drawing, commands and placement.
///
/// Drawing takes a scalar mode; commands borrow an `ActorCommand`, and placement
/// borrows an `ActorTransform`. The second payload is unused by all three.
/// The task borrows this terminated map and its callbacks until teardown.
static const TaskMessageEntry _gActor223600Messages[4] __attribute__((section(".data"))) = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor223600SetModelDraw },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor223600ApplyCommand },
    { ACTOR_MESSAGE_PLACE, actorMsgPlace },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor223600BloodSucklerTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_ACTOR } },
    .callback = _actor223600Task,
    .data     = { .model = &_gSucklerBloodBody },
};

static SVECTOR ActorContact_ScratchPosition = { 0 };

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

/// Effect spawn record overwritten with the most recently spawned model root.
///
/// Each spawn stores a borrowed coordinate and argument halves 0x100 and 1.
/// No reader or effect ID is established, so the halves' units and purpose are
/// unproven. The coordinate is not cleared when its owning task is destroyed.
static EffectSpawnArg _gActor223600EffectSpawnArg = { 0 };

#include "../../shared/actor/actor_contacts.h"

#include "../../shared/actor/actor_contacts.inc.c"

#include "../../shared/actor/anim_driver_tick.inc.c"

/// Selects a sound script from slot 1's animation cue, or returns zero.
///
/// WALK and set 3 emit bank 0x400C entry 1 once per held trigger cue; leaving
/// a trigger cue clears that latch. Set 5 emits entry 5 on a control-jump tick.
/// Other sets leave the latch unchanged. Requires initialized rig slot 1; the
/// returned id has a zero instance byte, which the update supplies.
static s32 _actor223600PollSoundCue(_Actor223600Work* work)
{
    enum {
        ACTOR_223600_SOUND_ANIM_CUE_PAIR     = 3,
        ACTOR_223600_SOUND_ANIM_CONTROL_JUMP = 5,
        ACTOR_223600_WALK_CUE_FIRST          = 0x11,
        ACTOR_223600_WALK_CUE_SECOND         = 0x15,
        ACTOR_223600_PAIR_CUE_FIRST          = 0xD,
        ACTOR_223600_PAIR_CUE_SECOND         = 0x12,
        ACTOR_223600_SOUND_CUE_POSE          = 0x400C0001,
        ACTOR_223600_SOUND_CUE_CONTROL_JUMP  = 0x400C0005,
    };
    u16 cueIndex;
    s32 promotedCueIndex;

    switch (work->driver.requestedSet) {
        case ACTOR_223600_ANIM_REQUEST_WALK:
            cueIndex         = work->rig.slots[ANIM_DRIVER_FIRST_SLOT].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            promotedCueIndex = cueIndex;
            if (promotedCueIndex != ACTOR_223600_WALK_CUE_SECOND) {
                goto checkOtherWalkCue;
            }
        latchCue:
            if (work->lastSoundCueIndex == promotedCueIndex) {
                goto heldCue;
            }
            work->lastSoundCueIndex = cueIndex;
            return ACTOR_223600_SOUND_CUE_POSE;
        checkOtherWalkCue:
            if (promotedCueIndex == ACTOR_223600_WALK_CUE_FIRST) {
                goto latchCue;
            }
        clearCueLatch:
            work->lastSoundCueIndex = 0;
            break;
        case ACTOR_223600_SOUND_ANIM_CUE_PAIR:
            cueIndex         = work->rig.slots[ANIM_DRIVER_FIRST_SLOT].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            promotedCueIndex = cueIndex;
            if (promotedCueIndex != ACTOR_223600_PAIR_CUE_FIRST && promotedCueIndex != ACTOR_223600_PAIR_CUE_SECOND) {
                goto clearCueLatch;
            }
            goto latchCue;
        heldCue:
            work->lastSoundCueIndex = cueIndex;
            break;
        case ACTOR_223600_SOUND_ANIM_CONTROL_JUMP:
            if (work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
                return ACTOR_223600_SOUND_CUE_CONTROL_JUMP;
            }
            break;
    }
    return 0;
}

/// Initializes this placement's enemy model, animation rig and owned task work.
///
/// Requires a live TMD model with coordinates 0..5 and a live enemy in the
/// task's second spawn argument. Allocation failure destroys the task and enemy.
/// On success, the task owns the work and lighting matrices until teardown;
/// the rig borrows the loaded animation table and the root borrows the view
/// coordinate. Starts hidden, publishes the effect anchor and advances task state.
static void _actor223600Spawn(Enemy* enemy, Task* task)
{
    enum { ACTOR_223600_PREVIOUS_STATE_UNSET = -1 };
    SVECTOR           initialFacingStep;
    _Actor223600Work* work;
    TmdObject*        model;
    GfxCoord*         coord;
    u32               placementIndex;
    u32               placementIsOdd;
    s32               hitPoints;

    // The task owns the work allocation; allocation failure destroys the instance.
    model      = task->extra.tmd;
    coord      = model->coords;
    work       = memCalloc(sizeof(_Actor223600Work), false);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    coord->parent  = &gGfxViewCoord;
    task->msgTable = _gActor223600Messages;
    model->flags   = 0;
    animationInitContext(&work->rig.anim, _gActor223600AnimationSets, model, work->rig.poses, work->rig.slots);

    enemy->field_4    = &coord->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &task->extra.tmd->coords[2];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->hpMax                  = 1;
    enemy->hp                     = 1;
    enemy->reactionFlags          = 0;
    hitPoints                     = _gActor223600EnemyParams.hpMax;
    enemy->param                  = &_gActor223600EnemyParams;
    enemy->recs                   = 0;
    enemy->hpMax                  = hitPoints;
    enemy->hp                     = hitPoints;

    work->driver.requestedSet = ACTOR_223600_ANIM_REQUEST_INITIAL;
    work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
    work->driver.rate         = ANIMATION_RATE_ONE;
    work->driver.rateBias     = 0;
    _animDriverTick(task);
    work->field_17E     = 0;
    work->field_8       = 0;
    model->lightMtx     = &work->lightMtx;
    model->colorMtx     = &work->colorMtx;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_184     = 5;
    work->field_186     = 0x14;

    // Odd placements speed up playback; even placements subtract half their index.
    placementIndex = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    placementIsOdd = placementIndex & 1;
    if (placementIsOdd == 1) {
        work->driver.rate += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_186   += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        work->field_184   += enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    } else {
        work->driver.rate -= placementIndex >> 1;
        work->field_186   -= (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
        work->field_184   -= (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) / 2;
    }

    work->spawnPos.vx = task->extra.tmd->coords->coord.t[0];
    work->spawnPos.vy = task->extra.tmd->coords->coord.t[1];
    work->spawnPos.vz = task->extra.tmd->coords->coord.t[2];

    // Keep the initial horizontal-facing calculation: it also updates GTE state.
    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, &initialFacingStep);
    initialFacingStep.vy = 0;
    _actorMovementBuildDisplacement(&initialFacingStep, 0x3E8);

    work->state     = ACTOR_223600_STATE_HIDDEN;
    work->prevState = ACTOR_223600_PREVIOUS_STATE_UNSET;

    _gActor223600EffectSpawnArg.coord      = task->extra.tmd->coords;
    _gActor223600EffectSpawnArg.spawnArgLo = 0x100;
    _gActor223600EffectSpawnArg.spawnArgHi = 1;
    task->state++;
}

/// Turns the model root toward the walk waypoint by at most 16 units per tick.
///
/// `task` must have a live model with an initialized root rotation. The caller's
/// reserved `turnScratch` holds the waypoint's signed 16-bit X/Z offset from
/// the root in world units. The offset is unchanged and Y is ignored.
/// Angles use 4096 units per turn. The wrapped bearing difference is clamped to
/// [-16, 16], then `turnScratch->angle` holds the resulting absolute yaw,
/// without wrapping that sum again. Rebuilds a pure Y rotation and preserves
/// translation.
///
/// Requires 0x24 free bytes below the block on the initialized scratch stack
/// for the rotation routine. The caller releases the block and marks the root
/// `GRAPHICS_COORD_DIRTY`; no pointer is retained.
static inline void _actor223600TurnTowardWalkTarget(Task* task, ActorTurnScratch* turnScratch)
{
    enum { ACTOR_223600_WALK_TURN_LIMIT = 16 }; // 4096 angle units per turn
    GfxCoord* rootCoord;

    // Limit the signed turn before converting it back to an absolute heading.
    rootCoord          = task->extra.tmd->coords;
    turnScratch->angle = _actorAngleNormalizeYaw(ratan2(turnScratch->delta.vx, turnScratch->delta.vz) -
                                                 ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]));
    if (turnScratch->angle > ACTOR_223600_WALK_TURN_LIMIT) {
        turnScratch->angle = ACTOR_223600_WALK_TURN_LIMIT;
    }
    if (turnScratch->angle < -ACTOR_223600_WALK_TURN_LIMIT) {
        turnScratch->angle = -ACTOR_223600_WALK_TURN_LIMIT;
    }
    turnScratch->angle += ratan2(-task->extra.tmd->coords->coord.m[2][0],
                                 task->extra.tmd->coords->coord.m[2][2]);
    gfxRotMatrixY(&task->extra.tmd->coords->coord, turnScratch->angle, GRAPHICS_ROTATION_REPLACE);
}

/// Initializes or advances the hidden-to-walk state toward its fixed waypoint.
///
/// Requires initialized work and a live model. Placements 0 and 1 have fixed
/// start positions; other indices retain their position. Turns by at most
/// 16/4096 of a revolution and steps five parent-coordinate units per tick.
/// The movement freeze gate skips translation only; turning and animation continue.
/// Reserves one turn block, plus the nested movement helper's vector.
static void _actor223600Walk(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_223600_WALK_DISTANCE = 5, // Parent-coordinate units per tick
    };
    _Actor223600Work* work;
    ActorTurnScratch* scratchTop;
    ActorTurnScratch* turnScratch;
    TmdObject*        model;
    u32               placementIndex;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        work->walkTarget.vx = 0x115D;
        work->walkTarget.vy = 1;
        work->walkTarget.vz = 0x12D5;

        placementIndex = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        switch (placementIndex) {
            case 0:
                task->extra.tmd->coords->coord.t[0] = 0xA8C;
                task->extra.tmd->coords->coord.t[1] = 1;
                task->extra.tmd->coords->coord.t[2] = 0xA28;
                break;
            case 1:
                task->extra.tmd->coords->coord.t[0] = 0x384;
                task->extra.tmd->coords->coord.t[1] = placementIndex;
                task->extra.tmd->coords->coord.t[2] = 0x960;
                break;
        }
        gfxRotMatrixY(&task->extra.tmd->coords->coord, 0, GRAPHICS_ROTATION_REPLACE);
        work->driver.requestedSet = ACTOR_223600_ANIM_REQUEST_WALK;
        work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
        _animDriverTick(task);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->stateFrame                      = 0;
        return;
    }

    // Turn toward the fixed waypoint before stepping along the new facing.
    work->stateFrame++;
    scratchTop                             = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    scratchTop[-1].delta.vx                = work->walkTarget.vx - task->extra.tmd->coords->coord.t[0];
    SCRATCH_STACK_CURSOR(ActorTurnScratch) = scratchTop - 1;
    turnScratch                            = scratchTop - 1;
    turnScratch->delta.vy                  = 0;
    turnScratch->delta.vz                  = work->walkTarget.vz - task->extra.tmd->coords->coord.t[2];

    _actor223600TurnTowardWalkTarget(task, turnScratch);
    _actorMovementStepForward(task->extra.tmd->coords, ACTOR_223600_WALK_DISTANCE);
    _animDriverTick(task);
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Runs the placement-specific walk lead-in, fall and landing sequence.
///
/// Placements 0..2 have defined entry poses; other indices retain the existing
/// pose and animation. World Y grows downward: falling starts at frame 40 and
/// reaching positive Y selects the landing set with its frame counter reset.
/// Placement 0 has the longer landing sway. Landing uses root-axis steps and
/// rotates coordinate 1; its steps continue while actor translation is frozen.
/// Requires initialized work, coordinates 0..5, loaded animation sets and
/// scratch room for one axis-step block plus nested helper storage.
static void _actor223600DropIn(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_223600_DROP_WALK_START_FRAME  = 19,
        ACTOR_223600_DROP_APPROACH_FRAME    = 35,
        ACTOR_223600_DROP_FALL_START_FRAME  = 40,
        ACTOR_223600_DROP_WALK_END_FRAME    = 50,
        ACTOR_223600_DROP_STEP_CYCLE_FRAMES = 5,
        ACTOR_223600_DROP_SLOW_STEP_FRAMES  = 2,
        ACTOR_223600_LAND_FORWARD_END_FRAME = 11,
        ACTOR_223600_LAND_LIFT_START_FRAME  = 5,
        ACTOR_223600_LAND_SWAY_START_FRAME  = 14,
        ACTOR_223600_LAND_SWAY_BIAS_FRAME   = 15,
        ACTOR_223600_LAND_SHORT_END_FRAME   = 30,
        ACTOR_223600_LAND_LONG_END_FRAME    = 37,
    };
    _Actor223600Work*            work;
    void**                       reserveCursorSlot;
    void**                       releaseCursorSlot;
    _Actor223600AxisStepScratch* scratchTop;
    SVECTOR*                     axisStep;
    SVECTOR*                     gteStep;
    TmdObject*                   model;
    s32                          placementIndex;
    s16                          fallFrame;
    u16                          fallStepPhase;
    s16                          swayFrame;

    work = task->work;
    // Stage each placement above the floor; placement 2 starts partway through its fall.
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = 0;
        tmdAllocPrimitiveBuffer(model);
        placementIndex = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        switch (placementIndex) {
            case 0:
                task->extra.tmd->coords->coord.t[0] = 0xA1E;
                task->extra.tmd->coords->coord.t[1] = -0x384;
                task->extra.tmd->coords->coord.t[2] = 0x1590;
                gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x7D0, GRAPHICS_ROTATION_REPLACE);
                work->stateFrame          = -0xA;
                work->fallSpeed           = 0xB4;
                work->driver.requestedSet = ACTOR_223600_ANIM_REQUEST_WALK;
                work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
                break;
            case 1:
                task->extra.tmd->coords->coord.t[0] = 0x12C;
                task->extra.tmd->coords->coord.t[1] = -0x4C4;
                task->extra.tmd->coords->coord.t[2] = 0x1194;
                gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x3E8, GRAPHICS_ROTATION_REPLACE);
                work->fallSpeed           = 0xBE;
                work->stateFrame          = 0;
                work->driver.requestedSet = ACTOR_223600_ANIM_REQUEST_WALK;
                work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
                break;
            case 2:
                task->extra.tmd->coords->coord.t[0] = 0x104A;
                task->extra.tmd->coords->coord.t[1] = -0x384;
                task->extra.tmd->coords->coord.t[2] = 0xFE6;
                gfxRotMatrixY(&task->extra.tmd->coords->coord, -0x400, GRAPHICS_ROTATION_REPLACE);
                _actorMovementStepForward(task->extra.tmd->coords, 0x15E);
                work->stateFrame          = 0x3C;
                work->fallSpeed           = 0x50;
                work->driver.rate         = 4 * ANIMATION_RATE_ONE;
                work->driver.requestedSet = ACTOR_223600_ANIM_REQUEST_FALL;
                work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
                do {
                    _animDriverTick(task);
                } while ((work->rig.slots[ANIM_DRIVER_FIRST_SLOT].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) == 0);
                work->driver.rate = ANIMATION_RATE_ONE;
                work->fallSpeed   = 0x46;
                break;
        }
        _animDriverTick(task);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        return;
    }

    // The walk is a lead-in; landing restarts this counter for the axis-driven finish.
    if (work->stateFrame == 0) {
        if (work->driver.requestedSet == ACTOR_223600_ANIM_REQUEST_WALK) {
            work->driver.requestedSet = ACTOR_223600_ANIM_REQUEST_FALL;
            work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
        }
    }
    _animDriverTick(task);

    reserveCursorSlot                                                       = SCRATCH_STACK_CURSOR_SLOT;
    scratchTop                                                              = SCRATCH_STACK_CURSOR_AT(reserveCursorSlot, _Actor223600AxisStepScratch);
    axisStep                                                                = &scratchTop[-1].step;
    gteStep                                                                 = &scratchTop[-1].step;
    SCRATCH_STACK_CURSOR_AT(reserveCursorSlot, _Actor223600AxisStepScratch) = scratchTop - 1;

    switch (work->driver.requestedSet) {
        case ACTOR_223600_ANIM_REQUEST_FALL:
            work->driver.rate = ANIMATION_RATE_ONE;
            if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) != 2) {
                if (work->stateFrame < ACTOR_223600_DROP_WALK_END_FRAME) {
                    if (work->stateFrame >= ACTOR_223600_DROP_FALL_START_FRAME) {
                        _actorMovementStepForward(task->extra.tmd->coords, 0x16);
                    } else if (work->stateFrame >= ACTOR_223600_DROP_APPROACH_FRAME) {
                        _actorMovementStepForward(task->extra.tmd->coords, 0xA);
                    } else if (work->stateFrame >= ACTOR_223600_DROP_WALK_START_FRAME) {
                        _actorMovementStepForward(task->extra.tmd->coords, 0xA);
                    }
                }
            }
            fallFrame = work->stateFrame;
            if (fallFrame >= ACTOR_223600_DROP_FALL_START_FRAME) {
                fallStepPhase = fallFrame % ACTOR_223600_DROP_STEP_CYCLE_FRAMES;
                if (fallStepPhase < ACTOR_223600_DROP_SLOW_STEP_FRAMES) {
                    task->extra.tmd->coords->coord.t[1] += work->fallSpeed - (fallFrame - ACTOR_223600_DROP_FALL_START_FRAME) / 4;
                } else {
                    task->extra.tmd->coords->coord.t[1] += work->fallSpeed + (fallFrame - ACTOR_223600_DROP_FALL_START_FRAME) / 2;
                }
                if (task->extra.tmd->coords->coord.t[1] >= 2) {
                    task->extra.tmd->coords->coord.t[1] = 1;
                }
            }
            if (task->extra.tmd->coords->coord.t[1] > 0) {
                work->driver.requestedSet = ACTOR_223600_ANIM_REQUEST_LAND;
                work->driver.state        = ANIM_DRIVER_STATE_RESTART_2;
                work->stateFrame          = 0;
            }
            break;
        case ACTOR_223600_ANIM_REQUEST_LAND:
            // Landing steps use the root axes without the actor-freeze gate.
            work->driver.rate = ANIMATION_RATE_ONE;
            if (work->stateFrame < ACTOR_223600_LAND_FORWARD_END_FRAME) {
                ACTOR_223600_STEP_LOCAL_AXIS(gfxReadMatrixZAxis, 0x23);
            }
            if (work->stateFrame >= ACTOR_223600_LAND_LIFT_START_FRAME && work->stateFrame < ACTOR_223600_LAND_SWAY_START_FRAME) {
                ACTOR_223600_STEP_LOCAL_AXIS(gfxReadMatrixYAxis, -0x14);
                gfxRotMatrixX(&task->extra.tmd->coords[1].coord,
                              -((work->stateFrame - 4) * 0xCC), GRAPHICS_ROTATION_COMPOSE);
            }
            if ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == 0) {
                if (work->stateFrame >= ACTOR_223600_LAND_SWAY_START_FRAME && work->stateFrame < ACTOR_223600_LAND_LONG_END_FRAME) {
                    ACTOR_223600_STEP_LOCAL_AXIS(gfxReadMatrixYAxis, 0xB);
                    gfxReadMatrixXAxis(&task->extra.tmd->coords->coord, axisStep);
                    VectorNormalSS(axisStep, axisStep);
                    gte_lddp(-0x1D);
                    gte_ldsv(gteStep);
                    gte_gpf12();
                    gte_stsv(gteStep);
                    swayFrame = work->stateFrame - ACTOR_223600_LAND_SWAY_BIAS_FRAME;
                    switch (swayFrame) {
                        case 0:
                        case 1:
                        case 3:
                        case 4:
                        case 5:
                        case 7:
                        case 9:
                            task->extra.tmd->coords->coord.t[0] -= gteStep->vx;
                            task->extra.tmd->coords->coord.t[1] -= gteStep->vy;
                            task->extra.tmd->coords->coord.t[2] -= gteStep->vz;
                            gfxRotMatrixX(&task->extra.tmd->coords[1].coord, -ACTOR_TRANSFORM_ANGLE_HALF_TURN, GRAPHICS_ROTATION_COMPOSE);
                            gfxRotMatrixZ(&task->extra.tmd->coords[1].coord,
                                          (work->stateFrame - 0xD) * 0x55 - 0x6E, GRAPHICS_ROTATION_COMPOSE);
                            break;
                        default:
                            task->extra.tmd->coords->coord.t[0] += gteStep->vx;
                            task->extra.tmd->coords->coord.t[1] += gteStep->vy;
                            task->extra.tmd->coords->coord.t[2] += gteStep->vz;
                            gfxRotMatrixX(&task->extra.tmd->coords[1].coord, -ACTOR_TRANSFORM_ANGLE_HALF_TURN, GRAPHICS_ROTATION_COMPOSE);
                            gfxRotMatrixZ(&task->extra.tmd->coords[1].coord,
                                          (work->stateFrame - 0xD) * 0x55, GRAPHICS_ROTATION_COMPOSE);
                            break;
                    }
                }
                if (work->stateFrame >= ACTOR_223600_LAND_LONG_END_FRAME) {
                    gfxRotMatrixX(&task->extra.tmd->coords[1].coord, -ACTOR_TRANSFORM_ANGLE_HALF_TURN, GRAPHICS_ROTATION_COMPOSE);
                    gfxRotMatrixZ(&task->extra.tmd->coords[1].coord, ACTOR_TRANSFORM_ANGLE_HALF_TURN, GRAPHICS_ROTATION_COMPOSE);
                }
            } else {
                if (work->stateFrame >= ACTOR_223600_LAND_SWAY_START_FRAME && work->stateFrame < ACTOR_223600_LAND_SHORT_END_FRAME) {
                    ACTOR_223600_STEP_LOCAL_AXIS(gfxReadMatrixYAxis, 0xB);
                    ACTOR_223600_STEP_LOCAL_AXIS(gfxReadMatrixXAxis, -0x1D);
                    gfxRotMatrixX(&task->extra.tmd->coords[1].coord, -ACTOR_TRANSFORM_ANGLE_HALF_TURN, GRAPHICS_ROTATION_COMPOSE);
                    gfxRotMatrixZ(&task->extra.tmd->coords[1].coord,
                                  (work->stateFrame - 0xD) * 0x78, GRAPHICS_ROTATION_COMPOSE);
                }
                if (work->stateFrame >= ACTOR_223600_LAND_SHORT_END_FRAME) {
                    gfxRotMatrixX(&task->extra.tmd->coords[1].coord, -ACTOR_TRANSFORM_ANGLE_HALF_TURN, GRAPHICS_ROTATION_COMPOSE);
                    gfxRotMatrixZ(&task->extra.tmd->coords[1].coord, ACTOR_TRANSFORM_ANGLE_HALF_TURN, GRAPHICS_ROTATION_COMPOSE);
                }
            }
            break;
        case ACTOR_223600_ANIM_REQUEST_WALK:
            work->driver.rate = 2 * ANIMATION_RATE_ONE;
            break;
    }
    // Both invalidations are retained from the original landing-state tail.
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    releaseCursorSlot                     = SCRATCH_STACK_CURSOR_SLOT;
    SCRATCH_STACK_RELEASE_BLOCK_AT(releaseCursorSlot, _Actor223600AxisStepScratch);
    work->stateFrame++;
}

/// Behavior-state callbacks for the staged Blood Suckler.
///
/// `_Actor223600Work::state` selects 0 hidden, 1 walk or 2 drop in
/// (`ACTOR_223600_STATE_*`). Each entry receives the live enemy and its task.
static const EnemyTaskFuncTable3 _gActor223600BehaviorStates = {
    {
        _actor223600Hide,
        _actor223600Walk,
        _actor223600DropIn,
    },
};

/// Advances the enemy state, requests spatial sound cues and refreshes lighting.
///
/// Requires initialized work with state HIDDEN, WALK or DROP_IN and a live
/// model whose composed matrix supplies sound positioning and lighting. Paused
/// and hidden combat-control modes return before state, sound or lighting work.
/// The placement index supplies the sound instance byte. A dirty coordinate or
/// view change schedules lighting from the next tick's cached translation.
static void _actor223600Update(Enemy* enemy, Task* task)
{
    enum { ACTOR_223600_SOUND_INSTANCE_SHIFT = 8 };
    _Actor223600Work*   work;
    EnemyTaskFuncTable3 stateHandlers;
    s32                 soundCue;
    s32                 soundId;
    s32                 panOffset;

    work          = task->work;
    stateHandlers = _gActor223600BehaviorStates;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            if (work->state != ACTOR_223600_STATE_HIDDEN) {
                task->extra.tmd->flags = 0;
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            if (work->state != ACTOR_223600_STATE_HIDDEN) {
                task->extra.tmd->flags = 0;
            }
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }

    // Publish entry before dispatch so a new state initializes exactly once.
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = work->state;
    stateHandlers.funcs[work->state](enemy, task);

    soundCue = _actor223600PollSoundCue(work);
    if (soundCue != 0) {
        soundId   = soundCue | (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACTOR_223600_SOUND_INSTANCE_SHIFT);
        panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(
            soundId, panOffset,
            (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    // Lighting samples the cached translation before this frame's root is composed.
    if (work->relightPending != 0) {
        worldCoordSetModelLighting(task->extra.tmd,
                                   task->extra.tmd->coords->workm.t, 0, 3);
    }
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (task->extra.tmd->coords->composeStamp == GRAPHICS_COORD_DIRTY) {
        work->relightPending = 1;
        return;
    }
    work->relightPending = 0;
}

enum {
    ACTOR_223600_BLOOD_SUCKLER_TASK_SPAWN   = 0,
    ACTOR_223600_BLOOD_SUCKLER_TASK_UPDATE  = 1,
    ACTOR_223600_BLOOD_SUCKLER_TASK_DESTROY = 2,
};

/// Blood Suckler lifecycle handlers in `Task::state` order: 0 spawn, 1 update, 2 destroy.
///
/// Copied by value and indexed by `Task::state` in 0..2, without a terminator
/// or bounds check. Callbacks require a live task; update requires setup
/// work, and teardown may release both the task and its owned `Enemy`.
/// Each callback receives the owned `Enemy` from `Task::spawnArg2` and the task.
static const EnemyTaskFuncTable3 _gActor223600BloodSucklerTaskStates = {
    .funcs = {
        [ACTOR_223600_BLOOD_SUCKLER_TASK_SPAWN]   = _actor223600Spawn,
        [ACTOR_223600_BLOOD_SUCKLER_TASK_UPDATE]  = _actor223600Update,
        [ACTOR_223600_BLOOD_SUCKLER_TASK_DESTROY] = enemyDestroy,
    },
};

/// Applies a model-draw message and selects the walk or hidden actor state.
///
/// Requires a live TMD model and initialized work. HIDE and SHOW select WALK
/// and allocate a primitive buffer if missing; their active-draw flag differs.
/// The two SKIP_AUTO_BUFFER modes select HIDDEN, preserving other flags or
/// clearing them first. Other modes change nothing. Ignores the message ID
/// and second argument; returns zero and retains no payload.
static s32 _actor223600SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    TmdObject*        model = task->extra.tmd;
    _Actor223600Work* work  = task->work;

    switch (drawMode) {
        case ACTOR_MESSAGE_VISIBILITY_HIDE:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            work->state = ACTOR_223600_STATE_WALK;
            break;
        case ACTOR_MESSAGE_VISIBILITY_SHOW:
            model->flags = 0;
            tmdAllocPrimitiveBuffer(model);
            work->state = ACTOR_223600_STATE_WALK;
            break;
        case ACTOR_MESSAGE_VISIBILITY_KEEP_FLAGS_SKIP_AUTO_BUFFER:
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            work->state   = ACTOR_223600_STATE_HIDDEN;
            break;
        case ACTOR_MESSAGE_VISIBILITY_CLEAR_FLAGS_SKIP_AUTO_BUFFER:
            model->flags  = 0;
            work->state   = ACTOR_223600_STATE_HIDDEN;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
    }
    return 0;
}

/// Records an actor command and applies this room's drop-in or hide selector.
///
/// Requires initialized work and a readable four-byte command through dispatch.
/// Records stage, area and the selector's low byte for every namespace, but
/// changes state only for `ACTOR_223600_COMMAND_CONTEXT` and its known selectors.
/// Ignores the message ID and second argument; returns zero and retains no pointer.
static s32 _actor223600ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    _Actor223600Work* work;

    work                   = task->work;
    work->lastCommandStage = command->context.loc.stage;
    work->lastCommandArea  = command->context.loc.area;
    work->lastCommand      = command->command;
    if (command->context.key == ACTOR_223600_COMMAND_CONTEXT) {
        switch (command->command) {
            case ACTOR_223600_COMMAND_HIDE_UNTIL_CUTSCENE:
                work->state = ACTOR_223600_STATE_HIDDEN;
                break;
            case ACTOR_223600_COMMAND_DROP_IN:
                work->state = ACTOR_223600_STATE_DROP_IN;
                break;
            case ACTOR_223600_COMMAND_HIDE:
                work->state = ACTOR_223600_STATE_HIDDEN;
                break;
            case ACTOR_223600_COMMAND_IGNORED:
                break;
        }
    }
    return 0;
}

#include "../../shared/actor/actor_messages_place.inc.c"

#include "../../shared/actor/coord_math_yaw_scale.inc.c"

/// Hides the model and disables lock-on on the hidden state's entry tick.
///
/// Requires initialized work, a live enemy and its TMD model; later ticks do nothing.
static void _actor223600Hide(Enemy* enemy, Task* task)
{
    _Actor223600Work* work;
    TmdObject*        model;

    work = task->work;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

/// Dispatches this enemy's spawn, update or teardown task state.
///
/// Requires task state 0..2 and a live enemy in the second spawn argument.
/// Spawn creates task-owned work, update requires it and teardown releases it.
/// The table is copied by value; the actor overlay must remain loaded for the call.
/// Spawn failure and teardown destroy the task and enemy before returning.
static void _actor223600Task(Task* task)
{
    EnemyTaskFuncTable3 taskHandlers;

    taskHandlers = _gActor223600BloodSucklerTaskStates;
    taskHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}
