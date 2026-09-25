#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"

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

/// 0x2C-byte scratch block `func_shelter_r48_80181C14` takes from
/// `G_SCRATCH_HEAD`: the coordinate's world position, the tip point offset from
/// it, and both points' projections. `otz0`/`sx0`/`sy0` belong to `base`,
/// `otz1`/`sx1`/`sy1` to `tip`; `r0`/`r1` are the wedge radii at each end.
typedef struct {
    SVECTOR base;
    SVECTOR tip;
    s32     otz0;
    s32     otz1;
    s32     flag;
    s32     r0;
    s32     r1;
    u16     sx0;
    u16     sy0;
    u16     sx1;
    u16     sy1;
} _ShelterR48BeamScratch;

/// Per-band radius and height offsets `func_shelter_r48_8017F124` adds to the
/// effect work's ring parameters: `rInner` widens the inner ring, `rExtra` the
/// outer ring on top of the step, `yOff` raises the inner ring.
typedef struct {
    s16 rInner;
    s16 yOff;
    s16 rExtra;
} _ShelterR48RingScale;

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);
void    func_80132F58(s32 arg0);
s32     func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

extern TaskDesc D_8014B958;
extern TaskDesc D_8014B964;
extern s32      D_8014BD48;
extern s32      D_8014C288;
extern s32      D_8014C540;
extern s32      D_8014CAF8;
extern s32      D_8014D158;

extern s32 D_8007107C;
extern u8  D_80071090;

/// The room's message table, installed on the room task.
extern GpMsgEntry D_shelter_r48_80182FB8[];

extern SVECTOR              D_shelter_r48_8018300C;
extern u8                   D_shelter_r48_8018BE54[6][16];
extern _ShelterR48RingScale D_shelter_r48_80182FE8[];

