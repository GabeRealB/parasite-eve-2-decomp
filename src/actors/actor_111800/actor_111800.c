#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/acropolis_square.h"

/// Work block `func_actor_111800_80132390` allocates with `memCalloc(0x498)`
/// and parks in `Task::work` (0x1C). The prefix is the shared actor anim
/// layout: a `AnimationContext` and the nineteen `AnimationSlot`s `func_800B3F84` seeds
/// from the animation bank and the frame handler ticks. `field_43C` /
/// `field_45C` are the light and colour matrices handed to the model
/// `TmdObject`.
typedef struct Actor111800Work {
    /* 0x000 */ ActorAnimRig19 rig;
    /* 0x43C */ MATRIX         field_43C;
    /* 0x45C */ MATRIX         field_45C;
    /* 0x47C */ void*          field_47C; // gameGetPtrSlot(3)
    /* 0x480 */ MATRIX*        field_480; // Player_Status.coordMtx, the player's coordinate matrix
    /* 0x484 */ u16            field_484; // sequence step the per-frame handler switches on
    /* 0x486 */ byte           pad_486[2];
    /* 0x488 */ u16            field_488; // frames spent in the current step
    /* 0x48A */ byte           pad_48A[2];
    /* 0x48C */ s16            field_48C; // angle ramped in steps 1 and 3
    /* 0x48E */ byte           pad_48E[4];
    /* 0x492 */ s16            field_492; // latched copy of slots[1].currentPose.indices.recordIndex
    /* 0x494 */ s16            field_494; // angle ramped in step 1; spawn seeds 0x155
} Actor111800Work;
STATIC_ASSERT_SIZEOF(Actor111800Work, 0x498);

/// Animation bank `func_800B3F84` builds the work block's clip context from;
/// the actor hands it over whole, so it is only ever a byte address here.
extern AnimationSet* D_actor_111800_8013A448[8];

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

/// Main-executable globals with no module header yet: `Gp_StateC08.field_A` is the
/// cutscene-mode flag and `gDisplayState.pendingMode` is a live cutscene. `func_acropolis_square_80182360` is
/// the room overlay's handler the view-matrix test calls with `t[0]`.

extern TmdSource D_actor_111800_80138004;
void             func_actor_111800_8013251C(Task*);

TmdBone D_actor_111800_801329C4[19] = {
#include "assets/actor_111800_model_061E4_skeleton.inc"
};

u32 D_actor_111800_80132C70[19] = {
#include "assets/actor_111800_model_061E4_partVerts.inc"
};

SVECTOR D_actor_111800_80132CBC[306] = {
#include "assets/actor_111800_model_061E4_verts.inc"
};

SVECTOR D_actor_111800_8013364C[365] = {
#include "assets/actor_111800_model_061E4_normals.inc"
};

u32 D_actor_111800_801341B4[3988] = {
#include "assets/actor_111800_model_061E4_stream.inc"
};

TmdSource D_actor_111800_80138004 = {
    0,
    20032,
    7672,
    19,
    D_actor_111800_80132C70,
    D_actor_111800_80132CBC,
    D_actor_111800_8013364C,
    D_actor_111800_801329C4,
    D_actor_111800_801341B4,
};

AnimationPackedPose D_actor_111800_80138028[4] = {
#include "assets/actor_111800_animation_065FC_bank1.inc"
};

AnimationPackedRotation D_actor_111800_80138058[69] = {
#include "assets/actor_111800_animation_065FC_bank4.inc"
};

AnimationRecord D_actor_111800_8013816C[162] = {
#include "assets/actor_111800_animation_065FC_records.inc"
};

u16 D_actor_111800_801383F4[20] = {
#include "assets/actor_111800_animation_065FC_indices.inc"
};

