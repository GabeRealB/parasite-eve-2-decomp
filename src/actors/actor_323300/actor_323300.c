#include "common.h"

#include <psyq/abs.h>

#include "actors/actor_323300.h"

#include "gameplay/3CD8.h"

#include "gameplay/gameplay.h"

/// Allocates the 0x504 `Actor323300Work` this actor's whole lifetime runs on,
/// seeds the `GpRec18` collision table and the display node at +0x480, then
/// binds the three message handlers and the animation presets the state
/// functions drive. Bails out through `Gp_EnemyTaskExit` when the room flag
/// 0x60 is already set (the actor already spawned) or the allocation fails.
void func_actor_323300_80161E78(Task* arg0)
{
    Actor323300Work* work;
    TmdObject*       extra;
    GpObj*           obj;

    if (GameFlag_GetNibble(0x60) != 0 || (work = Mem_Calloc(0x504, 0)) == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work      = (TaskIdMap*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_500 = 1;
    work->field_502 = -1;
    func_actor_323300_801626D0(arg0);
    extra         = arg0->extra;
    obj           = &work->obj;
    obj->field_8  = extra->coords + 1;
    obj->field_C  = &work->rec;
    obj->field_18 = 0x30000;
    obj->field_10 = 0;
    obj->field_12 = 0;
    obj->field_14 = 0;
    obj->field_1C = 0x100;
    obj->flags    = 1;
    Gp_LinkObj(2, obj);
    obj->flags |= 0x8000;
    Gp_InitRec18Table(obj->field_C, 1, 0);
    arg0->msgTable = &D_actor_323300_80172574;
    func_actor_323300_80162208(arg0, 0x7D5, 0, 0);
    func_actor_323300_801629F0(arg0, 0x7D3, &D_actor_323300_8017259C, 0);
    func_actor_323300_801628B8(arg0, 0x7D3, &D_actor_323300_801725B4, 0);
    SndEvt_EnqueueType6(0x52100006, 0, 0x28);
    arg0->exitCallback = func_actor_323300_8016269C;
    arg0->state       += 1;
}

/// Per-frame runner for the `Actor323300Work` block: dispatches on
/// `field_4FC` through the two-entry handler table it builds on the stack,
/// walks the 18 animation slots and, while `field_500` is set, posts one of the
/// two sound cues -- the pan/depth pair the session's `at4.loc.view` picks between
/// is built twice so the two calls cross-jump into a shared `jal`. Then, unless
/// `TmdObject::flags` says the model is hidden, draws the ground shadow under
/// coordinate 1, refreshes that coordinate's matrix and colour, and ticks the
/// `field_502` countdown that frees the model's buffers when it reaches zero.
void func_actor_323300_80161FE8(Task* arg0)
{
    TmdObject*       extra               = (TmdObject*)arg0->extra;
    Actor323300Work* work                = (Actor323300Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_323300_801626EC,
        func_actor_323300_801626F4,
    };
    VECTOR vec;
    s32    i;

    states[(s16)work->field_4FC](arg0);
    if (work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
        }
        if (work->field_500 != 0) {
            if (work->slots[1].field_10 & 2) {
                if (gGameSession->at4.loc.view == 2) {
                    SndEvt_EnqueueType6(0x52100006, 0, 0x28);
                } else {
                    SndEvt_EnqueueType6(0x52100006, 0, 0);
                }
            } else if (gGameSession->viewReady != 0) {
                if (gGameSession->at4.loc.view == 2) {
                    SndEvt_EnqueueTypeA(0x52100006, 0, 0x28);
                } else {
                    SndEvt_EnqueueTypeA(0x52100006, 0, 0);
                }
            }
        }
    }
    if (!(extra->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)arg0->extra)->coords[1].workm.t, (VECTOR3*)&vec) != 0) {
            Gp_DrawEffGroundQuad((VECTOR3*)&vec, 0x200, Gp_State1C->field_8);
        }
        Gp_ClearRec18Occupied(&work->rec);
        ((TmdObject*)arg0->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[1]);
        func_800D7A9C(extra, (VECTOR*)((TmdObject*)arg0->extra)->coords[1].workm.t, 0, 3);
    }
    if (work->field_502 >= 0) {
        if (work->field_502 == 0) {
            Tmd_FreeBuffers(extra);
        }
        work->field_502--;
    }
}

