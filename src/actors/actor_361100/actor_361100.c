#include "common.h"

INCLUDE_RODATA("actors/nonmatchings/actor_361100/actor_361100", D_actor_361100_80161E24);
INCLUDE_RODATA("actors/nonmatchings/actor_361100/actor_361100", D_actor_361100_80161E30);

#include <psyq/inline_c.h>

#include "actors/actor_361100.h"

#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/gfx.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

extern u8 D_801156F9;

extern Task* D_actor_361100_80171BE0;

extern s32 D_8005C374;
extern s32 D_8006D868;
extern s32 D_8007107C;
extern s32 D_8016069C;
extern u8  D_80071090;
extern u8  D_801153F4;

void func_80138C9C(Actor361100EffectState* state);
void func_801353D0(Actor361100EffectState* state, GsCOORDINATE2* coord);

void func_actor_361100_80161FF8(Task* arg0);

/// Runs while `D_8006D868` reports a streaming write in flight -- it is `-1`
/// until `Fs_LoadFile` has a chunk, and the mode byte in `gGameSession->at4.loc.view`
/// then picks this actor's part in the load: 11 hands the task to
/// `func_actor_361100_80161FF8`, 12 publishes the stream position `D_8016069C`
/// (half the remaining 0x18000-byte window past the write pointer, times the
/// per-chunk rate) and uploads the coordinate, and 10 exits the task.
///
/// State 0 allocates the `Actor361100EffectState` trail block into
/// `Task::work`, seeds its `field_8E` / `field_E0` halfwords and ticks it 0x1E
/// times, then resets the actor's root matrix to identity with the fixed
/// translation (0x1CA2, 0x712, 0x189C) and parks the view coordinate in its
/// `sub` slot. A failed allocation takes the exit call and is *not* branched
/// around: the block pointer is NULL for the rest of the state, as it was in
/// the original.
void func_actor_361100_80161E3C(Task* arg0)
{
    Actor361100EffectState* state;
    GsCOORDINATE2*          coord;
    MATRIX*                 mtx;
    s32                     i;
    s32                     writePtr;
    u32                     streamLeft;
    u8*                     modePtr;
    u8                      mode;

    state   = (Actor361100EffectState*)arg0->work;
    modePtr = &gGameSession->at4.loc.view;
    coord   = ((TmdObject*)arg0->extra)->coords;
    if (D_8006D868 != -1) {
        streamLeft  = 0x18000 - D_8006D868;
        streamLeft &= ~7;
        writePtr    = D_8005C374 + D_8006D868;
        if (arg0->state == 0) {
            state = memCalloc(sizeof(Actor361100EffectState), false);
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
    gte_rtv0_real();
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
/// advances by 0x20 per call while `D_801153F4` is clear.
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
    DisplayState*              disp;
    Actor361100RippleScratch*  block;
    Actor361100RippleScratch** slot;
    POLY_FT4*                  prim;
    s32                        left;
    s32                        adj;
    s32                        ptr;
    s32                        otBuf;
    s32                        mode;
    s32                        shift;
    s32                        ang2;
    s32                        ang;
    s32                        y;
    s32                        yTop;
    s32                        x0;
    s32                        x1;
    s32                        nprims;
    s32                        clip;
    s32                        otOff;
    s32                        fade;
    s32                        scale;
    s32                        xNeg;
    s32                        wave;
    s32                        wave1;
    s32                        baseY;
    s32                        one;
    s32                        dist;
    s32                        z;
    s32                        otz;
    s32                        i;
    s32                        yOff;
    s32                        fadeLen;
    s32                        xMin;
    s32                        xMax;
    s32                        xLeft;
    s32                        xRight;
    s32                        xL;
    s32                        xR;
    s32                        v;
    s32                        edge;
    s32                        sine;
    s32                        cosine;
    u16                        spare;

    left    = 0x18000 - D_8006D868;
    left   &= -8;
    adj     = left - 0x18000;
    ptr     = D_8005C374 - adj;
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
        if (D_801153F4 == 0) {
            arg0->killCountdown = (u16)arg0->killCountdown + 0x20;
        }
        ang2  = arg0->killCountdown * 2;
        ang   = arg0->killCountdown;
        slot  = (Actor361100RippleScratch**)G_SCRATCH_HEAD;
        *slot = *slot - 1;
        block = *slot;
        TransposeMatrix(&gGfxViewCoord.workm, &block->mtx);
        block->trans.vx = gGfxViewCoord.workm.t[0];
        block->trans.vy = gGfxViewCoord.workm.t[1];
        block->trans.vz = gGfxViewCoord.workm.t[2];
        _actor361100RotTrans(&block->mtx, &block->trans);
        block->depth  = block->trans.vy + 0x712;
        block->depth *= disp->screenDistance;
        block->in.vx  = 0;
        block->in.vz  = disp->screenDistance;
        gte_SetRotMatrix(&block->mtx);
        y = 0x50;
        do {
            yTop         = y - 0x78;
            block->in.vy = yTop;
            gte_ldv0(&block->in);
            gte_rtv0_real();
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
            gte_stsv(&block->out);
            if (block->out.vy > 0) {
                otz   = block->depth / block->out.vy;
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
        *(u8**)G_SCRATCH_HEAD += sizeof(Actor361100RippleScratch);
    }
}

void func_actor_361100_801627D4(Task* task)
{
    Task*               looker;
    Task*               target;
    Actor361100HeadAim* aim;
    u16                 rate;

    looker = gameGetPtrSlot(3);
    target = (Task*)Gp_LookupSlot4(2);
    if (D_801156F9 == 0) {
        if ((looker == NULL) || (target == NULL)) {
            task->state = -1;
        }
        switch (task->state) {
            case 0:
                aim = memCalloc(sizeof(Actor361100HeadAim), false);
                if (aim != NULL) {
                    task->work      = (TaskIdMap*)aim;
                    aim->yawLimit   = 0x300;
                    aim->pitchLimit = 0x200;
                    task->state++;
                        /* fallthrough */
                    case 1:
                        aim = (Actor361100HeadAim*)task->work;
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
                        func_800B17D4(looker, target, (GpHeadAim*)aim);
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
