#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

/// Work block allocated by `func_actor_361100_80162D28` and
/// `func_actor_361100_80163410` (`memCalloc(0x4A4)`)
/// and parked in that task's `Task::work` slot -- that slot is not a
/// `TaskIdMap` here. `func_actor_361100_80162E04` and
/// `func_actor_361100_801634B4` republish the two matrices
/// onto `TmdObject::lightMtx` / `colorMtx`, the light/colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`.
///
/// The size is the allocation, and the fields below are the ones the inits
/// seed: the three `sb` bytes at 0x43D/0x43E/0x4A2 are set to -1, and
/// `func_actor_361100_80162D28` also clears the three words at 0x480..0x488.
///
/// The six words at 0x480..0x498 are two groups of three, four bytes apart
/// within a group and twelve between them: `func_actor_361100_801630D4` writes
/// only the 0x490 group, `func_actor_361100_80162F58` clears both, and the two
/// words that fall between them (0x48C, 0x49C) are never touched by anything in
/// this overlay, which is the `pad` slot of a `VECTOR` apiece.
///
/// `field_4A0` is the halfword the 0x7DB handler `func_actor_361100_80163750`
/// arms alongside the first group, next door to the byte
/// `func_actor_361100_80163670` writes.
///
/// The block opens with the model's rig and model state.
typedef struct Actor361100Work {
    ActorAnimRig19   rig;
    ActorModelState  model;
    /* 0x480 */ s32  field_480;
    /* 0x484 */ s32  field_484;
    /* 0x488 */ s32  field_488;
    /* 0x48C */ byte pad_48C[0x4];
    /* 0x490 */ s32  field_490;
    /* 0x494 */ s32  field_494;
    /* 0x498 */ s32  field_498;
    /* 0x49C */ byte pad_49C[0x4];
    /* 0x4A0 */ s16  field_4A0;
    /* 0x4A2 */ s8   field_4A2;
    /* 0x4A3 */ byte pad_4A3[0x1];
} Actor361100Work;
STATIC_ASSERT_SIZEOF(Actor361100Work, 0x4A4);

extern u8  D_801156F9;
extern s32 D_8007107C;
extern s32 D_8016069C;
extern u8  D_80071090;

extern Task* D_actor_361100_80171BE0;

extern TaskDesc D_actor_361100_80165C58;

/// Script pair handed to `Gp_SpawnScript18` on every even frame of the blink.
extern s32 D_actor_361100_80166AD0;
extern s32 D_actor_361100_80166AD8;

extern void*      D_actor_361100_8016BAE0[];
extern GpMsgEntry D_actor_361100_8016BAF0[];
extern void*      D_actor_361100_80171BA8[];
extern u8         D_actor_361100_80171BB8[];

void func_80138C9C(ActorEffectState* state);
void func_801353D0(ActorEffectState* state, GpCoord* coord);

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_361100_80161FF8(Task* arg0);
void func_actor_361100_80162B18(Task* task);
void func_actor_361100_80162D28(Task* arg0);
void func_actor_361100_80162DE4(Task* arg0);
void func_actor_361100_80162E04(Task* arg0);
void func_actor_361100_801631C4(Task* task);
void func_actor_361100_80163410(Task* arg0);
void func_actor_361100_80163494(Task* arg0);
void func_actor_361100_801634B4(Task* arg0);

/// `Task::state` handlers `func_actor_361100_80162CBC` dispatches through.
const TaskFuncTable3 D_actor_361100_80161E24 = {
    {
        func_actor_361100_80162D28,
        func_actor_361100_80162B18,
        func_actor_361100_80162DE4,
    },
};

/// `Task::state` handlers `func_actor_361100_801633A4` dispatches through.
const TaskFuncTable3 D_actor_361100_80161E30 = {
    {
        func_actor_361100_80163410,
        func_actor_361100_801631C4,
        func_actor_361100_80163494,
    },
};

