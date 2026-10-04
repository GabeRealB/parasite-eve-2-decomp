#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "../../shared/actor_motion.h"
#include "../../shared/actor_messages.h"

/// Work block of the overlay's actor, allocated zeroed by its spawn state and
/// kept at `Task::work` for the task's life.
///
/// It opens with the head `actorMotionPlayAnim19` runs on
/// (`ActorMotion19PlayWork`) and keeps a scripted walker's walk state
/// directly after it, so the room script plays the actor's clips and sends it
/// to a placement with the same messages as any scripted walker. The model
/// object borrows `model.light` and `model.color` for as long as the block
/// lives.
///
/// What a walk request starts here is a leap rather than a walk: the actor
/// turns to the placement's yaw, launches forward and up along its own axes,
/// falls under gravity while `airborne` until it is back near the floor, and
/// then turns to face the player. The placement's position is recorded in
/// `walk.target` and never read, and nothing measures an arrival.
///
/// What follows `walk` is the package's own: the leap's gravity switch, the
/// weight of the head turn toward the player, and the delayed free of the
/// model's buffers once the model has been hidden.
typedef struct {
    ActorAnimRig19  rig;              // Playback storage of the nineteen-part body model; slots 1 to 18 are driven
    ActorModelState model;            // Clip and bank the rig plays, and the matrices the model is lit with
    ActorWalkState  walk;             // Leap in progress: the yaw it launches along, the per-frame velocity and the step; `target` and `lastDistance` are not read
    s8              airborne;         // The leap is in flight, so each tick adds gravity to `walk.velocity.vy` (0 on the floor, 1 from launch to landing)
    s8              turnWeightRising; // Direction `turnWeight` ramps, set by an actor command (0 falls by 0x80 a tick, 1 rises by 0x40)
    s16             turnWeight;       // Weight handed to the per-frame head turn toward the player, 0 to `ONE`; the package's turn takes it and does not read it
    s16             freeCountdown;    // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
} _Actor317000Work;
STATIC_ASSERT_SIZEOF(_Actor317000Work, 0x4CC);

/// Indexed by `_Actor317000Work::model.bank` for `animationInitContext`'s second
/// argument by `func_actor_317000_80162458` and `actorMotionPlayAnim19`.
/// Every preset the actor builds has `field_0` 0, so only the first word is
/// ever read; the words after it (among them the address of
/// `func_actor_317000_80162624`) suggest a larger record, not a bank array.
extern AnimationSet*  D_actor_317000_8016CF1C[9];
extern AnimationSet** gActorMotionAnimBanks19[1];

/// `taskMessageDispatch` handler table installed at `Task::msgTable` by
/// `func_actor_317000_8016267C`; terminator id `TASK_MESSAGE_TABLE_END`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_317000_8016CF50[];

static void func_actor_317000_80161E68(Task* task);
static void func_actor_317000_801620BC(Task* task);
static void func_actor_317000_801621F4(Task* task, Task* targetTask, s32 arg2, s32 arg3, s32 arg4);
static void func_actor_317000_8016267C(Task* arg0);
static void func_actor_317000_80162724(Task* arg0);
static void func_actor_317000_80162744(Task* arg0);
static void func_actor_317000_80162760(Task* arg0);
static void func_actor_317000_80162768(Task* arg0);
static void func_actor_317000_801627D0(Task* arg0);
static void func_actor_317000_801628D8(Task* task);
static void func_actor_317000_80162950(Task* arg0);
s32         func_actor_317000_80162BC4(Task* task, s32 arg1, s32 mode, s32 arg3);

/// The actor's three task states, which `func_actor_317000_80162624` runs by
/// `Task::state`: spawn, per-frame tick and exit.
static const TaskFuncTable3 D_actor_317000_80161E24 = { {
    func_actor_317000_8016267C,
    func_actor_317000_80161E68,
    func_actor_317000_80162724,
} };

/// The four step handlers `func_actor_317000_80162768` runs by
/// `_Actor317000Work::walk.motionStep`.
static const TaskFuncTable4 D_actor_317000_80161E30 = { {
    func_actor_317000_801627D0,
    func_actor_317000_801628D8,
    func_actor_317000_80162950,
    func_actor_317000_801620BC,
} };