/// Message-0x7D5 handler: the four-way visibility/mode switch on the message's
/// mode word, the same body `func_actor_141000_80133E8C`,
/// `func_actor_503500_80132584` and `func_actor_511000_801327A0` are, plus the
/// display-node toggles the 0x504 work block's own `GpObj` needs.
///
/// Mode 0 hides the model -- `TmdObject::flags` bit 0x80, the bit
/// `func_actor_323300_80161FE8` tests before drawing the ground shadow -- and
/// clears bit 4 so the buffers get reallocated; 1 shows it, puts the node back
/// in the pair walk and allocates the aux buffers; 2 hides it and arms the
/// `field_502` countdown that same per-frame runner frees the buffers with; 3
/// shows it while keeping them. Modes 2 and 3 set bit 4, so the update path
/// skips the realloc for the two frames the countdown runs. Anything else
/// returns 1 and leaves the object alone; the handled modes return 0.
///
/// The node's `GpObj::flags` halfword is the induction variable, strided by one
/// `GpObj` per step: the block owns a single node, so the walk covers one
/// element, but retail keeps the array shape. Bit 0x8000 is the one
/// `Gp_RunPairHandler` tests before pairing the node up, so this switch is what
/// takes the node in and out of the pair walk.
s32 func_actor_323300_80162208(Task* arg0, s32 arg1, s32 mode, s32 arg3)
{
    Actor323300Work* work;
    TmdObject*       extra;
    u16*             flags;
    s32              i;
    s32              ret;

    extra = arg0->extra;
    work  = (Actor323300Work*)arg0->work;
    ret   = 0;

    switch (mode) {
        case 0:
            extra->flags |= 0x80;
            flags         = &work->obj.flags;
            for (i = 0; i < 1; i++) {
                flags[i * (sizeof(GpObj) / sizeof(*flags))] &= 0x7FFF;
            }
            extra->flags &= ~4;
            break;
        case 1:
            extra->flags &= ~0x80;
            flags         = &work->obj.flags;
            for (i = 0; i < 1; i++) {
                flags[i * (sizeof(GpObj) / sizeof(*flags))] |= 0x8000;
            }
            Tmd_AllocBuffers(extra);
            extra->flags &= ~4;
            break;
        case 2:
            extra->flags |= 0x80;
            flags         = &work->obj.flags;
            for (i = 0; i < 1; i++) {
                flags[i * (sizeof(GpObj) / sizeof(*flags))] &= 0x7FFF;
            }
            work->field_502 = 2;
            extra->flags   |= 4;
            break;
        case 3:
            extra->flags &= ~0x80;
            flags         = &work->obj.flags;
            for (i = 0; i < 1; i++) {
                flags[i * (sizeof(GpObj) / sizeof(*flags))] |= 0x8000;
            }
            extra->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }

    return ret;
}

INCLUDE_RODATA("actors/nonmatchings/actor_323300/actor_323300", D_actor_323300_80161E20);

INCLUDE_RODATA("actors/nonmatchings/actor_323300/actor_323300", D_actor_323300_80161E24);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80162360);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80162630);

void func_actor_323300_8016269C(Task* arg0)
{
    Gp_UnlinkObj(&((Actor323300Work*)arg0->work)->obj);
    Gp_EnemyTaskExit(arg0);
}

void func_actor_323300_801626D0(Task* arg0)
{
    TmdObject*       ext;
    Actor323300Work* work;

    ext           = arg0->extra;
    work          = (Actor323300Work*)arg0->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

void func_actor_323300_801626EC(Task* arg0)
{
}

void func_actor_323300_801626F4(Task* arg0)
{
    Actor323300Work* work                = (Actor323300Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_323300_80162748,
        func_actor_323300_801627B4,
    };

    states[(s16)work->field_4FE](arg0);
}

void func_actor_323300_80162748(Task* arg0)
{
    Actor323300Work* work;
    s32              i;

    work = (Actor323300Work*)arg0->work;
    func_actor_323300_801628B8(arg0, 0x7D3, &D_actor_323300_801725C8, 0);
    for (i = 1; i < 0x13; i++) {
        work->slots[i].field_9 = 8;
    }
    work->field_4CC = 0;
    work->field_4D0 = 0;
    work->field_4D4 = 0;
    work->field_4FE++;
}

/// State handler at index 1 of the two-entry table `func_actor_323300_801626F4`
/// dispatches, the turn-to-face body whose twins are
/// `func_actor_335800_801631A4`, `func_actor_113100_801330E8` and
/// `func_actor_350700_80162764`. Euler-extracts the root coordinate into `vec`
/// and, while the yaw gap to the target `work->field_4F6` stays under 0x41,
/// snaps `vec.vy` to that target, plays anim 0x7D3 through
/// `func_actor_323300_801628B8` and parks all 18 animation slots at 0x16 --
/// `field_4FC` and `field_4FE` go back to zero, so the handler re-runs. A wider
/// gap steps `vec.vy` toward the target by 0x40 instead. Either way the root
/// coordinate is rebuilt as the identity matrix rotated by `vec`, with `flg`
/// cleared so the next `Gp_UpdateCoord` recomputes it.
void func_actor_323300_801627B4(Task* arg0)
{
    Actor323300Work*   work;
    Actor323300Matrix* words;
    GsCOORDINATE2*     coord;
    SVECTOR            vec;
    s16                diff;
    s32                vy;
    s32                i;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor323300Work*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->field_4F6 - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy = work->field_4F6;
        func_actor_323300_801628B8(arg0, 0x7D3, &D_actor_323300_801725DC, 0);
        for (i = 1; i < 0x13; i++) {
            work->slots[i].field_9 = 0x16;
        }
        work->field_4FC = 0;
        work->field_4FE = 0;
    }

    words                = (Actor323300Matrix*)&coord->coord;
    words->ident.m00_m01 = 0x1000;
    words->ident.m02_m10 = 0;
    words->ident.m11_m12 = 0x1000;
    words->ident.m20_m21 = 0;
    words->ident.m22     = 0x1000;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_801628B8);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_801629F0);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80162A6C);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80162BE4);