/// Runs while `D_8006D868` reports a streaming write in flight -- it is `-1`
/// until `Fs_LoadFile` has a chunk, and the mode byte in `gGameSession->at4.loc.view`
/// then picks this actor's part in the load: 11 hands the task to
/// `func_actor_361100_80161FF8`, 12 publishes the stream position `D_8016069C`
/// (half the remaining 0x18000-byte window past the write pointer, times the
/// per-chunk rate) and uploads the coordinate, and 10 exits the task.
///
/// State 0 allocates the `ActorEffectState` trail block into
/// `Task::work`, seeds its `field_8E` / `field_E0` halfwords and ticks it 0x1E
/// times, then resets the actor's root matrix to identity with the fixed
/// translation (0x1CA2, 0x712, 0x189C) and parks the view coordinate in its
/// `sub` slot. A failed allocation takes the exit call and is *not* branched
/// around: the block pointer is NULL for the rest of the state, as it was in
/// the original.
void func_actor_361100_80161E3C(Task* arg0)
{
    ActorEffectState* state;
    GpCoord*          coord;
    MATRIX*           mtx;
    s32               i;
    s32               writePtr;
    u32               streamLeft;
    u8*               modePtr;
    u8                mode;

    state   = (ActorEffectState*)arg0->work;
    modePtr = &gGameSession->at4.loc.view;
    coord   = arg0->extra.tmd->coords;
    if (D_8006D868 != -1) {
        streamLeft  = 0x18000 - D_8006D868;
        streamLeft &= ~7;
        writePtr    = (s32)D_8005C374 + D_8006D868;
        if (arg0->state == 0) {
            state = memCalloc(sizeof(ActorEffectState), false);
            if (state == NULL) {
                Task_CallExit(arg0);
                i = 0;
            }
            arg0->work      = (TaskIdMap*)state;
            state->field_E0 = 1;
            state->field_8E = 1;
            i               = 0;
            do {
                func_80138C9C(state);
                i += 1;
            } while (i < 0x1E);
            coord->sub                   = &gGfxViewCoord;
            mtx                          = &coord->coord;
            *(s32*)&coord->coord.m[0][0] = 0x1000;
            *(s32*)&mtx->m[0][2]         = 0;
            *(s32*)&mtx->m[1][1]         = 0x1000;
            *(s32*)&mtx->m[2][0]         = 0;
            mtx->m[2][2]                 = 0x1000;
            coord->coord.t[0]            = 0x1CA2;
            coord->coord.t[1]            = 0x712;
            coord->coord.t[2]            = 0x189C;
            coord->flg                   = 0;
            arg0->state                 += 1;
        }
        mode = *modePtr;
        if (mode == 11) {
            func_actor_361100_80161FF8(arg0);
            return;
        } else if (mode == 12) {
            D_8016069C = writePtr + (D_8007107C * ((s32)(streamLeft + (streamLeft >> 0x1F)) >> 1));
            func_80138C9C(state);
            func_801353D0(state, coord);
            return;
        } else if (mode == 10) {
            Task_CallExit(arg0);
        }
    }
}

/// Rotates `v` in place by `m` through the GTE, working from a stack copy so
/// the load and the store can name the same vector.
static inline void _actor361100RotTrans(MATRIX* m, SVECTOR* v)
{
    SVECTOR tmp;

    tmp = *v;
    gte_SetRotMatrix(m);
    gte_ldv0(&tmp);
    gte_rtv0();
    gte_stsv(v);
}