/// The constant local-space offset `func_actor_317000_801628D8` rotates
/// through the root coordinate into `_Actor317000Work::walk.velocity`.
static const VECTOR D_actor_317000_80161E40 = { 0, 0xFF800000, 0x400000, 0 };

static TmdSource _gActor317000GrinningStrangerBody;
s32              func_actor_317000_80162458(Task* task, s32 msgId, ActorTransform* place, ActorMotionWalkAnim*);
s32              func_actor_317000_80162BC4(Task*, s32, s32, s32);
s32              func_actor_317000_80162CA0(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
void             func_actor_317000_80162624(Task*);

static TmdBone _gActor317000GrinningStrangerBodySkeleton[19] = {
#include "assets/grinning_stranger_body_skeleton.inc"
};

static u32 _gActor317000GrinningStrangerBodyPartVerts[19] = {
#include "assets/grinning_stranger_body_partVerts.inc"
};

static SVECTOR _gActor317000GrinningStrangerBodyVerts[306] = {
#include "assets/grinning_stranger_body_verts.inc"
};

static SVECTOR _gActor317000GrinningStrangerBodyNormals[365] = {
#include "assets/grinning_stranger_body_normals.inc"
};

static u32 _gActor317000GrinningStrangerBodyStream[3988] = {
#include "assets/grinning_stranger_body_stream.inc"
};

static TmdSource _gActor317000GrinningStrangerBody = {
    0,
    20032,
    7672,
    19,
    _gActor317000GrinningStrangerBodyPartVerts,
    _gActor317000GrinningStrangerBodyVerts,
    _gActor317000GrinningStrangerBodyNormals,
    _gActor317000GrinningStrangerBodySkeleton,
    _gActor317000GrinningStrangerBodyStream,
};

static AnimationPackedPose _gActor317000Animation0699CBank1[4] = {
#include "assets/actor_317000_animation_0699C_bank1.inc"
};

static AnimationPackedRotation _gActor317000Animation0699CBank4[69] = {
#include "assets/actor_317000_animation_0699C_bank4.inc"
};

static AnimationRecord _gActor317000Animation0699CRecords[162] = {
#include "assets/actor_317000_animation_0699C_records.inc"
};

static u16 _gActor317000Animation0699CIndices[20] = {
#include "assets/actor_317000_animation_0699C_indices.inc"
};

static AnimationSet _gActor317000Animation0699C = {
    _gActor317000Animation0699CRecords,
    _gActor317000Animation0699CIndices,
    { NULL, _gActor317000Animation0699CBank1, NULL, NULL, _gActor317000Animation0699CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor317000Animation074C0Bank1[28] = {
#include "assets/actor_317000_animation_074C0_bank1.inc"
};

static AnimationPackedRotation _gActor317000Animation074C0Bank4[247] = {
#include "assets/actor_317000_animation_074C0_bank4.inc"
};

static AnimationRecord _gActor317000Animation074C0Records[362] = {
#include "assets/actor_317000_animation_074C0_records.inc"
};

static u16 _gActor317000Animation074C0Indices[20] = {
#include "assets/actor_317000_animation_074C0_indices.inc"
};

static AnimationSet _gActor317000Animation074C0 = {
    _gActor317000Animation074C0Records,
    _gActor317000Animation074C0Indices,
    { NULL, _gActor317000Animation074C0Bank1, NULL, NULL, _gActor317000Animation074C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor317000Animation07F44Bank1[32] = {
#include "assets/actor_317000_animation_07F44_bank1.inc"
};

static AnimationPackedRotation _gActor317000Animation07F44Bank4[223] = {
#include "assets/actor_317000_animation_07F44_bank4.inc"
};

static AnimationRecord _gActor317000Animation07F44Records[334] = {
#include "assets/actor_317000_animation_07F44_records.inc"
};

static u16 _gActor317000Animation07F44Indices[20] = {
#include "assets/actor_317000_animation_07F44_indices.inc"
};

static AnimationSet _gActor317000Animation07F44 = {
    _gActor317000Animation07F44Records,
    _gActor317000Animation07F44Indices,
    { NULL, _gActor317000Animation07F44Bank1, NULL, NULL, _gActor317000Animation07F44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor317000Animation08F08Bank1[29] = {
#include "assets/actor_317000_animation_08F08_bank1.inc"
};

static AnimationPackedRotation _gActor317000Animation08F08Bank4[374] = {
#include "assets/actor_317000_animation_08F08_bank4.inc"
};

static AnimationRecord _gActor317000Animation08F08Records[528] = {
#include "assets/actor_317000_animation_08F08_records.inc"
};

static u16 _gActor317000Animation08F08Indices[20] = {
#include "assets/actor_317000_animation_08F08_indices.inc"
};

static AnimationSet _gActor317000Animation08F08 = {
    _gActor317000Animation08F08Records,
    _gActor317000Animation08F08Indices,
    { NULL, _gActor317000Animation08F08Bank1, NULL, NULL, _gActor317000Animation08F08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor317000Animation09F30Bank1[27] = {
#include "assets/actor_317000_animation_09F30_bank1.inc"
};

static AnimationPackedRotation _gActor317000Animation09F30Bank4[406] = {
#include "assets/actor_317000_animation_09F30_bank4.inc"
};

static AnimationRecord _gActor317000Animation09F30Records[527] = {
#include "assets/actor_317000_animation_09F30_records.inc"
};

static u16 _gActor317000Animation09F30Indices[20] = {
#include "assets/actor_317000_animation_09F30_indices.inc"
};

static AnimationSet _gActor317000Animation09F30 = {
    _gActor317000Animation09F30Records,
    _gActor317000Animation09F30Indices,
    { NULL, _gActor317000Animation09F30Bank1, NULL, NULL, _gActor317000Animation09F30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor317000Animation0A338Bank1[6] = {
#include "assets/actor_317000_animation_0A338_bank1.inc"
};

static AnimationPackedRotation _gActor317000Animation0A338Bank4[70] = {
#include "assets/actor_317000_animation_0A338_bank4.inc"
};

static AnimationRecord _gActor317000Animation0A338Records[150] = {
#include "assets/actor_317000_animation_0A338_records.inc"
};

static u16 _gActor317000Animation0A338Indices[20] = {
#include "assets/actor_317000_animation_0A338_indices.inc"
};

static AnimationSet _gActor317000Animation0A338 = {
    _gActor317000Animation0A338Records,
    _gActor317000Animation0A338Indices,
    { NULL, _gActor317000Animation0A338Bank1, NULL, NULL, _gActor317000Animation0A338Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor317000Animation0A98CBank1[18] = {
#include "assets/actor_317000_animation_0A98C_bank1.inc"
};

static AnimationPackedRotation _gActor317000Animation0A98CBank4[146] = {
#include "assets/actor_317000_animation_0A98C_bank4.inc"
};

static AnimationRecord _gActor317000Animation0A98CRecords[185] = {
#include "assets/actor_317000_animation_0A98C_records.inc"
};

static u16 _gActor317000Animation0A98CIndices[20] = {
#include "assets/actor_317000_animation_0A98C_indices.inc"
};

static AnimationSet _gActor317000Animation0A98C = {
    _gActor317000Animation0A98CRecords,
    _gActor317000Animation0A98CIndices,
    { NULL, _gActor317000Animation0A98CBank1, NULL, NULL, _gActor317000Animation0A98CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor317000Animation0B0D4Bank1[13] = {
#include "assets/actor_317000_animation_0B0D4_bank1.inc"
};

static AnimationPackedRotation _gActor317000Animation0B0D4Bank4[180] = {
#include "assets/actor_317000_animation_0B0D4_bank4.inc"
};

static AnimationRecord _gActor317000Animation0B0D4Records[227] = {
#include "assets/actor_317000_animation_0B0D4_records.inc"
};

static u16 _gActor317000Animation0B0D4Indices[20] = {
#include "assets/actor_317000_animation_0B0D4_indices.inc"
};

static AnimationSet _gActor317000Animation0B0D4 = {
    _gActor317000Animation0B0D4Records,
    _gActor317000Animation0B0D4Indices,
    { NULL, _gActor317000Animation0B0D4Bank1, NULL, NULL, _gActor317000Animation0B0D4Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_317000_8016CF1C[9] = {
    NULL,
    &_gActor317000Animation0699C,
    &_gActor317000Animation074C0,
    &_gActor317000Animation07F44,
    &_gActor317000Animation08F08,
    &_gActor317000Animation09F30,
    &_gActor317000Animation0A338,
    &_gActor317000Animation0A98C,
    &_gActor317000Animation0B0D4,
};

AnimationSet** gActorMotionAnimBanks19[1] = {
    D_actor_317000_8016CF1C,
};

TaskDesc D_actor_317000_8016CF44 = { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_317000_80162624, { .model = &_gActor317000GrinningStrangerBody } };

TaskMessageEntry D_actor_317000_8016CF50[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, actorMotionPlayAnim19 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_317000_80162BC4 },
    { ACTOR_MESSAGE_WALK_TO, func_actor_317000_80162458 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_317000_80162CA0 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Per-frame tick. Runs the state body `_Actor317000Work::walk.motion` selects
/// from a two-entry stack table, then integrates the 16.16 position: `velocity` is
/// added to `walk.carry`, `velocity.vy` gains 0x120000 while `airborne` is raised, the
/// integer halves move the root coordinate and only the fractions are kept.
/// The animation slots tick, the second coordinate is refreshed while
/// `gGameSession->viewReady` is set, `turnWeight` ramps up by 0x40 to `ONE` or
/// down by 0x80 to 0 on `turnWeightRising`, and the aim body runs against slot 3.
/// A non-negative `freeCountdown` counts down and frees the model buffers at 0.
static void func_actor_317000_80161E68(Task* task)
{
    TmdObject*        ext                 = task->extra.tmd;
    _Actor317000Work* work                = task->work;
    void              (*states[2])(Task*) = { func_actor_317000_80162760, func_actor_317000_80162768 };
    GfxCoord*         coord;
    s32               i;

    states[work->walk.motion](task);

    coord                     = task->extra.tmd->coords;
    work->walk.carry[0].word += work->walk.velocity.vx;
    work->walk.carry[1].word += work->walk.velocity.vy;
    work->walk.carry[2].word += work->walk.velocity.vz;
    if (work->airborne != 0) {
        work->walk.velocity.vy += 0x120000;
    }
    coord->coord.t[0]       += work->walk.carry[0].halves.integer;
    coord->coord.t[1]       += work->walk.carry[1].halves.integer;
    coord->coord.t[2]       += work->walk.carry[2].halves.integer;
    coord->composeStamp      = GRAPHICS_COORD_DIRTY;
    work->walk.carry[0].word = work->walk.carry[0].halves.fraction;
    work->walk.carry[1].word = work->walk.carry[1].halves.fraction;
    work->walk.carry[2].word = work->walk.carry[2].halves.fraction;
    if (work->model.ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
    if (gGameSession->viewReady != 0) {
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        func_800D7A9C(ext, (VECTOR*)&task->extra.tmd->coords[1].workm.t, 0, 3);
    }
    if (work->turnWeightRising != 0) {
        work->turnWeight += 0x40;
        if (work->turnWeight > ONE) {
            work->turnWeight = ONE;
        }
    } else {
        work->turnWeight -= 0x80;
        if (work->turnWeight < 0) {
            work->turnWeight = 0;
        }
    }
    func_actor_317000_801621F4(task, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x400, 0x200, work->turnWeight);
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->freeCountdown--;
    }
}

/// Step handler at index 3 of `D_actor_317000_80161E30`. The actor's own coordinate and the `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)` task's
/// (the player) are normalised into `dir`, whose yaw `ratan2` takes over
/// `dir.vz`, and the result is written as the roll/pitch-free facing
/// `{ 0, yaw, 0 }` at `GfxCoord::param.rot`. The same yaw is then compared
/// against the yaw `gfxExtractSmallestEuler` reads back out of the node's own matrix:
/// when the two are within 0x40 (64 of 4096 units) the actor is facing its
/// target already, which clears the work's dispatch index and its companion
/// halfword; otherwise the matrix's yaw is stepped toward the target by that
/// same 0x40 and `RotMatrix` rebuilds the node from the adjusted angles.
static void func_actor_317000_801620BC(Task* task)
{
    _Actor317000Work* work;
    GfxCoord*         coord;
    GfxCoord*         target;
    VECTOR            delta;
    SVECTOR           dir;
    SVECTOR           rot;
    SVECTOR           ang;
    s16               diff;
    s32               absDiff;
    s32               y;

    coord  = task->extra.tmd->coords;
    target = ((gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd)->coords;
    work   = task->work;

    delta.vx = target->coord.t[0] - coord->coord.t[0];
    delta.vy = target->coord.t[1] - coord->coord.t[1];
    delta.vz = target->coord.t[2] - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;

    coord->param.rot.vx = rot.vx;
    coord->param.rot.vy = rot.vy;
    coord->param.rot.vz = rot.vz;

    gfxExtractSmallestEuler(&ang, &coord->coord);
    diff    = ratan2(dir.vx, dir.vz) - ang.vy;
    absDiff = abs(diff);
    if (absDiff >= 0x41) {
        y = ang.vy;
        if (diff < 0) {
            ang.vy = y - 0x40;
        } else {
            ang.vy = y + 0x40;
        }
    } else {
        work->walk.motion     = ACTOR_WALK_MOTION_IDLE;
        work->walk.motionStep = 0;
    }
    RotMatrix(&ang, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Aim body the per-frame tick `func_actor_317000_80161E68` calls with
/// `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)` (the player) as `targetTask`; it never reads the three
/// arguments after it (0x400, 0x200 and `_Actor317000Work::turnWeight`). The delta from the actor's sixth coordinate
/// (`coord[5]`) to the target's fifth is normalised, taken through the
/// transpose of the actor's third coordinate's `workm`, and normalised again
/// into `dir`. The three `ratan2`s reduce `dir` to an Euler triple -- the YZ,
/// XZ and XY plane angles, the middle one against the negated magnitude of
/// `dir.vz` -- which is added halfword-wise to the euler `gfxExtractSmallestEuler`
/// reads out of `coord[5]` before `RotMatrix` rebuilds it. An eighth of the
/// rebuilt euler then offsets `coord[3]`'s own euler, and `coord[5]` is
/// re-read and scaled by 5/8 before being rewritten as the identity rotated
/// by it.
///
/// `coord[5]` is kept as its own `head` pointer and `coord[3].coord` as its
/// own `arm` matrix rather than indexed off `coord`: the second and third
/// reads of `task->extra.tmd->coords` are re-derived rather than sharing the
/// first, which is what keeps the task argument live in `$s1` across the
/// calls and puts `&target[4]` in its own register.
static void func_actor_317000_801621F4(Task* task, Task* targetTask, s32 arg2, s32 arg3, s32 arg4)
{
    GfxCoord*         coord;
    GfxCoord*         target;
    GfxCoord*         head;
    GfxCoord*         aim;
    MATRIX*           arm;
    GfxRotationWords* words;
    VECTOR            delta;
    VECTOR            dir;
    SVECTOR           ang;
    SVECTOR           vec;
    SVECTOR           rot;

    coord  = task->extra.tmd->coords;
    target = targetTask->extra.tmd->coords;
    head   = &coord[5];
    aim    = &target[4];

    delta.vx = aim->workm.t[0] - head->workm.t[0];
    delta.vy = aim->workm.t[1] - head->workm.t[1];
    delta.vz = aim->workm.t[2] - head->workm.t[2];
    VectorNormal(&delta, &delta);
    ApplyTransposeMatrixLV(&task->extra.tmd->coords[2].workm, &delta, &delta);
    VectorNormal(&delta, &dir);

    rot.vx = ratan2(dir.vz, dir.vy);
    rot.vy = ratan2(dir.vx, -ABS(dir.vz));
    rot.vz = ratan2(dir.vx, dir.vy);

    gfxExtractSmallestEuler(&ang, &coord[5].coord);
    ang.vx = (u16)ang.vx + (u16)rot.vx;
    ang.vy = (u16)ang.vy + (u16)rot.vy;
    ang.vz = (u16)ang.vz + (u16)rot.vz;
    RotMatrix(&ang, &coord[5].coord);

    gfxExtractSmallestEuler(&rot, &coord[5].coord);
    arm = &task->extra.tmd->coords[3].coord;
    gfxExtractSmallestEuler(&vec, arm);
    vec.vx = (u16)vec.vx + rot.vx / 8;
    vec.vy = (u16)vec.vy + rot.vy / 8;
    vec.vz = (u16)vec.vz + rot.vz / 8;
    RotMatrix(&vec, arm);

    gfxExtractSmallestEuler(&vec, &coord[5].coord);
    vec.vx = vec.vx * 5 / 8;
    vec.vy = vec.vy * 5 / 8;
    vec.vz = vec.vz * 5 / 8;

    words         = (GfxRotationWords*)&coord[5].coord;
    words->m00M01 = ONE;
    words->m02M10 = 0;
    words->m11M12 = ONE;
    words->m20M21 = 0;
    words->m22    = ONE;
    RotMatrix(&vec, &coord[5].coord);
}

/// Message 0x7DD handler of the table `func_actor_317000_8016267C` installs,
/// and the actor's spawn body. The placement's position and rotation are
/// copied into the work's `walk.target` and `walk.targetRot`, `walk.motion` --
/// the index `func_actor_317000_80161E68` dispatches on -- is latched to 1, and
/// the animation preset is filled: bank 0, the start clip from `anim`
/// (`animationId`, or 2 when `anim` is absent), `nextAnimId` into
/// `model.nextAnimId` (or 1 when absent), then blend, 5 frames and world
/// collision on.
///
/// The preset is then installed the way `actorMotionPlayAnim19` installs
/// one, written out in-line: a changed `field_0` resets the bank in
/// `gActorMotionAnimBanks19` through `animationInitContext` (`model.bank` latches it,
/// `model.animId` goes back to -1), and a changed `field_4` -- or a preset asking
/// for slots when `model.ticking` says the slots are already ticking -- is pushed
/// onto `animationSeekSlotWithBlend`'s per-slot loop instead of the `animationResetSlot`
/// one, followed by a `animationTickSlot` pass over the same 0x12 slots and
/// `model.ticking` raised. Returns 0 either way.
s32 func_actor_317000_80162458(Task* task, s32 arg1, ActorTransform* place, ActorMotionWalkAnim* anim)
{
    _Actor317000Work*     work;
    _Actor317000Work*     w;
    AnimationPlayRequest  preset;
    AnimationPlayRequest* msg;
    s32                   i;
    TmdObject*            ext;

    w                    = task->work;
    w->walk.motion       = ACTOR_WALK_MOTION_WALKING;
    w->walk.target.vx    = place->pos.vx;
    w->walk.target.vy    = place->pos.vy;
    w->walk.target.vz    = place->pos.vz;
    w->walk.targetRot.vx = place->rot.vx;
    w->walk.targetRot.vy = place->rot.vy;
    w->walk.targetRot.vz = place->rot.vz;
    preset.source.index  = 0;
    if (anim != NULL) {
        preset.animationId  = anim->animationId;
        w->model.nextAnimId = anim->nextAnimId;
    } else {
        preset.animationId  = 2;
        w->model.nextAnimId = 1;
    }
    preset.blend                = ANIMATION_BLEND_INTERPOLATE;
    preset.blendFrames          = 5;
    preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    msg  = &preset;
    work = task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->model.bank) {
        work->model.bank   = msg->source.index;
        work->model.animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, gActorMotionAnimBanks19[work->model.bank], ext, work->rig.poses,
                             work->rig.slots);
    }
    if (msg->animationId != work->model.animId) {
        work->model.animId = msg->animationId;
        if (msg->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
            for (i = 1; i < 0x13; i++) {
                animationSeekSlotWithBlend(&work->rig.anim, i, work->model.animId, 0, msg->blendFrames);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                animationResetSlot(&work->rig.anim, i, work->model.animId);
            }
        }
        for (i = 1; i < 0x13; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
        work->model.ticking = 1;
    }
    return 0;
}

/// Task callback of the actor: copies the three-handler table
/// `D_actor_317000_80161E24` (spawn `func_actor_317000_8016267C`, per-frame
/// tick `func_actor_317000_80161E68`, exit `func_actor_317000_80162724`) onto
/// the stack and runs the entry `Task::state` selects.
void func_actor_317000_80162624(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_317000_80161E24;
    sp.funcs[task->state](task);
}

/// Spawn state of the enemy actor: allocates the 0x4CC-byte work block every
/// later handler reads through `Task::work`, seeds the three -1 fields and the
/// three cleared words the work's own init expects, republishes the light and
/// colour matrices onto the display object, sets `TmdObject::flags` bit 0x80
/// through the mode-0 call, then installs the message table and the exit
/// handler. An allocation failure ends the task instead of leaving a
/// half-built actor behind.
static void func_actor_317000_8016267C(Task* arg0)
{
    _Actor317000Work* work;

    work = memCalloc(sizeof(_Actor317000Work), false);
    if (work == NULL) {
        enemyTaskExit(arg0);
        return;
    }

    arg0->work               = work;
    work->model.animId       = ACTOR_MODEL_STATE_NONE;
    work->model.bank         = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown      = -1;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;

    func_actor_317000_80162744(arg0);
    func_actor_317000_80162BC4(arg0, ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);

    arg0->msgTable     = D_actor_317000_8016CF50;
    arg0->exitCallback = func_actor_317000_80162724;
    arg0->state++;
}

/// Exit handler, both the third entry of `D_actor_317000_80161E24` and the
/// `Task::exitCallback` `func_actor_317000_8016267C` installs: ends the task
/// through `enemyTaskExit`.
static void func_actor_317000_80162724(Task* arg0)
{
    enemyTaskExit(arg0);
}

/// Points the display object's light and colour matrices at the work block's
/// own copies, `light` and `color` of `_Actor317000Work::model`.
static void func_actor_317000_80162744(Task* arg0)
{
    TmdObject*        ext;
    _Actor317000Work* work;

    ext           = arg0->extra.tmd;
    work          = arg0->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

/// Index 0 of the two-entry table `func_actor_317000_80161E68` dispatches on
/// `_Actor317000Work::walk.motion`: does nothing.
static void func_actor_317000_80162760(Task* arg0)
{
}

/// Index 1 of the two-entry table `func_actor_317000_80161E68` dispatches on
/// `_Actor317000Work::walk.motion`: copies the four step handlers
/// `D_actor_317000_80161E30` onto the stack and runs the one
/// `_Actor317000Work::walk.motionStep` selects, read sign-extended.
static void func_actor_317000_80162768(Task* arg0)
{
    TaskFuncTable4    handlers;
    _Actor317000Work* work;

    work     = arg0->work;
    handlers = D_actor_317000_80161E30;
    handlers.funcs[work->walk.motionStep](arg0);
}

/// Step handler at index 0 of `D_actor_317000_80161E30`: Euler-extracts the
/// root coordinate into `vec`, and while the yaw gap to the target
/// `work->walk.targetRot.vy` is at least 0x41 it steps `vec.vy` toward it by 0x40 --
/// the step is taken on an `s32` widening of the extracted yaw -- and
/// otherwise snaps the yaw to the target and plays anim 0x7D3 with a preset
/// whose `field_4` is the literal 2, clearing the `airborne` flag and
/// advancing `walk.motionStep`. Either way the root coordinate is rebuilt as the
/// identity matrix rotated by `vec`.
static void func_actor_317000_801627D0(Task* arg0)
{
    _Actor317000Work*    work;
    GfxRotationWords*    words;
    GfxCoord*            coord;
    SVECTOR              vec;
    AnimationPlayRequest preset;
    s32                  vy;
    s16                  diff;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;

    gfxExtractSmallestEuler(&vec, &coord->coord);
    diff = (u16)work->walk.targetRot.vy - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy                      = work->walk.targetRot.vy;
        preset.source.index         = 0;
        preset.animationId          = 2;
        preset.blend                = ANIMATION_BLEND_INTERPOLATE;
        preset.blendFrames          = 5;
        preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        actorMotionPlayAnim19(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &preset, 0);
        work->airborne = 0;
        work->walk.motionStep++;
    }

    words         = (GfxRotationWords*)&coord->coord;
    words->m00M01 = ONE;
    words->m02M10 = 0;
    words->m11M12 = ONE;
    words->m20M21 = 0;
    words->m22    = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Step handler at index 1 of `D_actor_317000_80161E30`, reached by the
/// `walk.motionStep` advance `func_actor_317000_801627D0` ends with: rotates the constant local-space
/// offset `D_actor_317000_80161E40` through the root part's matrix into
/// `work->walk.velocity`, then raises `airborne` and moves the dispatcher on.
static void func_actor_317000_801628D8(Task* task)
{
    _Actor317000Work* work;
    GfxCoord*         coord;
    VECTOR            vec;

    coord = task->extra.tmd->coords;
    work  = task->work;

    vec = D_actor_317000_80161E40;
    ApplyMatrixLV(&coord->coord, &vec, &work->walk.velocity);
    work->airborne = 1;
    work->walk.motionStep++;
}

/// Step handler at index 2 of `D_actor_317000_80161E30`, the step after
/// `func_actor_317000_801628D8` and reached by the `walk.motionStep` advance that
/// body ends with. While the root coordinate's Y is below -0x30 it does
/// nothing; above it the rise is over: the local-space `work->walk.velocity` the
/// previous body wrote is cleared, the animation is re-applied through message
/// 0x7D3 with the latched `model.nextAnimId` state, and sound 0x400A000B is queued
/// panned and attenuated from the root coordinate's matrix. The `airborne`
/// flag the previous body raised is cleared and the dispatcher advances again.
static void func_actor_317000_80162950(Task* arg0)
{
    GfxCoord*            coord;
    _Actor317000Work*    work;
    AnimationPlayRequest preset;
    s32                  pan;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    if (coord->coord.t[1] < -0x30) {
        return;
    }
    preset.source.index         = 0;
    preset.animationId          = work->model.nextAnimId;
    preset.blend                = ANIMATION_BLEND_INTERPOLATE;
    preset.blendFrames          = 5;
    preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    actorMotionPlayAnim19(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &preset, 0);
    pan = (s8)worldCoordGetOriginAudioPan(coord);
    sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_ACTOR_311500, 0x0B), pan, (s8)worldCoordGetOriginAudioDepth(coord));

    work->walk.velocity.vx = 0;
    work->walk.velocity.vy = 0;
    work->walk.velocity.vz = 0;
    work->airborne         = 0;
    work->walk.motionStep++;
}

#include "../../shared/actor_motion_play19.inc.c"

#include "../../shared/actor_messages_place_euler.inc.c"

/// Message 0x7D5 handler of `D_actor_317000_8016CF50`, also called directly by
/// the spawn state `func_actor_317000_8016267C` with mode 0. `mode` sets or
/// clears bit 0x80 of `TmdObject::flags` and sets or clears `TMD_OBJECT_SKIP_AUTO_BUFFER`:
///
///   mode 0  set 0x80, clear `TMD_OBJECT_SKIP_AUTO_BUFFER`
///   mode 1  clear 0x80, `Tmd_AllocBuffers`, clear `TMD_OBJECT_SKIP_AUTO_BUFFER`
///   mode 2  set 0x80, store 2 in the countdown `_Actor317000Work::freeCountdown`
///           that `func_actor_317000_80161E68` ends in `Tmd_FreeBuffers`,
///           set `TMD_OBJECT_SKIP_AUTO_BUFFER`
///   mode 3  clear 0x80, set `TMD_OBJECT_SKIP_AUTO_BUFFER`
///
/// Any other mode returns 1; the four known ones return 0. `arg3` is unused.
s32 func_actor_317000_80162BC4(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject*        obj;
    _Actor317000Work* work;
    s32               ret;

    obj  = task->extra.tmd;
    work = task->work;
    ret  = 0;
    switch (mode) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags         |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = mode;
            obj->flags         |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Message 0x7DB handler of the table `func_actor_317000_8016267C` installs:
/// latches the payload's halfword at 0x2 into `_Actor317000Work::turnWeightRising` --
/// 0 for mode 0, the mode itself for mode 1 -- and for any other mode dumps the
/// root coordinate's matrix translation (`"pos"`) and the euler angles
/// `gfxExtractSmallestEuler` derives from its rotation matrix (`"rot"`) through
/// `GPU_printf`, both under the `"%s=(%d,%d,%d)\n"` format. Returns 0
/// either way.
s32 func_actor_317000_80162CA0(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    _Actor317000Work* work;
    GfxCoord*         coord;
    SVECTOR           rot;
    s32               mode;

    work  = task->work;
    coord = task->extra.tmd->coords;
    mode  = msg->command;
    switch (mode) {
        case 0:
            work->turnWeightRising = 0;
            break;
        case 1:
            work->turnWeightRising = mode;
            break;
        default:
            GPU_printf("%s=(%d,%d,%d)\n", "pos", coord->coord.t[0], coord->coord.t[1], coord->coord.t[2]);
            gfxExtractSmallestEuler(&rot, &coord->coord);
            GPU_printf("%s=(%d,%d,%d)\n", "rot", rot.vx, rot.vy, rot.vz);
            break;
    }
    return 0;
}