/// Per-frame squash driver for the 0x6B0 `Actor323300MtxWork` block, and the
/// runner the model-display path calls once the block's animation has been
/// started: it ticks the 18 slots like `func_actor_323300_80163718` does, folds
/// `field_44C` -- the 0x3000 countdown `func_actor_323300_80162BE4` seeds, 0x40
/// per frame -- into the 0..0xFFF ramp `func_actor_323300_80162A6C` blends the
/// model's vertices with, and republishes that ramp onto `TmdObject::lightLevel`,
/// the intensity the shading path scales its RGB by. While the countdown is
/// still above 0x1000 the turn angle handed to `func_actor_323300_8016359C` is
/// `(0x1000 - field_44C) / 4`, i.e. the ramp read the other way round.
///
/// The three coordinate nodes at parts 3..5 are then flattened: each is copied
/// off into `shadow[0..2]` first, then squashed in place through
/// `ScaleMatrix` -- parts 3 and 4 to 0.2 on Y, part 5 to identity -- and the
/// *copies* become the parents of parts 4, 5 and 6, so the squash does not
/// compound down the part chain. The Y translation the squash removes from
/// parts 4 and 5 is folded out of their own `coord.t[1]` by the same 0.8 and
/// the same ramp. Part 6's shading is rebound to the third copy's translation
/// before the countdown drops, so the whole ramp runs out exactly when it
/// reaches zero.
void func_actor_323300_80162DF0(Task* arg0)
{
    Actor323300MtxWork* work;
    TmdObject*          extra;
    GsCOORDINATE2*      coord;
    VECTOR              vec;
    s32                 blend;
    s32                 i;

    work  = (Actor323300MtxWork*)arg0->work;
    extra = (TmdObject*)arg0->extra;

    if (work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
        }
    }

    blend = work->field_44C;
    if (blend >= 0x2000) {
        blend = 0xFFF;
    } else if (blend > 0x1000) {
        blend -= 0x1000;
    } else {
        blend = 0;
    }

    func_actor_323300_80162A6C(arg0, &D_801865D0, blend);
    extra->lightLevel = blend;

    if (work->field_44C < 0x1000) {
        func_actor_323300_8016359C(arg0, (s16)(((0x1000 - work->field_44C) << 14) >> 16));
    }

    coord           = &((TmdObject*)arg0->extra)->coords[3];
    work->shadow[0] = *coord;
    vec.vx          = 0x1000;
    vec.vy          = 0x333;
    vec.vz          = 0x1000;
    ScaleMatrix(&coord->coord, &vec);

    coord           = &((TmdObject*)arg0->extra)->coords[4];
    work->shadow[1] = *coord;
    coord->sub      = &work->shadow[0];
    vec.vx          = 0x1000;
    vec.vy          = 0x333;
    vec.vz          = 0x1000;
    ScaleMatrix(&coord->coord, &vec);
    coord->coord.t[1] = work->field_584 - work->field_584 * 0.8 * blend / 4096.0;

    coord           = &((TmdObject*)arg0->extra)->coords[5];
    work->shadow[2] = *coord;
    coord->sub      = &work->shadow[1];
    vec.vx          = 0x1000;
    vec.vy          = 0x1000;
    vec.vz          = 0x1000;
    ScaleMatrix(&coord->coord, &vec);
    coord->coord.t[1] = work->field_594 - work->field_594 * 0.8 * blend / 4096.0;

    coord      = &((TmdObject*)arg0->extra)->coords[6];
    coord->sub = &work->shadow[2];
    func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);

    work->field_44C -= 0x40;
    if (work->field_44C < 0) {
        work->field_44C = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80163188);