/// Draws the refraction ripple over screen rows 0x50..0xEF while more than
/// 0x6680 bytes of the 0x18000-byte window past `D_8006D868` remain free,
/// building `POLY_FT4` strips downward from the `D_8005C374` side of it (half
/// the free space further in when `DisplayState::otBuffer` is set). Each row is
/// projected through the transposed view matrix to get its ordering-table
/// depth, and the strip samples the other display buffer (`otBuffer` picks the
/// texture page row and the v offset) displaced by a wave built from `rsin` /
/// `rcos` of two phases seeded from `Task::killCountdown`, which the task
/// advances by 0x20 per call while `Gp_StateF0.field_4` is clear.
///
/// The row window, fade, clip and mode locals are fixed values in this build,
/// so only the default arm of each mode switch ever runs.
///
/// Matching notes: `xNeg` is read uninitialised by the mode 2/3 arms and
/// `spare` is never assigned; both only exist in the register allocator's view
/// (the first adds one instruction to `z`'s live range, the second is a stack
/// slot the retail frame carries). The `do { } while (0)` around the wave latch
/// raises the loop weight of `wave`'s references so it outranks the two masks
/// for `$t3`, as retail does.
void func_actor_361100_80161FF8(Task* arg0)
{
    DisplayState*          disp;
    OverlayRippleScratch*  block;
    OverlayRippleScratch** slot;
    POLY_FT4*              prim;
    s32                    left;
    s32                    adj;
    s32                    ptr;
    s32                    otBuf;
    s32                    mode;
    s32                    shift;
    s32                    ang2;
    s32                    ang;
    s32                    y;
    s32                    yTop;
    s32                    x0;
    s32                    x1;
    s32                    nprims;
    s32                    clip;
    s32                    otOff;
    s32                    fade;
    s32                    scale;
    s32                    xNeg;
    s32                    wave;
    s32                    wave1;
    s32                    baseY;
    s32                    one;
    s32                    dist;
    s32                    z;
    s32                    otz;
    s32                    i;
    s32                    yOff;
    s32                    fadeLen;
    s32                    xMin;
    s32                    xMax;
    s32                    xLeft;
    s32                    xRight;
    s32                    xL;
    s32                    xR;
    s32                    v;
    s32                    edge;
    s32                    sine;
    s32                    cosine;
    u16                    spare;

    left    = 0x18000 - D_8006D868;
    left   &= -8;
    adj     = left - 0x18000;
    ptr     = (s32)D_8005C374 - adj;
    disp    = &gDisplayState;
    otBuf   = disp->otBuffer;
    mode    = 0;
    shift   = mode;
    clip    = 0;
    scale   = 0x1000;
    fade    = 0x1000;
    baseY   = 0x50;
    one     = 1;
    otOff   = 0;
    yOff    = 0;
    fadeLen = 8;
    xMin    = -0xA0;
    xMax    = 0xA0;
    xLeft   = -0xA0;
    xRight  = 0xA0;
    if ((u32)left >= 0x6680U) {
        if (otBuf != 0) {
            ptr += left >> 1;
        }
        prim = (POLY_FT4*)ptr - 1;
        if (Gp_StateF0.field_4 == 0) {
            arg0->killCountdown = (u16)arg0->killCountdown + 0x20;
        }
        ang2                                        = arg0->killCountdown * 2;
        ang                                         = arg0->killCountdown;
        slot                                        = (OverlayRippleScratch**)SCRATCH_HEAD_ADDR;
        SCRATCH_HEAD_AT(slot, OverlayRippleScratch) = SCRATCH_HEAD_AT(slot, OverlayRippleScratch) - 1;
        block                                       = SCRATCH_HEAD_AT(slot, OverlayRippleScratch);
        TransposeMatrix(&gGfxViewCoord.workm, &block->mtx);
        block->origin.vx = gGfxViewCoord.workm.t[0];
        block->origin.vy = gGfxViewCoord.workm.t[1];
        block->origin.vz = gGfxViewCoord.workm.t[2];
        _actor361100RotTrans(&block->mtx, &block->origin);
        block->depth  = block->origin.vy + 0x712;
        block->depth *= disp->screenDistance;
        block->row.vx = 0;
        block->row.vz = disp->screenDistance;
        gte_SetRotMatrix(&block->mtx);
        y = 0x50;
        do {
            yTop          = y - 0x78;
            block->row.vy = yTop;
            gte_ldv0(&block->row);
            gte_rtv0();
            x0     = xMin;
            x1     = xMax;
            nprims = 1;
            if (clip > 0) {
                if (y < 8) {
                    x1 = x0 + shift;
                    if (shift <= 0) {
                        x1 = xMax;
                        x0 = x1 + shift;
                    }
                    if (clip < y) {
                        nprims = 2;
                    }
                }
            } else {
                if ((clip < 0) && (-clip < y)) {
                    x0 = xLeft;
                    if (shift > 0) {
                        x1 = x0 + shift;
                    } else {
                        x1 = xRight;
                        x0 = x1 + shift;
                    }
                }
            }
            sine    = rsin(ang2);
            cosine  = rcos(ang + 0x134);
            sine   += 0x2000;
            wave1   = cosine + sine;
            wave1 >>= 9;
            if (fade == 0) {
                wave1 = (wave1 * scale) >> 12;
                do {
                    wave  = wave1;
                    wave1 = wave + 1;
                } while (0);
            } else {
                do {
                    wave  = wave1;
                    wave1 = wave + 1;
                } while (0);
            }
            if (baseY != 1) {
                dist = y - baseY;
                if (dist < fadeLen) {
                    wave1  = wave >> ((fadeLen - dist) >> one);
                    wave1 += one;
                }
            }
            gte_stsv(&block->rowView);
            if (block->rowView.vy > 0) {
                otz   = block->depth / block->rowView.vy;
                otz >>= 2;
            } else {
                otz = 0x3FFF;
            }
            v    = yTop + 0x78 + wave1;
            z    = otz;
            otz  = ((z << D_80071090) & 0x3FFF) >> 4;
            otz += otOff;
            if (v >= 0xEF) {
                v = 0x1DC - v;
            }
            if (mode == 1) {
                xNeg = -0xA0;
                if (y < 0x7D) {
                    x0 = xNeg;
                    x1 = 0xA0;
                } else {
                    x0 = xNeg;
                    if (y < 0xB3) {
                        nprims = 2;
                    }
                    x1 = -0x59;
                }
            } else if (mode == 2) {
                if (y < 0x83) {
                    x0 = xNeg;
                    x1 = 0xA0;
                } else {
                    if (y < 0xB7) {
                        nprims = 2;
                        x0     = 0x57;
                    } else {
                        x0 = 0x57;
                    }
                    x1 = 0xA0;
                }
            } else if (mode == 3) {
                nprims = 1;
                if (y < 0x43) {
                    x0 = xNeg;
                    x1 = 0xA0;
                } else {
                    nprims = 2;
                }
            }
            i = 0;
            if (nprims != 0) {
                do {
                    if (mode == 1) {
                        if (i != 0) {
                            x0 = 0x3C;
                            x1 = 0xA0;
                        }
                    } else if (mode == 2) {
                        if (i == 1) {
                            x0 = xNeg;
                            x1 = -0x69;
                        }
                    } else if (mode == 3) {
                        if (i == 0) {
                            if (y < 0x43) {
                                x0 = xNeg;
                                x1 = 0xA0;
                            } else {
                                x0 = xNeg;
                                x1 = -0x57;
                            }
                        } else {
                            if (y < 0xC1) {
                                x0 = 0x5D;
                                x1 = 0xA0;
                            } else {
                                x0 = 0x2A;
                                x1 = 0xA0;
                            }
                        }
                    } else if (i == 1) {
                        if (y - yOff < fadeLen) {
                            wave1 = wave >> ((fadeLen - (y - yOff)) >> one);
                            v     = yTop + 0x79 + wave1;
                            if (v >= 0xEF) {
                                v = 0x1DC - v;
                            }
                        }
                        edge = shift - 0x140;
                        if (shift <= 0) {
                            edge = shift + 0x140;
                        }
                        x0 = xMin;
                        if (edge > 0) {
                            x1 = edge + x0;
                        } else {
                            x1 = xMax;
                            x0 = edge + x1;
                        }
                    }
                    if (x1 > 0) {
                        prim++;
                        prim->y1    = yTop;
                        prim->y0    = yTop;
                        prim->y3    = yTop + 1;
                        prim->y2    = yTop + 1;
                        prim->tpage = getTPage(2, 0, 0x80, otBuf << 8);
                        xL          = x0;
                        if (x0 < 0) {
                            xL = 0;
                        }
                        prim->x2 = xL;
                        prim->x0 = xL;
                        prim->u2 = xL + 0x20;
                        prim->u0 = xL + 0x20;
                        prim->x3 = x1;
                        prim->x1 = x1;
                        prim->u3 = x1 + 0x20;
                        prim->u1 = x1 + 0x20;
                        prim->v1 = v + (otBuf << 4);
                        prim->v0 = v + (otBuf << 4);
                        prim->v3 = v + (otBuf << 4) + 1;
                        prim->v2 = v + (otBuf << 4) + 1;
                        setlen(prim, 9);
                        setcode(prim, 0x2D);
                        addPrim(&gGpuCurrentOt[otz], prim);
                    }
                    if (x0 <= 0) {
                        prim++;
                        prim->y1    = yTop;
                        prim->y0    = yTop;
                        prim->y2    = (prim->y3 = yTop + 1);
                        prim->tpage = getTPage(2, 0, 0, otBuf << 8);
                        xR          = x1;
                        if (x1 > 0) {
                            xR = 0;
                        }
                        prim->u2 = x0 - 0x60;
                        prim->u0 = x0 - 0x60;
                        prim->u3 = (x0 - 0x60) + (xR - x0);
                        prim->u1 = (x0 - 0x60) + (xR - x0);
                        prim->x2 = x0;
                        prim->x0 = x0;
                        prim->x3 = xR;
                        prim->x1 = xR;
                        prim->v1 = v + (otBuf << 4);
                        prim->v0 = v + (otBuf << 4);
                        prim->v3 = v + (otBuf << 4) + 1;
                        prim->v2 = v + (otBuf << 4) + 1;
                        setlen(prim, 9);
                        setcode(prim, 0x2D);
                        addPrim(&gGpuCurrentOt[otz], prim);
                    }
                    i += 1;
                } while (i < nprims);
            }
            ang2 += 0x1F + (spare >> 16);
            if (z >= 0x301) {
                ang += 0xC5 + (z - 0x300) / 4;
            } else {
                ang += 0xC5;
            }
            y += 1;
        } while (y < 0xF0);
        SCRATCH_POP_BYTES(sizeof(OverlayRippleScratch));
    }
}