void func_shelter_r48_8017E1A4(Task* arg0);
void func_shelter_r48_8017E214(Task* task);
void func_shelter_r48_8017FB7C(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_shelter_r48_8017FF74(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);
void func_shelter_r48_8017F124(GpEffWork* work, GsCOORDINATE2* coord, s32 part);
void func_shelter_r48_8018258C(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_r48_80180804(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_shelter_r48_80180C5C(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_shelter_r48_80181C14(GsCOORDINATE2* coord, s16 size, s32 yaw, s32 color);

static inline void _shelterR48RotTrans(MATRIX* m, SVECTOR* v)
{
    SVECTOR tmp;

    tmp = *v;
    gte_SetRotMatrix(m);
    gte_ldv0(&tmp);
    gte_rtv0();
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
    if (Gp_StateF0.field_4 == 0) {
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
        gte_rtv0();
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

/// State handlers of the room task `func_shelter_r48_8017E224` runs: its
/// setup, an idle state, and `taskKill`.
const TaskFuncTable3 D_shelter_r48_8017D608 = {
    { func_shelter_r48_8017E1A4, func_shelter_r48_8017E214, taskKill }
};

/// Message-table handler for message 0x13EE: copies the incoming record onto
/// the outgoing one and passes both on to `func_80179A04`. Always answers 1.
s32 func_shelter_r48_8017E044(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}

/// Message-table handler for message 0x13F0: does nothing and answers 0.
s32 func_shelter_r48_8017E088(void)
{
    return 0;
}

s32 func_shelter_r48_8017E090(Task* task, s32 msgId, RoomEventMsg* in)
{
    if (in->field_2 == 1) {
        switch (GameFlag_GetNibble(0x100)) {
            case 0:
                Gp_RunCapCmd1(6);
                break;
            case 1:
                Gp_RunCapCmd1(7);
                break;
        }
    }
    return 0;
}

s32 func_shelter_r48_8017E0EC(void)
{
    if (GameFlag_GetNibble(0x12A) == 1) {
        func_800E8614((s32)&D_8014D158, 0);
        GameFlag_SetNibble(0x12A, 2);
    } else {
        func_800E8634((s32)&D_8014C540, 0, (s32)&D_8014CAF8);
        Task_SpawnFromTable(&D_8014B964, 0, 0, 0);
        Task_SpawnFromTable(&D_8014B964, 0, 1, 0);
        Task_SpawnFromTable(&D_8014B964, 0, 3, 0);
        GameFlag_SetNibble(0x12A, 3);
    }
    return 0;
}

void func_shelter_r48_8017E1A4(Task* arg0)
{
    arg0->msgTable = D_shelter_r48_80182FB8;
    Game_SetPtrSlot(arg0, 7);
    func_80132F58(0);
    func_800E8634((s32)&D_8014BD48, 0, (s32)&D_8014C288);
    GameFlag_SetNibble(0x12A, 1);
    arg0->state = (s32)(arg0->state + 1);
}

/// The room task's idle state. It reserves a stack frame it never uses.
void func_shelter_r48_8017E214(Task* task)
{
    char pad[0x10];
}

/// Runs one tick of the room task through the three-state table
/// `D_shelter_r48_8017D608`, copying the table onto the stack and calling the
/// entry for the task's current state.
void func_shelter_r48_8017E224(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_r48_8017D608;
    sp.funcs[task->state](task);
}

void func_shelter_r48_8017E27C(u8 arg0)
{
    GpAreaKey* loc = &gGameSession->at4.loc;
    GpSprtRec* rec = Gp_SprtTables[loc->stage - 1]->field_0[loc->area - 1];
    GpSprtCmd* cmd;

    if (arg0 == 0) {
        cmd            = rec[1].field_4;
        cmd[1].field_4 = 1;
        cmd[3].field_4 = 1;
        cmd            = rec[2].field_4;
        cmd[1].field_4 = 1;
        cmd[4].field_4 = 1;
        cmd            = rec[3].field_4;
        cmd[1].field_4 = 1;
        cmd[4].field_4 = 1;
        cmd            = rec[5].field_4;
        cmd[2].field_4 = 1;
        cmd[3].field_4 = 1;
        cmd            = rec[6].field_4;
        cmd[1].field_4 = 1;
        cmd[4].field_4 = 1;
        cmd            = rec[7].field_4;
        cmd[1].field_4 = 1;
        cmd[5].field_4 = 1;
        cmd            = rec[17].field_4;
        cmd[1].field_4 = 1;
        cmd[5].field_4 = 1;
    } else if (arg0 == 1) {
        cmd            = rec[1].field_4;
        cmd[1].field_4 = 0;
        cmd[3].field_4 = 0;
        cmd            = rec[2].field_4;
        cmd[1].field_4 = 0;
        cmd[4].field_4 = 0;
        cmd            = rec[3].field_4;
        cmd[1].field_4 = 0;
        cmd[4].field_4 = 0;
        cmd            = rec[5].field_4;
        cmd[2].field_4 = 0;
        cmd[3].field_4 = 0;
        cmd            = rec[6].field_4;
        cmd[1].field_4 = 0;
        cmd[4].field_4 = 0;
        cmd            = rec[7].field_4;
        cmd[1].field_4 = 0;
        cmd[5].field_4 = 0;
        cmd            = rec[17].field_4;
        cmd[1].field_4 = 0;
        cmd[5].field_4 = 0;
    }
}

void func_shelter_r48_8017E3B8(Task* task)
{
    s32 viewMask;
    s32 i;
    s32 j;

    viewMask = 1 << Gp_GetViewIndex();
    if (task->state == 0) {
        Gp_State1C->groundTrace = 0;
        for (i = 0; i < 6; i++) {
            for (j = 0; j < 16; j++) {
                D_shelter_r48_8018BE54[i][j] = (Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16;
            }
        }
        task->state = 1;
    }
    if (viewMask & 0x401D8) {
        if (GameFlag_GetNibble(0x100) == 1) {
            func_shelter_r48_8018258C(&D_shelter_r48_8018300C, 0x100, 0x5C40);
        } else if (GameFlag_GetNibble(0x100) == 2) {
            func_shelter_r48_8018258C(&D_shelter_r48_8018300C, 0x100, 0x504C);
        }
    }
}

void func_shelter_r48_8017E4C4(Task* arg0)
{
    RoomEffWork*   mem;
    GsCOORDINATE2* coord;
    MATRIX*        m;
    s32            i;

    mem   = (RoomEffWork*)arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_r48_8017FF74(coord, ((s16)mem->field_22 / 2) & 0xFFFF, 0x380);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    if (arg0->state == 0) {
        m                            = &coord->coord;
        coord->sub                   = mem->field_8;
        *(s32*)&coord->coord.m[0][0] = 0x1000;
        *(s32*)&m->m[0][2]           = 0;
        *(s32*)&m->m[1][1]           = 0x1000;
        *(s32*)&m->m[2][0]           = 0;
        m->m[2][2]                   = 0x1000;
        coord->coord.t[2]            = 0;
        coord->coord.t[1]            = 0;
        coord->coord.t[0]            = 0;
        coord->flg                   = 0;
        Gp_UpdateCoord(coord);
        arg0->state = 1;
    }
    mem->field_22 += 1;
    switch (arg0->spawnArg1) {
        case 0:
            Gp_SpawnEff(0x6018B, coord, 0x14002400, NULL);
            arg0->spawnArg1 = 1;
            return;
        case 1:
            func_shelter_r48_8017FF74(coord, ((s16)mem->field_22 / 2) & 0xFFFF, 0x380);
            if (!(mem->field_22 & 1)) {
                Gp_SpawnEff(0x6018B, coord, 0x1001400, NULL);
            }
            mem->field_22 += 1;
            return;
        case 2:
            Gp_SpawnEff(0x6018B, coord, 0x10002380, NULL);
            for (i = 0; i < 4; i++) {
                Gp_SpawnEff(0x6018B, coord, 0x2002400, NULL);
                Gp_SpawnEff(0x6018C, coord, 0x2202300, NULL);
            }
            arg0->spawnArg1 = 3;
            return;
        case 3:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
    }
}

void func_shelter_r48_8017E704(Task* arg0)
{
    RoomEffWork*   mem;
    GsCOORDINATE2* coord;
    MATRIX*        m;

    mem   = (RoomEffWork*)arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_r48_80180804(coord, ((s16)((s16)mem->field_22 / 2) % 12) & 0xFFFF, 0x800, 0);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    if (arg0->state == 0) {
        m                            = &coord->coord;
        coord->sub                   = mem->field_8;
        *(s32*)&coord->coord.m[0][0] = 0x1000;
        *(s32*)&m->m[0][2]           = 0;
        *(s32*)&m->m[1][1]           = 0x1000;
        *(s32*)&m->m[2][0]           = 0;
        m->m[2][2]                   = 0x1000;
        coord->coord.t[2]            = 0;
        coord->coord.t[1]            = 0;
        coord->coord.t[0]            = 0;
        coord->flg                   = 0;
        Gp_UpdateCoord(coord);
        arg0->state = 1;
    }
    mem->field_22 += 1;
    switch (arg0->spawnArg1) {
        case 0:
            Gp_SpawnEff(0x6018B, coord, 0x14002800, NULL);
            arg0->spawnArg1 = 1;
            return;
        case 1:
            func_shelter_r48_80180804(coord, ((s16)((s16)mem->field_22 / 2) % 12) & 0xFFFF, 0x800, 0);
            if (!(mem->field_22 & 1)) {
                Gp_SpawnEff(0x6018B, coord, 0x12801800, NULL);
            }
            mem->field_22 += 1;
            return;
        case 2:
            if ((s16)((s16)mem->field_22 % 6) == 0) {
                Gp_SpawnEff(0x6018C, coord, 0x2802800, NULL);
            }
            if (!(mem->field_22 & 1)) {
                Gp_SpawnEff(0x6018B, coord, 0x12803800, NULL);
            }
            return;
    }
}

void func_shelter_r48_8017E9B8(Task* arg0)
{
    RoomEffWork*   mem;
    GsCOORDINATE2* coord;
    MATRIX*        m;

    mem   = (RoomEffWork*)arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_r48_80180804(coord, ((s16)((s16)mem->field_22 / 2) % 12 | 0x1000) & 0xFFFF, 0xA00, 0);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    if (arg0->state == 0) {
        m                            = &coord->coord;
        coord->sub                   = mem->field_8;
        *(s32*)&coord->coord.m[0][0] = 0x1000;
        *(s32*)&m->m[0][2]           = 0;
        *(s32*)&m->m[1][1]           = 0x1000;
        *(s32*)&m->m[2][0]           = 0;
        m->m[2][2]                   = 0x1000;
        coord->coord.t[2]            = 0;
        coord->coord.t[1]            = 0;
        coord->coord.t[0]            = 0;
        coord->flg                   = 0;
        Gp_UpdateCoord(coord);
        arg0->state = 1;
    }
    mem->field_22 += 1;
    switch (arg0->spawnArg1) {
        case 0:
            Gp_SpawnEff(0x6018C, coord, 0x94002A00, NULL);
            arg0->spawnArg1 = 1;
            return;
        case 1:
            func_shelter_r48_80180804(coord, ((s16)((s16)mem->field_22 / 2) % 12 | 0x1000) & 0xFFFF, 0x800, 0);
            if (!(mem->field_22 & 1)) {
                Gp_SpawnEff(0x6018C, coord, 0x92801800, NULL);
            }
            mem->field_22 += 1;
            return;
        case 2:
            if (!(mem->field_22 & 1)) {
                Gp_SpawnEff(0x6018C, coord, 0x92603C00, NULL);
            }
            return;
    }
}

void func_shelter_r48_8017EC18(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState < 4) {
        work->age++;
        switch (task->state) {
            case 0:
                rot               = (GpMtxWords*)&coord->coord;
                coord->sub        = work->parent;
                rot->w0           = 0x1000;
                rot->w1           = 0;
                rot->w2           = 0x1000;
                rot->w3           = 0;
                rot->h4           = 0x1000;
                coord->coord.t[2] = 0;
                coord->coord.t[1] = 0;
                coord->coord.t[0] = 0;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                task->spawnArg1 = 0x5A;
                task->state     = 1;
                work->scale     = 0;
                work->angle     = 0x100;
                work->step      = 0x100 / task->spawnArg1;
            case 1:
                if (Gp_State1C->eventState != 0) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale;
                    rgb[2] = (u16)work->scale >> 2;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    if (work->scale >= 0x61) {
                        rgb[0] = work->period;
                        rgb[1] = work->period;
                        rgb[2] = (u16)work->period >> 2;
                        Gp_DrawArc(coord, (s16)(task->spawnArg1 << 8), 0x200, rgb);
                    }
                    return;
                }
                work->scale += work->step;
                work->angle += 0x18;
                task->spawnArg1--;
                rgb[0] = work->scale;
                rgb[1] = work->scale;
                rgb[2] = (u16)work->scale >> 2;
                Gp_DrawRing(coord, work->angle, rgb);
                Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                if (work->scale >= 0x61) {
                    work->period += (u16)work->step * 2;
                    rgb[0]        = work->period;
                    rgb[1]        = work->period;
                    rgb[2]        = (u16)work->period >> 2;
                    Gp_DrawArc(coord, (s16)(task->spawnArg1 << 8), 0x200, rgb);
                }
                if (task->spawnArg1 == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    Gp_SpawnEff(0x6018F, coord, 0, NULL);
                }
                return;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale;
                    rgb[2] = (u16)work->scale >> 2;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    if (Gp_State1C->eventState == 0) {
                        work->scale -= 0x10;
                        work->angle -= 0x60;
                    }
                    Gp_DrawFadeQuad(rgb, 1);
                    return;
                }
                break;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

void func_shelter_r48_8017EFD8(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_r48_8017F124(work, coord, 0);
        func_shelter_r48_8017F124(work, coord, 1);
        func_shelter_r48_8017F124(work, coord, 2);
        if (Gp_State1C->eventState < 4) {
            return;
        }
    } else {
        work->age++;
        if (task->state == 0) {
            work->age   = 1;
            work->scale = 0x80;
            task->state = 1;
        }
        if (work->scale >= 9) {
            work->scale -= 8;
            work->angle += 0x200;
            work->step  += 0x100;
            func_shelter_r48_8017F124(work, coord, 0);
            func_shelter_r48_8017F124(work, coord, 1);
            func_shelter_r48_8017F124(work, coord, 2);
            return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

/// Draws one of the ring bands of the effect as sixteen textured `POLY_FT4`
/// segments. Builds an inner and an outer 16-vertex ring in the XZ plane from
/// the work's radii plus the band's `D_shelter_r48_80182FE8` offsets, moves them
/// into world space through `coord`, then projects each segment and picks its
/// texture cell from the band's `D_shelter_r48_8018BE54` row and the work's age.
void func_shelter_r48_8017F124(GpEffWork* work, GsCOORDINATE2* coord, s32 part)
{
    void**                scratch;
    u8*                   head;
    GpBandScratch*        block;
    SVECTOR*              op;
    POLY_FT4*             prim;
    _ShelterR48RingScale* row;
    s32                   i;
    s32                   next;
    s32                   ang;
    s32                   u;
    u16                   idx;
    s16                   r0;
    s16                   r1;
    u16                   y;
    u16                   f28;

    row      = &D_shelter_r48_80182FE8[part];
    f28      = (u16)work->period;
    r1       = (u16)work->angle;
    y        = f28 + (u16)row->yOff;
    r1      += (u16)row->rInner;
    r0       = r1 + (u16)work->step + (u16)row->rExtra;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch;
    *scratch = head - 0x118;
    block    = (GpBandScratch*)(head - 0x118);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = -y;
        block->inner[i].vz = (rcos(ang) * r0) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = *(u16*)&block->inner[i].vx + *(u16*)&coord->workm.t[0];
        block->inner[i].vy = *(u16*)&block->inner[i].vy + *(u16*)&coord->workm.t[1];
        block->inner[i].vz = *(u16*)&block->inner[i].vz + *(u16*)&coord->workm.t[2];
        block->outer[i].vx = (rsin(ang) * r1) >> 12;
        op                 = &block->inner[i] + 16;
        op->vy             = 0;
        op->vz             = (rcos(ang) * r1) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = *(u16*)&block->outer[i].vx + *(u16*)&coord->workm.t[0];
        op->vy             = *(u16*)&op->vy + *(u16*)&coord->workm.t[1];
        op->vz             = *(u16*)&op->vz + *(u16*)&coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps();
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt();
        idx = (D_shelter_r48_8018BE54[part][i] + work->age) % 6;
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            prim->code = 0x2E;
            setRGB0(prim, *(u8*)&work->scale, *(u8*)&work->scale, *(u8*)&work->scale);
            prim->tpage = 0x2A;
            prim->clut  = part < 3 ? 0x42C6 : 0x4282;
            u           = idx * 0x28;
            setUV4(prim, u, 0x60, u + 0x27, 0x60, u, 0x87, u + 0x27, 0x87);
            prim->x0 = *(u16*)&block->sxy0.vx;
            prim->y0 = *(u16*)&block->sxy0.vy;
            prim->x1 = *(u16*)&block->sxy1.vx;
            prim->y1 = *(u16*)&block->sxy1.vy;
            prim->x2 = *(u16*)&block->sxy2.vx;
            prim->y2 = *(u16*)&block->sxy2.vy;
            prim->x3 = *(u16*)&block->sxy3.vx;
            prim->y3 = *(u16*)&block->sxy3.vy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x118;
}

/// Per-frame update of an effect task drawn with `func_shelter_r48_8017FB7C`
/// (state 1) or `func_shelter_r48_8017FF74` (state 2). State 0 seeds the work
/// from `spawnArg1`: the two draw parameters (the second one random), the frame
/// period and, when the spawner left `field_10` zero, a velocity chosen by bits
/// 24-27 of `spawnArg1`, normalised and scaled by `field_2A` through the GTE. Later ticks draw, drift
/// the coordinate by that velocity with `vy` growing by 6 each tick, and advance
/// the frame every `field_28` ticks, releasing the task after frame 7. While an
/// event is running the task only draws, and it is released once the event
/// state reaches 4.
void func_shelter_r48_8017F6C0(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            kind;
    s32            step;
    s32            state;
    s32            level;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            if (task->state < 2) {
                func_shelter_r48_8017FB7C(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);
            } else {
                func_shelter_r48_8017FF74(coord, work->field_20, (s16)work->field_24);
            }
            return;
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    Gp_UpdateCoord(coord);
    work->field_22++;
    switch (task->state) {
        case 0:
            work->field_24 = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->field_28 = step;
            work->field_22 = 0;
            state          = 1;
            if (task->spawnArg1 & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if (((u16)work->field_10.vx | (u16)work->field_10.vy | (u16)work->field_10.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->field_2A = level;
                kind           = ((GpEffSpawnArgHi*)&task->spawnArg1)->field_3;
                switch (kind & 0xF) {
                    case 0:
                        work->field_2A = 0;
                        break;
                    case 1:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->field_10.vx = work->field_18;
                        work->field_10.vy = work->field_1A;
                        work->field_10.vz = work->field_1C;
                        break;
                }
                vec = &work->field_10;
                VectorNormalSS(vec, vec);
                gte_lddp(work->field_2A);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->field_2A = 0x40;
            }
            return;
        case 1:
            func_shelter_r48_8017FB7C(coord, work->field_20, (s16)work->field_24, (s16)work->field_26);
            break;
        case 2:
            func_shelter_r48_8017FF74(coord, work->field_20, (s16)work->field_24);
            break;
        default:
            return;
    }
    if ((s16)work->field_2A != 0) {
        coord->coord.t[0] += work->field_10.vx;
        coord->coord.t[1] += work->field_10.vy;
        coord->coord.t[2] += work->field_10.vz;
        coord->flg         = 0;
        work->field_10.vy += 6;
    }
    if (((s16)work->field_22 % (s16)work->field_28) == 0) {
        work->field_20++;
        if ((s16)work->field_20 >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Projects the coordinate's world position and, if it is in front of the
/// camera, queues a semi-transparent `POLY_FT4` centred on it. `arg1` selects
/// a 32-texel column of the texture page, `arg3` is the quad's rotation and
/// `arg2` its size, divided by depth so the quad shrinks with distance.
void func_shelter_r48_8017FB7C(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              u1;
    s32              ang2;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = *(u16*)&arg0->workm.t[1];
    vz                                        = *(u16*)&arg0->workm.t[2];
    block->vec.vz                             = vz;
    *scratch                                  = block;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D3;
        u0          = arg1 << 5;
        u1          = u0 + 0x1F;
        setUV4(prim, u0, 0xE0, u1, 0xE0, u0, 0xFF, u1, 0xFF);
        block->dx = (((arg2 * 31) / block->otz) * rsin(arg3)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(arg3)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = arg3 + 0x400;
        block->dx = (((arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

void func_shelter_r48_8017FF74(GsCOORDINATE2* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw14Scratch* block;
    POLY_FT4*          prim;
    SVECTOR*           vec;
    DisplayState*      ds;
    s32                tex;
    u32                cell;
    s32                u1;
    s32                v0;
    s32                v1;
    s32                sarg;
    s32                t;
    s16                xy;
    u16                vz;

    scratch                                     = (void**)G_SCRATCH_HEAD;
    head                                        = *scratch;
    ((RoomDraw14Scratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                       = (RoomDraw14Scratch*)(head - 0x18);
    block->vec.vy                               = *(u16*)&arg0->workm.t[1];
    vz                                          = *(u16*)&arg0->workm.t[2];
    tex                                         = arg1;
    SOFT_TOUCH_REG(tex);
    *scratch      = block;
    block->vec.vz = vz;
    vec           = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw14Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw14Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw14Scratch*)(head - 0x18))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2C;
        cell        = (u16)tex;
        prim->clut  = 0x4393;
        tex         = (cell & 3) * 0x38;
        SOFT_BARRIER();
        v0            = ((cell & 7) >> 2) * 0x38;
        u1            = tex + 0x37;
        prim->v0      = v0;
        prim->v1      = v0;
        v1            = v0 + 0x37;
        prim->u1      = u1;
        prim->u3      = u1;
        sarg          = (s16)arg2;
        prim->v2      = v1;
        prim->v3      = v1;
        t             = sarg * 0x38;
        prim->u0      = tex;
        prim->u2      = tex;
        block->radius = (t - sarg) / block->otz;
        xy            = *(u16*)&block->sx - *(u16*)&block->radius;
        prim->x0 = prim->x2 = xy;
        xy                  = *(u16*)&block->sx + *(u16*)&block->radius;
        prim->x1 = prim->x3 = xy;
        xy                  = (*(u16*)&block->sy - *(u16*)&block->radius) - (block->radius >> 1);
        ds                  = &gDisplayState;
        prim->y0 = prim->y1 = xy;
        xy                  = *(u16*)&block->sy + (block->radius >> 1);
        prim->y2 = prim->y3 = xy;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}

/// Per-frame handler for one animated sprite effect, drawn by
/// `func_shelter_r48_80180804`, or by `func_shelter_r48_80180C5C` when any of
/// `spawnArg1`'s top four bits is set. Its first frame unpacks `spawnArg1`: the
/// low 12 bits are the sprite size, bits 12..14 the frames per animation cell
/// (1 when zero), and the sign bit becomes the drawer's clut selector. When the
/// work block arrives without a velocity, bits 24..27 choose how one is rolled
/// from `Gp_LcgState` (0 leaves it still) and it is normalised to a speed from
/// bits 16..23 (0x40 when zero). Each later frame draws the current cell,
/// moves the coordinate by the velocity and bends its Y component, then frees
/// the effect after the drawer's last cell (12 or 10). While the player is in
/// an event it only draws, and frees once the event aborts.
void func_shelter_r48_80180210(Task* task)
{
    RoomEffWork*   work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            step;
    s32            level;
    s32            zero;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (task->spawnArg1 < 0) {
            func_shelter_r48_80180C5C(coord, work->field_20 | work->field_18, work->field_24, work->field_26);
        } else {
            func_shelter_r48_80180804(coord, work->field_20 | work->field_18, work->field_24, work->field_26);
        }
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }
    work->field_22++;
    switch (task->state) {
        case 0:
            work->field_24 = task->spawnArg1 & 0xFFF;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_26 = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 7;
            } else {
                step = 1;
            }
            work->field_28 = step;
            work->field_22 = 0;
            task->state    = task->spawnArg1 & 0xF0000000 ? 2 : 1;
            zero           = 0;
            work->field_18 = (task->spawnArg1 < zero) << 12;
            if (((u16)work->field_10.vx | (u16)work->field_10.vy | (u16)work->field_10.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->field_2A = level;
                switch ((task->spawnArg1 >> 24) & 0xF) {
                    case 0:
                        work->field_2A = 0;
                        break;
                    case 1:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->field_10.vx = work->field_18;
                        work->field_10.vy = work->field_1A;
                        work->field_10.vz = work->field_1C;
                        break;
                    case 6:
                        work->field_10.vy = 0;
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
                        work->field_10.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                }
                vec = &work->field_10;
                VectorNormalSS(vec, vec);
                gte_lddp(work->field_2A);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->field_2A = 0x40;
            }
            break;
        case 1:
            func_shelter_r48_80180804(coord, work->field_20 | work->field_18, work->field_24, work->field_26);
            if ((s16)work->field_2A != 0) {
                coord->coord.t[0] += (s16)work->field_10.vx;
                coord->coord.t[1] += (s16)work->field_10.vy;
                coord->coord.t[2] += (s16)work->field_10.vz;
                coord->flg         = 0;
                work->field_10.vy -= 2;
            }
            if (((s16)work->field_22 % (s16)work->field_28) == 0) {
                work->field_20++;
                if ((s16)work->field_20 >= 12) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
        case 2:
            func_shelter_r48_80180C5C(coord, work->field_20 | work->field_18, work->field_24, work->field_26);
            if ((s16)work->field_2A != 0) {
                coord->coord.t[0] += (s16)work->field_10.vx;
                coord->coord.t[1] += (s16)work->field_10.vy;
                coord->coord.t[2] += (s16)work->field_10.vz;
                coord->flg         = 0;
                work->field_10.vy -= 1;
            }
            if (((s16)work->field_22 % (s16)work->field_28) == 0) {
                work->field_20++;
                if ((s16)work->field_20 >= 10) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
    }
}

void func_shelter_r48_80180804(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    u8*                head;
    RoomDraw19Scratch* block;
    POLY_FT4*          prim;
    u16                col;
    u16                row;
    s32                u0;
    s32                v0;
    s32                ang;
    s32                ang2;
    s32                bank;
    s32                idx;

    head                                        = *(u8**)G_SCRATCH_HEAD;
    ((RoomDraw19Scratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    *(void**)G_SCRATCH_HEAD                     = head - 0x1C;
    block                                       = *(RoomDraw19Scratch**)G_SCRATCH_HEAD;
    block->vec.vy                               = arg0->workm.t[1];
    block->vec.vz                               = arg0->workm.t[2];
    idx                                         = arg1 & 0xFFF;
    bank                                        = arg1 >> 12;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps();
    gte_stsxy(&((RoomDraw19Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw19Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw19Scratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2C;
        if (bank != 0) {
            prim->clut = 0x428F;
        } else {
            prim->clut = (idx & 0x3F) | 0x4380;
        }
        col = (u16)idx % 5;
        row = (u16)idx / 5;
        ang = arg3;
        u0  = col * 0x30;
        v0  = row * 0x30;
        setUV4(prim, u0, v0 + 0x70, u0 + 0x2F, v0 + 0x70, u0, v0 - 0x61, u0 + 0x2F, v0 - 0x61);
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(u8**)G_SCRATCH_HEAD += 0x1C;
}

void func_shelter_r48_80180C5C(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    u8*                head;
    RoomDraw19Scratch* block;
    POLY_FT4*          prim;
    u16                col;
    u16                row;
    s32                u0;
    s32                v0;
    s32                ang;
    s32                ang2;
    s32                bank;
    u16                idx;

    head                                        = *(u8**)G_SCRATCH_HEAD;
    ((RoomDraw19Scratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    *(void**)G_SCRATCH_HEAD                     = head - 0x1C;
    block                                       = *(RoomDraw19Scratch**)G_SCRATCH_HEAD;
    block->vec.vy                               = arg0->workm.t[1];
    block->vec.vz                               = arg0->workm.t[2];
    idx                                         = arg1 & 0xFFF;
    bank                                        = arg1 >> 12;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps();
    gte_stsxy(&((RoomDraw19Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw19Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw19Scratch*)(head - 0x1C))->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        if (bank != 0) {
            prim->clut = 0x428F;
        } else {
            prim->clut = 0x43D0;
        }
        col = (u16)idx % 5;
        row = (u16)idx / 5;
        ang = arg3;
        u0  = col * 0x30;
        v0  = row * 0x30;
        setUV4(prim, u0, v0 - 0x80, u0 + 0x2F, v0 - 0x80, u0, v0 - 0x51, u0 + 0x2F, v0 - 0x51);
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(u8**)G_SCRATCH_HEAD += 0x1C;
}

void func_shelter_r48_801810B0(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GpEffWork*     eff;
    u8             rgb[3];
    s32            step;
    s16            scale;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState < 4) {
        work->age++;
        switch (task->state) {
            case 0:
                coord->sub = work->parent;
                Gfx_RotMatrixX(&coord->coord, -0x400, 1);
                coord->coord.t[2] = 0;
                coord->coord.t[1] = 0;
                coord->coord.t[0] = 0;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                task->state = 1;
                work->scale = 0;
                work->angle = 0x100;
                work->step  = 0x100 / task->spawnArg1;
            case 1:
                if (Gp_State1C->eventState != 0) {
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 1;
                    rgb[2] = (u16)work->scale >> 2;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, (s16)((task->spawnArg1 % 10) * (work->scale << 2)), 0x100, rgb);
                    return;
                }
                scale        = work->scale;
                step         = (u16)work->step;
                work->scale  = scale + step;
                work->angle += (u16)work->step * 8;
                task->spawnArg1--;
                rgb[0] = work->scale;
                rgb[1] = (u16)work->scale >> 1;
                rgb[2] = (u16)work->scale >> 2;
                Gp_DrawRing(coord, work->angle, rgb);
                Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                Gp_DrawArc(coord, (s16)((task->spawnArg1 % 10) * (work->scale << 2)), 0x100, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    eff         = Gp_SpawnEff(0x60191, coord, 0, NULL);
                    if (eff != NULL) {
                        Task_Reparent(task, eff->task);
                    }
                }
                return;
            case 2:
                if (work->scale >= 5) {
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 1;
                    rgb[2] = (u16)work->scale >> 2;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    if (Gp_State1C->eventState == 0) {
                        work->scale -= 4;
                    }
                    Gp_DrawFadeQuad(rgb, 1);
                    return;
                }
                break;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

void func_shelter_r48_8018147C(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    MATRIX*        m;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_r48_8017F124(work, coord, 3);
        func_shelter_r48_8017F124(work, coord, 4);
        func_shelter_r48_8017F124(work, coord, 5);
        if (Gp_State1C->eventState < 4) {
            return;
        }
    } else {
        work->age++;
        switch (task->state) {
            case 0:
                m                            = &coord->coord;
                coord->sub                   = work->parent;
                *(s32*)&coord->coord.m[0][0] = 0x1000;
                *(s32*)&m->m[0][2]           = 0;
                *(s32*)&m->m[1][1]           = 0x1000;
                *(s32*)&m->m[2][0]           = 0;
                m->m[2][2]                   = 0x1000;
                coord->coord.t[2]            = 0;
                coord->coord.t[1]            = 0;
                coord->coord.t[0]            = 0;
                coord->flg                   = 0;
                Gp_UpdateCoord(coord);
                work->age   = 1;
                work->scale = 0x80;
                task->state = 1;
                return;
            case 1:
                func_shelter_r48_8017F124(work, coord, 3);
                func_shelter_r48_8017F124(work, coord, 4);
                func_shelter_r48_8017F124(work, coord, 5);
                work->angle  += 0x10;
                work->period += 0x10;
                work->step   += 0x10;
                rgb[0]        = work->scale;
                rgb[1]        = (u16)work->scale >> 1;
                rgb[2]        = (u16)work->scale >> 2;
                Gp_DrawFadeQuad(rgb, 1);
                if (work->age >= 0x31) {
                    task->state = 2;
                }
                return;
            case 2:
                if (work->scale >= 0x11) {
                    work->scale  -= 0x10;
                    work->angle  += 0x10;
                    work->period += 0x10;
                    work->step   += 0x10;
                    func_shelter_r48_8017F124(work, coord, 3);
                    func_shelter_r48_8017F124(work, coord, 4);
                    func_shelter_r48_8017F124(work, coord, 5);
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 1;
                    rgb[2] = (u16)work->scale >> 2;
                    Gp_DrawFadeQuad(rgb, 1);
                    return;
                }
                break;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

void func_shelter_r48_80181704(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    MATRIX*        m;
    u8             rgb[3];
    s32            step;
    s16            scale;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState < 4) {
        work->age++;
        switch (task->state) {
            case 0:
                m                            = &coord->coord;
                coord->sub                   = work->parent;
                *(s32*)&coord->coord.m[0][0] = 0x1000;
                *(s32*)&m->m[0][2]           = 0;
                *(s32*)&m->m[1][1]           = 0x1000;
                *(s32*)&m->m[2][0]           = 0;
                m->m[2][2]                   = 0x1000;
                coord->coord.t[2]            = 0;
                coord->coord.t[1]            = 0;
                coord->coord.t[0]            = 0;
                coord->flg                   = 0;
                Gp_UpdateCoord(coord);
                task->state     = 1;
                task->spawnArg1 = 0x1E;
                work->scale     = 0;
                work->angle     = 0x100;
                work->step      = 0x100 / task->spawnArg1;
            case 1:
                if (Gp_State1C->eventState != 0) {
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 2;
                    rgb[2] = (u16)work->scale >> 1;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, (s16)((u16)task->spawnArg1 * 16 + 0x800), 0x100, rgb);
                    rgb[0] >>= 1;
                    rgb[1] >>= 1;
                    rgb[2] >>= 1;
                    Gp_DrawArc(coord, (s16)((u16)task->spawnArg1 * 32 + 0xC00), 0xC0, rgb);
                    return;
                }
                scale        = work->scale;
                step         = (u16)work->step;
                work->scale  = scale + step;
                work->angle += (u16)work->step * 8;
                task->spawnArg1--;
                rgb[0] = work->scale;
                rgb[1] = (u16)work->scale >> 2;
                rgb[2] = (u16)work->scale >> 1;
                Gp_DrawRing(coord, work->angle, rgb);
                Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                Gp_DrawArc(coord, (s16)((u16)task->spawnArg1 * 16 + 0x800), 0x100, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                Gp_DrawArc(coord, (s16)((u16)task->spawnArg1 * 32 + 0xC00), 0xC0, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale  = 0xFF;
                    task->state  = 2;
                    work->period = 0x600;
                    work->step   = 0;
                }
                return;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 2;
                    rgb[2] = (u16)work->scale >> 1;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    if (Gp_State1C->eventState == 0) {
                        work->scale -= 0x10;
                        work->angle -= 0x60;
                    }
                    Gp_DrawFadeQuad(rgb, 1);
                } else {
                    task->state = 3;
                }
                func_shelter_r48_80181C14(coord, work->period, work->step, 0xC36);
                func_shelter_r48_80181C14(coord, work->period, (s16)-work->step, 0xC36);
                return;
            case 3:
                func_shelter_r48_80181C14(coord, work->period, work->step, 0xC36);
                func_shelter_r48_80181C14(coord, work->period, (s16)-work->step, 0xC36);
                if (Gp_State1C->eventState == 0) {
                    if (work->step < 0x200) {
                        work->period -= 0x18;
                        work->step   += 0x10;
                    } else {
                        task->state = 4;
                    }
                }
                return;
            case 4:
                if (work->period > 0) {
                    func_shelter_r48_80181C14(coord, work->period, work->step, 0xC36);
                    func_shelter_r48_80181C14(coord, work->period, (s16)-work->step, 0xC36);
                    if (Gp_State1C->eventState == 0) {
                        work->period -= 0x30;
                    }
                    return;
                }
                break;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

/// Offsets a point (0, 0x800, 0x1400) from `coord`, turned by `yaw` about Y
/// and then by the coordinate's world matrix, and projects both the
/// coordinate's world position and that tip through `GsWSMATRIX`. When both
/// project, it draws two passes of gouraud `POLY_G4` wedges, with radii of
/// `size * 64` and `size * 128` over each end's depth: every 0x400 step
/// across the half-turn facing along the screen-space line between the points
/// queues a fan at the tip, a fan at the base and a quad joining them. `color`
/// packs `0xRGB` nibbles, each shifted into its channel's high nibble, with
/// `gDisplayState.animFrame & 1` shifted into bit 4 of every channel.
void func_shelter_r48_80181C14(GsCOORDINATE2* coord, s16 size, s32 yaw, s32 color)
{
    MATRIX                  m;
    void**                  scratch;
    u8*                     head;
    _ShelterR48BeamScratch* block;
    POLY_G4*                prim;
    s32                     pass;
    u8                      r;
    u8                      g;
    u8                      b;
    s32                     limit;
    s32                     angStart;
    s32                     scaled;
    s32                     ang;
    s32                     next;
    s32                     mid;
    s32                     blend;
    s32                     tr;
    s32                     tg;

    scratch       = (void**)G_SCRATCH_HEAD;
    head          = *scratch;
    block         = (_ShelterR48BeamScratch*)(*scratch = head - 0x2C);
    block->tip.vy = 0x800;
    block->tip.vx = 0;
    block->tip.vz = 0x1400;
    Gfx_RotMatrixY(&m, (s16)yaw, 1);
    ang = color;
    gte_SetRotMatrix(&m);
    gte_ldv0(&block->tip);
    gte_rtv0();
    gte_stsv(&block->tip);
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&block->tip);
    gte_rtv0();
    gte_stsv(&block->tip);
    block->base.vx = coord->workm.t[0];
    block->base.vy = coord->workm.t[1];
    block->base.vz = coord->workm.t[2];
    block->tip.vx += block->base.vx;
    block->tip.vy += block->base.vy;
    block->tip.vz += block->base.vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->base);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(&block->tip);
        gte_rtps();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        gte_stszotz(&block->otz1);
        if (block->flag >= 0) {
            tr    = (((u16)ang >> 8) & 0xF) << 4;
            tg    = (((u16)ang >> 4) & 0xF) << 4;
            blend = (*(u8*)&gDisplayState.animFrame & 1) << 4;
            r     = tr + blend;
            g     = tg + blend;
            // Dead: never read before the loop sets it, but it has to be
            // here for `b` to read `color` rather than the copy in `ang`.
            ang = 0;
            b   = ((color & 0xF) << 4) + blend;
            for (pass = 1; pass < 3; pass++) {
                scaled    = size * (pass << 6);
                block->r0 = scaled / block->otz0;
                block->r1 = scaled / block->otz1;
                ang       = (s16)ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
                if (ang < ang + 0x800) {
                    angStart = ang;
                    limit    = ang + 0x800;
                    do {
                        prim           = (POLY_G4*)gGpuPrimCursor;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, 0, 0, 0);
                        prim->x0 = block->sx1 + ((block->r1 * rsin(ang + 0x800)) >> 12);
                        prim->y0 = block->sy1 + ((block->r1 * rcos(ang + 0x800)) >> 12);
                        prim->x1 = block->sx1 + ((block->r1 * rsin(ang + 0xA00)) >> 12);
                        prim->y1 = block->sy1 + ((block->r1 * rcos(ang + 0xA00)) >> 12);
                        prim->x2 = block->sx1;
                        prim->y2 = block->sy1;
                        prim->x3 = block->sx1 + ((block->r1 * rsin(ang + 0xC00)) >> 12);
                        prim->y3 = block->sy1 + ((block->r1 * rcos(ang + 0xC00)) >> 12);
                        addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                                prim);
                        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);

                        prim           = (POLY_G4*)gGpuPrimCursor;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, 0, 0, 0);
                        prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                        prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                        prim->x1 = block->sx0 + ((block->r0 * rsin(ang + 0x200)) >> 12);
                        prim->y1 = block->sy0 + ((block->r0 * rcos(ang + 0x200)) >> 12);
                        next     = ang + 0x400;
                        prim->x2 = block->sx0;
                        prim->y2 = block->sy0;
                        prim->x3 = block->sx0 + ((block->r0 * rsin(next)) >> 12);
                        prim->y3 = block->sy0 + ((block->r0 * rcos(next)) >> 12);
                        addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                                prim);
                        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                        prim           = (POLY_G4*)gGpuPrimCursor;
                        mid            = angStart + (ang - angStart) * 2;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, r, g, b);
                        prim->x0 = block->sx0 + ((block->r0 * rsin(mid)) >> 12);
                        prim->y0 = block->sy0 + ((block->r0 * rcos(mid)) >> 12);
                        prim->x1 = block->sx1 + ((block->r1 * rsin(mid)) >> 12);
                        prim->y1 = block->sy1 + ((block->r1 * rcos(mid)) >> 12);
                        prim->x2 = block->sx0;
                        prim->y2 = block->sy0;
                        prim->x3 = block->sx1;
                        prim->y3 = block->sy1;
                        addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                                prim);
                        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                        ang = next;
                    } while (ang < limit);
                }
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x2C;
}

/// Projects `arg0` through `Gfx_ViewWorldMtx` and, when the GTE flag is
/// non-negative, queues a glow of gouraud `POLY_G4` wedges around the projected
/// point: an outer ring pairing a half-brightness wedge at full radius with a
/// full-brightness one at half radius, then four inner cross wedges in the
/// half-brightness colour. The depth is pulled 0xC0 towards the camera (clamped
/// at 16) before it sizes the radii and sorts the primitives. `arg1` is the
/// signed half-extent, and `arg2` packs `[flicker shift][r][g][b]` nibbles,
/// with `gDisplayState.animFrame & 1` shifted by the top nibble added to every
/// channel.
void func_shelter_r48_8018258C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw05Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                ua;
    s32                ub;
    s32                uc;
    s32                frame;
    s32                packed;
    s32                blend;
    s32                r;
    s32                g;
    s32                b;
    s32                outer;
    s32                inner;
    s32                hr;
    s32                hg;
    s32                hb;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        tmp     = (*scratch = (u8*)*scratch - 0x14);
        block   = (RoomDraw05Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz -= 0xC0;
        if (block->otz < 0x10) {
            block->otz = 0x10;
        }
        arg1        <<= 16;
        arg1        >>= 16;
        outer         = (arg1 * 64) / block->otz;
        frame         = gDisplayState.animFrame;
        block->rOuter = outer;
        inner         = (arg1 * 8) / block->otz;
        ang           = 0;
        packed        = arg2 << 16;
        blend         = (frame & 1) << (packed >> 28);
        r             = blend + ((packed >> 20) & 0xF0);
        g             = blend + ((packed >> 16) & 0xF0);
        b             = blend + ((arg2 & 0xF) << 4);
        block->rInner = inner;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            hr = (u8)r >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            hg = (u8)g >> 1;
            hb = (u8)b >> 1;
            setRGB2(prim, hr, hg, hb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        r   = (u8)hr;
        g   = (u8)hg;
        b   = (u8)hb;
        do {
            ua             = ang - 0x400;
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ua)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(ua)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            ub       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ub)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(ub)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(ub)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(ub)) >> 11);
            uc       = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(uc)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(uc)) >> 12);
            ang      = uc;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
}