void func_actor_323300_801634B0(Task* arg0)
{
    GsCOORDINATE2* base;
    GsCOORDINATE2* node;
    GsCOORDINATE2* sub;

    do {
        base      = ((TmdObject*)arg0->extra)->coords;
        sub       = base + 3;
        node      = base + 4;
        node->sub = sub;
    } while (0);
    sub                                      = ((TmdObject*)arg0->extra)->coords + 5;
    sub->sub                                 = node;
    ((TmdObject*)arg0->extra)->coords[6].sub = sub;
    Task_Kill(arg0);
}

/// Splats an identity light/colour pair into the `Mem_Calloc(0x6B0)` work block
/// `func_actor_323300_80162BE4` parked in `Task::work`, republishes them onto
/// `TmdObject::lightMtx` / `field_20`, then re-derives model part 1's world
/// matrix -- clearing its dirty flag, rebuilding it from its parent and
/// rebinding the actor's shading to the part's translation.
void func_actor_323300_80163510(Task* arg0)
{
    Actor323300MtxWork* work;
    Actor323300Matrix*  light;
    Actor323300Matrix*  color;
    GsCOORDINATE2*      coords;
    TmdObject*          extra;

    extra  = arg0->extra;
    work   = (Actor323300MtxWork*)arg0->work;
    coords = extra->coords;

    work->light.ident.m00_m01 = 0x1000;
    light                     = &work->light;
    light->ident.m02_m10      = 0;
    light->ident.m11_m12      = 0x1000;
    light->ident.m20_m21      = 0;
    light->ident.m22          = 0x1000;

    work->color.ident.m00_m01 = 0x1000;
    color                     = &work->color;
    color->ident.m02_m10      = 0;
    color->ident.m11_m12      = 0x1000;
    color->ident.m20_m21      = 0;
    color->ident.m22          = 0x1000;

    extra->lightMtx = &light->mat;
    extra->colorMtx = &color->mat;

    coords[1].flg = 0;
    Gp_UpdateCoord(&coords[1]);
    func_800D7A9C(extra, (VECTOR*)coords[1].workm.t, 0, 3);
}

/// Re-aims the per-part coordinate nodes at index 5 and index 2 from one turn
/// angle: the angle is clamped to +-0x400 -- a quarter turn either way -- then
/// `func_actor_323300_80163188` rebuilds node 5 from two thirds of it and node
/// 2 from half, and nodes 5 down to 2 have their dirty flag cleared so the next
/// `Gp_UpdateCoord` re-derives them. The lower clamp tests `arg1` rather than
/// the clamped copy; that is the same test, because the upper clamp has already
/// pinned the copy to 0x400 whenever the angle was out of range upwards.
void func_actor_323300_8016359C(Task* arg0, s16 arg1)
{
    s16 var;

    var = arg1;
    if (var > 0x400) {
        var = 0x400;
    }
    if (arg1 < -0x400) {
        var = -0x400;
    }

    func_actor_323300_80163188(&((TmdObject*)arg0->extra)->coords[5], (var * 2) / 3);
    func_actor_323300_80163188(&((TmdObject*)arg0->extra)->coords[2], var / 2);

    ((TmdObject*)arg0->extra)->coords[5].flg = 0;
    ((TmdObject*)arg0->extra)->coords[4].flg = 0;
    ((TmdObject*)arg0->extra)->coords[3].flg = 0;
    ((TmdObject*)arg0->extra)->coords[2].flg = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_8016369C);

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Start-preset handler for the 0x6B0 `Actor323300MtxWork` block
/// `func_actor_323300_80162BE4` parks in `Task::work`, and the twin of
/// `func_actor_323300_801628B8` (which drives the 0x504 block the same way).
/// A preset bank the block is not already on re-seeds it: the animation id is
/// reset to -1, the bank is stored and the bank's animation source goes to
/// `func_800B3F84` with the block's context, slots and matrix table. A
/// different animation id then restarts every slot 1..0x12 -- through
/// `func_800B4114` when the preset asks for it and the block has been started
/// before, through `Gp_AnimResetSlot` otherwise -- ticks them once and latches
/// `field_43C` so the next preset takes the first branch.
s32 func_actor_323300_80163718(Task* arg0, s32 arg1, Actor323300AnimPreset* arg2, s32 arg3)
{
    Actor323300MtxWork* work;
    TmdObject*          ext;
    s32                 i;

    work = (Actor323300MtxWork*)arg0->work;
    ext  = arg0->extra;
    if (arg2->field_0 != work->field_440) {
        work->field_440 = arg2->field_0;
        work->field_444 = -1;
        func_800B3F84(&work->anim, D_actor_323300_80174A70[work->field_440], (GpAnimObj*)ext,
                      work->pad_30C, work->slots);
    }
    if (arg2->field_4 != work->field_444) {
        work->field_444 = arg2->field_4;
        if (arg2->field_8 != 0 && work->field_43C != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->anim, i, work->field_444, 0, arg2->field_C);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->anim, i, work->field_444);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
        work->field_43C = 1;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80163840);