/// Head-aim state of the actor, run only while `D_801156F9` is clear: a looker
/// task that is missing, or a target task that is, parks the state machine on
/// -1. State 0 allocates the `GpHeadAim` record into `Task::work` and
/// seeds its clamps to 0x300 yaw and 0x200 pitch; state 1 ramps its `rate` up
/// toward 0x1000 while `Task::spawnArg1` is set and back down toward 0 while it
/// is not, then hands the record to `func_800B17D4` between the slot-3 task
/// (`gameGetPtrSlot(3)`, the skeleton whose head turns) and the
/// `Gp_LookupSlot4(2)` task it turns toward. Every other state kills the task
/// and clears `D_actor_361100_80171BE0`. State 0 reaching a NULL allocation
/// falls out of its own `if` into that kill, rather than into state 1.
void func_actor_361100_801627D4(Task* task)
{
    Task*      looker;
    Task*      target;
    GpHeadAim* aim;
    u16        rate;

    looker = gameGetPtrSlot(3);
    target = (Task*)Gp_LookupSlot4(2);
    if (D_801156F9 == 0) {
        if ((looker == NULL) || (target == NULL)) {
            task->state = -1;
        }
        switch (task->state) {
            case 0:
                aim = memCalloc(sizeof(GpHeadAim), false);
                if (aim != NULL) {
                    task->work      = (TaskIdMap*)aim;
                    aim->yawLimit   = 0x300;
                    aim->pitchLimit = 0x200;
                    task->state++;
                        /* fallthrough */
                    case 1:
                        aim = (GpHeadAim*)task->work;
                        if (task->spawnArg1 != 0) {
                            rate      = aim->rate + 0x100;
                            aim->rate = rate;
                            if ((s16)rate >= 0x1001) {
                                aim->rate = 0x1000;
                            }
                        } else {
                            rate      = aim->rate - 0x100;
                            aim->rate = rate;
                            if ((s16)rate < 0) {
                                aim->rate = 0;
                            }
                        }
                        func_800B17D4(looker, target, aim);
                        return;
                }
                /* fallthrough */
            default:
                taskKill(task);
                D_actor_361100_80171BE0 = NULL;
                break;
        }
    }
}

