#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgte.h>

/// Per-call work block carved from the scratchpad stack. Nothing in the body
/// touches the last 0x10 bytes; the size is the amount the scratch head moves.
typedef struct {
    MATRIX  mtx;
    SVECTOR in;
    SVECTOR out;
    SVECTOR trans;
    s32     depth;
    u8      _pad[0x10];
} _ShelterR48RippleScratch;

#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

extern TaskDesc D_8014B958;

extern s32 D_8007107C;
extern u8  D_80071090;
extern u8  D_801153F4;

static inline void _shelterR48RotTrans(MATRIX* m, SVECTOR* v)
{
    SVECTOR tmp;

    tmp = *v;
    gte_SetRotMatrix(m);
    gte_ldv0(&tmp);
    gte_rtv0_real();
    gte_stsv(v);
}

void func_shelter_r48_8017D660(Task* arg0)
{
    DisplayState*              disp;
    TmdObject*                 tmd;
    _ShelterR48RippleScratch*  block;
    _ShelterR48RippleScratch** slot;
    POLY_FT4*                  prim;
    u8*                        ptr;
    s32                        otBuf;
    s32                        view;
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
    s32                        start;
    s32                        end;
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

    tmd   = (TmdObject*)arg0->extra;
    otBuf = D_8007107C;
    view  = gGameSession->at4.loc.view;
    if (GameFlag_GetNibble(0x100) == 0) {
        return;
    }
    shift = 0;
    switch (view) {
        case 2:
            start = 0xC3;
            end   = 0xF0;
            xMin  = 1;
            xMax  = 0xA0;
            break;
        case 3:
            start = 0x68;
            end   = 0x88;
            xMin  = 0;
            xMax  = 0x44;
            break;
        case 4:
            start = 0x84;
            end   = 0xAA;
            xMin  = -0xB;
            xMax  = 0x3E;
            break;
        case 6:
            start = 0xC2;
            end   = 0xF0;
            xMin  = 4;
            xMax  = 0xA0;
            break;
        case 7:
        case 18:
            start = 0x6A;
            end   = 0x8A;
            xMin  = -5;
            xMax  = 0x46;
            break;
        case 8:
            start = 0x7B;
            end   = 0xAB;
            xMin  = -0x11;
            xMax  = 0x78;
            break;
        default:
            return;
    }
    mode    = 0;
    clip    = 0;
    scale   = 0x1000;
    fade    = 0x1000;
    otOff   = 0;
    yOff    = 0;
    fadeLen = 8;
    xLeft   = -0xA0;
    xRight  = 0xA0;
    if (arg0->state == 0) {
        tmd->flags &= ~4;
        Tmd_AllocBuffers(tmd);
        arg0->state++;
    }
    disp = &gDisplayState;
    ptr  = tmd->buffer;
    if (disp->otBuffer != 0) {
        ptr += 0x4000;
    }
    prim = (POLY_FT4*)ptr - 1;
    if (D_801153F4 == 0) {
        arg0->killCountdown = (u16)arg0->killCountdown + 0x20;
    }
    ang2  = arg0->killCountdown * 2;
    ang   = arg0->killCountdown;
    slot  = (_ShelterR48RippleScratch**)G_SCRATCH_HEAD;
    *slot = *slot - 1;
    block = *slot;
    TransposeMatrix(&gGfxViewCoord.workm, &block->mtx);
    block->trans.vx = gGfxViewCoord.workm.t[0];
    block->trans.vy = gGfxViewCoord.workm.t[1];
    block->trans.vz = gGfxViewCoord.workm.t[2];
    _shelterR48RotTrans(&block->mtx, &block->trans);
    block->depth  = block->trans.vy + 0xD02;
    block->depth *= disp->screenDistance;
    block->in.vx  = 0;
    block->in.vz  = disp->screenDistance;
    gte_SetRotMatrix(&block->mtx);
    for (y = start; y < end; y++) {
        yTop         = y - 0x78;
        block->in.vy = yTop;
        gte_ldv0(&block->in);
        gte_rtv0_real();
        x0     = xMin;
        x1     = xMax;
        nprims = 1;
        if (clip > 0) {
            if (y < clip + 8) {
                x0 = xLeft;
                if (shift > 0) {
                    x1 = x0 + shift;
                } else {
                    x1 = xRight;
                    x0 = x1 + shift;
                }
                if (clip < y) {
                    nprims = 2;
                }
            }
        } else if ((clip < 0) && (-clip < y)) {
            x0 = xLeft;
            if (shift > 0) {
                x1 = x0 + shift;
            } else {
                x1 = xRight;
                x0 = x1 + shift;
            }
        }
        sine    = rsin(ang2);
        cosine  = rcos(ang + 0x134);
        sine   += 0x2000;
        wave1   = cosine + sine;
        wave1 >>= 9;
        if (fade == 0) {
            wave1 = (wave1 * scale) >> 12;
        }
        wave  = wave1;
        wave1 = wave + 1;
        if (start != 1) {
            dist = y - start;
            if (dist < fadeLen) {
                do {
                    do {
                        do {
                            do {
                                wave1  = wave >> ((fadeLen - dist) >> 1);
                                wave1 += 1;
                            } while (0);
                        } while (0);
                    } while (0);
                } while (0);
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
                        wave1 = wave >> ((fadeLen - (y - yOff)) >> 1);
                        v     = yTop + 0x79 + wave1;
                        if (v >= 0xEF) {
                            v = 0x1DC - v;
                        }
                    }
                    edge = shift - 0x140;
                    if (shift <= 0) {
                        edge = shift + 0x140;
                    }
                    x0 = xLeft;
                    if (edge > 0) {
                        x1 = edge + x0;
                    } else {
                        x1 = xRight;
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
        ang2 += 0x1F;
        if (z >= 0x301) {
            ang += 0xC5 + (z - 0x300) / 4;
        } else {
            ang += 0xC5;
        }
    }
    *(u8**)G_SCRATCH_HEAD += sizeof(_ShelterR48RippleScratch);
}

s32 func_shelter_r48_8017DF50(s32 arg0, s32 arg1, s32 arg2)
{
    GpObj4C* node;
    s32      found;
    s32      ret;

    ret = 0;
    if (arg2 == 0x121 || arg2 == 0x122) {
        if (GameFlag_GetNibble(0x100) == 1) {
            node  = Gp_PendingObj4C;
            found = 1;
            while (node != NULL) {
                if (node->field_46 == 5 && node->field_48 == 0xFF && node->field_4B != 0) {
                    goto check;
                }
                node = node->next;
            }
            found = 0;
        check:
            if (found != 0) {
                gGameSession->eventState = 1;
                D_80115768               = 1;
                Task_SpawnOnDefaultList(&D_8014B958, 0, 0, 0);
                GameFlag_SetNibble(0x100, 2);
                GameFlag_SetNibble(0x12A, 4);
                ret = 1;
            }
        }
    }
    return ret;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_r48/shelter_r48", D_shelter_r48_8017D608);