AnimationSet D_actor_111800_8013841C = {
    D_actor_111800_8013816C,
    D_actor_111800_801383F4,
    { NULL, D_actor_111800_80138028, NULL, NULL, D_actor_111800_80138058, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_111800_80138444[28] = {
#include "assets/actor_111800_animation_07120_bank1.inc"
};

AnimationPackedRotation D_actor_111800_80138594[247] = {
#include "assets/actor_111800_animation_07120_bank4.inc"
};

AnimationRecord D_actor_111800_80138970[362] = {
#include "assets/actor_111800_animation_07120_records.inc"
};

u16 D_actor_111800_80138F18[20] = {
#include "assets/actor_111800_animation_07120_indices.inc"
};

AnimationSet D_actor_111800_80138F40 = {
    D_actor_111800_80138970,
    D_actor_111800_80138F18,
    { NULL, D_actor_111800_80138444, NULL, NULL, D_actor_111800_80138594, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_111800_80138F68[32] = {
#include "assets/actor_111800_animation_07BA4_bank1.inc"
};

AnimationPackedRotation D_actor_111800_801390E8[223] = {
#include "assets/actor_111800_animation_07BA4_bank4.inc"
};

AnimationRecord D_actor_111800_80139464[334] = {
#include "assets/actor_111800_animation_07BA4_records.inc"
};

u16 D_actor_111800_8013999C[20] = {
#include "assets/actor_111800_animation_07BA4_indices.inc"
};

AnimationSet D_actor_111800_801399C4 = {
    D_actor_111800_80139464,
    D_actor_111800_8013999C,
    { NULL, D_actor_111800_80138F68, NULL, NULL, D_actor_111800_801390E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_111800_801399EC[6] = {
#include "assets/actor_111800_animation_07FAC_bank1.inc"
};

AnimationPackedRotation D_actor_111800_80139A34[70] = {
#include "assets/actor_111800_animation_07FAC_bank4.inc"
};

AnimationRecord D_actor_111800_80139B4C[150] = {
#include "assets/actor_111800_animation_07FAC_records.inc"
};

u16 D_actor_111800_80139DA4[20] = {
#include "assets/actor_111800_animation_07FAC_indices.inc"
};

AnimationSet D_actor_111800_80139DCC = {
    D_actor_111800_80139B4C,
    D_actor_111800_80139DA4,
    { NULL, D_actor_111800_801399EC, NULL, NULL, D_actor_111800_80139A34, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_111800_80139DF4[18] = {
#include "assets/actor_111800_animation_08600_bank1.inc"
};

AnimationPackedRotation D_actor_111800_80139ECC[146] = {
#include "assets/actor_111800_animation_08600_bank4.inc"
};

AnimationRecord D_actor_111800_8013A114[185] = {
#include "assets/actor_111800_animation_08600_records.inc"
};

u16 D_actor_111800_8013A3F8[20] = {
#include "assets/actor_111800_animation_08600_indices.inc"
};

AnimationSet D_actor_111800_8013A420 = {
    D_actor_111800_8013A114,
    D_actor_111800_8013A3F8,
    { NULL, D_actor_111800_80139DF4, NULL, NULL, D_actor_111800_80139ECC, NULL, NULL, NULL },
};

AnimationSet* D_actor_111800_8013A448[8] = {
    &D_actor_111800_8013841C,
    &D_actor_111800_80138F40,
    &D_actor_111800_801399C4,
    NULL,
    NULL,
    &D_actor_111800_80139DCC,
    &D_actor_111800_8013A420,
    NULL,
};

TaskDesc D_actor_111800_8013A468 = { 257, 192, func_actor_111800_8013251C, { .model = &D_actor_111800_80138004 } }; /// Turns joint `coord` by `yaw` about the world Y axis: builds its world

static void           func_actor_111800_80131E40(GfxCoord* coord, s16 yaw);
static inline void    _actor111800TickAnim(Task* task);
static inline void    _actor111800Reseed(Task* task, u16 id, u16 frames);
static void           func_actor_111800_8013214C(Task* task);
static void           func_actor_111800_80132390(Task* task);
static __inline__ s32 Actor111800_Accumulate(GfxCoord* arg0, MATRIX* arg1, MATRIX* src);

/// rotation in a matrix carved off the scratchpad head, applies the turn,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it.
static void func_actor_111800_80131E40(GfxCoord* coord, s16 yaw)
{
    MATRIX*   rotation;
    GfxCoord* out;

    SCRATCH_PUSH(MATRIX);
    rotation = SCRATCH_HEAD(MATRIX);
    actorAccumulateRotation(coord, rotation, &gGfxViewCoord);
    RotMatrixY(yaw, rotation);
    out = actorLocalizeRotation(coord, rotation);
    memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(out);
    SCRATCH_POP(MATRIX);
}

/// Advances animation slots 1..0x12 by one frame and latches slot 1's current
/// record into `field_492`.
static inline void _actor111800TickAnim(Task* task)
{
    Actor111800Work* work = (Actor111800Work*)task->work;
    u16              i;

    for (i = 1; i < 0x13; i++) {
        Gp_AnimTickIndex(&work->rig.anim, i);
    }
    work->field_492 = work->rig.slots[1].currentPose.indices.recordIndex;
}

/// Cross-fades body slots 1..0x12 of `work`'s animation context to animation
/// `id` over `frames` frames.
#define _ACTOR111800_BLEND_SLOTS(work, id, frames)                   \
    do {                                                             \
        u16 _i;                                                      \
        for (_i = 1; _i < 0x13; _i++) {                              \
            func_800B4114(&(work)->rig.anim, _i, (id), 0, (frames)); \
        }                                                            \
    } while (0)

/// Clears the latched record and cross-fades every body slot to `id`.
static inline void _actor111800Reseed(Task* task, u16 id, u16 frames)
{
    Actor111800Work* work = (Actor111800Work*)task->work;

    work->field_492 = 0;
    _ACTOR111800_BLEND_SLOTS(work, id, frames);
}

/// Per-frame handler: ticks animation slots 1..0x12, latches `slots[1].currentPose.indices.recordIndex`
/// into `field_492`, then runs the seven-step sequence in `field_484` (reseed,
/// ramp the two angles, wait, reverse the first angle, reseed again, wait,
/// then drop the model coordinate's Z and clear its flag).
static void func_actor_111800_8013214C(Task* task)
{
    Actor111800Work* work;
    GfxCoord*        coord;
    s32              flag1;
    s32              flag2;

    work  = (Actor111800Work*)task->work;
    coord = task->extra.tmd->coords;
    _actor111800TickAnim(task);
    switch (work->field_484) {
        case 0:
            _actor111800Reseed(task, 0, 0xF);
            work->field_488 = 0;
            work->field_484++;
            break;
        case 1:
            flag1 = 0;
            flag2 = 0;
            if (work->field_48C >= -0x154) {
                work->field_48C -= 0x10;
            } else {
                flag1 = 1;
            }
            if (work->field_494 >= -0x154) {
                work->field_494 -= 0x10;
            } else {
                flag2 = 1;
            }
            if (flag2 & flag1) {
                work->field_488 = 0;
                work->field_484++;
            }
            break;
        case 2:
            work->field_488++;
            if (work->field_488 >= 0x1F) {
                work->field_484++;
            }
            break;
        case 3:
            if (work->field_48C < 0x2AA) {
                work->field_48C += 0x80;
            } else {
                work->field_488 = 0;
                work->field_484++;
            }
            break;
        case 4:
            work->field_488++;
            if (work->field_488 >= 0x10) {
                _actor111800Reseed(task, 2, 0xA);
                work->field_488 = 0;
                work->field_484++;
            }
            break;
        case 5:
            work->field_488++;
            if (work->field_488 >= 2) {
                work->field_488 = 0;
                work->field_484++;
            }
            break;
        case 6:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[2]  -= 0x96;
            break;
    }
}

/// Spawn/setup handler for the actor's model: allocates the 0x498-byte work
/// block into `Task::work`, hands the model object the view coordinate and the
/// block's two matrices, builds the animation context over the nineteen slots
/// and applies the nested area record matching id 0x13 through `Gp_SetTmdBytes`.
///
/// The allocation is parked in `Task::work` and read back before it is used, so
/// the first thing the block is named by is a reload: the `memCalloc` result is
/// stored straight from `$v0` and the failing branch tests that register, which
/// is what leaves the surviving copy of it to be emitted *after* the branch --
/// one declaration earlier and the copy lands before the test.
static void func_actor_111800_80132390(Task* task)
{
    Actor111800Work* work;
    Actor111800Work* work2;
    TmdObject*       obj;
    GfxCoord*        coord;
    AreaPlacement*   place;
    s32              i;

    coord      = task->extra.tmd->coords;
    obj        = task->extra.tmd;
    task->work = memCalloc(0x498, false);
    if (task->work == NULL) {
        taskKill(task);
        return;
    }
    work = (Actor111800Work*)task->work;
    Mem_Set(work, 0U, 0x498U);
    coord->parent = &gGfxViewCoord;
    Tmd_AllocBuffers(obj);
    obj->lightMtx = &work->field_43C;
    obj->colorMtx = &work->field_45C;
    obj->flags    = 0;
    func_800B3F84(&work->rig.anim, D_actor_111800_8013A448, obj, work->rig.poses,
                  &work->rig.slots[0]);
    work->field_47C  = gameGetPtrSlot(3);
    work->field_480  = Player_Status.coordMtx;
    i                = 1;
    work2            = (Actor111800Work*)task->work;
    work2->field_492 = 0;
    do {
        work2->rig.slots[i & 0xFFFF].rate = ANIMATION_RATE_ONE;
        Gp_AnimResetSlot(&work2->rig.anim, i & 0xFFFF, 5);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
    work->field_494 = 0x155;
    place           = Gp_GetNestedAreaRec(&gGameSession->at4.loc)->field_0;
    while (place->entryId != AREA_PLACEMENT_END && place->entryId != 0x13) {
        place++;
    }
    Gp_SetTmdBytes(obj, place->texturePageOffset, place->clutRowOffset);
}

/// Builds `arg0`'s absolute rotation in `arg1`, seeded from `src` rather than
/// from `arg0->coord`: each ancestor is pre-multiplied in turn (renormalised
/// after every step) up to but not including the view coordinate. Returns
/// whether the walk reached the view coordinate. The caller passes the part's
/// own rotation as `src`, addressed through the coordinate array.
static __inline__ s32 Actor111800_Accumulate(GfxCoord* arg0, MATRIX* arg1, MATRIX* src)
{
    MATRIX    matrix;
    GfxCoord* coord;
    GfxCoord* view;

    coord = arg0->parent;
    view  = &gGfxViewCoord;
    *arg1 = *src;
    while (1) {
        if (coord == NULL) {
            return 0;
        }
        if (coord == view) {
            return 1;
        }
        gte_SetRotMatrix(&coord->coord);
        MulRotMatrix(arg1);
        MatrixNormal(arg1, &matrix);
        *arg1 = matrix;
        coord = coord->parent;
    }
}

/// Per-frame state machine. State 0 waits until no cutscene is up, then runs
/// the spawn handler and advances. State 1 ticks slots 1..0x12, latches
/// `field_492`, and advances after `func_acropolis_square_80182360` when the player is in
/// range. State 2 runs the sequence handler and kills the task once the
/// session is idle. Every path but the state-0 wait then pitches part 5 by
/// `field_494`, writes it back, yaws it through `func_actor_111800_80131E40`, and
/// rebuilds the colour matrix around part 1's translation.
void func_actor_111800_8013251C(Task* task)
{
    MATRIX           mtx;
    Actor111800Work* work;
    Actor111800Work* ctx;
    Actor111800Work* work2;
    TmdObject*       extra;
    TmdObject*       obj;
    GfxCoord*        coords;
    GfxCoord*        part;
    MATRIX*          viewMtx;
    s32              state;
    s32              i;
    s32              x;
    u16              angle;

    state = task->state;
    work  = (Actor111800Work*)task->work;
    switch (state) {
        case 0:
            if ((Gp_StateC08.field_A != 1) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                func_actor_111800_80132390(task);
                task->state += 1;
                break;
            }
            return;
        case 1:
            ctx = work;
            i   = 1;
            do {
                Gp_AnimTickIndex(&ctx->rig.anim, i & 0xFFFF);
                i += 1;
            } while ((u32)(i & 0xFFFF) < 0x13U);
            ctx->field_492 = ctx->rig.slots[1].currentPose.indices.recordIndex;
            viewMtx        = work->field_480;
            x              = viewMtx->t[0];
            if ((x >= 0x5DD && viewMtx->t[2] >= -0x513) || (x >= 0xC81 && viewMtx->t[2] < -0x514)) {
                work->field_484 = 0;
                func_acropolis_square_80182360(x);
                task->state += 1;
            }
            break;
        case 2:
            func_actor_111800_8013214C(task);
            if (gGameSession->eventState == 0) {
                taskKill(task);
            }
            break;
    }
    extra  = task->extra.tmd;
    angle  = (u16)work->field_494;
    coords = extra->coords;
    part   = coords + 5;
    Actor111800_Accumulate(part, &mtx, &coords[5].coord);
    RotMatrixX((s32)(s16)angle, &mtx);
    actorLocalizeRotation(part, &mtx);
    Mem_CopyUnaligned(&mtx, &part->coord, 0x12U);
    part->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(part);
    func_actor_111800_80131E40(task->extra.tmd->coords + 5, work->field_48C);
    obj                 = task->extra.tmd;
    work2               = (Actor111800Work*)task->work;
    ((VECTOR*)&mtx)->vx = obj->coords[1].workm.t[0];
    ((VECTOR*)&mtx)->vy = task->extra.tmd->coords[1].workm.t[1];
    ((VECTOR*)&mtx)->vz = task->extra.tmd->coords[1].workm.t[2];
    func_800D7A9C(obj, (VECTOR*)&mtx, 0, 3);
    ((VECTOR*)&mtx)->vz = 0x555;
    ((VECTOR*)&mtx)->vy = 0x555;
    ((VECTOR*)&mtx)->vx = 0x555;
    ScaleMatrix(&work2->field_45C, (VECTOR*)&mtx);
}