/// Record handler (opcode 0x0D) of the actor's script data: queues the
/// replacing load of overlay 0x82.
void func_actor_361100_8016291C(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Record handler (opcode 0x0D) of the actor's script data: queues the load
/// of overlay 0x81.
void func_actor_361100_8016293C(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Record handler (opcode 0x0D) of the actor's script data: restores the
/// stream random-number state.
void func_actor_361100_8016295C(void)
{
    Gp_RestoreStreamRng();
}

/// Record handler (opcode 0x0D) of the actor's script data: cancels the queued
/// CD command and restarts the CD queue.
void func_actor_361100_8016297C(void)
{
    CdCmd_CancelReplaceAndActivate();
}

void func_actor_361100_8016299C(void)
{
    D_actor_361100_80171BE0 = Task_SpawnFromTable(&D_actor_361100_80165C58, 0, 0, 0);
}

void func_actor_361100_801629D0(s32 arg0)
{
    if (D_actor_361100_80171BE0 != NULL) {
        if (arg0 < 2) {
            if (arg0 >= 0) {
                D_actor_361100_80171BE0->spawnArg1 = arg0;
                return;
            }
        }
        taskKill(D_actor_361100_80171BE0);
        D_actor_361100_80171BE0 = NULL;
    }
}

void func_actor_361100_80162A24(s32 arg0)
{
    Task_SpawnFromTable(&D_actor_361100_80165C58, 1, arg0, 0);
}

void func_actor_361100_80162A54(Task* arg0)
{
    s32 countdown;

    countdown       = arg0->spawnArg1 - 1;
    arg0->spawnArg1 = countdown;
    if (countdown > 0) {
        Display_ClampField126((countdown & 1) ? 0 : -1);
        Gp_SpawnScript18((s32)&D_actor_361100_80166AD0, (s32)&D_actor_361100_80166AD8);
    }
    if ((arg0->spawnArg1 <= 0) || (gGameSession->evtSkipped != 0)) {
        Display_ClampField126(0);
        taskKill(arg0);
    }
}

/// Record handler (opcode 0x0D) of the actor's script data, taking the
/// record's argument word: ORs it into `GameSession::flowFlags` (the script
/// passes 1 and 2).
void func_actor_361100_80162AEC(s32 bits)
{
    gGameSession->flowFlags |= bits;
}

void func_actor_361100_80162B0C(void)
{
    D_actor_361100_80171BE0 = 0;
}

/// Per-frame tick of the armed variant: integrates the 16.16 accumulator at
/// 0x480 three words at a time into the root part's local translation -- whole
/// part onto `coord.t`, then it is truncated back to its fraction -- runs the
/// `field_4A0` countdown that zeroes the 0x490 step while it is at 0, ticks the
/// animation slots once `model.ticking` has latched, and while the part is visible
/// rebuilds its world matrix and hands the result to `Gp_UpdateActorColor`.
/// `field_4A2` counts the root part's buffers down to the free.
///
/// The twin of `func_actor_361100_801631C4` with the two groups swapped: that
/// one integrates the 0x490 group `func_actor_361100_801630D4` arms and draws
/// a ground shadow under the second part, this one the 0x480 group
/// `func_actor_361100_80163750` arms.
void func_actor_361100_80162B18(Task* task)
{
    TmdObject*       ext  = task->extra.tmd;
    Actor361100Work* work = (Actor361100Work*)task->work;
    GpCoord*         coord;
    VECTOR           pos;
    s32              i;

    coord              = ext->coords;
    work->field_480   += work->field_490;
    work->field_484   += work->field_494;
    work->field_488   += work->field_498;
    coord->coord.t[0] += (s16)(work->field_480 >> 16);
    coord->coord.t[1] += (s16)(work->field_484 >> 16);
    coord->coord.t[2] += (s16)(work->field_488 >> 16);
    coord->flg         = 0;
    work->field_480    = (u16)work->field_480;
    work->field_484    = (u16)work->field_484;
    work->field_488    = (u16)work->field_488;
    if (work->field_4A0 >= 0) {
        if (work->field_4A0 == 0) {
            work->field_490 = 0;
            work->field_494 = 0;
            work->field_498 = 0;
        }
        work->field_4A0--;
    }
    if (work->model.ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
    if (!(ext->flags & 0x80)) {
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        pos.vx = coord->workm.t[0];
        pos.vy = coord->workm.t[1];
        pos.vz = coord->workm.t[2];
        Gp_UpdateActorColor(task->spawnArg2, &pos, 0, 0);
    }
    if (work->field_4A2 >= 0) {
        if (work->field_4A2 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_4A2--;
    }
}

/// State dispatcher: runs `Task::state` through `D_actor_361100_80161E24` --
/// setup (`func_actor_361100_80162D28`), per-frame tick (`func_actor_361100_80162B18`)
/// and exit (`func_actor_361100_80162DE4`) -- while `Gp_StateF0.field_4` is clear.
void func_actor_361100_80162CBC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_361100_80161E24;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// Spawn callback: allocates the work block into `Task::work`, seeds the
/// three -1 bytes, clears the first vector accumulator and arms the spawn
/// argument `GpEnemy` with the coordinate's root matrix, then enters the
/// `func_actor_361100_80162E04` state with `D_actor_361100_8016BAF0`
/// installed at `Task::msgTable`. The task exits through
/// `func_actor_361100_80162DE4` if the allocation fails.
void func_actor_361100_80162D28(Task* arg0)
{
    Actor361100Work* work;
    GpCoord*         coord;
    GpEnemy*         enemy;

    enemy = arg0->spawnArg2;
    coord = arg0->extra.tmd->coords;

    work = (Actor361100Work*)memCalloc(sizeof(Actor361100Work), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->work         = (TaskIdMap*)work;
    work->model.animId = -1;
    work->model.bank   = -1;
    work->field_4A2    = -1;
    work->field_480    = 0;
    work->field_484    = 0;
    work->field_488    = 0;

    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    enemy->recs     = 0;

    func_actor_361100_80162E04(arg0);
    TOUCH_REG(enemy);

    arg0->msgTable     = D_actor_361100_8016BAF0;
    arg0->exitCallback = func_actor_361100_80162DE4;
    arg0->state       += 1;
}

void func_actor_361100_80162DE4(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_361100_80162E04(Task* arg0)
{
    TmdObject*       ext;
    Actor361100Work* work;

    work          = (Actor361100Work*)arg0->work;
    ext           = arg0->extra.tmd;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

/// Message 0x7D3 handler, listed in `D_actor_361100_8016BAF0` -- the table
/// `func_actor_361100_80162D28` installs at `Task::msgTable`, and the twin of
/// `D_actor_361100_80171BB8` where `func_actor_361100_801634D0` serves the same
/// id. Re-seeds the whole animation slot array through `func_800B3F84` -- off
/// this variant's bank table, `D_actor_361100_8016BAE0` -- whenever the
/// preset's bank index changes.
///
/// Where its twin stores the preset's animation id unconditionally, this one
/// gates on it: an unchanged `field_4` skips the slot re-seed, the tick pass
/// and the `model.ticking` latch alike. That gate is also the only reason this body
/// differs from `func_actor_361100_801634D0` at all; the loops and the
/// short-circuit on `field_8` / `model.ticking` are the same code.
s32 func_actor_361100_80162E20(Task* task, s32 arg1, GpAnimArg* msg)
{
    Actor361100Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor361100Work*)task->work;
    ext  = task->extra.tmd;
    if (msg->animBlock.index != work->model.bank) {
        work->model.bank   = msg->animBlock.index;
        work->model.animId = -1;
        func_800B3F84(&work->rig.anim, D_actor_361100_8016BAE0[work->model.bank], ext, work->rig.poses,
                      work->rig.slots);
    }
    if (msg->field_4 != work->model.animId) {
        work->model.animId = msg->field_4;
        if (msg->field_8 != 0 && work->model.ticking != 0) {
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&work->rig.anim, i, work->model.animId, 0, msg->field_C);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                Gp_AnimResetSlot(&work->rig.anim, i, work->model.animId);
            }
        }
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
        work->model.ticking = 1;
    }
    return 0;
}

/// Places the actor at `placement`: drops the opcode's translation straight
/// into the root part's local matrix, stores its Euler angles in the
/// coordinate's own `rot` slot and rebuilds the rotation from them with
/// `RotMatrixZYX`. Clearing `flg` makes `_gpUpdateCoordTree` recompute the
/// world matrix from it, and the six words the body then clears are the work
/// block's two vector accumulators.
s32 func_actor_361100_80162F58(Task* task, s32 arg1, GpXformArg* placement)
{
    GpCoord*         coord;
    Actor361100Work* work;

    work                = (Actor361100Work*)task->work;
    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->param.rot.vx = placement->rot.vx;
    coord->param.rot.vy = placement->rot.vy;
    coord->param.rot.vz = placement->rot.vz;
    RotMatrixZYX(&coord->param.rot, &coord->coord);
    coord->flg      = 0;
    work->field_480 = 0;
    work->field_484 = 0;
    work->field_488 = 0;
    work->field_490 = 0;
    work->field_494 = 0;
    work->field_498 = 0;
    return 0;
}

s32 func_actor_361100_80162FF4(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra.tmd;
    ret = 0;
    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags                               |= 0x80;
            ((Actor361100Work*)task->work)->field_4A2 = mode;
            obj->flags                               |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Message 0x7DB handler, listed in `D_actor_361100_8016BAF0` -- the table
/// `func_actor_361100_80162D28` installs at `Task::msgTable`, and the twin of
/// `D_actor_361100_80171BB8` where `func_actor_361100_80163750` serves the same
/// id. 0 parks the actor, clearing the work block's second vector accumulator;
/// 1, 2 and 3 arm it with one of three preset vectors and the halfword at
/// `field_4A0`; every other sub-command exits the task through its own
/// `Task::exitCallback`.
s32 func_actor_361100_801630D4(Task* task, s32 arg1, GpCmdArg* msg)
{
    Actor361100Work* work;

    work = (Actor361100Work*)task->work;
    switch (msg->command) {
        case 0:
            work->field_490 = 0;
            work->field_494 = 0;
            work->field_498 = 0;
            break;
        case 1:
            work->field_490 = 0xFFF6CCCD;
            work->field_494 = 0xFEC13334;
            work->field_498 = 0xB9999;
            work->field_4A0 = 0x19;
            break;
        case 2:
            work->field_490 = 0xFFCC13B2;
            work->field_494 = 0xFF559D8A;
            work->field_498 = 0x1C7627;
            work->field_4A0 = 0x1A;
            break;
        case 3:
            work->field_490 = 0x606DB6;
            work->field_494 = 0x1C4DB6D;
            work->field_498 = 0xFED84925;
            work->field_4A0 = 0xE;
            break;
        default:
            task->exitCallback(task);
            break;
    }
    return 0;
}

/// Per-frame tick of the actor: integrates the 16.16 accumulator at 0x490
/// three words at a time into the root part's local translation -- whole part
/// onto `coord.t`, then it is truncated back to its fraction -- runs the
/// `field_4A0` countdown that zeroes the 0x480 step while it is at 1, ticks the
/// animation slots once `model.ticking` has latched, and while the part is visible
/// draws its ground shadow, rebuilds the second part's world matrix from it and
/// re-ranks it through `func_800D7A9C`. `field_4A2` counts the second part's
/// buffers down to the free. Every use of the second part's coordinate
/// (`TmdObject::coords[1]`) is re-read from `task`, not cached.
void func_actor_361100_801631C4(Task* task)
{
    TmdObject*       ext  = task->extra.tmd;
    Actor361100Work* work = (Actor361100Work*)task->work;
    GpCoord*         coord;
    VECTOR3          pos;
    s32              i;

    coord              = ext->coords;
    work->field_490   += work->field_480;
    work->field_494   += work->field_484;
    work->field_498   += work->field_488;
    coord->coord.t[0] += (s16)(work->field_490 >> 16);
    coord->coord.t[1] += (s16)(work->field_494 >> 16);
    coord->coord.t[2] += (s16)(work->field_498 >> 16);
    coord->flg         = 0;
    work->field_490    = (u16)work->field_490;
    work->field_494    = (u16)work->field_494;
    work->field_498    = (u16)work->field_498;
    if (work->field_4A0 > 0) {
        if (work->field_4A0 == 1) {
            work->field_480 = 0;
            work->field_484 = 0;
            work->field_488 = 0;
        }
        work->field_4A0--;
    }
    if (work->model.ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
    if (!(ext->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)task->extra.tmd->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, Gp_State1C->groundShade);
        }
        task->extra.tmd->coords[1].flg = 0;
        Gp_UpdateCoord(&task->extra.tmd->coords[1]);
        func_800D7A9C(ext, (VECTOR*)task->extra.tmd->coords[1].workm.t, 0, 3);
    }
    if (work->field_4A2 >= 0) {
        if (work->field_4A2 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_4A2--;
    }
}

/// State dispatcher: runs `Task::state` through `D_actor_361100_80161E30` --
/// setup (`func_actor_361100_80163410`), per-frame tick (`func_actor_361100_801631C4`)
/// and exit (`func_actor_361100_80163494`) -- while `Gp_StateF0.field_4` is clear.
void func_actor_361100_801633A4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_361100_80161E30;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

void func_actor_361100_80163410(Task* arg0)
{
    Actor361100Work* work;

    work = (Actor361100Work*)memCalloc(sizeof(Actor361100Work), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->work         = (TaskIdMap*)work;
    work->model.animId = -1;
    work->model.bank   = -1;
    work->field_4A2    = -1;
    func_actor_361100_801634B4(arg0);
    arg0->msgTable     = D_actor_361100_80171BB8;
    arg0->exitCallback = func_actor_361100_80163494;
    arg0->state       += 1;
}

void func_actor_361100_80163494(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_361100_801634B4(Task* arg0)
{
    TmdObject*       ext;
    Actor361100Work* work;

    ext           = arg0->extra.tmd;
    work          = (Actor361100Work*)arg0->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

/// Message 0x7D3 handler, listed in `D_actor_361100_80171BB8` next to the
/// spawn states. Re-seeds the whole animation slot array through
/// `func_800B3F84` whenever the preset's bank index changes, stores the
/// preset's animation id, then either re-seeds every slot through
/// `func_800B4114` (preset `field_8` set and the slots already started once)
/// or resets them through `Gp_AnimResetSlot`, and finally ticks the whole
/// array with `Gp_AnimTickIndex`.
s32 func_actor_361100_801634D0(Task* task, s32 arg1, GpAnimArg* msg)
{
    Actor361100Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor361100Work*)task->work;
    ext  = task->extra.tmd;
    if (msg->animBlock.index != work->model.bank) {
        work->model.bank   = msg->animBlock.index;
        work->model.animId = -1;
        func_800B3F84(&work->rig.anim, D_actor_361100_80171BA8[work->model.bank], ext, work->rig.poses,
                      work->rig.slots);
    }
    work->model.animId = msg->field_4;
    if (msg->field_8 != 0 && work->model.ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            func_800B4114(&work->rig.anim, i, work->model.animId, 0, msg->field_C);
        }
    } else {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimResetSlot(&work->rig.anim, i, work->model.animId);
        }
    }
    for (i = 1; i < 0x13; i++) {
        Gp_AnimTickIndex(&work->rig.anim, i);
    }
    work->model.ticking = 1;
    return 0;
}

/// Message 0x7D4 handler, listed in `D_actor_361100_80171BB8`: places the
/// model at once. Writes the payload's translation into the root coordinate,
/// keeps its Euler angles in the coordinate's `rot` slot and rebuilds the
/// rotation from them with `RotMatrix`, then clears `flg` so the world matrix
/// is recomputed.
s32 func_actor_361100_801635F4(Task* task, s32 arg1, GpXformArg* placement)
{
    GpCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->param.rot.vx = placement->rot.vx;
    coord->param.rot.vy = placement->rot.vy;
    coord->param.rot.vz = placement->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

s32 func_actor_361100_80163670(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra.tmd;
    ret = 0;
    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags                               |= 0x80;
            ((Actor361100Work*)task->work)->field_4A2 = mode;
            obj->flags                               |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Message 0x7DB handler, listed in `D_actor_361100_80171BB8`, the table the
/// task installs at `Task::msgTable`. 0 parks the actor, clearing the work
/// block's first vector accumulator; 1 arms it, dropping 0x2D000 into the
/// accumulator's middle word and 0xA0 into `field_4A0`; every other sub-command
/// exits the task through its own `Task::exitCallback`.
s32 func_actor_361100_80163750(Task* task, s32 msgId, GpCmdArg* msg)
{
    Actor361100Work* work;

    work = (Actor361100Work*)task->work;
    switch (msg->command) {
        case 0:
            work->field_480 = 0;
            work->field_484 = 0;
            work->field_488 = 0;
            work->field_4A0 = 0;
            break;
        case 1:
            work->field_484 = 0x2D000;
            work->field_480 = 0;
            work->field_488 = 0;
            work->field_4A0 = 0xA0;
            break;
        default:
            task->exitCallback(task);
            break;
    }
    return 0;
}
