#include "common.h"

#include "actors/actor_323300.h"
#include "actors/actors_shared_80132808.h"

#include "gameplay/3CD8.h"

#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/tmd.h"

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

    if (GameFlag_GetNibble(0x60) != 0 || (work = memCalloc(0x504, 0)) == NULL) {
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
    obj->coord    = extra->coords + 1;
    obj->ctx.recs = &work->rec;
    obj->key      = 0x30000;
    obj->pos.vx   = 0;
    obj->pos.vy   = 0;
    obj->pos.vz   = 0;
    obj->radius   = 0x100;
    obj->flags    = 1;
    Gp_LinkObj(2, obj);
    obj->flags |= 0x8000;
    Gp_InitRec18Table(obj->ctx.recs, 1, 0);
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
            if (work->slots[1].flags & 2) {
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
            Gp_DrawEffGroundQuad((VECTOR3*)&vec, 0x200, Gp_State1C->groundShade);
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
INCLUDE_RODATA("actors/nonmatchings/actor_323300/actor_323300", D_actor_323300_80161E24);
